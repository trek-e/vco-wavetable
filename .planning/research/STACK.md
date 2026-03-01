# Stack Research

**Domain:** VCV Rack 2 polyphonic wavetable oscillator plugin (C++)
**Researched:** 2026-03-01
**Confidence:** HIGH — verified against local Rack SDK 2.6.6, official docs, and compile.mk

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| VCV Rack SDK | 2.6.6 (current) | Module framework, DSP utilities, UI widgets, build system | Only supported way to build Rack 2 plugins; provides `rack.hpp` with all module lifecycle, SIMD, DSP helpers, and port/param APIs |
| C++11 | `-std=c++11` (enforced by compile.mk) | Plugin language standard | Hardcoded in `compile.mk`; the SDK enforces this. Do not attempt C++14/17 without verifying SDK compatibility first |
| GNU Make + plugin.mk | Bundled with SDK | Build system | VCV Rack's Makefile-based build is the standard. CMake wrappers exist but are community-maintained and add complexity with no benefit for a single-plugin project |
| GCC / Clang | Platform default (macOS arm64 / x64 both supported) | Compiler | SDK 2.6.6 distributes separate SDKs for mac-arm64 and mac-x64. The project's existing Makefile uses `RACK_DIR ?= ../Rack-SDK` and already resolves this correctly |

**Build flags confirmed from local `compile.mk`:**
```
-std=c++11 -O3 -funsafe-math-optimizations -fno-omit-frame-pointer
-march=nehalem  (x64 — SSE4.2 baseline)
-march=armv8-a+fp+simd  (arm64)
```

### DSP Utilities (SDK-Provided — Use These First)

These are already available via `#include "plugin.hpp"` → `rack.hpp`. No additional install required.

| Utility | Header | Purpose | Notes |
|---------|--------|---------|-------|
| `simd::float_4` | `simd/Vector.hpp` | SIMD 4-lane float vector | Process 4 voices simultaneously; `getPolyVoltageSimd<float_4>(c)` for polyphonic input reads |
| `simd::functions` | `simd/functions.hpp` | SIMD `sin`, `pow`, `floor`, `trunc`, `ifelse`, etc. | Use instead of `std::sin` in DSP loops for 4x throughput |
| `dsp::minblep.hpp` | `dsp/minblep.hpp` | MinBLEP band-limited step generation | For any waveform with discontinuities; `dsp::minBlepImpulse()` + ring buffer pattern (see existing VCO reference) |
| `dsp::approx.hpp` | `dsp/approx.hpp` | Fast math: `dsp::exp2_taylor5()`, approximations | Use for pitch CV → frequency conversion in hot path |
| `dsp::digital.hpp` | `dsp/digital.hpp` | `SchmittTrigger`, `PulseGenerator`, `ClockDivider` | Gate output timing, trigger detection |
| `dsp::filter.hpp` | `dsp/filter.hpp` | RC filter, biquad, one-pole | Optional anti-aliasing post-oscillator |
| `dsp::resampler.hpp` | `dsp/resampler.hpp` | `SampleRateConverter` via libsamplerate | If wavetable playback needs rate conversion |
| `math::interpolateLinear()` | `math.hpp` | Linear interpolation between table values | Use for interpolated wavetable scanning mode |
| `pffft.h` | `dep/include/pffft.h` | PFFFT fast FFT | Available but not needed for this module |
| `samplerate.h` | `dep/include/samplerate.h` | libsamplerate (high-quality SRC) | Available if wavetable pitch-shifting requires resampling |

### Supporting Libraries (Must Be Vendored — Not in SDK)

The Rack SDK does **not** include a WAV file loading library. You must vendor one in `src/`.

| Library | Version | Purpose | How to Use |
|---------|---------|---------|------------|
| **dr_wav** | v0.13.16 (header-only) | WAV file decoding for user wavetable loading | Copy `dr_wav.h` into `src/`. Add `#define DR_WAV_IMPLEMENTATION` in one `.cpp` file. VCV Fundamental plugin uses this exact approach (`src/dr_wav.h` / `src/dr_wav.c`). Supports 8/16/24/32-bit int and 32-bit float WAV. |
| **AudioFile.h** (alternative) | v1.1.1+ | WAV/AIFF decoding, simpler API | Single-header, more readable API than dr_wav. Community consensus: use with `rack::asset::plugin()` for path resolution. Has known Unicode path issues on Windows — test carefully. |

**Recommendation: Use dr_wav.** Reason: VCV's own Fundamental plugin uses it, ensuring compatibility with the exact same WAV format constraints (8/16/24/32-bit int, 32-bit float) that VCV documents. The Fundamental VCO2 is the direct precedent for this module's WAV loading behavior.

### Panel Design Tools

| Tool | Version | Purpose | Notes |
|------|---------|---------|-------|
| Inkscape | 1.x | SVG panel creation | Set units to mm. Height = 128.5 mm. Width = N × 5.08 mm. All text must be converted to paths (Path > Object to Path). |
| VCV Panel Guide layers | — | Component placement via SVG layers | Red circles = params, green = inputs, blue = outputs, magenta = lights. VCV's `helper.py` auto-generates C++ from layer labels. |

**Panel width estimate for Hurricane 8:**
- 8 individual voice outputs + 8 gate outputs = 16 mono jacks
- 3 polyphonic connectors (voice, gate, velocity)
- 1 mix output
- ~10 knobs + CV inputs for scanning, LFO, glide, bit depth, start position
- Minimum viable: 28–32 HP. Comfortable: 34–38 HP. (Verify during panel layout phase)

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `make` | Build plugin | `make` → build, `make dist` → zip, `make install` → copy to Rack plugins dir |
| `helper.py` (SDK) | Scaffold new module, generate C++ stubs from SVG layers | Run `python3 helper.py createmodule <ModuleName>` inside plugin dir |
| `jq` | Parses `plugin.json` slug in Makefile | Already required by existing project Makefile |
| VCV Rack 2.6.6 | Runtime testing | Load plugin via `make install` target in existing Makefile |
| osdialog | `dep/include/osdialog.h` | Cross-platform file picker for "Load wavetable" menu item | Already in SDK deps; use `osdialog_file(OSDIALOG_OPEN, ...)` in context menu handler |
| Jansson | `dep/include/jansson.h` | JSON for `dataToJson()` / `dataFromJson()` | Already in SDK; use for serializing wavetable path and module state to patch |

---

## Installation

The SDK is already present at `../Rack-SDK` relative to the plugin directory. No package manager needed.

```bash
# Clone/init the plugin (from vcvrack_modules/ dir)
mkdir vco-wavetable && cd vco-wavetable
python3 ../Rack-SDK/helper.py createplugin Hurricane8

# Scaffold the wavetable oscillator module
python3 ../Rack-SDK/helper.py createmodule Hurricane8

# Vendor dr_wav (download from https://github.com/mackron/dr_libs)
curl -O https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h
# Place in src/dr_wav.h

# Build
RACK_DIR=../Rack-SDK make

# Install to Rack 2 plugins directory
RACK_DIR=../Rack-SDK make install
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| GNU Make + plugin.mk | CMake (StoneyDSP/Rack-SDK) | Only if you need IDE integration (CLion/VS Code CMake extension) and are comfortable with an unofficial wrapper. Adds complexity for no audio quality benefit. |
| dr_wav (header-only) | AudioFile.h | AudioFile.h has a cleaner API and handles AIFF too. Use it if you want AIFF support or find dr_wav's C-style API uncomfortable. Must test Unicode paths on Windows. |
| AudioFile.h / dr_wav | libsndfile | libsndfile is a shared library, not header-only. Linking a shared lib from a VCV Rack plugin creates platform-specific distribution pain. Avoid. |
| SDK SIMD (float_4) | Manually written SSE intrinsics | Only if profiling shows SDK SIMD is a bottleneck (it won't be). SDK SIMD uses SSE4.2 on x64 and NEON on arm64 transparently. |
| Rack SDK DSP filters | JUCE DSP / KFR | These are large external dependencies with incompatible licensing. The SDK filter.hpp covers all anti-aliasing needs for this oscillator. |
| Makefile build | rack-plugin-toolchain (Docker) | Only for cross-platform release builds targeting all 3 OSes from one machine. Not needed during development on macOS. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `new` / `delete` in `process()` | Heap allocation in audio callback causes glitches and priority inversion. Banned by VCV real-time constraint. | Pre-allocate all voice state in module constructor as arrays. Stack arrays or module-level members only. |
| `std::string`, STL containers in `process()` | May allocate. Also banned in the hot path. | Use fixed-size arrays. Reserve any STL containers at init time. |
| File I/O in `process()` | WAV loading blocks the audio thread. VCV explicitly warns against this. | Load WAV in `onAdd()` event or in a background thread with mutex. Write path to JSON in `dataToJson()`. |
| `sleep()`, `mutex::lock()` in `process()` | Blocking in audio callback causes dropouts. | Use `std::atomic` for flag communication between threads. |
| libsndfile (shared library) | Requires distributing a shared lib with the plugin zip, with platform-specific linking. | dr_wav or AudioFile.h (both header-only). |
| C++14 or C++17 features | compile.mk explicitly sets `-std=c++11`. Using newer features will break cross-platform builds via the toolchain. | Stay within C++11. Use SDK-provided alternatives (e.g., `rack::string::f()` instead of `std::format`). |
| Polyphonic cable channel count > 16 | Hard limit in VCV Rack engine. Attempting to set more causes undefined behavior. | 8 voices is well within the 16-channel limit. |
| Fixed-only 8-bit quantization | Locks users out of cleaner sounds and limits usefulness. | Implement switchable 8/12/16-bit DAC emulation as described in PROJECT.md. |

---

## Stack Patterns by Use Case

**For polyphonic voice processing (8 voices, SIMD-optimized):**
- Process 4 voices per SIMD group using `simd::float_4`
- Loop: `for (int c = 0; c < channels; c += 4)` with `float_4` accumulation
- Voice state arrays: `float phase[16]` (allocate for max 16, use 8)
- Get poly input: `inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c)`
- Set poly output: `outputs[POLY_OUT].setVoltageSimd(voltage, c)`

**For wavetable lookup (interpolated mode):**
- Store wavetable as `float table[numFrames][frameSize]`
- Phase accumulates 0.0–1.0 per cycle
- Read: `math::interpolateLinear(frame, phase * frameSize)`
- Stepped mode: `table[frameIdx][(int)(phase * frameSize)]` (no interpolation)

**For DAC bit depth emulation:**
- Apply quantization after wavetable lookup: `std::round(sample * maxVal) / maxVal`
- 8-bit: maxVal = 127.0f, 12-bit: maxVal = 2047.0f, 16-bit: maxVal = 32767.0f
- Apply in the SIMD path using `simd::round()`

**For wavetable file loading (user WAVs):**
- Use dr_wav in context menu handler (not in `process()`)
- Path resolution: `rack::asset::plugin(pluginInstance, "samples/ppg.wav")` for bundled tables
- User files: `osdialog_file()` → path stored in module state → load in `onAdd()`
- Serialize path in `dataToJson()` so patch reload reloads the wavetable

**For individual voice outputs + polyphonic output (both simultaneously):**
- Write voice N to both: `outputs[VOICE_OUT + n].setVoltage(v)` AND `outputs[POLY_OUT].setVoltage(v, n)`
- Set poly channel count once on each sample rate change or voice count change: `outputs[POLY_OUT].setChannels(numVoices)`

---

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Rack SDK 2.6.6 | Rack 2.x (any minor) | Plugin MAJOR version in plugin.json must be "2". Minor/patch can be anything. |
| Rack SDK 2.6.6 | Rack 3 (forward) | PROJECT.md goal. Avoid deprecated `rack0.hpp` compatibility shim. Use only `rack.hpp` APIs. Rack 3 is announced but no release date as of 2026-03. |
| dr_wav v0.13+ | C++11 | header-only, compiles under `-std=c++11`. |
| `simd::float_4` | x64 SSE4.2, arm64 NEON | SDK 2.6.6 ships separate mac-x64 and mac-arm64 SDK archives. The existing project Makefile does not hardcode arch — it inherits from `RACK_DIR`. |
| `-march=nehalem` | Any CPU from ~2008+ | Enables SSE4.2. All modern Macs (including Apple Silicon via Rosetta or native arm64 build) support this. |

---

## Sources

- Local Rack SDK 2.6.6 `compile.mk` — confirmed C++11 standard, -O3, -march=nehalem (HIGH confidence, direct inspection)
- Local Rack SDK 2.6.6 `include/` and `dep/include/` — full list of SDK-bundled headers (HIGH confidence, direct inspection)
- [VCV Rack Downloads](https://vcvrack.com/downloads/) — SDK 2.6.6 is current as of 2026-03 (HIGH confidence, official source)
- [VCV Rack Plugin API Guide](https://vcvrack.com/manual/PluginGuide) — Module lifecycle, polyphonic API, SIMD patterns, file I/O (HIGH confidence, official docs)
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony) — 16-channel max, SIMD channel processing (HIGH confidence, official docs)
- [VCV Rack Panel Guide](https://vcvrack.com/manual/Panel) — 128.5mm height, 5.08mm HP, SVG layer conventions (HIGH confidence, official docs)
- [VCV Rack News: Rack 2.6 Released](https://vcvrack.com/news/2025-03-27-Rack-2.6) — Rack 3 development announced, no release date (HIGH confidence, official announcement)
- [VCV Community: Best way to incorporate WAV audio](https://community.vcvrack.com/t/best-way-to-incorporate-wav-audio-into-a-module/16920) — dr_wav and AudioFile.h community recommendations (MEDIUM confidence, community consensus)
- VCVRack/Fundamental GitHub — dr_wav vendored as `src/dr_wav.h` and `src/dr_wav.c` (HIGH confidence, official plugin source)
- Local reference plugin (`../vco/`) — confirms Makefile pattern, plugin.json structure, SIMD VCO implementation (HIGH confidence, first-party reference)

---

*Stack research for: VCV Rack 2 polyphonic wavetable oscillator plugin*
*Researched: 2026-03-01*
