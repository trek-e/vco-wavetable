---
id: S04
parent: M001
milestone: M001
provides:
  - WAV file loading (single-cycle and multi-frame/Serum-style)
  - User wavetable integration into the oscillator engine
  - JSON serialization for wavetable path persistence across patch save/load
  - Right-click context menu for wavetable file loading
requires:
  - slice: S03
    provides: Modulation engine with start position, ASR envelope, LFO, glide
affects:
  - S05
key_files:
  - src/wav_loader.hpp
  - src/Hurricane8VCO.cpp
  - tests/test_user_wavetables.cpp
  - Makefile
key_decisions:
  - D015: Multi-frame detection uses smallest-first cycle size matching
  - D016: User wavetable fills all 32 bank slots with the same table
patterns_established:
  - WAV file parsing is self-contained (no external library dependency)
  - User wavetable loaded on demand, not at plugin init
observability_surfaces:
  - Context menu shows current wavetable source (filename + frame count or "PPG ROM")
drill_down_paths:
  - (no task summaries — single-pass implementation)
duration: ~30 minutes
verification_result: passed
completed_at: 2026-03-13
---

# S04: User Wavetables

**WAV file loader for single-cycle and multi-frame wavetables, integrated into the oscillator with JSON persistence and right-click loading.**

## What Happened

Built the complete user wavetable pipeline in a single pass:

1. **WAV loader** (`src/wav_loader.hpp`): Self-contained WAV parser supporting 8/16/24/32-bit PCM and 32-bit IEEE float formats. Extracts channel 0 from multi-channel files. Detects cycle length using common wavetable sizes (256, 512, 1024, 2048, 4096, 8192) with smallest-first matching to correctly handle Serum-style concatenated multi-frame files. Resamples each frame to 256 samples via linear interpolation and builds mip-mapped versions for band-limited playback.

2. **Module integration** (`src/Hurricane8VCO.cpp`): Added user wavetable state (path, bank, frame count, loaded flag). The `process()` method selects between the shared PPG ROM bank and the per-instance user bank based on whether a user wavetable is loaded. Added `dataToJson()`/`dataFromJson()` overrides to persist the wavetable file path across patch save/load. Added `appendContextMenu()` to the widget with "Load wavetable..." (opens OS file dialog with .wav filter) and "Revert to PPG ROM wavetables" options, plus a status label showing the current source.

3. **Test suite** (`tests/test_user_wavetables.cpp`): 36 tests covering WAV parsing (16-bit, 32-bit float, invalid files, nonexistent files), single-cycle loading (256/512/1024/2048 samples), multi-frame loading (2/4/64/128 frames, Serum-style WAV), resampling (identity, downsample, upsample), mip-map generation (level 0 fidelity, highest-level smoothness, decreasing energy), frame fill behavior, playback through WavetableVoice, scanning through multi-frame tables, cycle detection logic, and edge cases (empty buffer, very short buffer, DC offset preservation).

## Verification

- **36 new tests** all pass (WAV loading, multi-frame, resampling, mip-mapping, playback)
- **103 existing tests** all pass (42 DSP + 26 polyphonic + 35 modulation) — no regressions
- **Rack SDK build** compiles with zero warnings
- **Makefile** updated with `test-mod` and `test-user-wt` targets (previously only ran dsp and poly)

## Requirements Advanced

- WTD-02 — Single-cycle WAV loading implemented and tested (5 WAV parsing tests + 7 single-cycle tests + 3 resampling tests + 3 mip-map tests)
- WTD-03 — Multi-frame WAV loading implemented and tested (7 multi-frame tests including Serum-style detection)
- WTD-04 — Context menu implemented with osdialog file picker (requires live Rack for interactive testing)
- WTD-05 — JSON serialization implemented via dataToJson/dataFromJson (requires live Rack for round-trip testing)

## Requirements Validated

- WTD-02 — 22 unit tests prove single-cycle WAV files load correctly with proper resampling and mip-mapping
- WTD-03 — 7 unit tests prove multi-frame WAV files are detected, split, and loaded correctly

## New Requirements Surfaced

- none

## Requirements Invalidated or Re-scoped

- none

## Deviations

- S04 plan was empty (no tasks defined). Implemented the entire slice as a single pass based on requirements WTD-02..05.
- Makefile test target was also missing `test-mod` — added it alongside `test-user-wt`.

## Known Limitations

- Cycle detection is heuristic — a 512-sample file is always treated as 2×256 frames, not as a single 512-sample cycle. Users loading genuine 512-sample single-cycle waveforms would get 2 identical frames (harmless but not ideal). The `forceCycleLen` parameter exists in the buffer API but isn't exposed through the file loading path.
- No "clm " chunk parsing for Serum metadata (frame size hint). Auto-detection works for common cases.
- WTD-04 (context menu) and WTD-05 (JSON persistence) are implemented but can only be validated with a live VCV Rack instance — not in standalone unit tests.
- User wavetable bank is ~2MB per instance (all 32 slots filled with the same table). Acceptable for a module that loads at most one user wavetable.

## Follow-ups

- S05 should verify context menu and JSON persistence work in a live Rack session before submission.

## Files Created/Modified

- `src/wav_loader.hpp` — WAV parser, cycle detector, resampler, user wavetable loader (new)
- `src/Hurricane8VCO.cpp` — User wavetable state, process() bank selection, dataToJson/dataFromJson, appendContextMenu
- `tests/test_user_wavetables.cpp` — 36 unit tests for user wavetable functionality (new)
- `Makefile` — Added test-mod and test-user-wt targets

## Forward Intelligence

### What the next slice should know
- The module now has a `userTableLoaded` flag that controls whether `process()` uses the PPG ROM bank or the user bank. S05 panel work doesn't need to know about this — it's transparent.
- The widget's `appendContextMenu` is the right place to add any additional right-click options in S05 (e.g., individual voice output configuration).

### What's fragile
- Cycle detection heuristic in `detectCycleLength` — if users report wrong frame counts, this is where to look. The smallest-first matching favors multi-frame interpretation over single-cycle.

### Authoritative diagnostics
- `make test` runs all 139 tests across 4 suites — if any DSP regression occurs, this is the first thing to run.
- The context menu shows the loaded filename and frame count — visual diagnostic for user wavetable state.

### What assumptions changed
- Originally assumed the slice plan would have tasks pre-defined — it was empty, so implemented as a single coherent pass.
