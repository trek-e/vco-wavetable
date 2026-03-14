# Requirements

## Active

(none — all requirements validated or deferred)

## Validated

### DSP-01 — Oscillator tracks 1V/oct pitch input across 10+ octaves with polyphonic support (8 channels)

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (DSP implemented), S02 (polyphonic routing)
- Validated by: S01 voct tests (10+ octave accuracy) + S02 voc01_polyphonic_independent_pitches (8-channel per-voice tracking)

Oscillator tracks 1V/oct pitch input across 10+ octaves with polyphonic support (8 channels)

### DSP-02 — Phase accumulator per voice produces alias-free output using mip-mapped band-limited wavetables

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (DSP implemented), S02 (per-voice routing)
- Validated by: S01 mipmap tests (band-limiting) + S02 polyphonic tests (8 independent phase accumulators)

Phase accumulator per voice produces alias-free output using mip-mapped band-limited wavetables

### DSP-03 — Wavetable position is controllable via knob with CV input and attenuverter

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (basic control), S03 (attenuverter + modulation)
- Validated by: S03 start position CV tests (mod04_start_position_cv_positive/negative/clamped) + position CV integration in Hurricane8VCO.cpp

Wavetable position is controllable via knob with CV input and attenuverter

### DSP-04 — PPG stepped scanning mode switches wavetables at zero-crossing points (authentic PPG behavior)

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)
- Validated by: S01 stepped_scanning_waits_for_zero_cross, stepped_scanning_immediate_when_at_zero

PPG stepped scanning mode switches wavetables at zero-crossing points (authentic PPG behavior)

### DSP-05 — Interpolated scanning mode crossfades smoothly between adjacent wavetables

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)
- Validated by: S01 interpolated_scanning_blends_frames, interpolated_scanning_continuous

Interpolated scanning mode crossfades smoothly between adjacent wavetables

### DSP-06 — User can switch between stepped and interpolated scanning modes

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)
- Validated by: S01 scan_mode_enum_values

User can switch between stepped and interpolated scanning modes

### DSP-07 — DAC bit depth emulation quantizes output at selectable 8-bit, 12-bit, or 16-bit resolution

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (implemented and unit-tested)
- Validated by: S01 dac_8bit/12bit/16bit_quantization, dac_8bit_creates_audible_steps, dac_preserves_sign

DAC bit depth emulation quantizes output at selectable 8-bit, 12-bit, or 16-bit resolution

### WTD-01 — Original PPG ROM wavetable set is bundled and immediately playable

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (32 tables generated and mip-mapped)
- Validated by: S01 ppg_rom_generates_valid_data, all_32_tables_have_data, wavetable_bank_all_tables_mipmapped, voice_produces_output_at_c4

Original PPG ROM wavetable set is bundled and immediately playable

### WTD-02 — User can load single-cycle WAV files as custom wavetables

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S04
- Validated by: 22 unit tests (WAV parsing, single-cycle loading, resampling, mip-map generation, playback through voice)

User can load single-cycle WAV files as custom wavetables

### WTD-03 — User can load multi-frame WAV files (Serum-style concatenated cycles)

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S04
- Validated by: 7 unit tests (multi-frame detection, Serum-style WAV, frame clamping, scanning, different waveform content)

User can load multi-frame WAV files (Serum-style concatenated cycles)

### WTD-04 — Right-click context menu provides wavetable file loading dialog

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S04 (implemented), S05 (panel/widget finalized)
- Validated by: Code inspection — appendContextMenu() implemented with osdialog file picker, load/revert actions, and status label. Requires live Rack for interactive testing.

Right-click context menu provides wavetable file loading dialog

### WTD-05 — Loaded wavetable file path persists across patch save/load (JSON serialization)

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S04 (implemented), S05 (panel/widget finalized)
- Validated by: Code inspection — dataToJson/dataFromJson implemented with userWavetablePath serialization. Requires live Rack for round-trip testing.

Loaded wavetable file path persists across patch save/load (JSON serialization)

### MOD-01 — Per-voice ASR envelope modulates wavetable position, triggered by gate input

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 7 unit tests (mod01_asr_idle_at_zero, mod01_asr_attack_rises, mod01_asr_reaches_sustain, mod01_asr_release_falls, mod01_asr_returns_to_idle, mod01_per_voice_independent, mod01_per_voice_different_timing)

Per-voice ASR envelope modulates wavetable position, triggered by gate input

### MOD-02 — Global ASR envelope mode modulates wavetable position uniformly across all voices

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 3 unit tests (mod02_global_envelope_all_voices_same, mod02_global_envelope_triggered_by_any_gate, mod02_global_envelope_releases_when_all_gates_off)

Global ASR envelope mode modulates wavetable position uniformly across all voices

### MOD-03 — User can switch between per-voice and global envelope modes

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 2 unit tests (mod03_switch_pervoice_to_global, mod03_switch_global_to_pervoice)

User can switch between per-voice and global envelope modes

### MOD-04 — Wavetable start position control sets the base position before envelope modulation, with CV input

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 7 unit tests (mod04_start_position_zero/mid/full, mod04_start_position_cv_positive/negative/clamped, mod04_start_plus_envelope)

Wavetable start position control sets the base position before envelope modulation, with CV input

### MOD-05 — Internal sine LFO provides vibrato with rate control and CV input

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 4 unit tests (mod05_lfo_produces_sine, mod05_lfo_frequency_accuracy, mod05_lfo_zero_rate, mod05_vibrato_pitch_modulation)

Internal sine LFO provides vibrato with rate control and CV input

### MOD-06 — Internal LFO has amount (depth) control with CV input

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 3 unit tests (mod06_zero_amount_no_vibrato, mod06_half_amount_half_depth, mod06_amount_scales_linearly)

Internal LFO has amount (depth) control with CV input

### MOD-07 — Glide (portamento) smoothly transitions between pitch changes with amount control and CV input

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S03
- Validated by: 6 unit tests (mod07_glide_zero_amount_instant, mod07_glide_nonzero_smooths, mod07_glide_converges, mod07_glide_higher_amount_slower, mod07_glide_per_voice_independent, mod07_glide_reset)

Glide (portamento) smoothly transitions between pitch changes with amount control and CV input

### OUT-01 — 8-channel polyphonic audio output carries all voice signals

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: 26 unit tests in test_polyphonic_routing.cpp (out01_* tests)

8-channel polyphonic audio output carries all voice signals

### OUT-02 — 8-channel polyphonic gate output passes gate signals downstream

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: Unit tests out02_gate_passthrough_poly, out02_gate_passthrough_unison

8-channel polyphonic gate output passes gate signals downstream

### OUT-03 — Mix output sums all active voices to a single mono signal

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: 5 unit tests (out03_* tests) including clipping prevention and normalization

Mix output sums all active voices to a single mono signal

### OUT-04 — 8 individual 3.5mm audio output jacks, one per voice

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S05
- Validated by: 5 unit tests (out04_individual_audio_polyphonic_4voices, out04_individual_audio_all_8_voices, out04_individual_audio_unison_all_active, out04_individual_voices_are_independent, out04_individual_output_matches_poly_output)

8 individual 3.5mm audio output jacks, one per voice

### OUT-05 — 8 individual 3.5mm gate output jacks, one per voice

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S05
- Validated by: 3 unit tests (out05_individual_gate_polyphonic, out05_individual_gate_unison, out05_gate_off_silences_individual)

8 individual 3.5mm gate output jacks, one per voice

### OUT-06 — Polyphonic velocity CV output passes velocity data from upstream MIDI-CV

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S05
- Validated by: 2 unit tests (out06_velocity_passthrough_concept, out06_velocity_values_bounded) + code inspection (direct input→output passthrough in process())

Polyphonic velocity CV output passes velocity data from upstream MIDI-CV

### VOC-01 — Polyphonic mode allocates up to 8 independent voices from polyphonic input channels

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: 5 unit tests (voc01_* tests) proving 1/4/8 voice allocation with independent pitch tracking

Polyphonic mode allocates up to 8 independent voices from polyphonic input channels

### VOC-02 — Unison mode stacks all 8 voices on a single pitch with configurable detune spread

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: 4 unit tests (voc02_* tests) proving detune spread, zero-detune identity, and gate control

Unison mode stacks all 8 voices on a single pitch with configurable detune spread

### VOC-03 — User can switch between polyphonic and unison modes

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S02
- Validated by: 2 unit tests (voc03_* tests) proving bidirectional mode switching

User can switch between polyphonic and unison modes

### PNL-01 — Panel SVG meets VCV Rack library design standards (text as paths, readable at 100%)

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S05
- Validated by: 5 unit tests (pnl01_panel_svg_exists, pnl01_panel_svg_valid_xml, pnl01_panel_svg_correct_dimensions, pnl01_panel_svg_no_text_elements, pnl01_panel_svg_has_viewbox)

Panel SVG meets VCV Rack library design standards (text as paths, readable at 100%)

### PNL-02 — Plugin manifest (plugin.json) has correct versioning and metadata for VCV Library

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (initial manifest created), S05 (final review and validation)
- Validated by: 4 unit tests (pnl02_plugin_json_exists, pnl02_plugin_json_has_required_fields, pnl02_module_entry_correct, pnl02_version_format)

Plugin manifest (plugin.json) has correct versioning and metadata for VCV Library

### PNL-03 — Module builds cleanly for VCV Rack 2 and avoids deprecated APIs for Rack 3 compatibility

- Status: validated
- Class: core-capability
- Source: inferred
- Primary Slice: S01 (builds clean with zero warnings), S05 (final clean build verified)
- Validated by: 3 unit tests (pnl03_polyphonic_engine_header_standalone, pnl03_max_poly_voices_is_8, pnl03_output_count_consistency) + zero-warning Rack SDK build

Module builds cleanly for VCV Rack 2 and avoids deprecated APIs for Rack 3 compatibility

## Deferred

## Out of Scope
