# Project Research Summary

**Project:** Hurricane 8 — PPG-Inspired Polyphonic Wavetable Oscillator
**Domain:** VCV Rack 2 C++ plugin, wavetable DSP, 8-voice polyphony
**Researched:** 2026-03-01
**Confidence:** HIGH

## Executive Summary

Hurricane 8 is a polyphonic wavetable oscillator VCV Rack plugin that emulates the PPG Wave's distinctive stepped-scanning and DAC-quantization character with full 8-voice polyphony. The VCV Rack SDK 2.6.6 is a well-documented, self-contained build environment — all necessary DSP utilities (SIMD float_4, interpolation, slew limiters), file I/O helpers (osdialog, Jansson), and the build toolchain (GNU Make + plugin.mk, C++11) are bundled. The only external dependency is dr_wav (single header, already used by VCV Fundamental) for WAV file loading. No package manager is required; the existing project Makefile and SDK at `../Rack-SDK` are already configured.

The recommended approach is to build bottom-up: establish the core DSP engine (phase accumulator, wavetable bank with mip-maps, DAC emulation) as the monophonic foundation, then layer in polyphonic routing, modulation (ASR envelope, glide, LFO), user wavetable loading (off-thread), and finally the full panel and individual voice outputs. This ordering is dictated by hard architectural dependencies — voices cannot exist without a wavetable bank, the polyphonic output stage cannot exist without working per-voice DSP, and the panel cannot be finalized until HP width is locked by the jack count. No existing VCV Rack wavetable module combines authentic PPG stepped scanning, explicit 8/12/16-bit DAC emulation, the original PPG ROM tables, and per-voice wavetable-position envelopes — this combination is the unique value proposition.

The dominant risk cluster is real-time correctness: memory allocation in `process()`, file I/O on the audio thread, float phase accumulator denormals, and aliasing without mip-mapped tables. All five critical pitfalls identified must be addressed in the first DSP phase — they cannot be retrofitted cheaply. The secondary risk is VCV Library compliance: polyphonic channel handling, correct `setChannels()` calls, and SVG panel standards must be correct from the start to avoid late rework.

---

## Key Findings

### Recommended Stack

The VCV Rack SDK 2.6.6 provides everything needed except a WAV decoder. Use dr_wav (header-only, vendored in `src/dr_wav.h`) as VCV Fundamental does. The C++11 constraint is hard-enforced by compile.mk — no newer standard features. SIMD processing with `simd::float_4` lets 4 voices share one ALU pass, reducing 8-voice cost to roughly 2x mono. All panel tooling (osdialog file picker, Jansson JSON, SVG + helper.py scaffolding) is SDK-bundled.

**Core technologies:**
- VCV Rack SDK 2.6.6: Module framework, DSP utilities, build system — the only supported plugin mechanism
- C++11 / GNU Make + plugin.mk: Enforced by compile.mk; do not attempt C++14/17
- `simd::float_4`: Process 4 voices per SIMD op via `getPolyVoltageSimd<float_4>(c)` — the hot-path pattern
- dr_wav (header-only): WAV decoding; vendored in `src/`; proven by VCV Fundamental
- osdialog (SDK dep): Cross-platform file picker for right-click "Load wavetable" menu
- Jansson (SDK dep): JSON serialization for `dataToJson`/`dataFromJson` patch persistence

### Expected Features

The VCV Rack community expects any polyphonic oscillator to have 1V/oct pitch tracking, a polyphonic cable output, CV-controllable wavetable position, user-loadable WAVs, a mix output, polyphonic gate output, and right-click context menu access — these are table stakes. Hurricane 8's differentiators are orthogonal to what any competitor currently offers: no VCV module combines all of stepped PPG scanning, explicit DAC bit-depth emulation, bundled PPG ROM tables, and per-voice wavetable-position envelopes.

**Must have (table stakes):**
- 1V/oct polyphonic pitch tracking — without this the module can't integrate with any patch
- Polyphonic audio and gate cable outputs — VCV Rack 2 norm
- Wavetable position control with CV input — core identity of any wavetable VCO
- User-loadable single-cycle WAV — expected since VCV Fundamental VCO-2
- Mix (summed) output — required for downstream filter/effects routing
- Right-click context menu with wavetable load — VCV UI convention
- Panel SVG meeting VCV Library standards + correct plugin.json versioning

**Should have (competitive differentiators for v1):**
- PPG stepped scanning mode (authentic zero-crossing switch, not crossfade) — no competitor has this
- Configurable DAC bit depth emulation (8/12/16-bit) — defines PPG era character
- Bundled PPG Wave ROM wavetable set — gives immediate PPG identity without user setup
- Per-voice ASR envelope for wavetable position (Env3 equivalent) — expressive polyphony
- Interpolated scanning mode as switchable alternative to stepped — modern fallback
- Glide (portamento) per-voice with CV input — PPG playing style essential
- Internal LFO for vibrato — reduces patch complexity for common use case
- Unison mode with detune — wall-of-sound PPG pad character
- Multi-frame WAV support (Serum-style 2048-sample) — expands usable wavetable libraries
- 8 individual voice audio + gate outputs (jacks + poly cable) — power-user differentiator

**Defer (v2+):**
- Wavetable start position as a dedicated control knob — nuance feature, low discovery
- Velocity CV pass-through output — low priority
- Additional bundled wavetable sets beyond PPG ROM — content work after core ships
- Wavetable editor / display visualization — out of scope for v1

**Anti-features (explicitly reject):**
- Built-in VCF, VCA, or effects — scope creep, defeats VCV modular philosophy
- MIDI input — handled by upstream MIDI-CV module
- 2D XY wavetable morphing — different synthesis paradigm; the 1D PPG scan IS the identity
- 16-voice mode — PPG was 8 voices; doubles cost and jack count with no PPG authenticity gain

### Architecture Approach

VCV Rack enforces a hard engine/UI thread split. All DSP lives in `Module::process()` on the audio thread; all widget rendering and file picking lives in `ModuleWidget` on the UI thread. The canonical architecture is a fixed array of 8 Voice structs (no heap allocation in process), a shared WavetableBank with atomic pointer swap for thread-safe user table loading, and an Output Router that writes each voice to both individual jacks and the polyphonic cable simultaneously. Build order follows strict dependencies: PPG ROM tables → WavetableBank → AHREnvelope + DacEmulator → Voice → Hurricane8 Module → WavetableLoader → Hurricane8Widget → plugin.cpp.

**Major components:**
1. `WavetableBank` — stores all tables (PPG ROM as static constexpr + user-loaded); provides sample lookup; mip-map management
2. `Voice[8]` — per-voice DSP: phase accumulator (double), glide SlewLimiter, wavetable-position AHR envelope, DAC quantization
3. `WavetableLoader` — off-thread WAV decode (dr_wav); atomic pointer swap into WavetableBank; mip-map rebuild
4. `Hurricane8 Module` — process() entry: voice routing (poly vs unison), param/CV reads, output dispatch
5. `Hurricane8Widget` — SVG panel, context menu, file picker; communicates with Module only via params/lights arrays
6. Output Router (within Module) — per-voice jacks (8 audio + 8 gate), poly cables (audio/gate/velocity), mix sum

### Critical Pitfalls

1. **Memory allocation or file I/O in process()** — pre-allocate all voice state in constructor; load WAVs off-thread via `std::thread` + atomic swap; never touch heap in the audio callback
2. **No mip-mapped wavetable tables** — aliasing above C5 is severe and visible to any user; must generate one band-limited table per octave at initialization, selected per-voice based on current pitch; cannot be retrofitted cheaply
3. **Float phase accumulator denormals** — use `double` for all phase accumulators from day one; VCV Rack 2 engine enables FTZ/DAZ flags but double precision eliminates drift for all practical note lengths
4. **Non-compliant polyphonic channel handling** — loop over `inputs[VOCT_INPUT].getChannels()` capped at 8; call `outputs[POLY_OUT].setChannels(numVoices)` every process() call; test with 1, 2, 4, and 8 poly channels
5. **Wavetable position stepping transition clicks** — keep phase accumulator running continuously across table-index changes; change table index only at zero crossings or apply a 1–2 sample crossfade; never reset phase on a table switch

---

## Implications for Roadmap

The architecture's build-order dependency graph directly dictates phase structure. Five phases map cleanly from the bottom-up component order.

### Phase 1: DSP Foundation (Core Oscillator Engine)

**Rationale:** Every other component depends on a working, anti-aliased, real-time-safe wavetable engine. Mip-maps and double-precision phase must be in from the start — both are non-retrofittable. This phase validates the PPG character before any polyphonic plumbing.

**Delivers:** Monophonic PPG wavetable oscillator with stepped and interpolated scanning, DAC bit-depth emulation, mip-mapped anti-aliasing, correct 1V/oct tracking at any sample rate.

**Addresses:** PPG wavetable ROM bundled, stepped scanning mode, DAC emulation, 1V/oct pitch tracking, phase accumulator correctness.

**Avoids:** All five critical pitfalls — mip-maps (Pitfall 2), double phase (Pitfall 4), stepped clicking (Pitfall 5), no allocation in process() (Pitfall 1), sample rate hard-coding (Technical Debt table).

**Research flag:** Needs phase research — mip-map generation from PPG 64-sample tables to modern frame sizes has specific implementation tradeoffs (FFT zeroing vs. polynomial approximation). PPG ROM licensing also needs a concrete decision before shipping.

---

### Phase 2: Polyphonic Integration + Voice Routing

**Rationale:** Polyphonic channel handling is architecturally foundational and must be established before any modulation or output features are added on top. Getting this right early prevents a systematic rewrite later.

**Delivers:** Full 8-voice polyphonic oscillator — all voices routed from poly V/OCT and gate inputs, all outputs (poly audio, poly gate, mix) using correct `setChannels()`, unpatched-cable edge cases handled.

**Addresses:** Polyphonic audio output, polyphonic gate output, mix output, polyphonic channel compliance.

**Avoids:** Pitfall 3 (non-compliant poly handling), integration gotchas (wrong setChannels, mono gate reads, mix normalization).

**Research flag:** Standard VCV Rack patterns — no additional research needed. Official polyphony manual + community references cover this thoroughly.

---

### Phase 3: Modulation (Per-Voice Envelope, Glide, LFO)

**Rationale:** Modulation features (ASR envelope for wavetable position, glide, internal LFO) all attach to the Voice struct and are architecturally isolated from polyphonic routing. Building them after polyphony is stable means they can be tested correctly across voices immediately.

**Delivers:** Per-voice AHR envelope driving wavetable scan position, per-voice glide SlewLimiter, internal vibrato LFO, unison mode with detune spread.

**Addresses:** Per-voice ASR envelope (Env3 equivalent), glide with CV, internal LFO, unison mode.

**Avoids:** Per-voice gate tracking errors (UX pitfall: ASR triggering on wrong voice), glide applied globally instead of per-voice.

**Research flag:** Standard patterns for SlewLimiter and AHR envelope are well-documented in SDK. Unison mode detune spread calculation is straightforward but needs validation against PPG reference patches.

---

### Phase 4: User Wavetable Loading + State Persistence

**Rationale:** User WAV loading is architecturally isolated (WavetableLoader + atomic swap) and can be added cleanly after the core engine is proven. Multi-frame WAV support (Serum format) adds complexity and should be sequenced after single-cycle loading works.

**Delivers:** Right-click "Load wavetable" menu, osdialog file picker, dr_wav decode, single-cycle and multi-frame WAV support, off-thread mip-map rebuild, `dataToJson`/`dataFromJson` patch persistence of wavetable path.

**Addresses:** User-loadable single-cycle WAV, multi-frame WAV (Serum-style), state serialization.

**Avoids:** Pitfall 1 (file I/O on audio thread) via std::thread + atomic swap; mip-map rebuild after user load.

**Research flag:** Multi-frame WAV frame boundary detection (Serum loop marker tags) may need research — the format is not formally documented. Single-cycle loading has clear precedent from VCV Fundamental.

---

### Phase 5: Panel, Individual Voice Outputs, Library Submission

**Rationale:** The panel HP width cannot be finalized until the full jack count is confirmed, which requires knowing the individual voice output count. This phase is last because SVG rework is expensive — better to finalize once all features are known.

**Delivers:** Full SVG panel (30–40 HP), 8 individual voice audio jacks (V1–V8), 8 individual gate jacks (G1–G8), velocity CV pass-through output, all labels and visual feedback, plugin.json library manifest, library submission package.

**Addresses:** 8 individual voice audio + gate outputs, panel SVG standards, plugin.json versioning, velocity CV output (if confirmed in scope).

**Avoids:** Panel HP too narrow (UX pitfall: jacks too close), SVG text not converted to paths, NaN propagation (output safety checks), cross-platform build failures.

**Research flag:** VCV Library submission process — review current submission requirements. Panel width is a planning decision that needs to happen before SVG work begins, not after.

---

### Phase Ordering Rationale

- **Bottom-up dependency graph drives all ordering:** WavetableBank must exist before Voice, Voice must exist before Module, Module must exist before Widget. No phase can jump this sequence.
- **Real-time correctness is front-loaded:** All five critical pitfalls are Phase 1 concerns. Discovering any of them in Phase 4 or 5 requires touching all voice code. The recovery cost table in PITFALLS.md rates aliasing retrofit as HIGH.
- **Polyphonic compliance second:** Channel routing must be correct before modulation — wrong poly plumbing would contaminate every modulation feature test.
- **User wavetable loading is isolated:** The atomic swap pattern means WAV loading can be added to a working engine cleanly. It is not a dependency for any other feature.
- **Panel last:** HP width decision is critical but the decision is informed by the full feature set. Finalizing panel after all jacks are known avoids expensive SVG rework.

### Research Flags

Phases needing deeper research during planning:
- **Phase 1:** Mip-map generation from 64-sample PPG tables (FFT zeroing approach vs. polynomial approximation tradeoffs); PPG ROM licensing status (public domain vs. requires permission).
- **Phase 4:** Serum/multi-frame WAV format — loop marker and metadata tag specifications for frame boundary detection are not formally documented; needs specific format research.
- **Phase 5:** Current VCV Library submission requirements — verify all required plugin.json fields, required tags, and submission process for Rack 2.6.x.

Phases with standard patterns (skip research-phase):
- **Phase 2:** VCV Rack polyphony is thoroughly documented in official manual + community posts; established patterns cover all channel routing cases.
- **Phase 3:** SlewLimiter, AHR envelope, and LFO patterns are standard DSP, all SDK-provided; unison detune spread is arithmetic.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Direct inspection of local SDK 2.6.6 files (compile.mk, include/, dep/include/); official VCV docs; VCV Fundamental source as precedent |
| Features | MEDIUM | PPG stepped scanning behavior sourced from community analysis and hardware documentation; DAC bit depths from Wikipedia and PPG Wave technical literature; no single authoritative spec exists |
| Architecture | HIGH | VCV Rack Plugin API Guide + Polyphony Manual are official and detailed; open-source reference modules (BogaudioModules) confirm patterns; atomic swap and thread model are well-established |
| Pitfalls | HIGH | Sourced from official VCV Rack docs, canonical real-time audio references (Ross Bencina), and VCV community posts with high vote counts |

**Overall confidence:** HIGH

### Gaps to Address

- **PPG ROM table licensing:** Original PPG Wave 2 ROM data is the intended source (Hermann Seib's documentation). Whether it is unambiguously public domain or requires permission from Wolfgang Palm / PPG needs a concrete decision before shipping. Use a synthetic approximation as a stand-in if licensing is unclear.
- **Mip-map implementation approach for 64-sample source tables:** PPG waveforms are 64 samples — too small for standard FFT mip-map generation without upsampling first. The exact approach (upsample to 2048 first, then FFT-zero per octave) needs validation in Phase 1 planning.
- **Serum multi-frame WAV format specification:** Frame boundary detection relies on WAV loop markers and metadata that Serum writes — these are not formally documented. Research required before implementing multi-frame WAV in Phase 4.
- **HP width decision:** Panel width (estimated 30–40 HP) affects all SVG work and should be locked before Phase 5 begins. A concrete jack-count-to-HP calculation should be done at the end of Phase 3 or start of Phase 5.
- **Individual voice jacks in v1 vs v1.x:** FEATURES.md places individual voice outputs in "v1.x" (after validation), but Phase 5 groups them with panel work. If they are deferred, Phase 5 becomes significantly smaller. This decision should be made explicit in the roadmap.

---

## Sources

### Primary (HIGH confidence)
- Local Rack SDK 2.6.6 `compile.mk`, `include/`, `dep/include/` — C++11 standard, build flags, bundled headers
- [VCV Rack Plugin API Guide](https://vcvrack.com/manual/PluginGuide) — Module lifecycle, file I/O rules, thread safety
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony) — channel count rules, getChannels/setChannels patterns
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) — audio levels, gate thresholds, NaN handling
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) — aliasing, band-limiting, SIMD optimization
- [VCV Rack Panel Guide](https://vcvrack.com/manual/Panel) — SVG standards, 128.5mm height, component color coding
- VCVRack/Fundamental GitHub — dr_wav vendored pattern, WAV loading, VCO2 reference
- [Ross Bencina: Real-time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) — canonical real-time audio rules
- [EarLevel Engineering: Wavetable Oscillator Series](https://www.earlevel.com/main/2012/11/18/a-wavetable-oscillator-end-notes/) — mip-mapping, band-limiting
- [Hermann Seib PPG Wavetable documentation](https://www.hermannseib.com/documents/PPGWTbl.pdf) — PPG ROM structure reference

### Secondary (MEDIUM confidence)
- [Surge XT Wavetable VCO documentation](https://surge-synthesizer.github.io/rack_xt_manual/) — multi-frame WAV format, scanning modes
- [Gearspace PPG Wave interpolation discussion](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1273412-ppg-wave-doest-interpolate-between-single-cycles.html) — stepped scanning behavior confirmation
- [VCV Community: Best way to incorporate WAV audio](https://community.vcvrack.com/t/best-way-to-incorporate-wav-audio-into-a-module/16920) — dr_wav and AudioFile.h recommendations
- [VCV Community: Polyphony reminders for plugin developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572) — channel handling gotchas
- [BogaudioModules VCO](https://github.com/bogaudio/BogaudioModules) — polyphonic VCO reference implementation
- [PPG Wave Wikipedia](https://en.wikipedia.org/wiki/PPG_Wave) — hardware bit depth, voice count, stepping architecture
- [Groove Synthesis 3rd Wave 8M](https://groovesynthesis.com/8m/) and [PPG W2.2x4 Eurorack](https://synthanatomy.com/2025/09/ppg-w2-2x4-legendary-german-brand-makes-a-comeback-with-a-new-dual-wavetable-oscillator.html) — contemporary PPG-style market context

### Tertiary (LOW confidence)
- [VCV Community wavetable VCO discussion](https://community.vcvrack.com/t/wave-table-vco/14759) — community feature expectations (limited browsed portion)
- [KVR Audio: Wavetable oscillator implementation](https://www.kvraudio.com/forum/viewtopic.php?t=509014) — mip-map spacing, interpolation tradeoffs (needs validation against implementation)

---
*Research completed: 2026-03-01*
*Ready for roadmap: yes*

# Architecture Research

**Domain:** VCV Rack polyphonic wavetable oscillator module (C++ / VCV Rack SDK)
**Researched:** 2026-03-01
**Confidence:** HIGH (VCV Rack SDK documentation + official API reference + verified open-source patterns)

---

## Standard Architecture

### System Overview

VCV Rack enforces a hard two-layer split between DSP and UI. The engine thread runs `process()` at audio rate. The UI thread runs the widget rendering. They share state only through the Module struct's params/inputs/outputs/lights arrays, which the engine mediates safely.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        UI THREAD (App Layer)                         │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  ModuleWidget (Hurricane8Widget)                              │   │
│  │  - SVG panel rendering                                        │   │
│  │  - ParamWidget placement (knobs, switches, buttons)           │   │
│  │  - PortWidget placement (inputs, outputs)                     │   │
│  │  - LightWidget placement                                      │   │
│  │  - appendContextMenu() for user wavetable loading             │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │  reads params[], lights[] (safe)           │
├─────────────────────────┼───────────────────────────────────────────┤
│                  VCV Rack Engine Boundary                            │
├─────────────────────────┼───────────────────────────────────────────┤
│                        AUDIO THREAD (Engine Layer)                   │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  Module (Hurricane8)                                          │   │
│  │  process(const ProcessArgs& args)                             │   │
│  │                                                               │   │
│  │  ┌────────────────────────────────────────────────────────┐  │   │
│  │  │  Voice Manager                                          │  │   │
│  │  │  - Reads polyphonic input channels (up to 16)           │  │   │
│  │  │  - Routes to Voice[0..7] or routes unison               │  │   │
│  │  │  - Reads params, applies CV modulation                  │  │   │
│  │  └────────────────────────────────────────────────────────┘  │   │
│  │                                                               │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ... ┌──────────┐   │   │
│  │  │ Voice[0] │ │ Voice[1] │ │ Voice[2] │     │ Voice[7] │   │   │
│  │  │          │ │          │ │          │     │          │   │   │
│  │  │ - Phase  │ │ - Phase  │ │ - Phase  │     │ - Phase  │   │   │
│  │  │ - Glide  │ │ - Glide  │ │ - Glide  │     │ - Glide  │   │   │
│  │  │ - WT Env │ │ - WT Env │ │ - WT Env │     │ - WT Env │   │   │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘     └────┬─────┘   │   │
│  │       │             │             │                 │         │   │
│  │  ┌────▼─────────────▼─────────────▼─────────────────▼─────┐ │   │
│  │  │  WavetableEngine (shared read-only tables)               │ │   │
│  │  │  - Built-in PPG tables (ROM, static)                     │ │   │
│  │  │  - User-loaded tables (heap, swapped off-thread)         │ │   │
│  │  │  - Sample lookup + interpolation                         │ │   │
│  │  └─────────────────────────────────────────────────────────┘ │   │
│  │                                                               │   │
│  │  ┌─────────────────────────────────────────────────────────┐ │   │
│  │  │  Output Mixer                                            │ │   │
│  │  │  - Per-voice jacks (8x audio, 8x gate)                  │ │   │
│  │  │  - Polyphonic cable outputs (audio, gate, velocity)      │ │   │
│  │  │  - Summed mix output                                     │ │   │
│  │  └─────────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

| Component | Responsibility | Communicates With |
|-----------|----------------|-------------------|
| `Hurricane8Widget` | SVG panel, widget placement, context menu (load wavetable) | Module via params[], lights[], createMenuItem callbacks |
| `Hurricane8` (Module) | Owns all DSP state, process() entry point, voice routing | Rack engine (inputs/outputs), WavetableBank, Voice[8] |
| `Voice` struct | Single-voice DSP: phase accumulator, glide slew, WT position ASR envelope | WavetableEngine (read-only table access), SlewLimiter, AHREnvelope |
| `WavetableBank` | Stores all tables (PPG built-in + user-loaded); provides sample lookup | Voice (read-only during process), Widget (write during load, off-thread) |
| `WavetableLoader` | Loads WAV files off the audio thread, decodes frames, validates format | WavetableBank (writes new table pointer via atomic swap) |
| Output router | Distributes per-voice output to individual jacks, poly cable, and mix bus | Module inputs/outputs array |

---

## Recommended Project Structure

```
src/
├── Hurricane8.hpp          # Module and Widget declarations, param/input/output enums
├── Hurricane8.cpp          # ModuleWidget: panel layout, context menus, widget creation
├── Hurricane8Module.cpp    # Module: constructor, process(), dataToJson/dataFromJson
│
├── dsp/
│   ├── Voice.hpp           # Voice struct: all per-voice state and process()
│   ├── Voice.cpp           # Voice DSP implementation
│   ├── WavetableBank.hpp   # Table storage, sample lookup interface
│   ├── WavetableBank.cpp   # Built-in PPG ROM tables, user table management
│   ├── WavetableLoader.hpp # Off-thread WAV file loading
│   ├── WavetableLoader.cpp # WAV decode (libsamplerate or stb_vorbis-style)
│   ├── AHREnvelope.hpp     # ASR/AHR envelope for wavetable position modulation
│   └── DacEmulator.hpp     # Bit-depth quantization (8/12/16-bit DAC emulation)
│
├── ppg_tables/
│   └── PPGWavetables.hpp   # PPG Wave 2.3 ROM data as static constexpr arrays
│
└── plugin.cpp              # Plugin registration, createModel<Hurricane8>()
```

### Structure Rationale

- **`dsp/` subfolder:** Keeps all audio-thread code isolated from UI code. The `Voice` and `WavetableBank` are audio-thread-only; the `WavetableLoader` explicitly crosses the thread boundary with care.
- **`ppg_tables/` subfolder:** ROM data is large and should be in its own header to keep it out of the way. Declared `constexpr` so it lives in read-only segment, not heap.
- **Split Module/Widget files:** `Hurricane8.cpp` (widget) can change without touching DSP. `Hurricane8Module.cpp` (DSP) can change without recompiling widget code. This matches how well-structured VCV modules like BogaudioModules are organized.

---

## Architectural Patterns

### Pattern 1: Engine-per-Voice Array

**What:** Declare a fixed-size array of Voice structs (`Voice voices[8]`), one per polyphonic channel. Each voice is fully self-contained with its own phase, glide, and envelope state.

**When to use:** Always, for polyphonic modules. This is the canonical VCV Rack pattern (confirmed in official polyphony documentation and multiple open-source modules including BogaudioModules VCO).

**Trade-offs:** Fixed 8-voice limit avoids heap allocation in process(); all voice state lives on the stack frame of the Module struct. CPU cost scales linearly with active voice count.

**Example:**
```cpp
struct Hurricane8 : Module {
    Voice voices[8];   // fixed, no allocation

    void process(const ProcessArgs& args) override {
        int channels = inputs[VOCT_INPUT].getChannels();
        channels = clamp(channels, 1, 8);

        for (int c = 0; c < channels; c++) {
            float pitch = inputs[VOCT_INPUT].getPolyVoltage(c);
            float gate  = inputs[GATE_INPUT].getPolyVoltage(c);
            float wtPos = params[WTPOS_PARAM].getValue()
                        + inputs[WTPOS_CV_INPUT].getPolyVoltage(c) * 0.1f;

            voices[c].process(args.sampleTime, pitch, gate, wtPos, bank);

            outputs[AUDIO_OUTPUT].setVoltage(voices[c].output * 5.f, c);
            outputs[GATE_POLY_OUTPUT].setVoltage(voices[c].gateOut, c);
        }

        outputs[AUDIO_OUTPUT].setChannels(channels);
        outputs[GATE_POLY_OUTPUT].setChannels(channels);
    }
};
```

### Pattern 2: Phase Accumulator with Fractional Index

**What:** Each voice maintains a `double phase` in [0, 1). Each sample, advance by `freq / sampleRate`. Multiply by `tableSize` to get a fractional table index. Apply linear interpolation between adjacent samples.

**When to use:** Always for wavetable oscillators. This is standard practice.

**Trade-offs:** `double` phase prevents drift over long sustains at low pitches. Fractional index enables smooth timbre at all pitches. PPG hardware used no interpolation at the wavecycle level (integer index only), so the stepped/interpolated switch maps cleanly onto this: interpolated = linear interp, stepped = truncate to integer index.

**Example:**
```cpp
struct Voice {
    double phase = 0.0;    // [0, 1)
    float  output = 0.f;

    void process(float sampleTime, float pitchVolts, const WavetableBank& bank,
                 int tableIndex, int waveIndex, bool stepped) {
        float freq = dsp::FREQ_C4 * std::pow(2.f, pitchVolts);
        phase += freq * sampleTime;
        if (phase >= 1.0) phase -= 1.0;

        float tablePos = phase * WAVE_SIZE;   // WAVE_SIZE = 64 (PPG) or 2048 (modern)
        int   idx0 = (int)tablePos;
        float frac = tablePos - idx0;

        if (stepped) {
            output = bank.sample(tableIndex, waveIndex, idx0);
        } else {
            float s0 = bank.sample(tableIndex, waveIndex, idx0);
            float s1 = bank.sample(tableIndex, waveIndex, (idx0 + 1) % WAVE_SIZE);
            output = s0 + frac * (s1 - s0);
        }
    }
};
```

### Pattern 3: Glide via SlewLimiter

**What:** Use `rack::dsp::SlewLimiter` (or equivalent first-order lag) per voice to smooth pitch transitions. The slew rate is set from the GLIDE knob + CV.

**When to use:** Any module with portamento. The VCV Rack DSP library provides `SlewLimiter` directly. MEDIUM confidence — confirmed present in `rack::dsp` namespace from official API docs.

**Trade-offs:** Per-voice slew limiter means each voice can glide independently (correct for polyphonic portamento). Shared global glide time is simpler but sounds wrong in poly mode.

**Example:**
```cpp
struct Voice {
    dsp::SlewLimiter pitchSlew;

    void setGlide(float glideTime) {
        // SlewLimiter uses rise/fall in units/second
        float slewRate = (glideTime > 0.001f) ? (10.f / glideTime) : 1e6f;
        pitchSlew.setRiseFall(slewRate, slewRate);
    }

    float getSlewedPitch(float targetPitch, float sampleTime) {
        return pitchSlew.process(sampleTime, targetPitch);
    }
};
```

### Pattern 4: Off-Thread Wavetable Loading via Atomic Swap

**What:** WAV file I/O must never happen on the audio thread. Load the new table on a worker thread (std::thread or VCV's worker), then swap the active table pointer atomically so the audio thread sees it safely.

**When to use:** Any time user files must be loaded at runtime. This is required by VCV Rack's real-time constraints — file I/O in process() causes audio dropouts and crashes.

**Trade-offs:** Adds complexity. The swap must be atomic or protected. A simple approach: allocate new table, store in `std::atomic<WavetableData*>`, audio thread reads it. A double-buffer approach (ping-pong) avoids any lock. The old table must be freed off the audio thread too.

**Example:**
```cpp
// In WavetableBank
std::atomic<WavetableData*> userTable{nullptr};

// Called from UI thread (context menu action)
void loadFromFile(const std::string& path) {
    std::thread([this, path]() {
        WavetableData* newData = WavetableLoader::load(path);
        WavetableData* old = userTable.exchange(newData);
        delete old;  // safe: audio thread no longer holds old pointer
    }).detach();
}

// In process() — audio thread reads atomically
WavetableData* tbl = userTable.load(std::memory_order_relaxed);
if (tbl) { /* use tbl->samples */ }
```

### Pattern 5: ASR Envelope for Wavetable Position

**What:** Each voice has an Attack-Sustain-Release envelope whose output drives the wavetable scan position. Gate high = attack then sustain at top; gate low = release. In "per-voice" mode, each voice runs its own envelope triggered by its gate input. In "global" mode, all voices share the same envelope driven by any active gate.

**When to use:** PPG-style wavetable scanning. The envelope controls position in the wavetable (which waveform is active), not amplitude.

**Trade-offs:** Per-voice is more expressive but uses 8x the CPU for envelope computation. Given 8 voices max (not 16), this is fine.

**Example:**
```cpp
struct Voice {
    AHREnvelope wtEnvelope;   // drives wavetable position
    float wtPosition = 0.f;   // 0.0 = first wave, 1.0 = last wave

    void process(float sampleTime, float gate, float wtStart, float wtEnvAmt) {
        wtEnvelope.process(sampleTime, gate > 1.f);
        // wtStart sets the base position; envelope adds to it
        wtPosition = clamp(wtStart + wtEnvelope.out * wtEnvAmt, 0.f, 1.f);
    }
};
```

### Pattern 6: DAC Emulation via Quantization

**What:** After computing a float sample in [-1, 1], apply bit-depth quantization to emulate the PPG's 8-bit DACs or intermediate depths. `floor(sample * levels + 0.5f) / levels`.

**When to use:** After all interpolation, before writing to output. This is a post-process step, not part of the oscillator core.

**Trade-offs:** 8-bit quantization is audibly gritty (32 steps per half-cycle at standard tuning). 12-bit is warmer. 16-bit is effectively clean. The `levels = pow(2, bits) - 1`.

**Example:**
```cpp
float emulateDac(float sample, int bits) {
    float levels = (float)((1 << bits) - 1);
    return std::round(sample * levels) / levels;
}
```

---

## Data Flow

### Per-Sample Audio Flow (process() called ~44,100 times/sec)

```
inputs[VOCT_INPUT].getPolyVoltage(c)   ← 1V/oct pitch per channel
inputs[GATE_INPUT].getPolyVoltage(c)   ← 10V gate per channel
params[WTPOS_PARAM].getValue()         ← global wavetable position knob
inputs[WTPOS_CV].getPolyVoltage(c)     ← per-voice wavetable position CV
params[GLIDE_PARAM].getValue()         ← glide time
params[DACBITS_PARAM].getValue()       ← DAC bit depth (8/12/16)
         │
         ▼
  Voice[c].process()
    │  SlewLimiter → slewedPitch
    │  phase += freq(slewedPitch) * sampleTime
    │  wtEnvelope.process(gate) → wtPosition
    │  WavetableBank.lookup(tableIndex, wtPosition, phase, stepped) → rawSample
    │  DacEmulator.quantize(rawSample, dacBits) → quantizedSample
    │
    ▼
  outputs[AUDIO_POLY].setVoltage(quantizedSample * 5.f, c)    ← poly cable
  outputs[AUDIO_VOICE_0 + c].setVoltage(quantizedSample * 5.f)  ← individual jacks
  outputs[MIX_OUTPUT].addVoltage(quantizedSample * 5.f / channels)  ← summed mix
  outputs[GATE_POLY].setVoltage(gateOut * 10.f, c)
  outputs[VELOCITY_POLY].setVoltage(velocity * 10.f, c)
```

### Wavetable Load Flow (off audio thread)

```
User right-clicks panel
    → appendContextMenu() shows "Load wavetable…"
    → std::filesystem::path selection (platform file dialog or path input)
    → WavetableLoader::loadAsync(path, &bank)
        → std::thread: open file, read WAV headers, decode samples
        → Validate: single-cycle (N samples) or multi-frame (N * frameCount)
        → Normalize to float [-1, 1]
        → Resize frames to consistent WAVE_SIZE if needed
        → atomic swap into bank.userTable
    → Audio thread picks up new table on next process() call
```

### Unison Mode vs Polyphonic Mode

```
POLY MODE:
    voct_channels = inputs[VOCT_INPUT].getChannels()  → up to 8
    Voice[0..voct_channels-1] each get their own pitch/gate
    Outputs: poly cable has voct_channels channels

UNISON MODE:
    One pitch (channel 0 of VOCT input) → spread to all 8 voices
    Each voice gets pitch + detune[i] offset
    detune[i] = (i - (voices/2)) * detuneCents / 100.f * (1/12.f) volts
    Poly cable output still carries 8 channels (all at similar pitch)
    Individual voice outputs still active
```

### JSON State Persistence

```
dataToJson():
    → save: wavetableBankPath (path to user WAV, if loaded)
    → save: dacBits, stepped mode, polyphonic/unison switch
    → save: wtStartPos, glideAmount, LFO rate/amount
    (param values saved automatically by Rack)

dataFromJson():
    → restore: wavetableBankPath → trigger async reload
    → restore: mode switches (atomic flags)
```

---

## Build Order (Component Dependencies)

The architecture has clear dependency ordering. Build bottom-up.

```
1. PPGWavetables.hpp        — static ROM data, no dependencies
        ↓
2. WavetableBank            — owns tables, provides sample lookup
        ↓
3. AHREnvelope              — standalone DSP, no module dependencies
4. DacEmulator              — pure function, no dependencies
        ↓
5. Voice struct             — depends on WavetableBank, AHREnvelope, DacEmulator
        ↓
6. Hurricane8 Module        — depends on Voice[], WavetableBank
        ↓
7. WavetableLoader          — depends on WavetableBank (writes to it), std::thread
        ↓
8. Hurricane8Widget         — depends on Hurricane8 Module (reads params/lights)
        ↓
9. plugin.cpp               — registers Hurricane8 with Rack
```

### Phase Build Implications

| Build Phase | Components | Can Stand Alone? |
|-------------|------------|-----------------|
| Phase 1: Core oscillator | PPGWavetables, WavetableBank, Voice (basic), Hurricane8 Module (mono) | YES — test with single voice |
| Phase 2: Polyphony | Voice array, poly input routing, setChannels() on outputs | YES — builds on Phase 1 |
| Phase 3: Modulation | AHREnvelope, SlewLimiter (glide), LFO | YES — adds to Voice struct |
| Phase 4: DAC emulation | DacEmulator, bit-depth param | YES — post-process step |
| Phase 5: User wavetables | WavetableLoader, off-thread swap, dataToJson/From | YES — optional feature |
| Phase 6: Panel + outputs | Hurricane8Widget, individual jacks, mix output | YES — final assembly |

---

## Anti-Patterns

### Anti-Pattern 1: File I/O in process()

**What people do:** Load a WAV file when they detect a changed parameter in `process()`.
**Why it's wrong:** process() is called on the audio thread. File I/O blocks for milliseconds to seconds. This causes audio dropouts, Rack stalls, and is explicitly forbidden by VCV Rack's real-time rules.
**Do this instead:** Trigger file loading from the UI thread (context menu action). Use std::thread + atomic pointer swap. Audio thread only reads the pre-loaded data.

### Anti-Pattern 2: Heap Allocation in process()

**What people do:** `new`, `malloc`, `std::vector::push_back`, or any dynamic allocation inside `process()`.
**Why it's wrong:** Memory allocators use mutexes that can block the audio thread unpredictably, causing audio dropouts. Also forbidden by Rack's real-time rules.
**Do this instead:** Pre-allocate all buffers in the Module constructor. Fixed arrays (`Voice voices[8]`) on the struct. No dynamic containers in the hot path.

### Anti-Pattern 3: Sharing Mutable State Between Threads Without Atomics

**What people do:** Write to a `bool` flag in the UI thread, read it in `process()` without synchronization.
**Why it's wrong:** Data races cause undefined behavior (torn reads, cache incoherence, compiler reordering). On ARM/x86 you may get away with it briefly, but it is undefined behavior.
**Do this instead:** Use `std::atomic<bool>` or `std::atomic<int>` for flags that cross the thread boundary. For wavetable pointer swaps, use `std::atomic<WavetableData*>` with `memory_order_relaxed` (x86 TSO gives this for free; atomic ensures correctness on ARM too).

### Anti-Pattern 4: Using getVoltage() Without getPolyVoltage() for CV Inputs

**What people do:** Read `inputs[WTPOS_CV].getVoltage()` (channel 0 only) for a CV input that could carry per-voice polyphonic data.
**Why it's wrong:** Silently discards channels 1-7, making poly CV inputs behave as mono.
**Do this instead:** Use `inputs[WTPOS_CV].getPolyVoltage(c)` inside the voice loop. This returns channel 0 voltage if the input is monophonic, and the per-channel voltage if polyphonic — correct behavior in both cases.

### Anti-Pattern 5: Fixed 16-Voice Loop for an 8-Voice Module

**What people do:** Loop `for (int c = 0; c < 16; c++)` always, even though the module only has 8 voices.
**Why it's wrong:** Wastes ~2x CPU. The loop should run only for the number of active channels on the input, clamped to the module's voice count maximum.
**Do this instead:** `int channels = clamp(inputs[VOCT_INPUT].getChannels(), 1, 8);` then `for (int c = 0; c < channels; c++)`.

---

## Integration Points

### VCV Rack Engine Boundary

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Module ↔ Rack Engine | params[], inputs[], outputs[], lights[] arrays | Engine-managed, safe across threads |
| ModuleWidget ↔ Module | params[] (read), lights[] (read), custom JSON (save/load) | Widget reads; Module writes lights |
| Module ↔ User filesystem | WAV file paths in dataToJson(); async load via std::thread | Never in process() |
| Module ↔ Other Rack Modules | Polyphonic cables: VOCT, GATE in; AUDIO, GATE, VELOCITY out | Standard VCV voltage conventions |

### Voltage Conventions (HIGH confidence — official VCV docs)

| Signal | Convention | Notes |
|--------|------------|-------|
| 1V/oct pitch | `f = 261.626 * 2^V` Hz, 0V = C4 | Use `dsp::FREQ_C4 * pow(2.f, pitch)` |
| Gate | 0V = off, 10V = on; Schmitt trigger thresholds ~0.1V low, ~1.0V high | Read with `> 1.f` threshold |
| Audio output | ±5V (10Vpp) | Multiply normalized float output by 5.f |
| CV modulation | 0–10V unipolar or ±5V bipolar | Wavetable position CV: 0–10V makes sense |
| Velocity | 0–10V | Polyphonic VELOCITY output to downstream envelopes |
| Polyphonic channels | Up to 16 channels per cable; module limits to 8 | Set with `setChannels(count)` per output |

---

## Scaling Considerations

This is a DSP module, not a server. "Scaling" means CPU budget, not user load.

| Concern | Approach |
|---------|----------|
| 8 voices × sample rate | Fixed array of Voices; no allocation. Linear cost ~8x mono. Acceptable. |
| SIMD optimization | If CPU is tight: `dsp::float_4` processes 4 voices per SIMD op. Reduces 8-voice cost to ~2x mono. Worth doing in Phase 2 if profiling shows need. |
| Wavetable size | PPG ROM: 64-sample wavecycles are tiny. User tables up to 2048 samples/frame × N frames. Keep in flat float array; pointer stays constant during process(). |
| Wavetable table-switch transition | Crossfade between old/new table pointer over N samples to avoid clicks. Simple linear fade of ~256 samples suffices. |

---

## Sources

- VCV Rack Plugin Development Tutorial: https://vcvrack.com/manual/PluginDevelopmentTutorial (HIGH confidence)
- VCV Rack Plugin API Guide: https://vcvrack.com/manual/PluginGuide (HIGH confidence)
- VCV Rack Voltage Standards: https://vcvrack.com/manual/VoltageStandards (HIGH confidence)
- VCV Rack Polyphony Manual: https://vcvrack.com/manual/Polyphony (HIGH confidence)
- VCV Rack DSP Manual: https://vcvrack.com/manual/DSP (HIGH confidence)
- rack::dsp Namespace Reference: https://vcvrack.com/docs-v2/namespacerack_1_1dsp (HIGH confidence)
- rack::engine::Port API: https://vcvrack.com/docs-v2/structrack_1_1engine_1_1Port.html (HIGH confidence)
- VCV Community: Polyphony reminders for plugin developers: https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572 (MEDIUM confidence)
- VCV Community: Making your monophonic module polyphonic: https://community.vcvrack.com/t/making-your-monophonic-module-polyphonic/6926 (MEDIUM confidence)
- BogaudioModules VCO (reference implementation): https://github.com/bogaudio/BogaudioModules (MEDIUM confidence)
- WvTable-logue PPG Wave implementation (PPG table structure reference): https://github.com/vuki/WvTable-logue (MEDIUM confidence)
- EarLevel Engineering — Wavetable oscillator C++ patterns: https://www.earlevel.com/main/2012/05/25/a-wavetable-oscillator-the-code/ (MEDIUM confidence)

---
*Architecture research for: VCV Rack polyphonic wavetable oscillator (Hurricane 8)*
*Researched: 2026-03-01*

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

# Feature Research

**Domain:** VCV Rack wavetable oscillator module (PPG-inspired, polyphonic)
**Researched:** 2026-03-01
**Confidence:** MEDIUM — VCV Rack ecosystem well-documented; PPG-authentic feature expectations cross-referenced from hardware, community, and competitor analysis. No single authoritative source covers all feature expectations for this specific niche.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete or incompatible with standard VCV Rack workflow.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 1V/oct pitch tracking | Every VCV oscillator has this; without it the module won't integrate with any patch | LOW | VCV voltage standard: ±5V audio, 0-10V or ±5V CV. Precision required — logarithmic conversion must be accurate across 10+ octaves |
| Polyphonic cable output | VCV Rack 2 norm; users expect to drive the module from a MIDI-CV module with a single poly cable | MEDIUM | Up to 16 channels per Rack cable. Hurricane 8 targets 8 voices — must advertise channel count correctly |
| Wavetable position control with CV input | Core identity of any wavetable VCO — without CV-controllable position, there's no "wavetable scanning" | LOW | WT POS knob + attenuator + CV input is the minimum expected by any user familiar with Fundamental VCO-2 or Surge XT WT |
| User-loadable wavetables | Expected since VCV Fundamental VCO-2 supports it; users have libraries of .WAV files | MEDIUM | Must support at minimum: single-cycle WAV files. Multi-frame (Serum-style 2048-sample tagged) is strongly preferred |
| Mix (summed) output | Standard on all polyphonic voice modules; needed to route through a filter or effects chain | LOW | Simple sum of all active voices; clip-guard at output helpful |
| Polyphonic gate output | Required for downstream envelope generators when used in a full voice patch | LOW | VCV convention: gate high = 10V, low = 0V |
| Right-click context menu with wavetable load | VCV Rack UI convention; users look here first | LOW | Standard pattern from VCV Fundamental VCO-2 |
| Panel SVG that meets library standards | Required for VCV Library submission | MEDIUM | Must follow VCV Panel Guide: text converted to paths, readable at 100% on non-HiDPI, inverted backgrounds on outputs |
| Plugin manifest with correct versioning | Required for library submission; MAJOR version must match Rack SDK major | LOW | Format: MAJOR.MINOR.REVISION, no "v" prefix, MAJOR = 2 for Rack 2 |

### Differentiators (Competitive Advantage)

Features that set Hurricane 8 apart. These align with the core value: "Authentic PPG wavetable character accessible as a polyphonic VCV Rack module with modern flexibility."

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| PPG stepped vs interpolated scanning modes | The PPG Wave did NOT crossfade between wavetable entries — it switched at zero-crossing only. This stepped behavior is the defining sonic character. No VCV module currently replicates this authentically | HIGH | Stepped = switch wave at start of next cycle (zero-crossing). Interpolated = linear crossfade. Switchable gives users both authentic character and modern smoothness. Confirmed from PPG technical analysis: "the PPG ones didn't crossfade at all, they just switched waves at the beginning/end of the next cycle" |
| Configurable DAC bit depth emulation (8/12/16-bit) | The PPG Wave 2 used 8-bit DACs producing quantization artifacts that define its sound. Wave 2.3 upgraded to 12-bit. Emulating this gives users the ability to dial in the exact era of PPG character | HIGH | 8-bit = authentic early PPG grit and aliasing; 12-bit = Wave 2.3 character; 16-bit = transparent/modern. No existing VCV wavetable module offers this tiered DAC emulation specifically targeting PPG authenticity |
| Original PPG wavetable set bundled | Users who want the PPG sound need the original waveforms. Sourcing these is non-trivial for most users | MEDIUM | Hermann Seib's PPG Wave ROM documentation provides wavetable structure reference. 30 wavetables of 64 waves each. Licensing must be considered — original ROM data may be in the public domain given age |
| Multi-frame WAV support (Serum-style) | Serum, Bitwig, and WaveEdit users have large libraries in multi-frame format. Supporting this dramatically expands usable content | HIGH | Serum format: 2048-sample tagged WAV. Must parse WAV loop markers or metadata tags to detect frame boundaries. Surge XT already supports this — sets user expectation |
| Per-voice ASR envelope for wavetable position | The authentic PPG had envelope-controlled wavetable scanning (Env3 was the dedicated "wavetable position" envelope). Per-voice tracking enables expressive polyphony where each note independently scans the table | HIGH | Per-voice = each of the 8 voices has its own envelope phase. Global = all voices share one envelope. Switchable gives flexibility. ASR (Attack-Sustain-Release) matches PPG's original design |
| Wavetable start position control | Sets where in the wavetable scanning begins — allows tuning a specific wave character as the "base" timbre | LOW | Independent of the scan envelope — the start position sets the ground state before the envelope moves it |
| Internal LFO for vibrato | The PPG Wave had dedicated vibrato circuitry. Having it onboard reduces patch complexity for a common use case | MEDIUM | Rate and amount controls with CV inputs for each. Sine wave LFO routed to pitch. External LFO can always be patched, but internal saves two modules for a typical live performance patch |
| Glide (portamento) with CV input | Essential for PPG-style expressive playing — the original PPG had portamento between note pitches | LOW | Per-voice slew limiting on the pitch signal. CV input for dynamic glide amount (e.g., from mod wheel) |
| Unison mode with detune | Stacking all 8 voices on a single monophonic pitch with detuning gives the characteristic "super saw" / wall-of-sound effect that PPG players use for pads | MEDIUM | Unison = all 8 voices receive the same 1V/oct; spread detune across ±N cents. Polyphonic mode = standard voice allocation. Switchable per VCV convention |
| 8 individual voice audio outputs | Allows each voice to be processed independently (different filter depths, panning, effects per voice) — power-user capability | MEDIUM | Both as individual jacks AND packed into a polyphonic cable. Individual jacks are unusual in VCV — most modules use poly cables + Split utility. Having both is the differentiator |
| 8 individual gate outputs | Enables per-voice envelope triggering with external envelope generators, bypassing the internal ASR | LOW | Both individual jacks and polyphonic cable. Same pattern as audio outputs |
| Velocity CV output (polyphonic) | Passes velocity data from upstream MIDI-CV module through — enables downstream dynamics without losing velocity information | LOW | Polyphonic only (16 channels max from MIDI-CV); pass-through rather than generation |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems for this module's scope and quality.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Built-in filter (VCF) | PPG Wave had a 24dB/oct analog filter — users want the "complete PPG voice" | Adds massive complexity, doubles scope; VCV's modular philosophy means filters belong in separate modules (Vult Freak, Bogaudio VCF, etc). Scope creep destroys timeline | Pair with external filter modules; document recommended patch topologies in presets |
| Built-in VCA/amplitude envelope | Complete voice = OSC + VCF + VCA; users want one-module voice | Same scope problem; VCA and amplitude envelopes are well-served by existing VCV modules (ADSR + VCA) | External envelope + VCA; the mix output routes directly to any VCA |
| Built-in reverb/delay/chorus | "PPG with effects" requests will come | Completely outside oscillator scope; adds CPU cost for effects most users don't want | Effect modules exist in abundance; out of scope by project definition |
| MIDI input | Easier patching for keyboard players | VCV Rack's MIDI-CV module handles this; duplicating it adds maintenance burden and reduces interoperability | Upstream MIDI-CV module feeds poly cables into Hurricane 8's inputs |
| 2D XY wavetable morphing | E370-style XY morphing is requested by power users | PPG's wavetable structure is 1D (linear scan through a 64-wave table); 2D morphing is a different synthesis paradigm that changes the fundamental PPG character | The 1D scan IS the PPG. Stick to 1D. Users wanting XY morphing should use E370 |
| Sequencer or arpeggiator | "Complete instrument" requests | Out of scope; VCV has excellent sequencers (Geodesics, Impromptu, Count Modula) | External sequencer modules |
| Preset management UI | Save/load sound presets inside the module | VCV Rack patches ARE the preset system. In-module presets duplicate this and add complexity | VCV patch save/load serves this need |
| 16-voice mode | More voices = more CPU, hitting Rack's polyphonic cable max | 8-voice is the PPG's voice count and maps cleanly to module design; 16 voices doubles CPU and individual jack count | Document that 8 voices is intentional PPG fidelity |

---

## Feature Dependencies

```
1V/Oct Pitch Input (per voice)
    └──requires──> Phase Accumulator (per voice)
                       └──requires──> Wavetable Lookup
                                          └──requires──> Wavetable Data (ROM PPG tables or user WAV)

Polyphonic Cable Output
    └──requires──> Voice Allocation (maps poly channels to voices)
                       └──requires──> Gate Input (polyphonic)

Wavetable Position CV
    └──enhances──> Wavetable Lookup

Per-Voice ASR Envelope (wavetable position)
    └──requires──> Gate Input (per voice)
    └──enhances──> Wavetable Position CV (envelope drives position)

Wavetable Start Position
    └──enhances──> Per-Voice ASR Envelope (defines envelope start point)

PPG Stepped Scanning Mode
    └──requires──> Wavetable Lookup (zero-crossing detection logic)
    └──conflicts──> Interpolated Scanning Mode (mutually exclusive per-voice modes)

DAC Bit Depth Emulation
    └──requires──> Wavetable Lookup output (post-lookup quantization step)

Multi-frame WAV Support
    └──requires──> WAV File Parser (frame boundary detection)
    └──enhances──> User-Loadable Wavetables

Unison Mode
    └──requires──> Voice Allocation
    └──conflicts──> Polyphonic Mode (mutually exclusive modes)

Internal LFO (vibrato)
    └──enhances──> 1V/Oct Pitch (adds pitch modulation)

Glide (portamento)
    └──enhances──> 1V/Oct Pitch (slew limits pitch transitions)

Individual Voice Audio Outputs (8 jacks)
    └──requires──> Per-Voice Audio Generation

Individual Gate Outputs (8 jacks)
    └──requires──> Voice Allocation (knows which voice is on gate)

Velocity CV Output (polyphonic)
    └──requires──> Gate/Velocity Input from upstream MIDI-CV

Mix Output
    └──requires──> Per-Voice Audio Generation (sum of all voices)
```

### Dependency Notes

- **Per-Voice ASR Envelope requires Gate Input:** The envelope must know when a note starts and ends. Each polyphonic channel carries its own gate signal.
- **PPG Stepped Scanning conflicts with Interpolated Mode:** These are mutually exclusive per-voice (or global switch). Cannot crossfade AND step-switch simultaneously.
- **DAC Bit Depth Emulation is post-lookup:** The quantization happens AFTER wavetable lookup — it's a processing stage on the output, not part of the table storage.
- **Unison Mode conflicts with Polyphonic Mode:** In unison, all 8 voices track a single monophonic pitch. In polyphonic mode, voices are independently allocated. A panel switch or right-click menu controls this.
- **Wavetable Start Position enhances ASR Envelope:** The start position is the envelope's "zero point" — where the table scan begins before the attack stage moves it forward.

---

## MVP Definition

### Launch With (v1)

Minimum viable product — what validates the PPG polyphonic wavetable concept.

- [ ] 1V/oct pitch tracking (polyphonic, 8 channels) — fundamental oscillator function
- [ ] Phase accumulator per voice — the DSP core
- [ ] PPG wavetable data bundled (ROM set) — gives immediate PPG identity
- [ ] Wavetable scanning with stepped mode — the defining PPG character
- [ ] Wavetable position control with CV input — essential wavetable feature
- [ ] Per-voice ASR envelope for wavetable position — the PPG "Env3" equivalent
- [ ] DAC bit depth emulation (8/12/16-bit) — differentiator, builds in from day one
- [ ] Mix (summed) output — required for basic patching
- [ ] Polyphonic audio output (8 channels) — standard VCV polyphonic output
- [ ] Polyphonic gate output — downstream envelope generators need this
- [ ] Basic panel SVG — required for any use in VCV Rack
- [ ] User-loadable single-cycle WAV — expected by all users

### Add After Validation (v1.x)

Features to add once core DSP is working and validated.

- [ ] Multi-frame WAV support (Serum-style) — expands usable wavetable libraries; add when user feedback confirms demand
- [ ] Interpolated scanning mode (switchable with stepped) — modern fallback; add after stepped is validated
- [ ] Glide (portamento) with CV input — expressive feature, add in v1.1
- [ ] Internal LFO for vibrato — convenience feature; add once core patch is stable
- [ ] Unison mode with detune — high-value differentiator; requires voice allocation redesign; add after polyphonic mode is stable
- [ ] 8 individual voice audio outputs (jacks) — power user feature; HP-constrained, requires panel redesign
- [ ] 8 individual gate outputs (jacks) — accompanies individual voice audio outputs

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Wavetable start position as separate control — nuance feature; users may not discover it; add if requested
- [ ] Velocity CV output pass-through — low complexity but low priority; add when available
- [ ] Additional bundled wavetable sets beyond original PPG ROM — content work; defer until core is stable
- [ ] Wavetable editor or display visualization — complex UI; out of scope for v1

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 1V/oct pitch tracking (polyphonic) | HIGH | LOW | P1 |
| PPG wavetable ROM bundled | HIGH | MEDIUM | P1 |
| Stepped scanning mode (PPG authentic) | HIGH | HIGH | P1 |
| Wavetable position control + CV | HIGH | LOW | P1 |
| Per-voice ASR envelope for WT position | HIGH | HIGH | P1 |
| DAC bit depth emulation (8/12/16-bit) | HIGH | HIGH | P1 |
| Mix output | HIGH | LOW | P1 |
| Polyphonic audio output | HIGH | LOW | P1 |
| Polyphonic gate output | HIGH | LOW | P1 |
| User-loadable single-cycle WAV | HIGH | MEDIUM | P1 |
| Panel SVG + library submission | HIGH | MEDIUM | P1 |
| Interpolated scanning mode (switchable) | MEDIUM | MEDIUM | P2 |
| Multi-frame WAV support | HIGH | HIGH | P2 |
| Glide (portamento) with CV | MEDIUM | LOW | P2 |
| Internal LFO for vibrato | MEDIUM | MEDIUM | P2 |
| Unison mode with detune | HIGH | MEDIUM | P2 |
| 8 individual voice audio outputs (jacks) | MEDIUM | HIGH | P2 |
| 8 individual gate outputs (jacks) | MEDIUM | LOW | P2 |
| Wavetable start position control | MEDIUM | LOW | P2 |
| Velocity CV output pass-through | LOW | LOW | P3 |
| Additional bundled wavetable sets | MEDIUM | MEDIUM | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

The VCV Rack wavetable space has several occupants; none precisely overlap with Hurricane 8's combination of PPG authenticity, 8-voice polyphony, and individual voice outputs.

| Feature | VCV Fundamental VCO-2 | Surge XT Wavetable VCO | Valley Terrorform | E370 Quad Morphing VCO | Hurricane 8 (proposed) |
|---------|----------------------|------------------------|-------------------|------------------------|------------------------|
| Wavetable scanning | Interpolated crossfade | Interpolated (2 modes) | FM + wavetable hybrid | 2D XY morph, smooth/glitched | Stepped + interpolated (switchable) |
| DAC bit depth emulation | None | "WaveTable mode" simulates early DAC (undocumented fidelity) | None | None | 8/12/16-bit explicit emulation |
| PPG ROM tables bundled | No | No | No | No | Yes |
| User-loadable wavetables | Single-cycle WAV | Serum, Bitwig, WaveEdit formats | Yes (format TBD) | Via WaveEdit tool | Single-cycle + multi-frame WAV |
| Polyphonic | Yes | Yes | Yes | No (quad mono voices) | Yes (8-voice) |
| Voice count | Up to 16 (Rack limit) | Up to 16 | Up to 16 | 4 mono voices | 8 fixed (PPG authentic) |
| Unison mode | No (external module needed) | Via separate UnisonHelper module | No | No | Yes (built-in) |
| Glide/portamento | No | No | No | No | Yes (built-in) |
| Internal vibrato LFO | No | Via modulation matrix | No | No | Yes (built-in) |
| Per-voice ASR envelope | No | No | No | No | Yes (built-in) |
| Individual voice outputs | No (poly cable only) | No (poly cable only) | No (poly cable only) | Yes (4 mono outs) | Yes (8 jacks + poly cable) |
| Individual gate outputs | No | No | No | No | Yes (8 jacks + poly cable) |

**Key gap Hurricane 8 fills:** No existing VCV Rack module combines authentic PPG stepped scanning, explicit DAC bit depth emulation, the original PPG wavetable set, built-in PPG-style voice envelope, AND 8-voice polyphony in a single module. Users currently need to assemble a complex patch (wavetable VCO + envelope + poly unison utility + glide module + LFO) to approximate this. Hurricane 8 makes it a single drop.

---

## Sources

- [VCV Library - Oscillator tag](https://library.vcvrack.com/?tag=Oscillator) — MEDIUM confidence (paginated, not exhaustive)
- [VCV Fundamental VCO-2 (Wavetable VCO)](https://library.vcvrack.com/Fundamental/VCO2) — HIGH confidence (official VCV module)
- [Surge XT Wavetable VCO documentation](https://surge-synthesizer.github.io/rack_xt_manual/) — MEDIUM confidence (official Surge docs)
- [PPG Wave Wikipedia](https://en.wikipedia.org/wiki/PPG_Wave) — MEDIUM confidence (well-sourced Wikipedia article)
- [PPG Wave ROM Wavetable documentation - Hermann Seib](https://www.hermannseib.com/documents/PPGWTbl.pdf) — HIGH confidence (primary source researcher)
- [Gearspace PPG Wave interpolation discussion](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1273412-ppg-wave-doest-interpolate-between-single-cycles.html) — MEDIUM confidence (community with technical detail)
- [Groove Synthesis 3rd Wave 8M - PPG-style polyphonic](https://groovesynthesis.com/8m/) — MEDIUM confidence (contemporary commercial analog)
- [PPG W2.2x4 Eurorack module](https://synthanatomy.com/2025/09/ppg-w2-2x4-legendary-german-brand-makes-a-comeback-with-a-new-dual-wavetable-oscillator.html) — HIGH confidence (official PPG product, 2025)
- [WAEVE82 PPG-inspired AE Modular polysynth](https://www.tangiblewaves.com/store/p235/WAEVE82.html) — HIGH confidence (official product page, 2025/2026)
- [VCV Rack Polyphony manual](https://vcvrack.com/manual/Polyphony) — HIGH confidence (official VCV documentation)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) — HIGH confidence (official VCV documentation)
- [Erica Synths Black Wavetable VCO - bitcrush features](https://gearnews.com/erica-synths-ports-black-wavetable-vco-and-black-octasource-to-vcv-rack/) — MEDIUM confidence (product announcement)
- [VCV Community wavetable VCO discussion](https://community.vcvrack.com/t/wave-table-vco/14759) — LOW confidence (community forum, limited to browsed portion)
- [Synthesis Technology E370 features](https://vcvrack.com/SynthesisTechnology) — MEDIUM confidence (official product page)

---

*Feature research for: PPG-inspired polyphonic wavetable oscillator VCV Rack module (Hurricane 8)*
*Researched: 2026-03-01*

# Pitfalls Research

**Domain:** VCV Rack polyphonic wavetable oscillator module (PPG Wave emulation)
**Researched:** 2026-03-01
**Confidence:** HIGH (multiple official sources + verified community patterns)

---

## Critical Pitfalls

### Pitfall 1: Memory Allocation or File I/O Inside process()

**What goes wrong:**
Any call to `new`, `delete`, `malloc`, `free`, or file I/O inside the `process()` method causes non-deterministic stalls. The OS memory allocator acquires locks and may swap to disk. A single stall of even 10ms in the audio thread causes a buffer underrun (audible click or dropout). User wavetable loading (WAV parsing) is a common trigger for this.

**Why it happens:**
Developers port non-realtime patterns (load on demand, lazy-init) into the audio callback. WAV file loading in particular is tempting to do inline when a user patches a new file, but `process()` is on the audio thread. The VCV Rack docs explicitly warn: "accessing patch files in Module::process() can block the DSP thread, resulting in audio hiccups."

**How to avoid:**
- Pre-allocate all per-voice state arrays at module construction time (8 voices fixed)
- Use a worker thread or `onAdd()`/`onReset()` events for file loading
- Communicate between UI/file-loading thread and audio thread via lock-free mechanisms (e.g., `dsp::RingBuffer`, atomic flags, or double-buffered ping-pong approach)
- Wavetables should be fully loaded into a pre-allocated buffer before any audio thread reads them; use an atomic "ready" flag to switch

**Warning signs:**
- Occasional audio pops when changing wavetable files
- `valgrind` or sanitizers detecting allocations in process()
- CPU spikes on file load coinciding with audio dropouts

**Phase to address:** Wavetable engine core (Phase 1) and user wavetable loading (whichever phase introduces WAV import)

---

### Pitfall 2: No Band-Limited Mip-Map Tables — Aliasing at High Pitches

**What goes wrong:**
A single wavetable used across all pitches produces severe aliasing above roughly C5. Harmonics that exceed the Nyquist limit fold back as audible, dissonant mirror frequencies. The problem is worst with complex waveforms (sawtooth, PPG tables with many harmonics) and is invisible at C3 but catastrophic at C6+.

**Why it happens:**
Developers build a working oscillator at middle frequencies, test it there, and don't notice the problem until testing high-register patches. The PPG waveforms contain many high-order harmonics that alias immediately at high pitches without mip-mapping.

**How to avoid:**
- Build a mip-map set at module initialization: generate one band-limited version per octave (or semi-tone region) by taking an FFT, zeroing harmonics above Nyquist for that pitch range, and inverse-FFTing
- Select the appropriate mip-map level based on the current playback frequency per voice
- Table size of 2048 samples is a common baseline; linear interpolation between samples is sufficient if tables are large enough
- Rebuild mip-map set after any user wavetable is loaded (on the worker thread)

**Warning signs:**
- Metallic, dissonant ringing at high pitches that disappears at lower notes
- High-frequency notes sound "wrong" compared to the reference PPG sound
- Pitch-dependent timbre changes that go beyond the intended character

**Phase to address:** Wavetable engine core (Phase 1) — must be designed in from the start, not retrofitted

---

### Pitfall 3: Non-Compliant Polyphonic Channel Handling

**What goes wrong:**
The module produces incorrect or inconsistent behavior when receiving polyphonic cables. Common failures:
- Reading only channel 0 from a polyphonic V/OCT input (silently ignoring voices 1–7)
- Setting output channel count to a hardcoded 8 instead of querying the connected polyphonic input
- Not calling `setChannels()` on output ports, leaving downstream modules confused about active voice count

**Why it happens:**
Developers test with a single voice and never validate polyphonic behavior. The VCV Rack polyphony model is not enforced at compile time — incorrect code compiles and runs, it just does the wrong thing silently.

**How to avoid:**
- For the polyphonic V/OCT input: read all channels up to `inputs[VOCT_INPUT].getChannels()`, capped at 8
- Use `std::max(inputs[VOCT_INPUT].getChannels(), 1)` to handle unpatched case (returns 0 channels when unpatched)
- Call `outputs[POLY_OUT].setChannels(numVoices)` every process() (or at least when voice count changes) so downstream modules know how many channels are valid
- For audio inputs on mono paths, use `getVoltageSum()` not `getVoltage()`
- Test with VCV Core's MIDI-CV module set to 2, 4, 8, and 16 channels

**Warning signs:**
- Only one voice sounds when MIDI-CV is in polyphonic mode
- Downstream modules (like poly-to-mono mixers) show wrong channel counts
- Works in unison mode but not polyphonic mode

**Phase to address:** Core module scaffold (earliest phase) — the module's process() loop must be written for polyphony from day one

---

### Pitfall 4: Floating-Point Phase Accumulator Drift and Denormal Numbers

**What goes wrong:**
Single-precision float phase accumulators accumulate rounding error over long sustained notes. At very low frequencies (sub-audio LFO rates or very slow wavetable scanning), the phase increment becomes a subnormal (denormal) floating-point number. Denormal arithmetic on x86 CPUs is 10–100x slower than normal FP operations, causing sudden CPU spikes that can manifest as audio dropouts.

**Why it happens:**
It is natural to use `float` for phase values since Rack's audio samples are `float`. But accumulated phase drift is real at 44100 Hz over minutes of runtime, and nobody tests for it. The denormal trap is especially insidious because it appears only at very low LFO rates or extreme portamento.

**How to avoid:**
- Use `double` for phase accumulators (double-precision eliminates drift for all practical note lengths)
- Flush-to-zero (FTZ) mode: VCV Rack's engine enables FTZ/DAZ CPU flags in the audio thread, which converts denormals to zero automatically. Verify this is active (it is in Rack 2's engine) and don't disable it
- Add a small DC offset trick or clamp phase to [0.0, 1.0] after each wrap to prevent accumulation
- For wavetable scan position, clamp to [0.0, numWaves-1] after each increment

**Warning signs:**
- CPU usage spikes unpredictably at slow LFO rates
- Very slow portamento glides producing audio dropouts
- Phase-related glitches after long sustained notes

**Phase to address:** DSP engine core (Phase 1) — use double for all phase accumulators from the start

---

### Pitfall 5: Wavetable Position Stepping Transition Clicks (The Wrong Kind of PPG Character)

**What goes wrong:**
When scanning between wavetable positions in stepped mode (authentic PPG behavior), abrupt transitions between adjacent waveforms create discontinuities that produce loud clicks at audio rate. This is different from the intended "digital stepped" character — the real PPG's stepping artifacts are at the *sample level*, not at the waveform boundary level. Badly implemented stepping causes click artifacts that are purely bugs, not character.

**Why it happens:**
Developers implement stepping by directly substituting the new waveform table index on every new period or on every incoming CV change. If the waveform transition happens at a non-zero-crossing point, an instantaneous voltage jump creates a click. The PPG's stepping was between fixed wave positions on the wavetable — the waveform itself loops continuously.

**How to avoid:**
- Stepped mode changes the target wavetable table index but the phase accumulator runs continuously — never reset the phase on a table change
- Apply hysteresis to the stepped table selection: don't switch tables faster than the current audio period
- Optional: crossfade over 1–2 samples when switching table index even in "stepped" mode (imperceptible but click-free)
- Separate the authenticity of stepping (integer table indices, no interpolation between tables) from click-free implementation (continuous phase, smooth enough to not introduce new artifacts)

**Warning signs:**
- Audible clicks when sweeping the wavetable position knob quickly
- Clicks at low wavetable positions in stepped mode but not in interpolated mode
- Users report "glitchy" sound that isn't the intended PPG character

**Phase to address:** Wavetable engine core (Phase 1), specifically when implementing the stepped/interpolated mode switch

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Single wavetable (no mip-map) | Faster to build, simpler code | Severe aliasing above C4; users will reject the module | Never — must be in Phase 1 |
| `float` phase accumulators | Trivially easy | Drift on long notes; denormal risk at slow rates | Never for final product |
| `getVoltage(0)` instead of per-channel loop | Simpler process() | Module silently monophonic; polyphonic cables ignored | Never — defeats module purpose |
| Blocking file load in process() | Simpler WAV loading code | Audio dropouts on file change | Never |
| Hardcoded 44100 Hz assumption | Simpler frequency calculation | Incorrect pitch tracking at 48000/96000 Hz | Never — use `APP->engine->getSampleRate()` |
| Single wavetable buffer shared across voices | Less memory | Race condition if voices read while file-loading thread writes | Never without proper sync |
| All voice state on stack per-sample | Avoids heap, simple | Stack overflow with 8 voices of deep state | Never — allocate in constructor |

---

## Integration Gotchas

Common mistakes when connecting to external VCV Rack systems.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| MIDI-CV → V/OCT input | Assuming mono V/OCT; reading only channel 0 | Loop over `inputs[VOCT_INPUT].getChannels()` per process() call |
| Polyphonic gate input | Treating gate as mono; all 8 voices trigger simultaneously | Read gate per channel: `inputs[GATE_INPUT].getVoltage(voiceIndex)` |
| Mix output | Summing voice outputs without normalization; very loud output | Sum then divide by `numVoices` or use a fixed attenuation |
| Polyphonic output channel count | Not setting `setChannels()` on output | Call `outputs[POLY_OUT].setChannels(numActiveVoices)` every frame |
| CV inputs for wavetable position | Unpatched CV reads as 0V — correct | But ensure 0V maps to a musically useful wavetable position (first table), not silence |
| Velocity CV output | Outputting velocity as raw MIDI 0–127 | VCV standard is 0–10V; map velocity: `velocity / 127.0f * 10.0f` |
| SVG panel components layer | Leaving component markers visible in shipped SVG | Hide the "components" layer before exporting final SVG |

---

## Performance Traps

Patterns that work for 1 voice but fail at 8 voices under load.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-sample `std::pow(2, cv/12)` for V/oct | Correct but slow | Pre-compute or use lookup table; `dsp::approxExp2_taylor5()` from Rack DSP | Any time; 8 voices × 44100 Hz = significant overhead |
| Linear interpolation across all voices naively | CPU spikes at 8 voices | Vectorize inner loop with SIMD; profile before optimizing | Above 4–6 active voices at 44100 Hz |
| Wavetable mip-map rebuild on every process() call | Constant ~100% CPU | Rebuild only when wavetable changes (triggered by worker thread flag) | Immediately |
| Complex widget `draw()` called every frame | High GPU/CPU usage on panel redraw | Wrap in `FramebufferWidget` so it only redraws on change | Any time the Rack window is animating at 60fps |
| FFT-based mip-map generation on module add | Long stall when adding module to rack | Generate mip-maps asynchronously; play silence until ready | Tables larger than ~1024 samples |
| Unison mode: 8 voices summed without level compensation | Output clips when unison enabled | Scale output by `1.0f / numUnisonVoices` in mix output | Immediately on first unison note |

---

## UX Pitfalls

Common user experience mistakes specific to wavetable oscillator modules.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Wavetable position knob jumps between tables with no visual feedback | Users can't see which wave they're on; confusing | Display current table index in panel label or LED display |
| Stepped and interpolated modes produce identical-sounding output | Users can't tell modes apart; feature appears broken | Ensure stepped mode has audibly distinct stepping artifacts vs. smooth crossfade |
| Per-voice ASR envelopes all reset on ANY gate | Voices not playing sustain inappropriately | Only trigger ASR on the gate for *that specific voice index* |
| All 8 individual jacks always present but unlabelled | Users confused which jack is which voice | Clear labels V1–V8 on panel; label gates G1–G8 |
| User wavetable loading with no progress feedback | Long WAV files stall UI; user thinks app is frozen | Show a loading state (LED blink or label change) while file loads |
| Module too narrow for 8+8 jacks | Jacks too close to patch without catching adjacent jack | Plan HP width early: 8 voice outs + 8 gate outs + poly outs + controls requires ~30–40 HP minimum |
| Glide applies to all voices equally without per-voice tracking | Playing polyphonic chord with glide produces wrong pitch slides | Track glide target independently per voice index |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Pitch tracking:** 1V/oct formula uses `APP->engine->getSampleRate()` not hardcoded 44100 — verify at 48kHz
- [ ] **Polyphony:** Module produces correct output with 1, 2, 4, AND 8 polyphonic channels (not just 1 and 8)
- [ ] **Wavetable loading:** WAV files with sample rates other than the module's rate are handled (resample or reject gracefully)
- [ ] **Multi-frame WAV:** Frame boundary detection handles files where total samples is not an exact multiple of the frame size (truncate last partial frame, don't crash)
- [ ] **Unison mode:** Switching from polyphonic to unison and back mid-patch does not leave orphan voices sounding
- [ ] **Stepped mode:** Changing wavetable position at audio rate does not produce clicks louder than the intended PPG stepping character
- [ ] **State serialization:** `dataToJson`/`dataFromJson` correctly round-trips the wavetable file path AND the loaded wavetable data reference — patch save/load restores the correct sound without re-showing a file picker
- [ ] **Mip-map rebuild:** Loading a new user wavetable correctly rebuilds mip-maps for ALL voices, not just voice 0
- [ ] **Library compliance:** All module parameters have descriptions, names, and units configured; module has tags and description in plugin.json
- [ ] **NaN safety:** Outputs are checked with `std::isfinite()`; any NaN from bad CV inputs outputs 0V rather than propagating NaN to downstream modules
- [ ] **Cross-platform build:** Header `#include` paths match case of actual filenames — compiles on Linux (case-sensitive filesystem) not just macOS

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Aliasing discovered after wavetable engine is built | HIGH | Retrofit mip-map system; requires rethinking table storage and lookup; touches all voice code |
| Memory allocation in process() | MEDIUM | Identify all allocation sites with sanitizer; move to constructor or worker thread; redesign data ownership |
| Polyphonic channel handling wrong | MEDIUM | Systematic rewrite of process() loop from mono to per-channel; test with poly MIDI-CV |
| State serialization broken | LOW-MEDIUM | Add version field to JSON; implement migration path; re-test save/load |
| Panel HP too narrow | HIGH | Redesign SVG and all component coordinates; must adjust C++ widget positions to match |
| Click on wavetable stepping | LOW | Add crossfade or hysteresis to table index selection; isolated change |
| CPU too high at 8 voices | MEDIUM | Profile with VTune or Instruments; likely the V/oct conversion or mip-map lookup; vectorize hot path |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Memory allocation in process() | Phase 1: Wavetable DSP engine | No allocations reported by sanitizer; no audio dropouts on WAV load |
| No band-limited mip-maps | Phase 1: Wavetable DSP engine | No aliasing at C6 and above (spectrogram verification) |
| Polyphonic channel handling | Phase 1 or 2: VCV Rack integration | Module behaves correctly with 1, 4, 8 poly channels from MIDI-CV |
| Float phase drift / denormals | Phase 1: Wavetable DSP engine | 1-hour sustained note test; CPU stable; no pitch drift measurable |
| Wavetable position clicking | Phase 1: Wavetable DSP engine | Stepped mode sweeps produce no clicks above the intended stepping character |
| Missing mip-map rebuild on user load | Phase with WAV loading | Load new wavetable while sustaining notes; no aliasing introduced mid-note |
| Wrong panel HP | Pre-Phase 1: Planning + SVG design | All 16 individual jacks + polyphonic jacks + controls fit with thumb clearance |
| State serialization | Final Phase: Library prep | Save patch, quit, reopen → identical sound; preset system works |
| Cross-platform build failures | Every phase | Plugin toolchain CI builds for Windows, Mac, Linux on each merge |
| NaN propagation | Throughout | Stress test with no cables patched; extreme CV values (±10V); verify no NaN on outputs |

---

## Sources

- [VCV Rack Plugin API Guide](https://vcvrack.com/manual/PluginGuide) — thread safety, file I/O, bypass, expander messaging (HIGH confidence)
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) — aliasing, bandlimiting, minBLEP, SIMD optimization (HIGH confidence)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) — audio levels, gate thresholds, Schmitt triggers, NaN handling (HIGH confidence)
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony) — channel count rules, getChannels(), setChannels() (HIGH confidence)
- [VCV Community: Polyphony Reminders for Plugin Developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572) — getVoltageSum() for audio, channel count with std::max (HIGH confidence, official community post)
- [VCV Community: Common Issues for Plugin Developers](https://community.vcvrack.com/t/help-wanted-for-reporting-common-issues-to-rack-plugin-developers/11031) — metadata, polyphony compliance, crash patterns (HIGH confidence)
- [VCV Rack Panel Guide](https://vcvrack.com/manual/Panel) — 128.5mm height, mm units only, component color coding, text-to-paths (HIGH confidence)
- [EarLevel Engineering: Wavetable Oscillator Series](https://www.earlevel.com/main/2012/11/18/a-wavetable-oscillator-end-notes/) — mip-mapping, band-limiting, interpolation tradeoffs (HIGH confidence, authoritative DSP reference)
- [KVR Audio: Wavetable oscillator implementation](https://www.kvraudio.com/forum/viewtopic.php?t=509014) — mip-map spacing, linear vs cubic interpolation (MEDIUM confidence)
- [KVR Audio: Comparing phase truncation and interpolation](https://www.kvraudio.com/forum/viewtopic.php?t=546829) — interpolation vs stepping modes (MEDIUM confidence)
- [Ross Bencina: Real-time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) — malloc, blocking, pre-allocation rules (HIGH confidence, canonical real-time audio reference)
- [PPG Wave Wikipedia](https://en.wikipedia.org/wiki/PPG_Wave) — original hardware bit depth, stepping architecture (MEDIUM confidence)
- [VCV Community: Glide/portamento subtleties](https://community.vcvrack.com/t/some-subtleties-in-glide-portamento/16136) — per-voice gate tracking, rate vs time glide (MEDIUM confidence)
- [VCV Community: Lights and thread safety](https://community.vcvrack.com/t/lights-buttons-and-thread-safety/21797) — thread model in Rack 2 (HIGH confidence)

---
*Pitfalls research for: VCV Rack polyphonic wavetable oscillator (PPG Wave-style, 8-voice, Hurricane 8)*
*Researched: 2026-03-01*