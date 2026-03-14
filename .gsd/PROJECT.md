# Synth-etic Intelligence Hurricane 8

## What This Is

A polyphonic digital wavetable oscillator module for VCV Rack, inspired by the legendary PPG Wave synthesizers of the 1980s. Hurricane 8 provides 8 independent voices with authentic PPG-style wavetable scanning, configurable vintage DAC emulation, and both polyphonic and unison voice modes. It ships with the original PPG wavetable set and supports user-loadable wavetable files.

## Core Value

Authentic PPG wavetable character — the gritty, stepped, digital-analog hybrid sound that defined an era — accessible as a polyphonic VCV Rack module with modern flexibility.

## Requirements

### Validated

All 31 requirements validated across S01-S05.

### Active

(none — all features implemented and tested)

### Out of Scope

- Effects processing (reverb, delay, chorus) — keep module focused on oscillator duties
- MIDI input handling — VCV Rack's MIDI-CV modules handle this upstream
- Built-in filter — pair with external filter modules in the rack
- Sequencer or arpeggiator functionality — external modules handle this

## Context

- **Platform:** VCV Rack 2 (primary target), with forward compatibility for Rack 3
- **Language:** C++ using VCV Rack SDK
- **Existing work:** Developer has an existing VCV Rack plugin in a separate directory that can serve as reference for build system and module registration patterns
- **Sound design heritage:** PPG Wave 2.2/2.3 used 8-bit DACs and discrete wavetable stepping that created the signature "digital but warm" character. The quantization artifacts and table-stepping transitions are features, not bugs.
- **Wavetable format:** Original PPG tables stored as fixed data arrays. User tables loaded from WAV files — both single-cycle (one waveform per file) and multi-frame (multiple cycles concatenated in one WAV, Serum-style).
- **Architecture:** Each voice needs independent state: phase accumulator, envelope, glide, wavetable position. The module must handle polyphonic cables (VCV Rack's channel system) and individual jack outputs simultaneously.

## Constraints

- **VCV Rack SDK**: Must conform to Rack 2 API conventions (Module, Widget, process(), step())
- **Real-time audio**: All DSP must run within the audio callback — no allocations, no blocking, no file I/O in process()
- **Panel size**: VCV Rack modules are measured in HP (horizontal pitch, 5.08mm each). An 8-voice module with this many jacks will need significant panel width
- **Library submission**: Must meet VCV Rack library guidelines (licensing, code review, panel standards)
- **Compatibility**: Build for Rack 2 but avoid deprecated APIs to ease Rack 3 migration

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Flexible bit depth (8/12/16) over fixed 8-bit | Gives users tonal range from authentic to clean | — Pending |
| Switchable stepped/interpolated scanning | Authenticity (stepped) vs modern smoothness (interpolated) | — Pending |
| Both polyphonic and unison voice modes | Polyphonic for playability, unison for massive sounds | — Pending |
| Per-voice/global switchable ASR envelope | Per-voice for expressive play, global for uniform movement | — Pending |
| Support both single-cycle and multi-frame WAV | Maximizes compatibility with existing wavetable libraries | — Pending |
| Rack 2 primary, Rack 3 compatible | Ship on current stable, don't get blocked on unreleased version | — Pending |

---
*Last updated: 2026-03-13 after S05 completion — panel SVG (28HP), individual voice/gate outputs, velocity CV passthrough, VCV Library submission readiness. 163 total tests passing across 5 suites. All 31 requirements validated. Milestone M001 complete.*
