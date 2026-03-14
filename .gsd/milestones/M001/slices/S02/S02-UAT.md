# S02: Polyphonic Routing — UAT

**Milestone:** M001
**Written:** 2026-03-13

## UAT Type

- UAT mode: artifact-driven
- Why this mode is sufficient: All polyphonic routing logic is in Rack-SDK-independent headers tested via standalone unit tests. The VCV Rack module wiring is a thin integration layer. Unit tests cover all six requirements (VOC-01..03, OUT-01..03) with deterministic inputs and verifiable outputs.

## Preconditions

- Repository cloned with `src/` and `tests/` directories intact
- C++17 compiler available (g++ or clang++)
- VCV Rack SDK available at `../Rack-SDK` (for module build verification only)

## Smoke Test

Run `make test` from project root. Should print `Results: 68/68 passed` (42 DSP + 26 polyphonic). Any failure = slice is broken.

## Test Cases

### 1. Polyphonic voice allocation (VOC-01)

1. Run `make test-poly`
2. Observe tests `voc01_polyphonic_single_voice`, `voc01_polyphonic_four_voices`, `voc01_polyphonic_eight_voices`
3. **Expected:** All pass. Single voice produces output on channel 0 only; 4 voices activate channels 0-3 with silent 4-7; 8 voices activate all channels.

### 2. Independent pitch tracking per voice (VOC-01)

1. Run `make test-poly`
2. Observe test `voc01_polyphonic_independent_pitches`
3. **Expected:** Pass. Four voices at -1V, 0V, 1V, 2V produce four distinct phase accumulator values after 1024 samples, proving each oscillator runs independently.

### 3. Gate-controlled voice activation (VOC-01, OUT-02)

1. Run `make test-poly`
2. Observe tests `voc01_gate_controls_voice_activation`, `out02_gate_passthrough_poly`, `out02_gate_passthrough_unison`
3. **Expected:** All pass. Voices with gate=0V produce zero output and are marked inactive. Voices with gate>0V produce non-zero output. In unison, channel 0 gate controls all 8 voices.

### 4. Unison mode — all 8 voices active (VOC-02)

1. Run `make test-poly`
2. Observe tests `voc02_unison_all_voices_active`, `voc02_unison_gate_controls_all`
3. **Expected:** All pass. With gate on, all 8 voices produce non-zero output. With gate off, all 8 voices are silent.

### 5. Unison detune spread (VOC-02)

1. Run `make test-poly`
2. Observe tests `voc02_unison_detune_spread`, `voc02_unison_zero_detune`, `engine_unison_detune_range`
3. **Expected:** All pass. With detune > 0, voices have divergent phases (different frequencies). With detune = 0, all voices produce identical output. At max detune (100 cents), low/high voices are approximately ±1 semitone from base pitch (~246.94 Hz and ~277.18 Hz around C4).

### 6. Voice mode switching (VOC-03)

1. Run `make test-poly`
2. Observe tests `voc03_switch_poly_to_unison`, `voc03_switch_unison_to_poly`
3. **Expected:** Both pass. Switching poly→unison changes active count from input count to 8. Switching unison→poly changes active count from 8 to input count.

### 7. Polyphonic audio output channels (OUT-01)

1. Run `make test-poly`
2. Observe tests `out01_poly_output_channels_match_input`, `out01_poly_output_per_voice_independent`, `out01_unison_always_8_channels`
3. **Expected:** All pass. Poly mode: output channel count matches input. Different pitches produce different outputs. Unison: always 8 active output channels.

### 8. Mix output normalization (OUT-03)

1. Run `make test-poly`
2. Observe tests `out03_mix_single_voice`, `out03_mix_multiple_voices_normalized`, `out03_mix_normalized_prevents_clipping`, `out03_mix_silent_when_no_voices`, `out03_mix_unison_voices`
3. **Expected:** All pass. Single voice: mix = voice output. Multiple voices: mix = average. 8 voices: mix stays in [-1, 1]. No voices: mix = 0. Unison: mix = average of 8.

### 9. Engine reset (integrity)

1. Run `make test-poly`
2. Observe test `engine_reset_clears_all_state`
3. **Expected:** Pass. After reset: zero outputs, no active voices, all phases at 0.

### 10. Shared parameter propagation (integrity)

1. Run `make test-poly`
2. Observe test `engine_shared_params_propagate`
3. **Expected:** Pass. Table, scan mode, and DAC depth set on the engine propagate to all 8 voice instances.

### 11. Module builds cleanly

1. Run `make` from project root
2. **Expected:** Compiles `Hurricane8VCO.cpp` and links `plugin.dylib` with zero warnings. The module stub includes all S02 params/inputs/outputs (voice mode switch, detune knob, gate I/O, mix output).

### 12. Performance — real-time processing

1. Run `make test-poly`
2. Observe tests `perf_8voice_polyphonic`, `perf_8voice_unison`
3. **Expected:** Both pass. 48,000 samples (1 second) processed in well under 1,000,000 µs. Typical: ~3,000 µs for polyphonic, ~3,500 µs for unison.

## Edge Cases

### Zero input channels

1. In `PolyphonicEngine::process()`, pass `numInputChannels = 0`
2. **Expected:** Engine defaults to 1 channel (handled by the module's `std::max(numVoctChannels, numGateChannels)` with minimum 1). No crash.

### Rapid mode switching

1. Alternate between Polyphonic and Unison every sample for 1000 samples
2. **Expected:** No crash, no NaN/Inf outputs. Engine correctly updates active count each sample.

### All gates off in polyphonic mode

1. Set 8 input channels with all gates at 0V
2. **Expected:** Zero active voices, zero output on all channels, zero mix. No division by zero in `getMixOutput()`.

## Failure Signals

- Any test in `make test-poly` fails → polyphonic routing is broken
- Any test in `make test-dsp` fails → S01 regression, likely from header changes
- `make` produces compiler warnings → module integration issue
- Performance tests exceed 1,000,000 µs → real-time processing budget violated

## Requirements Proved By This UAT

- VOC-01 — 8 independent polyphonic voices from input channels (tests 1-3)
- VOC-02 — Unison mode with configurable detune spread (tests 4-5)
- VOC-03 — Runtime switching between modes (test 6)
- OUT-01 — 8-channel polyphonic audio output (test 7)
- OUT-02 — Gate passthrough controls voice activation (test 3)
- OUT-03 — Normalized mono mix output (test 8)

## Not Proven By This UAT

- OUT-04, OUT-05 — Individual per-voice output jacks (deferred to S05, requires panel wiring)
- OUT-06 — Velocity CV passthrough (deferred to S05)
- Live VCV Rack integration — module loads, cables connect, audio routes correctly in the GUI (requires runtime test in Rack, not automated here)
- Panel layout and visual design (S05)

## Notes for Tester

- The module has no panel SVG yet — it renders as a blank 20HP rectangle in VCV Rack. This is expected until S05.
- If you load the module in VCV Rack, connect a polyphonic MIDI-CV's V/Oct and Gate outputs to the corresponding inputs, and patch the Audio output to a mixer. You should hear polyphonic playback. Switch the Voice Mode switch to hear unison with detune.
- The detune knob has no effect in Polyphonic mode — only in Unison mode.
