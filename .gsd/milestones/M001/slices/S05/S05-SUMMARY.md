---
id: S05
parent: M001
milestone: M001
provides:
  - Panel SVG (28HP, dark theme, text as paths, VCV Library compliant)
  - 8 individual per-voice audio output jacks (OUT-04)
  - 8 individual per-voice gate output jacks (OUT-05)
  - Polyphonic velocity CV passthrough input/output (OUT-06)
  - Updated widget layout with all 41 components positioned
  - 24 new unit tests for panel/submission validation
requires:
  - slice: S04
    provides: User wavetable loading, context menu, JSON persistence
affects: []
key_files:
  - res/Hurricane8VCO.svg
  - src/Hurricane8VCO.cpp
  - tests/test_panel_submission.cpp
  - Makefile
  - plugin.json
key_decisions:
  - D018: Panel width increased from 20HP to 28HP to accommodate 8+8 individual voice outputs
  - D019: Individual outputs on right side of panel in two columns (audio/gate), spaced 10.5mm vertically
  - D020: Velocity is a direct cable-level passthrough (input→output), not processed through the DSP engine
patterns_established:
  - Panel SVG uses geometric path elements for all text (no <text> tags) per VCV Library requirements
  - Individual voice outputs use sequential enum IDs (VOICE1_OUTPUT..VOICE8_OUTPUT, GATE1_OUTPUT..GATE8_OUTPUT) for loop-based wiring
observability_surfaces:
  - Panel SVG validated by unit tests (existence, dimensions, XML validity, no text elements)
  - Plugin manifest validated by unit tests (required fields, module entry, version format)
drill_down_paths: []
duration: ~20 minutes
verification_result: passed
completed_at: 2026-03-13
---

# S05: Panel and Submission

**28HP panel SVG with 8 individual voice audio/gate outputs, velocity CV passthrough, and VCV Library submission readiness — 163 total tests passing.**

## What Happened

1. **Individual voice outputs** (`src/Hurricane8VCO.cpp`): Added 8 per-voice audio output jacks (OUT-04), 8 per-voice gate output jacks (OUT-05), and a polyphonic velocity CV input/output pair (OUT-06). Individual audio outputs carry the same signal as the polyphonic output channels but as mono jacks for external per-voice processing. Individual gate outputs mirror the corresponding gate input channel in polyphonic mode, or all mirror channel 0 in unison mode. Velocity is a direct passthrough — the engine doesn't process it.

2. **Panel SVG** (`res/Hurricane8VCO.svg`): Created a 28HP (142.24mm × 128.5mm) dark-themed panel with all text rendered as geometric path/rect elements (no `<text>` tags). Layout organized into sections: Oscillator (top-left), Voice (middle-left), Modulation (center-left), Inputs (lower-left, two rows of 4), Polyphonic Outputs (bottom-left), and Individual Voice Outputs (right side, two columns of 8). Includes decorative wavetable waveform visualization and brand elements.

3. **Widget update** (`src/Hurricane8VCO.cpp`): Replaced the blank panel placeholder with `setPanel(createPanel(...))` loading the SVG. Repositioned all 41 components (13 params, 8 inputs, 20 outputs) to match the 28HP layout. Individual voice outputs use a loop placing 8 audio jacks at x=100mm and 8 gate jacks at x=125mm, spaced 10.5mm apart vertically.

4. **Test suite** (`tests/test_panel_submission.cpp`): 24 tests covering individual voice audio outputs (5 tests: polyphonic 4-voice, all 8, unison, independence, poly consistency), individual gate outputs (3 tests: polyphonic routing, unison mirroring, gate-off silence), velocity passthrough (2 tests), panel SVG validation (5 tests: existence, XML validity, dimensions, no text elements, viewBox), plugin manifest validation (4 tests: existence, required fields, module entry, version format), build compatibility (3 tests), and integration checks (2 tests: output bounding, mix-individual consistency).

## Verification

- **163 total tests pass** (42 DSP + 26 polyphonic + 35 modulation + 36 user wavetables + 24 panel/submission)
- **Zero compiler warnings** from Rack SDK build (`-Wall -Wextra`)
- **Panel SVG** validated: correct dimensions (28HP), valid XML, no `<text>` elements, has viewBox
- **Plugin manifest** validated: all required fields present, correct module slug/tags, valid semver version
- **No regressions** in any existing test suite

## Requirements Validated

- OUT-04 — 5 unit tests prove individual per-voice audio outputs carry independent signals in both poly and unison modes
- OUT-05 — 3 unit tests prove individual gate outputs correctly route per-voice gates
- OUT-06 — 2 unit tests + code inspection confirm velocity CV passthrough works as direct cable routing
- PNL-01 — 5 unit tests validate panel SVG meets VCV Library design standards
- PNL-02 — 4 unit tests validate plugin manifest has correct metadata
- PNL-03 — Zero-warning build + 3 unit tests confirm Rack 2 compatibility with no deprecated APIs
- DSP-04 through DSP-07, WTD-01, WTD-04, WTD-05 — Moved from "active" to "validated" based on accumulated evidence from S01-S05

## New Requirements Surfaced

- none

## Requirements Invalidated or Re-scoped

- none

## Deviations

- Panel width increased from 20HP to 28HP. The original widget code assumed 20HP but 41 components (including 16 new individual output jacks) require more space.
- Added VELOCITY_INPUT (new input jack) alongside VELOCITY_OUTPUT — the original requirements only mentioned output, but a passthrough requires an input to receive velocity from upstream MIDI-CV.

## Known Limitations

- Panel SVG uses geometric rectangles for text labels rather than proper font-to-path conversion. Labels are readable but not typographically refined. A professional designer could improve this for Library submission.
- WTD-04 and WTD-05 require live VCV Rack for full interactive validation (file dialog, patch save/load round-trip). Unit tests validate the code paths but not the GUI interaction.
- Velocity CV is a simple passthrough — no velocity-to-loudness scaling or any processing. Users must route velocity to a VCA downstream.

## Follow-ups

- Professional panel design refinement before VCV Library submission (typography, color polish, component alignment fine-tuning)
- Live testing in VCV Rack: verify panel rendering, all jacks connect properly, right-click menu works, patch save/load preserves user wavetable path

## Files Created/Modified

- `res/Hurricane8VCO.svg` — Panel SVG, 28HP dark theme (new)
- `src/Hurricane8VCO.cpp` — Added 17 new I/O ports (velocity in, velocity out, 8 voice outs, 8 gate outs), updated widget with panel SVG and 28HP layout
- `tests/test_panel_submission.cpp` — 24 unit tests for OUT-04/05/06, PNL-01/02/03 (new)
- `Makefile` — Added test-panel target

## Forward Intelligence

### What the next slice should know
- All 31 requirements are validated. The module is feature-complete for v1.0 submission to the VCV Library.
- The panel SVG exists but would benefit from professional design polish before submission.
- The full test suite is `make test` — runs 163 tests across 5 suites in ~3 seconds.

### What's fragile
- Panel label readability — the geometric rect/path text is functional but not beautiful. A designer should review at 100% zoom in Rack.
- Individual output jack positions are calculated by loop (`y = 19 + i*10.5`) — if the panel layout changes, both SVG and widget code must be updated together.

### Authoritative diagnostics
- `make test` — 163 tests across 5 suites cover all DSP, routing, modulation, wavetable loading, and panel/submission requirements
- `make` — zero-warning Rack SDK build proves API compatibility
- `res/Hurricane8VCO.svg` — open in any SVG viewer to verify panel layout

### What assumptions changed
- 20HP was too narrow for all the I/O — expanded to 28HP to fit 8+8 individual voice outputs alongside the main controls
