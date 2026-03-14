# Requirements

## Active

### DSP-01 — Oscillator tracks 1V/oct pitch input across 10+ octaves with polyphonic support (8 channels)

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (DSP implemented), S02 (polyphonic routing)

Oscillator tracks 1V/oct pitch input across 10+ octaves with polyphonic support (8 channels)

### DSP-02 — Phase accumulator per voice produces alias-free output using mip-mapped band-limited wavetables

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (DSP implemented), S02 (per-voice routing)

Phase accumulator per voice produces alias-free output using mip-mapped band-limited wavetables

### DSP-03 — Wavetable position is controllable via knob with CV input and attenuverter

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (basic control), S03 (attenuverter + modulation)

Wavetable position is controllable via knob with CV input and attenuverter

### DSP-04 — PPG stepped scanning mode switches wavetables at zero-crossing points (authentic PPG behavior)

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)

PPG stepped scanning mode switches wavetables at zero-crossing points (authentic PPG behavior)

### DSP-05 — Interpolated scanning mode crossfades smoothly between adjacent wavetables

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)

Interpolated scanning mode crossfades smoothly between adjacent wavetables

### DSP-06 — User can switch between stepped and interpolated scanning modes

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)

User can switch between stepped and interpolated scanning modes

### DSP-07 — DAC bit depth emulation quantizes output at selectable 8-bit, 12-bit, or 16-bit resolution

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)

DAC bit depth emulation quantizes output at selectable 8-bit, 12-bit, or 16-bit resolution

### WTD-01 — Original PPG ROM wavetable set is bundled and immediately playable

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (32 tables generated and mip-mapped)

Original PPG ROM wavetable set is bundled and immediately playable

### WTD-02 — User can load single-cycle WAV files as custom wavetables

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S04

User can load single-cycle WAV files as custom wavetables

### WTD-03 — User can load multi-frame WAV files (Serum-style concatenated cycles)

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S04

User can load multi-frame WAV files (Serum-style concatenated cycles)

### WTD-04 — Right-click context menu provides wavetable file loading dialog

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S04

Right-click context menu provides wavetable file loading dialog

### WTD-05 — Loaded wavetable file path persists across patch save/load (JSON serialization)

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S04

Loaded wavetable file path persists across patch save/load (JSON serialization)

### MOD-01 — Per-voice ASR envelope modulates wavetable position, triggered by gate input

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Per-voice ASR envelope modulates wavetable position, triggered by gate input

### MOD-02 — Global ASR envelope mode modulates wavetable position uniformly across all voices

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Global ASR envelope mode modulates wavetable position uniformly across all voices

### MOD-03 — User can switch between per-voice and global envelope modes

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

User can switch between per-voice and global envelope modes

### MOD-04 — Wavetable start position control sets the base position before envelope modulation, with CV input

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Wavetable start position control sets the base position before envelope modulation, with CV input

### MOD-05 — Internal sine LFO provides vibrato with rate control and CV input

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Internal sine LFO provides vibrato with rate control and CV input

### MOD-06 — Internal LFO has amount (depth) control with CV input

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Internal LFO has amount (depth) control with CV input

### MOD-07 — Glide (portamento) smoothly transitions between pitch changes with amount control and CV input

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S03

Glide (portamento) smoothly transitions between pitch changes with amount control and CV input

### OUT-01 — 8-channel polyphonic audio output carries all voice signals

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

8-channel polyphonic audio output carries all voice signals

### OUT-02 — 8-channel polyphonic gate output passes gate signals downstream

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

8-channel polyphonic gate output passes gate signals downstream

### OUT-03 — Mix output sums all active voices to a single mono signal

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

Mix output sums all active voices to a single mono signal

### OUT-04 — 8 individual 3.5mm audio output jacks, one per voice

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S05

8 individual 3.5mm audio output jacks, one per voice

### OUT-05 — 8 individual 3.5mm gate output jacks, one per voice

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S05

8 individual 3.5mm gate output jacks, one per voice

### OUT-06 — Polyphonic velocity CV output passes velocity data from upstream MIDI-CV

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S05

Polyphonic velocity CV output passes velocity data from upstream MIDI-CV

### VOC-01 — Polyphonic mode allocates up to 8 independent voices from polyphonic input channels

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

Polyphonic mode allocates up to 8 independent voices from polyphonic input channels

### VOC-02 — Unison mode stacks all 8 voices on a single pitch with configurable detune spread

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

Unison mode stacks all 8 voices on a single pitch with configurable detune spread

### VOC-03 — User can switch between polyphonic and unison modes

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S02

User can switch between polyphonic and unison modes

### PNL-01 — Panel SVG meets VCV Rack library design standards (text as paths, readable at 100%)

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S05

Panel SVG meets VCV Rack library design standards (text as paths, readable at 100%)

### PNL-02 — Plugin manifest (plugin.json) has correct versioning and metadata for VCV Library

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (initial manifest created), S05 (final review)

Plugin manifest (plugin.json) has correct versioning and metadata for VCV Library

### PNL-03 — Module builds cleanly for VCV Rack 2 and avoids deprecated APIs for Rack 3 compatibility

- Status: active
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (builds clean with zero warnings)

Module builds cleanly for VCV Rack 2 and avoids deprecated APIs for Rack 3 compatibility

## Validated

## Deferred

## Out of Scope
