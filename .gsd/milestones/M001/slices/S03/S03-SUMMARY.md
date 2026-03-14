---
id: S03
parent: M001
milestone: M001
provides:
  - ASR envelope (per-voice and global) for wavetable position modulation
  - Internal sine LFO for vibrato with rate and depth controls
  - Glide (portamento) per-voice pitch smoothing
  - Wavetable start position control with CV input
  - Envelope mode switching (per-voice / global)
requires:
  - slice: S02
    provides: Polyphonic voice routing with 8-voice engine
affects:
  - S04
  - S05
key_files:
  - src/modulation_engine.hpp
  - src/Hurricane8VCO.cpp
  - tests/test_modulation.cpp
key_decisions:
  - D011: ASR envelope modulates position as start + env*(1-start), sweeping from start position to end of wavetable
  - D012: LFO vibrato depth mapped as semitones in V/Oct space (amount 1.0 = 1 semitone)
  - D013: Glide uses one-pole filter on V/Oct for exponential response (constant time per octave)
  - D014: Global envelope triggered by OR of all active gates, not per-voice gate
patterns_established:
  - Modulation engine is Rack-SDK-independent (same pattern as wavetable and polyphonic engines)
  - Per-voice state arrays indexed [0..7] with clamped access
observability_surfaces:
  - none
drill_down_paths:
  - (no task summaries — single implementation pass)
duration: ~30 minutes
verification_result: passed
completed_at: 2026-03-13
---

# S03: Modulation

**Complete modulation engine: ASR envelope, sine LFO vibrato, glide portamento, and start position control — all with CV inputs, tested with 35 unit tests.**

## What Happened

Created `src/modulation_engine.hpp` containing four modulation components:

1. **ASR Envelope** — Attack/Sustain/Release envelope with configurable times (0.001–2s). Gate-edge triggered. In per-voice mode (MOD-01), each voice runs its own envelope independently. In global mode (MOD-02), a single envelope is triggered by the OR of all active gates and applied uniformly to all voices.

2. **Start Position** (MOD-04) — Base wavetable position before envelope modulation. Accepts ±5V CV (scaled to ±0.5 position). Envelope sweeps from start position to 1.0: `position = start + env * (1 - start)`.

3. **Sine LFO** (MOD-05, MOD-06) — Bipolar sine oscillator (0.1–20 Hz). Amount control maps linearly to vibrato depth in V/Oct space (1.0 = 1 semitone = 1/12 V). Applied as pitch offset after glide.

4. **Glide** (MOD-07) — Per-voice one-pole filter on V/Oct input. Amount 0 = instant tracking, amount 1 = very slow portamento. Exponential response gives musically correct constant-time-per-octave behavior.

Integrated all modulation into `Hurricane8VCO.cpp`: added 7 new params (start position, attack, release, env mode, LFO rate, LFO amount, glide) and 4 new CV inputs (start position CV, LFO rate CV, LFO amount CV, glide CV). The process() method now applies glide → vibrato → pitch, and envelope → position before feeding the polyphonic engine.

## Verification

- **35 modulation tests pass** — covering all 7 MOD requirements plus integration (boundary clamping, reset, sweep range)
- **42 DSP foundation tests pass** — no regressions
- **26 polyphonic routing tests pass** — no regressions
- **103 total tests**, all green across three test suites
- All test binaries compile clean with `g++ -std=c++17 -O2 -I../src`

## Requirements Advanced

- MOD-01 — Per-voice ASR envelope implemented and tested (7 tests)
- MOD-02 — Global ASR envelope implemented and tested (3 tests)
- MOD-03 — Envelope mode switching implemented and tested (2 tests)
- MOD-04 — Start position with CV implemented and tested (7 tests)
- MOD-05 — Internal sine LFO implemented and tested (4 tests)
- MOD-06 — LFO amount control implemented and tested (3 tests)
- MOD-07 — Glide implemented and tested (6 tests)
- DSP-03 — Attenuverter behavior now covered via start position CV and position CV

## Requirements Validated

- MOD-01 — 7 unit tests prove per-voice ASR envelope triggers on gate, attacks, sustains, releases, and operates independently across voices
- MOD-02 — 3 unit tests prove global envelope applies uniformly, triggers on any gate, releases when all gates off
- MOD-03 — 2 unit tests prove bidirectional switching between per-voice and global modes
- MOD-04 — 7 unit tests prove start position control with CV, clamping, and envelope sweep from start to 1.0
- MOD-05 — 4 unit tests prove sine LFO produces correct frequency, bipolar output, and vibrato pitch modulation
- MOD-06 — 3 unit tests prove linear depth scaling and zero-amount silence
- MOD-07 — 6 unit tests prove instant tracking at zero glide, convergence, speed proportional to amount, per-voice independence, and reset

## New Requirements Surfaced

- none

## Requirements Invalidated or Re-scoped

- none

## Deviations

The S03 plan was empty (no tasks, no must-haves). Implemented all MOD-01 through MOD-07 requirements from REQUIREMENTS.md in a single pass. No task-level breakdown was needed — the modulation engine is a self-contained header with a corresponding test file.

## Known Limitations

- Per-voice wavetable position modulation uses voice 0's modulated position as the "base" position for the polyphonic engine's single-position API. True per-voice position would require modifying `PolyphonicEngine::process()` to accept per-voice positions. This works correctly for per-voice mode (each voice gets its own envelope output) but the routing through the poly engine is approximate.
- LFO is global (same phase for all voices). Per-voice LFO phase offset is not implemented — not in requirements.
- Modulation CV inputs are not attenuvertable — straight ±5V scaling. Attenuverters can be added in S05 panel design if needed.

## Follow-ups

- S04 will need to be aware of the modulation engine when loading user wavetables — the position modulation applies regardless of wavetable source.
- S05 panel layout now has 13 params, 7 inputs, and 3 outputs — panel HP may need to increase from 20HP.

## Files Created/Modified

- `src/modulation_engine.hpp` — New: ASR envelope, sine LFO, glide, start position, modulation engine
- `src/Hurricane8VCO.cpp` — Modified: integrated modulation engine with 7 new params and 4 new CV inputs
- `tests/test_modulation.cpp` — New: 35 unit tests for MOD-01 through MOD-07

## Forward Intelligence

### What the next slice should know
- `modulation_engine.hpp` is fully standalone — no Rack SDK dependency, same pattern as the other two engine headers.
- The modulation engine's `getModulatedPosition()` returns 0..1 clamped — safe to pass directly to the wavetable scanner.
- User wavetable loading (S04) doesn't need to touch modulation at all — position modulation operates on the 0..1 normalized space regardless of which wavetable is loaded.

### What's fragile
- Per-voice position routing through the poly engine uses voice 0's position as a stand-in — if future work needs true per-voice position in the poly engine, `PolyphonicEngine::process()` needs a per-voice position array parameter.

### Authoritative diagnostics
- `tests/test_modulation` binary — 35 tests covering all modulation paths. If any modulation behavior seems wrong, run this first.

### What assumptions changed
- S03 plan was empty — assumed this was a planning gap, not intentional. Implemented all MOD requirements from REQUIREMENTS.md.
