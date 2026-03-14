# S01: Dsp Foundation — UAT

**Milestone:** M001
**Written:** 2026-03-13

## UAT Type

- UAT mode: artifact-driven
- Why this mode is sufficient: S01 is pure DSP math with no UI — all behavior is verifiable through unit tests and build verification. No runtime VCV Rack instance needed.

## Preconditions

- macOS with Xcode command line tools installed (g++ with C++17 support)
- VCV Rack 2 SDK at `../Rack-SDK` relative to project root
- No external dependencies beyond standard library and Rack SDK

## Smoke Test

Run `make test` from project root. Expect 42/42 tests passed with zero failures.

## Test Cases

### 1. Unit tests pass

1. `cd` to project root
2. Run `make test`
3. **Expected:** Output shows "Results: 42/42 passed" with exit code 0

### 2. Module builds cleanly

1. Run `make -j4`
2. **Expected:** Compiles `plugin.dylib` with zero errors and zero warnings (Makefile warnings about duplicate `install` target are acceptable)

### 3. 1V/oct pitch tracking accuracy

1. In test output, verify `voct_c4_is_261hz` passes (0V → 261.626 Hz)
2. Verify `voct_octave_doubling` passes (each 1V step doubles frequency)
3. Verify `voct_10_octave_range` passes (C-1 to C9, ratio of 1024)
4. **Expected:** All three tests pass, confirming ≥10 octave tracking range

### 4. Phase accumulator stability

1. Verify `phase_accumulator_wraps` passes (100,000 samples, phase always in [0,1))
2. Verify `phase_accumulator_frequency_accuracy` passes (1kHz → ~1000 cycles/second)
3. **Expected:** Phase never escapes [0,1), cycle count is accurate

### 5. Wavetable ROM integrity

1. Verify `ppg_rom_generates_valid_data` passes (all 524,288 samples finite)
2. Verify `ppg_rom_bounded` passes (no sample exceeds ±1.5)
3. Verify `all_32_tables_have_data` passes (no empty tables)
4. **Expected:** 32 tables × 64 frames × 256 samples, all valid

### 6. Mip-map band-limiting

1. Verify `mipmap_level0_preserves_source` passes (DFT round-trip error < 0.01)
2. Verify `mipmap_higher_levels_have_less_energy` passes
3. Verify `mipmap_level_selection_low_freq` passes (100Hz → level 0)
4. Verify `mipmap_level_selection_high_freq` passes (10kHz → level > 0)
5. Verify `mipmap_level_selection_very_high_freq` passes (20kHz → high level)
6. **Expected:** Band-limiting selects appropriate harmonic content per frequency

### 7. PPG stepped scanning

1. Verify `stepped_scanning_waits_for_zero_cross` passes
2. Verify `stepped_scanning_immediate_when_at_zero` passes
3. **Expected:** Frame changes only occur at zero crossings (click-free transitions)

### 8. Interpolated scanning

1. Verify `interpolated_scanning_blends_frames` passes
2. Verify `interpolated_scanning_continuous` passes (max inter-sample jump < 0.1)
3. **Expected:** Smooth crossfade between adjacent frames with no discontinuities

### 9. DAC bit-depth emulation

1. Verify `dac_8bit_quantization` passes (snaps to 256-level grid)
2. Verify `dac_12bit_quantization` passes (snaps to 4096-level grid)
3. Verify `dac_16bit_quantization` passes (nearly transparent)
4. Verify `dac_8bit_creates_audible_steps` passes (8-bit error ≥ 16-bit error)
5. Verify `dac_zero_stays_zero` passes at all bit depths
6. **Expected:** Each depth quantizes to correct number of levels

### 10. Performance budget

1. Verify `voice_performance_48k_samples` passes
2. Check printed timing is well under 1,000,000 µs
3. **Expected:** Single voice processes 1 second of audio in < 500,000 µs (leaves >87% budget for 8 voices + other processing)

## Edge Cases

### Out-of-range table/frame indices

1. Verify `mipmap_clamps_out_of_range_table` passes (indices -1 and 999)
2. Verify `mipmap_clamps_out_of_range_frame` passes (indices -1 and 999)
3. **Expected:** Clamped to valid range, no crash, finite output

### Extreme frequencies

1. Verify `phase_accumulator_handles_zero_freq` passes
2. Verify `phase_accumulator_handles_very_high_freq` passes (23kHz)
3. **Expected:** No crash, phase stays bounded

### Position boundary clamping

1. Verify `scanner_position_clamping` passes (positions -0.5 and 1.5)
2. **Expected:** Target frame clamped to valid range

## Failure Signals

- Any test reporting FAIL in `make test` output
- Non-zero exit code from test binary
- Compiler errors or warnings from `make`
- NaN or Inf in any test output
- Performance test exceeding 500ms for 48k samples

## Requirements Proved By This UAT

- DSP-01 — 1V/oct tracking across 10+ octaves (tests 3, 4)
- DSP-02 — Mip-mapped band-limited wavetables (test 6)
- DSP-04 — PPG stepped scanning at zero crossings (test 7)
- DSP-05 — Interpolated scanning crossfade (test 8)
- DSP-06 — Switchable scan modes (tests 7, 8 together + `voice_mode_switching`)
- DSP-07 — DAC bit-depth emulation at 8/12/16-bit (test 9)
- WTD-01 — PPG ROM wavetable set bundled and playable (test 5)

## Not Proven By This UAT

- DSP-03 — Wavetable position CV input with attenuverter (module stub exists but not tested under live CV)
- Polyphonic operation (8 voices) — deferred to S02
- Audio quality subjective evaluation — requires listening test in VCV Rack
- Panel design and visual layout — deferred to S05

## Notes for Tester

- The `make` command will show a warning about duplicate `install` target — this is harmless (our Makefile's install rule overrides the SDK's default).
- Init time of ~670ms is for plugin load only, not per-sample. Acceptable for a wavetable synth.
- Test binary is at `tests/test_dsp_foundation` after build.
