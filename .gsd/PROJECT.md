# Synth-etic Intelligence Hurricane 8

## What This Is

A polyphonic digital wavetable oscillator module for VCV Rack, inspired by the legendary PPG Wave synthesizers of the 1980s. Hurricane 8 provides 8 independent voices with authentic PPG-style wavetable scanning, configurable vintage DAC emulation, and both polyphonic and unison voice modes. It ships with 32 PPG-style wavetable banks and supports user-loadable wavetable files.

## Current State

**Milestone M001 complete.** The module is feature-complete with all 31 requirements validated across 163 unit tests. Builds clean against VCV Rack 2 SDK with zero warnings. Ready for live testing in VCV Rack and VCV Library submission.

### What's Built

- **DSP Engine**: Mip-mapped band-limited wavetable playback with DFT analysis and 8 mip levels. Phase accumulator with 1V/oct tracking across 10+ octaves. PPG stepped scanning (zero-crossing frame switching) and interpolated scanning. DAC emulation at 8/12/16-bit.
- **Voice Routing**: 8-voice polyphonic engine with independent voice allocation and gate control. Unison mode stacks all 8 voices with symmetric V/Oct detune (0–100 cents). Normalized mix output prevents clipping.
- **Modulation**: Per-voice and global ASR envelope modulating wavetable position. Sine LFO vibrato (0.1–20 Hz). Glide portamento via one-pole filter. Start position control with CV.
- **User Wavetables**: Self-contained WAV parser (8/16/24/32-bit PCM, 32-bit float). Multi-frame detection for Serum-style concatenated files. Resampling to 256 samples with mip-mapping. JSON persistence for patch save/load. Right-click context menu.
- **Panel & I/O**: 28HP dark-themed SVG panel (text as paths). 13 params, 8 inputs, 20 outputs. 8 individual per-voice audio outputs, 8 individual gate outputs, velocity CV passthrough.
- **Tests**: 163 unit tests across 5 standalone suites (`make test`).

### Architecture

All DSP engine headers (`wavetable_engine.hpp`, `polyphonic_engine.hpp`, `modulation_engine.hpp`, `wav_loader.hpp`) are VCV Rack SDK-independent. They compile and test standalone with `g++ -std=c++17`. `Hurricane8VCO.cpp` orchestrates the engines into the Rack module.

## Requirements

### Validated (31)

DSP-01..07, WTD-01..05, MOD-01..07, VOC-01..03, OUT-01..06, PNL-01..03

### Active

(none)

### Out of Scope

- Effects processing (reverb, delay, chorus) — keep module focused on oscillator duties
- MIDI input handling — VCV Rack's MIDI-CV modules handle this upstream
- Built-in filter — pair with external filter modules in the rack
- Sequencer or arpeggiator functionality — external modules handle this

## Context

- **Platform:** VCV Rack 2 (primary target), forward compatible with Rack 3
- **Language:** C++ using VCV Rack SDK
- **Build:** `make` for Rack module, `make test` for 163 unit tests
- **Panel:** 28HP dark theme, `res/Hurricane8VCO.svg`

## Key Decisions

See `.gsd/DECISIONS.md` for the full register (20 decisions, D001–D020).

## Next Steps

- Live testing in VCV Rack (patch cables, right-click menu, patch save/load round-trip)
- Professional panel design polish (typography, color refinement) before Library submission
- VCV Library submission (code review, licensing, metadata)

---
*Last updated: 2026-03-13 — M001 complete. 163 tests, 31 requirements validated, zero warnings.*
