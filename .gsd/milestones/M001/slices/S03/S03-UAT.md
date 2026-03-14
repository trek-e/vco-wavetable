# S03: Modulation — UAT

**Milestone:** M001
**Written:** 2026-03-13

## UAT Type

- UAT mode: artifact-driven
- Why this mode is sufficient: All modulation DSP is tested via standalone unit tests that exercise the exact same code paths used in the VCV Rack module. No Rack SDK runtime needed — the modulation engine is header-only and SDK-independent.

## Preconditions

- C++17 compiler available (g++ or clang++)
- Source tree at project root with `src/` and `tests/` directories
- No VCV Rack SDK required for test execution

## Smoke Test

```bash
cd tests && g++ -std=c++17 -O2 -I../src -o test_modulation test_modulation.cpp -lm && ./test_modulation
```
Expected: "35 tests run, 35 passed, 0 failed"

## Test Cases

### 1. ASR Envelope Basic Lifecycle (MOD-01)

1. Build and run `test_modulation`
2. Tests `mod01_asr_idle_at_zero`, `mod01_asr_attack_rises`, `mod01_asr_reaches_sustain`, `mod01_asr_release_falls`, `mod01_asr_returns_to_idle` all pass
3. **Expected:** Envelope starts at 0 (idle), rises monotonically during attack, holds at 1.0 during sustain, falls monotonically during release, returns to 0 at idle

### 2. Per-Voice Envelope Independence (MOD-01)

1. Tests `mod01_per_voice_independent`, `mod01_per_voice_different_timing` pass
2. **Expected:** Voice 0 with gate active has high envelope value. Voice 1 with no gate stays at start position. Voices triggered at different times are at different envelope stages.

### 3. Global Envelope Mode (MOD-02)

1. Tests `mod02_global_envelope_all_voices_same`, `mod02_global_envelope_triggered_by_any_gate`, `mod02_global_envelope_releases_when_all_gates_off` pass
2. **Expected:** All voices read the same modulated position regardless of per-voice gate state. Global envelope triggers when any gate is active, releases when all gates go off.

### 4. Envelope Mode Switching (MOD-03)

1. Tests `mod03_switch_pervoice_to_global`, `mod03_switch_global_to_pervoice` pass
2. **Expected:** Switching from per-voice to global makes all voice positions equal. Switching from global to per-voice allows voices to diverge based on individual gates.

### 5. Start Position Control (MOD-04)

1. Tests `mod04_start_position_zero`, `mod04_start_position_mid`, `mod04_start_position_full` pass
2. **Expected:** With no gate (envelope idle), modulated position equals the start position exactly (0.0, 0.5, or 1.0).

### 6. Start Position CV (MOD-04)

1. Tests `mod04_start_position_cv_positive`, `mod04_start_position_cv_negative`, `mod04_start_position_cv_clamped` pass
2. **Expected:** +5V CV adds 0.5 to position, -5V subtracts 0.5 (clamped to 0), extreme CV values never push position outside [0, 1].

### 7. Envelope + Start Position Interaction (MOD-04)

1. Test `mod04_start_plus_envelope` passes
2. **Expected:** Start position at 0.5, no gate → position is 0.5. After full attack → position sweeps to 1.0. The sweep range is always [start, 1.0].

### 8. Sine LFO Output (MOD-05)

1. Tests `mod05_lfo_produces_sine`, `mod05_lfo_frequency_accuracy`, `mod05_lfo_zero_rate` pass
2. **Expected:** LFO produces bipolar output reaching ±1.0. A 5 Hz LFO produces exactly 5 positive-going zero crossings per second. Zero rate outputs 0.

### 9. Vibrato Pitch Modulation (MOD-05)

1. Test `mod05_vibrato_pitch_modulation` passes
2. **Expected:** With amount=1.0, vibrato depth is ±1/12 V/Oct (1 semitone). The pitch modulation is symmetric.

### 10. LFO Amount Scaling (MOD-06)

1. Tests `mod06_zero_amount_no_vibrato`, `mod06_half_amount_half_depth`, `mod06_amount_scales_linearly` pass
2. **Expected:** Zero amount produces zero vibrato. Amount scales linearly — doubling amount doubles depth.

### 11. Glide Instant Tracking (MOD-07)

1. Test `mod07_glide_zero_amount_instant` passes
2. **Expected:** With glide amount = 0, pitch changes track instantly with no smoothing.

### 12. Glide Smoothing (MOD-07)

1. Tests `mod07_glide_nonzero_smooths`, `mod07_glide_converges`, `mod07_glide_higher_amount_slower` pass
2. **Expected:** Non-zero glide smooths pitch transitions. Glide converges to target over time. Higher amount = slower convergence.

### 13. Glide Per-Voice Independence (MOD-07)

1. Test `mod07_glide_per_voice_independent` passes
2. **Expected:** Voice 0 gliding to +5V and voice 1 gliding to -2V operate independently.

## Edge Cases

### Position Always Bounded

1. Test `integration_position_bounded_0_1` passes
2. **Expected:** Modulated position is always in [0, 1] for all combinations of CV (-10V to +10V), start position (0 to 1), and gate state (on/off).

### Reset Clears All State

1. Test `integration_reset_clears_all` passes
2. **Expected:** After reset, all envelopes are idle at 0, LFO phase is 0, glide processors reinitialize.

### Full Envelope Sweep Range

1. Test `integration_envelope_sweeps_position_range` passes
2. **Expected:** With start=0.25, position follows: idle→0.25, full attack→1.0, full release→0.25.

## Failure Signals

- Any test in `test_modulation` failing indicates a regression in modulation DSP
- Compilation errors in `modulation_engine.hpp` when building any test suite
- Regressions in `test_dsp_foundation` (42 tests) or `test_polyphonic_routing` (26 tests) after modulation changes

## Requirements Proved By This UAT

- MOD-01 — Per-voice ASR envelope modulates wavetable position, triggered by gate input (7 tests)
- MOD-02 — Global ASR envelope mode modulates wavetable position uniformly across all voices (3 tests)
- MOD-03 — User can switch between per-voice and global envelope modes (2 tests)
- MOD-04 — Wavetable start position control sets the base position before envelope modulation, with CV input (7 tests)
- MOD-05 — Internal sine LFO provides vibrato with rate control (4 tests)
- MOD-06 — Internal LFO has amount (depth) control (3 tests)
- MOD-07 — Glide (portamento) smoothly transitions between pitch changes with amount control (6 tests)
- DSP-03 — Wavetable position is controllable via knob with CV input and attenuverter (start position CV test coverage)

## Not Proven By This UAT

- MOD-05/06/07 CV input behavior in VCV Rack runtime (tested via code review of Hurricane8VCO.cpp, not unit-tested because CV input requires Rack SDK)
- Panel layout and knob placement (deferred to S05)
- Audio quality of modulated output in actual playback (requires loading the module in VCV Rack)

## Notes for Tester

- All tests run in < 1 second. The entire suite is deterministic — no randomness, no timing dependencies.
- Run all three test suites to verify no regressions: `test_dsp_foundation` (42), `test_polyphonic_routing` (26), `test_modulation` (35) = 103 total.
