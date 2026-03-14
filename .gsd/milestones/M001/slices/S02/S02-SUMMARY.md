---
id: S02
parent: M001
milestone: M001
provides:
  - 8-voice polyphonic engine (PolyphonicEngine struct)
  - Polyphonic mode — each input channel drives one independent voice
  - Unison mode — all 8 voices stacked on single pitch with configurable detune spread
  - Voice mode switching (polyphonic ↔ unison at runtime)
  - Polyphonic audio output (up to 8 channels)
  - Polyphonic gate passthrough
  - Normalized mono mix output (average of active voices)
  - Gate-controlled voice activation (per-channel in poly, channel-0 in unison)
requires:
  - slice: S01
    provides: WavetableVoice, PhaseAccumulator, WavetableScanner, DacEmulator, WavetableBank, PPG ROM
affects:
  - S03
  - S05
key_files:
  - src/polyphonic_engine.hpp
  - src/Hurricane8VCO.cpp
  - tests/test_polyphonic_routing.cpp
  - Makefile
key_decisions:
  - Mix output uses normalized average (sum / activeCount) rather than raw sum — prevents clipping with 8 voices
  - Unison detune is linear spread in V/Oct space (not cents-per-voice) — voices distribute symmetrically from -maxDetune to +maxDetune
  - Gate not connected = all V/Oct channels treated as active (10V default gate) — matches VCV Rack convention where oscillators run freely without gate
  - Unison mode always activates all 8 voices regardless of input channel count — uses channel 0 pitch and gate only
patterns_established:
  - PolyphonicEngine is Rack-SDK-independent (same as S01 DSP headers) — testable standalone
  - Separate test binary per slice (`test-poly` alongside `test-dsp`) with shared test framework macros
  - `make test` runs all test suites; `make test-dsp` and `make test-poly` run individually
observability_surfaces:
  - `make test-poly` runs 26 unit tests with pass/fail counts and performance timing
  - Performance tests report µs/48k-samples for both polyphonic and unison modes
drill_down_paths: []
duration: 20min
verification_result: passed
completed_at: 2026-03-13
---

# S02: Polyphonic Routing

**8-voice polyphonic engine with independent voice allocation, unison mode with configurable detune, gate-controlled activation, and normalized mix output — 26 unit tests passing.**

## What Happened

Built the polyphonic routing layer on top of S01's single-voice DSP engine.

**Polyphonic Engine** (`polyphonic_engine.hpp`): New `PolyphonicEngine` struct manages an array of 8 `WavetableVoice` instances. In polyphonic mode, each input channel drives one voice — gate signals control which voices are active, and inactive voices produce zero output. In unison mode, all 8 voices process the same base pitch (from channel 0) with a configurable symmetric detune spread. The detune is applied in V/Oct space: voice 0 gets `-maxDetune` and voice 7 gets `+maxDetune`, with linear interpolation across the middle voices.

**Module Update** (`Hurricane8VCO.cpp`): Expanded from single-voice to full polyphonic I/O. Added voice mode switch (Polyphonic/Unison), unison detune knob (0-100 cents), gate input, polyphonic gate output, and mono mix output. The module reads polyphonic V/Oct and gate cables, feeds them to `PolyphonicEngine`, and sets output channel counts to match — polyphonic mode mirrors input channel count, unison always outputs 8 channels. Gate passthrough copies input gates to the gate output port.

**Mix Output**: Normalized by active voice count (average, not sum) to prevent clipping. With 8 voices all producing ±1.0 range signals, the mix stays bounded. The 5V scaling happens in the module, not the engine.

## Verification

- 26 polyphonic routing unit tests: all pass (26/26)
- 42 DSP foundation tests: still pass (42/42) — no regressions
- 68 total tests passing
- Performance: 8-voice polyphonic processes 48k samples in ~2,974 µs; 8-voice unison in ~3,487 µs (budget: 1,000,000 µs)
- Module builds cleanly against VCV Rack 2 SDK with zero compiler warnings

## Requirements Advanced

- VOC-01 — 8 independent polyphonic voices implemented and tested with 1-8 channel inputs
- VOC-02 — Unison mode stacks all 8 voices with configurable detune (0-100 cents), symmetric spread
- VOC-03 — Runtime switching between polyphonic and unison modes verified in both directions
- OUT-01 — 8-channel polyphonic audio output carries per-voice signals
- OUT-02 — Polyphonic gate passthrough copies input gates to output
- OUT-03 — Mix output sums and normalizes all active voices to mono

## Requirements Validated

- VOC-01 — Unit tests prove 1/4/8 voice allocation with independent pitch tracking and gate control
- VOC-02 — Unit tests prove unison detune spread, zero-detune identity, and gate-off silence
- VOC-03 — Unit tests prove bidirectional mode switching preserves correct voice count
- OUT-01 — Unit tests prove per-voice channel independence and correct channel count
- OUT-02 — Unit tests prove gate signals control voice activation in both modes
- OUT-03 — Unit tests prove mix normalization, clipping prevention, and silence when no voices active

## New Requirements Surfaced

- none

## Requirements Invalidated or Re-scoped

- none

## Deviations

The S02 plan had no tasks or must-haves defined. Implemented the full polyphonic routing based on the requirements (VOC-01..03, OUT-01..03) and S01's forward intelligence.

## Known Limitations

- No panel SVG — deferred to S05
- Individual per-voice output jacks (OUT-04, OUT-05) are not yet wired — deferred to S05
- Velocity CV passthrough (OUT-06) not yet implemented — deferred to S05
- Unison mode uses channel 0 only for pitch/gate — no chord-unison or multi-note unison

## Follow-ups

- S03 needs to add per-voice and global ASR envelopes modulating wavetable position
- S03 needs to add vibrato LFO and glide — these operate per-voice inside `PolyphonicEngine`
- S05 needs to wire 8 individual audio/gate output jacks from `PolyphonicEngine::outputs[]`

## Files Created/Modified

- `src/polyphonic_engine.hpp` — 8-voice polyphonic engine with polyphonic/unison modes, detune, mix output (new)
- `src/Hurricane8VCO.cpp` — Expanded module with polyphonic I/O, gate, voice mode, detune, mix output (rewritten)
- `tests/test_polyphonic_routing.cpp` — 26 unit tests for VOC-01..03 and OUT-01..03 (new)
- `Makefile` — Added `test-poly` and `test-dsp` targets; `make test` runs both (modified)

## Forward Intelligence

### What the next slice should know
- `PolyphonicEngine` is the central struct. S03 modulation (envelopes, LFO, glide) should be added as per-voice state inside the engine or as a separate modulation layer that feeds into `process()`.
- The `process()` method takes a single `position` float — S03 needs to make this per-voice (envelope-modulated position).
- `polyphonic_engine.hpp` is Rack-SDK-independent. Keep it that way.
- Unison mode always uses channel 0 for pitch and gate. The other channels are ignored.

### What's fragile
- Mix normalization divides by `activeCount` — if a voice produces very large values (e.g., from DC offset in wavetable), the mix could still clip before the 5V scaling in the module.
- Unison detune is in V/Oct space, not linear frequency — the detune spread is wider in Hz at higher pitches (exponential). This is musically correct but worth noting.

### Authoritative diagnostics
- `make test-poly` — 26 tests covering all S02 requirements with clear pass/fail
- `make test` — runs both DSP foundation and polyphonic routing (68 tests total)
- Test names map to requirement IDs: `voc01_*` → VOC-01, `out03_*` → OUT-03, etc.

### What assumptions changed
- S02 plan was empty (no tasks/must-haves). Built everything from requirements and S01 forward intelligence in a single pass.
