# Requirements: Hurricane 8

**Defined:** 2026-03-01
**Core Value:** Authentic PPG wavetable character — the gritty, stepped, digital-analog hybrid sound — accessible as a polyphonic VCV Rack module with modern flexibility.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### DSP Core

- [ ] **DSP-01**: Oscillator tracks 1V/oct pitch input across 10+ octaves with polyphonic support (8 channels)
- [ ] **DSP-02**: Phase accumulator per voice produces alias-free output using mip-mapped band-limited wavetables
- [ ] **DSP-03**: Wavetable position is controllable via knob with CV input and attenuverter
- [ ] **DSP-04**: PPG stepped scanning mode switches wavetables at zero-crossing points (authentic PPG behavior)
- [ ] **DSP-05**: Interpolated scanning mode crossfades smoothly between adjacent wavetables
- [ ] **DSP-06**: User can switch between stepped and interpolated scanning modes
- [ ] **DSP-07**: DAC bit depth emulation quantizes output at selectable 8-bit, 12-bit, or 16-bit resolution

### Wavetable Data

- [ ] **WTD-01**: Original PPG ROM wavetable set is bundled and immediately playable
- [ ] **WTD-02**: User can load single-cycle WAV files as custom wavetables
- [ ] **WTD-03**: User can load multi-frame WAV files (Serum-style concatenated cycles)
- [ ] **WTD-04**: Right-click context menu provides wavetable file loading dialog
- [ ] **WTD-05**: Loaded wavetable file path persists across patch save/load (JSON serialization)

### Modulation

- [ ] **MOD-01**: Per-voice ASR envelope modulates wavetable position, triggered by gate input
- [ ] **MOD-02**: Global ASR envelope mode modulates wavetable position uniformly across all voices
- [ ] **MOD-03**: User can switch between per-voice and global envelope modes
- [ ] **MOD-04**: Wavetable start position control sets the base position before envelope modulation, with CV input
- [ ] **MOD-05**: Internal sine LFO provides vibrato with rate control and CV input
- [ ] **MOD-06**: Internal LFO has amount (depth) control with CV input
- [ ] **MOD-07**: Glide (portamento) smoothly transitions between pitch changes with amount control and CV input

### Outputs

- [ ] **OUT-01**: 8-channel polyphonic audio output carries all voice signals
- [ ] **OUT-02**: 8-channel polyphonic gate output passes gate signals downstream
- [ ] **OUT-03**: Mix output sums all active voices to a single mono signal
- [ ] **OUT-04**: 8 individual 3.5mm audio output jacks, one per voice
- [ ] **OUT-05**: 8 individual 3.5mm gate output jacks, one per voice
- [ ] **OUT-06**: Polyphonic velocity CV output passes velocity data from upstream MIDI-CV

### Voice Architecture

- [ ] **VOC-01**: Polyphonic mode allocates up to 8 independent voices from polyphonic input channels
- [ ] **VOC-02**: Unison mode stacks all 8 voices on a single pitch with configurable detune spread
- [ ] **VOC-03**: User can switch between polyphonic and unison modes

### Panel & Submission

- [ ] **PNL-01**: Panel SVG meets VCV Rack library design standards (text as paths, readable at 100%)
- [ ] **PNL-02**: Plugin manifest (plugin.json) has correct versioning and metadata for VCV Library
- [ ] **PNL-03**: Module builds cleanly for VCV Rack 2 and avoids deprecated APIs for Rack 3 compatibility

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Extended Content

- **EXT-01**: Additional bundled wavetable sets beyond original PPG ROM
- **EXT-02**: Wavetable waveform display/visualization on panel

### Advanced Features

- **ADV-01**: Fine/coarse tuning knobs for pitch offset
- **ADV-02**: FM input for frequency modulation from external sources

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Built-in filter (VCF) | VCV modular philosophy — use external filter modules |
| Built-in VCA / amplitude envelope | External envelope + VCA modules serve this |
| Built-in effects (reverb, delay, chorus) | Outside oscillator scope, CPU cost |
| MIDI input | VCV Rack's MIDI-CV modules handle this upstream |
| 2D XY wavetable morphing | PPG is 1D linear scan; XY is a different paradigm |
| Sequencer / arpeggiator | External modules handle this |
| In-module preset management | VCV Rack patches ARE the preset system |
| 16-voice mode | 8 voices matches PPG fidelity; 16 doubles CPU and panel complexity |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DSP-01 | — | Pending |
| DSP-02 | — | Pending |
| DSP-03 | — | Pending |
| DSP-04 | — | Pending |
| DSP-05 | — | Pending |
| DSP-06 | — | Pending |
| DSP-07 | — | Pending |
| WTD-01 | — | Pending |
| WTD-02 | — | Pending |
| WTD-03 | — | Pending |
| WTD-04 | — | Pending |
| WTD-05 | — | Pending |
| MOD-01 | — | Pending |
| MOD-02 | — | Pending |
| MOD-03 | — | Pending |
| MOD-04 | — | Pending |
| MOD-05 | — | Pending |
| MOD-06 | — | Pending |
| MOD-07 | — | Pending |
| OUT-01 | — | Pending |
| OUT-02 | — | Pending |
| OUT-03 | — | Pending |
| OUT-04 | — | Pending |
| OUT-05 | — | Pending |
| OUT-06 | — | Pending |
| VOC-01 | — | Pending |
| VOC-02 | — | Pending |
| VOC-03 | — | Pending |
| PNL-01 | — | Pending |
| PNL-02 | — | Pending |
| PNL-03 | — | Pending |

**Coverage:**
- v1 requirements: 31 total
- Mapped to phases: 0
- Unmapped: 31

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after initial definition*
