# S05: Panel and Submission — UAT

**Milestone:** M001
**Written:** 2026-03-13

## UAT Type

- UAT mode: mixed
- Why this mode is sufficient: Panel SVG existence and format are artifact-driven (unit tests validate). Individual outputs, velocity passthrough, and integration behavior are tested by standalone DSP tests. Live Rack runtime is needed for visual panel verification, jack connectivity, and context menu interaction.

## Preconditions

- VCV Rack 2 installed and running
- Plugin built and installed via `make install`
- A MIDI-CV module available in the patch (for polyphonic input + velocity)
- At least one VCA or Scope module for output verification

## Smoke Test

1. Launch VCV Rack 2
2. Add Hurricane 8 from the module browser (Synth-etic Intelligence → Hurricane 8)
3. **Expected:** Module appears with dark panel, 28HP wide, showing sections for Oscillator, Voice, Modulation, Inputs, Outputs, and individual Voice Outputs on the right side

## Test Cases

### 1. Panel renders correctly at 100% zoom

1. Add Hurricane 8 module to an empty patch
2. Verify the panel shows the module title "HURRICANE 8" at the top
3. Verify all 13 knobs/switches are visible and positioned within labeled sections
4. Verify 8 input jacks are visible in the Inputs section (2 rows of 4)
5. Verify 4 polyphonic output jacks are visible at the bottom left
6. Verify 16 individual output jacks are visible on the right side (2 columns of 8)
7. **Expected:** All components are readable, properly spaced, and no overlaps. Panel screws visible in 4 corners.

### 2. Individual voice audio outputs carry per-voice signals (OUT-04)

1. Connect a polyphonic MIDI-CV (or polyphonic LFO) to V/Oct and Gate inputs
2. Play 4 notes simultaneously
3. Connect Voice 1 individual audio output to a Scope module
4. Connect Voice 2 individual audio output to a second Scope channel
5. **Expected:** Each individual output shows a different waveform (different pitches). Voice 5-8 outputs are silent (no input on those channels). Voice 1-4 outputs each carry one voice.

### 3. Individual voice gate outputs mirror input gates (OUT-05)

1. With the same polyphonic MIDI-CV setup as Test 2
2. Connect Gate 1 individual output to a Scope or LED
3. Connect Gate 3 individual output to another LED
4. Play notes and release them
5. **Expected:** Each individual gate output mirrors the corresponding channel's gate — high when that voice's key is held, low when released.

### 4. Individual outputs in unison mode (OUT-04/OUT-05)

1. Switch Voice Mode to Unison
2. Play a single note
3. Check all 8 individual audio outputs
4. **Expected:** All 8 individual audio outputs produce sound (8 detuned voices on one pitch). All 8 individual gate outputs are high (all mirror channel 0's gate).

### 5. Velocity CV passthrough (OUT-06)

1. Connect a MIDI-CV module with Velocity output to Hurricane 8's Velocity input
2. Connect Hurricane 8's Velocity polyphonic output to a VCA's CV input
3. Play notes at different velocities (soft vs hard)
4. **Expected:** Velocity output carries the same polyphonic velocity data as the MIDI-CV provides. Softer notes produce lower voltage, harder notes produce higher voltage. Values pass through unmodified.

### 6. Polyphonic outputs still work alongside individual outputs

1. Connect both the polyphonic Audio output and individual Voice 1 output to separate Scope channels
2. Play a single note
3. **Expected:** Polyphonic Audio channel 0 and Voice 1 individual output carry the same signal (both scaled to ±5V).

### 7. Mix output consistency

1. Play 4 polyphonic notes
2. Connect Mix output to a Scope
3. **Expected:** Mix output shows a composite waveform (normalized sum of 4 voices). Level does not clip regardless of voice count.

### 8. Plugin manifest correctness

1. Open `plugin.json` in a text editor
2. **Expected:** slug is "Hurricane8", version is "2.0.0", license is "GPL-3.0-or-later", module slug is "Hurricane8VCO", tags include "Oscillator", "Polyphonic", "Wavetable", "Digital"

### 9. Right-click context menu (WTD-04 live verification)

1. Right-click on the Hurricane 8 module panel
2. **Expected:** Context menu shows "Wavetable: PPG ROM (built-in)" status, and a "Load wavetable..." option
3. Click "Load wavetable..." and select a .wav file
4. **Expected:** Status changes to show the loaded filename and frame count. Sound changes to the loaded wavetable. "Revert to PPG ROM wavetables" option appears.

### 10. Patch save/load preserves wavetable (WTD-05 live verification)

1. Load a user wavetable via right-click menu
2. Save the patch (Ctrl/Cmd+S)
3. Close and reopen the patch
4. **Expected:** The user wavetable is automatically reloaded. Context menu shows the same filename. Sound matches what was loaded before save.

## Edge Cases

### No gate connected — voices run freely

1. Connect only V/Oct (no Gate connection)
2. Check individual audio outputs
3. **Expected:** All connected V/Oct channels produce audio on their individual outputs (default 10V gate applied internally). Individual gate outputs are 0V (no gate to pass through).

### Single voice — only Voice 1 active

1. Connect a monophonic (1-channel) V/Oct and Gate
2. **Expected:** Voice 1 individual output has audio. Voice 2-8 individual outputs are silent. Only Gate 1 passes through the gate signal.

### Velocity input not connected

1. Leave Velocity input unconnected
2. Check Velocity output
3. **Expected:** Velocity output is 0V (no signal to pass through). No errors or crashes.

## Failure Signals

- Module doesn't appear in the module browser → plugin.json slug mismatch or build failure
- Panel renders blank or wrong size → SVG dimensions incorrect or file not found
- Individual outputs all carry the same signal → output routing loop not indexing per-voice
- Individual outputs are at wrong voltage level → missing or incorrect ×5V scaling
- Gate individual outputs don't match input → gate routing uses wrong channel index
- Velocity output always 0V when input is connected → velocity passthrough code not executing
- Compiler warnings on build → API compatibility issue

## Requirements Proved By This UAT

- OUT-04 — Tests 2, 3, 4 prove individual audio outputs carry per-voice signals
- OUT-05 — Tests 3, 4 prove individual gate outputs route correctly
- OUT-06 — Test 5 proves velocity CV passthrough
- PNL-01 — Test 1 proves panel renders correctly
- PNL-02 — Test 8 proves manifest correctness
- WTD-04 — Test 9 proves context menu works in live Rack
- WTD-05 — Test 10 proves JSON persistence works across save/load

## Not Proven By This UAT

- Panel design quality/aesthetics at publication standard — requires VCV team review
- Cross-platform build (Windows, Linux) — only tested on macOS
- Performance under heavy polyphony load in a large patch
- Accessibility of panel text at zoom levels other than 100%

## Notes for Tester

- The panel SVG uses geometric shapes for text labels rather than professional font-to-path conversion. Labels are functional but may look rough compared to commercial modules. This is acceptable for initial Library submission review.
- Individual voice outputs are in the right panel section — two columns labeled "AUD" and "GATE" with voice numbers 1-8 alongside.
- The module is 28HP wide — wider than typical oscillators. This is necessary to accommodate the 16 individual output jacks.
- All unit tests (`make test`) should pass before attempting live UAT. If any fail, fix those first.
