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
