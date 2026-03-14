---
id: S01
parent: M001
milestone: M001
provides:
  - PPG-style wavetable ROM data (32 tables, 64 frames, 256 samples each)
  - Mip-mapped band-limited wavetable engine (DFT analysis, 8 mip levels)
  - Phase accumulator with 1V/oct pitch tracking
  - PPG stepped scanning (zero-crossing frame switching)
  - Interpolated scanning (smooth crossfade between frames)
  - DAC bit-depth emulation (8/12/16-bit)
  - Complete single-voice wavetable oscillator pipeline
  - VCV Rack module stub with build system
requires: []
affects:
  - S02
key_files:
  - src/ppg_wavetables.hpp
  - src/wavetable_engine.hpp
  - src/Hurricane8VCO.cpp
  - tests/test_dsp_foundation.cpp
  - Makefile
  - plugin.json
key_decisions:
  - Wavetable data generated via additive synthesis (not ROM dumps) — avoids copyright issues while producing authentic PPG-style timbral progressions
  - Mip-mapping built from DFT analysis + band-limited resynthesis rather than simple low-pass filtering — eliminates phase errors at octave transitions
  - DSP engine headers are Rack-SDK-independent — enables standalone unit testing without full Rack build environment
  - Wavetable ROM and bank are static singletons shared across all module instances — ~2MB one-time cost, init takes ~670ms at plugin load
  - 256 samples per frame (power of 2) allows bitwise wrapping in lookup — no branch/modulo in hot path
patterns_established:
  - Pure DSP headers (no Rack dependency) tested via standalone g++ test binary
  - `make test` target runs DSP unit tests independently of Rack module build
  - Test framework uses macro-based registration (TEST/EXPECT/EXPECT_NEAR) with no external dependencies
observability_surfaces:
  - `make test` runs 42 unit tests with pass/fail counts and performance timing
  - Performance test reports µs/48k-samples (budget vs actual)
drill_down_paths: []
duration: 45min
verification_result: passed
completed_at: 2026-03-13
---

# S01: Dsp Foundation

**Complete wavetable DSP engine with mip-mapped band-limited playback, PPG scanning modes, DAC emulation, and 42 passing unit tests.**

## What Happened

Built the entire DSP foundation for Hurricane 8 from scratch — no prior source code existed in the project.

**PPG Wavetable ROM** (`ppg_wavetables.hpp`): Generated 32 wavetable banks using additive synthesis, each containing 64 single-cycle frames of 256 samples. Tables progress through: sawtooth harmonic series (0-3), square/pulse waves (4-7), PWM sweeps (8-11), triangle morphing with foldback (12-15), vocal/formant waveforms (16-19), digital noise textures via wavefolding (20-23), oscillator sync emulation (24-27), and organ drawbar combinations (28-31). All frames are normalized to prevent clipping.

**Mip-Mapped Wavetable Engine** (`wavetable_engine.hpp`): Built a band-limiting system using DFT analysis. For each of the 2,048 frames (32×64), the engine performs a 256-point DFT to extract harmonic amplitudes, then resynthesizes 8 mip levels with progressively fewer harmonics (128 → 64 → 32 → ... → 1). At runtime, `selectMipLevel()` chooses the appropriate level based on oscillator frequency vs. sample rate. `readSampleSmooth()` crossfades between adjacent mip levels for seamless transitions.

**Phase Accumulator**: Standard 1V/oct tracking using `261.626 * pow(2, volts)`. Wraps phase via floor-subtract to handle large FM jumps. Clamps delta to 0.49 to prevent instability near Nyquist.

**Scanning Modes**: Stepped mode (PPG-authentic) buffers frame changes until a zero crossing is detected — this eliminates clicks during table switching, exactly as the original PPG hardware behaved. Interpolated mode crossfades between adjacent frames using fractional position for modern smooth timbral animation.

**DAC Emulation**: Quantizes output to 256/4096/65536 levels (8/12/16-bit) using scale-round-rescale. The 8-bit mode produces the characteristic staircase distortion of vintage DACs.

**Module Stub** (`Hurricane8VCO.cpp`): Minimal but functional VCV Rack module with wavetable bank selection, position control with CV, scan mode switch, and DAC depth selector. Single-voice for now — polyphonic routing is S02's scope. Compiles clean against Rack SDK with zero warnings.

## Verification

- 42 unit tests: all pass (42/42)
- Tests cover all 7 DSP requirements plus WTD-01 (bundled wavetable set)
- Performance: single voice processes 48k samples in ~233-811 µs (budget: 1,000,000 µs)
- Module builds cleanly against VCV Rack 2 SDK with zero compiler warnings
- `make test` runs standalone test suite; `make` builds the Rack module

## Requirements Advanced

- DSP-01 — Phase accumulator with 1V/oct tracking verified across 10+ octaves (C-1 to C9), tested with frequency accuracy at 1kHz
- DSP-02 — Mip-mapped band-limited wavetable playback implemented and verified: DFT analysis, 8 mip levels, smooth inter-level crossfading
- DSP-03 — Wavetable position controllable via parameter and CV input (module stub wired up)
- DSP-04 — PPG stepped scanning implemented with zero-crossing detection, tested for correct frame-switch timing
- DSP-05 — Interpolated scanning with smooth crossfade between adjacent frames, tested for continuity
- DSP-06 — Both scan modes implemented as enum, switchable at runtime, tested for correct behavior in each mode
- DSP-07 — DAC bit-depth emulation at 8/12/16-bit with correct quantization levels, tested for step sizes and sign preservation
- WTD-01 — 32 PPG-style wavetable banks generated and verified (all non-zero, all finite, all mip-mapped)

## Requirements Validated

- none — DSP requirements need integration with polyphonic routing (S02) for full validation

## New Requirements Surfaced

- none

## Requirements Invalidated or Re-scoped

- none

## Deviations

none

## Known Limitations

- Wavetable initialization takes ~670ms at plugin load (DFT of 2048 frames). Acceptable for plugin startup but would need lazy-loading if more tables are added.
- Module stub is single-voice only — polyphonic routing deferred to S02.
- No panel SVG — deferred to S05.
- ROM wavetables are mathematically generated approximations, not authentic PPG ROM dumps.

## Follow-ups

- S02 needs to wire up 8 independent `WavetableVoice` instances with polyphonic cable routing
- Consider lazy-loading mip-map generation if user wavetable loading (S04) increases init time significantly

## Files Created/Modified

- `src/ppg_wavetables.hpp` — PPG-style wavetable ROM data generator (32 tables × 64 frames × 256 samples)
- `src/wavetable_engine.hpp` — Mip-mapped band-limited wavetable engine (MipMappedTable, WavetableBank, WavetableScanner, DacEmulator, PhaseAccumulator, WavetableVoice)
- `src/Hurricane8VCO.cpp` — VCV Rack module stub with single-voice wavetable oscillator
- `src/plugin.cpp` — VCV Rack plugin registration
- `src/plugin.hpp` — Plugin header with model declaration
- `tests/test_dsp_foundation.cpp` — 42 unit tests covering all DSP requirements
- `plugin.json` — VCV Rack plugin manifest
- `Makefile` — Build system with `make` (module) and `make test` (unit tests) targets

## Forward Intelligence

### What the next slice should know
- `WavetableVoice` is the per-voice struct. S02 needs an array of 8 and polyphonic I/O routing.
- `wavetableBank` is a static singleton — all module instances share it. No per-instance allocation needed.
- The DSP headers are Rack-SDK-independent. Keep it that way for testability.
- `Hurricane8VCO.cpp` currently has a minimal param/input/output set. S02 will expand this significantly.

### What's fragile
- Mip-map initialization does ~2048 DFT operations at plugin load. Adding more tables without lazy-loading will slow startup.
- The stepped scanning zero-crossing detection relies on `lastSample` sign tracking — if the waveform has DC offset, zero crossings may not occur where expected.

### Authoritative diagnostics
- `make test` — 42 tests with clear pass/fail output and performance timing
- Test names map directly to requirement IDs (e.g., `voct_*` → DSP-01, `mipmap_*` → DSP-02)

### What assumptions changed
- Original plan had no tasks defined — the entire DSP foundation was designed and built in a single implementation pass.
