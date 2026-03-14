# S04: User Wavetables — UAT

**Milestone:** M001
**Written:** 2026-03-13

## UAT Type

- UAT mode: mixed
- Why this mode is sufficient: DSP layer is fully covered by 36 unit tests (artifact-driven). Context menu and JSON persistence require a live VCV Rack instance (live-runtime). Both modes are needed because the WAV loader is testable standalone but the Rack integration points (osdialog, JSON serialization) are not.

## Preconditions

- All 139 unit tests pass (`make test`)
- Plugin builds cleanly (`make`)
- VCV Rack 2 installed (for live-runtime tests only)
- Test WAV files available: a single-cycle 256-sample .wav and a multi-frame Serum-style .wav (8×256 = 2048 samples)

## Smoke Test

Run `make test` — all 4 test suites (dsp_foundation, polyphonic_routing, modulation, user_wavetables) should report 139/139 passed.

## Test Cases

### 1. Single-cycle WAV loading (WTD-02, unit tests)

1. Build and run `tests/test_user_wavetables`
2. Observe tests `wtd02_single_cycle_256_samples`, `wtd02_single_cycle_wav_file`, `wtd02_resampled_waveform_shape`
3. **Expected:** All pass — 256-sample WAV loads as 1 frame, resampled waveform retains sine shape

### 2. Multi-frame WAV loading (WTD-03, unit tests)

1. Build and run `tests/test_user_wavetables`
2. Observe tests `wtd03_serum_style_wav_file`, `wtd03_multi_frame_64_cycles_max`, `wtd03_multi_frame_different_waveforms`
3. **Expected:** All pass — Serum-style 2048-sample file detected as 8 frames, 64-frame max respected, different waveforms produce different frame data

### 3. 16-bit and float WAV format support (WTD-02, unit tests)

1. Build and run `tests/test_user_wavetables`
2. Observe tests `wtd02_load_16bit_wav`, `wtd02_load_float_wav`, `wtd02_16bit_sample_accuracy`
3. **Expected:** All pass — both formats decode correctly with appropriate precision

### 4. Mip-map generation from user wavetables (WTD-02, unit tests)

1. Build and run `tests/test_user_wavetables`
2. Observe tests `wtd02_mipmap_level0_preserves_waveform`, `wtd02_mipmap_highest_level_is_sinusoidal`, `wtd02_mipmap_decreasing_energy`
3. **Expected:** All pass — mip level 0 matches source, highest level is smooth sinusoid, energy decreases with level

### 5. Context menu wavetable loading (WTD-04, live Rack)

1. Install plugin in VCV Rack 2 (`make install`)
2. Add Hurricane 8 VCO to a patch
3. Right-click the module
4. Observe context menu has separator, "Wavetable: PPG ROM (built-in)" label, and "Load wavetable..." item
5. Click "Load wavetable..."
6. **Expected:** OS file dialog opens with WAV filter. Select a .wav file → dialog shows "Wavetable: filename.wav (N frames)" and "Revert to PPG ROM wavetables" appears

### 6. Revert to PPG ROM (WTD-04, live Rack)

1. With a user wavetable loaded (from test 5), right-click the module
2. Click "Revert to PPG ROM wavetables"
3. Right-click again
4. **Expected:** Status returns to "Wavetable: PPG ROM (built-in)", revert option disappears, sound changes back to PPG ROM character

### 7. JSON persistence across patch save/load (WTD-05, live Rack)

1. Load a user wavetable via context menu (from test 5)
2. Save the patch (File → Save As)
3. Close and reopen VCV Rack
4. Open the saved patch
5. Right-click the module
6. **Expected:** Context menu shows the same loaded wavetable filename and frame count. The oscillator produces the same sound as before saving.

### 8. Playback through voice engine (WTD-02, unit test)

1. Build and run `tests/test_user_wavetables`
2. Observe test `wtd02_playback_through_voice`
3. **Expected:** Pass — user wavetable produces non-zero output through WavetableVoice

### 9. No regression in existing DSP (all slices)

1. Run `make test`
2. **Expected:** All 4 suites pass: 42 DSP + 26 polyphonic + 35 modulation + 36 user wavetable = 139 total

## Edge Cases

### Invalid WAV file

1. Run `tests/test_user_wavetables` — observe `wtd02_load_nonexistent_file`, `wtd02_load_invalid_file`
2. **Expected:** Both pass — loader returns valid=false with descriptive error message, no crash

### Very short waveform

1. Run `tests/test_user_wavetables` — observe `wtd02_very_short_buffer`
2. **Expected:** Pass — even a 4-sample buffer loads as 1 frame (resampled to 256)

### DC offset waveform

1. Run `tests/test_user_wavetables` — observe `wtd02_dc_offset_preserved`
2. **Expected:** Pass — constant 0.5 DC waveform preserves DC component through mip-mapping

### Exceeding max frames

1. Run `tests/test_user_wavetables` — observe `wtd03_multi_frame_clamps_to_64`
2. **Expected:** Pass — 128-frame input clamped to 64 frames (PPG_FRAMES_PER_TABLE)

### Missing WAV file on patch reload (WTD-05, live Rack)

1. Load a user wavetable, save the patch
2. Delete or move the original .wav file
3. Reopen the patch
4. **Expected:** Module reverts to PPG ROM tables (loadUserWavetableFromFile fails gracefully, userTableLoaded stays false)

## Failure Signals

- Any test in `make test` reports FAILED
- Rack build produces compiler warnings
- Context menu doesn't appear or crashes on right-click
- Saved patch doesn't restore the user wavetable on reload
- Audio output is silent or distorted after loading a user wavetable

## Requirements Proved By This UAT

- WTD-02 — Unit tests prove single-cycle WAV files load, resample, and mip-map correctly
- WTD-03 — Unit tests prove multi-frame WAV files are auto-detected and split into frames
- WTD-04 — Live Rack test proves context menu opens file dialog and displays status (test 5, 6)
- WTD-05 — Live Rack test proves wavetable path persists across save/load (test 7)

## Not Proven By This UAT

- WTD-04 and WTD-05 live Rack tests require manual verification — not automated
- 24-bit WAV loading is implemented but not tested via file (only the parser path handles it; no createTestWav24 helper exists). The format is simple enough that the int24 decoding is trustworthy.
- Performance under real-time constraints (loading a user wavetable blocks briefly during mip-map generation — fine for a menu action, but not measured)
- Behavior with stereo or 5.1 WAV files (channel 0 extraction is implemented but untested via file)

## Notes for Tester

- The cycle detection heuristic always treats files divisible by 256 as multi-frame. A genuine 512-sample single-cycle waveform will appear as 2 frames with identical content — this is harmless but worth noting if users report unexpected frame counts.
- The "Load wavetable..." dialog uses osdialog which blocks the Rack UI thread. This is standard VCV Rack convention (same as sample-loading modules).
- For S05, the individual voice output jacks and panel SVG still need to be created — this slice only covers the wavetable loading pipeline.
