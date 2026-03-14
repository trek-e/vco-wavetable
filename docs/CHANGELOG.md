# Changelog

All notable changes to Hurricane 8 will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] — 2026-03-13

Initial release.

### Added
- 32 PPG-style wavetable banks generated via additive synthesis (sawtooth, square, PWM, triangle, vocal, noise, sync, organ)
- Mip-mapped band-limited wavetable playback with DFT analysis and 8 resolution levels
- Phase accumulator with 1V/oct pitch tracking across 10+ octaves
- PPG stepped scanning (zero-crossing frame switching)
- Interpolated scanning (smooth crossfade between adjacent frames)
- DAC bit-depth emulation (8-bit, 12-bit, 16-bit)
- 8-voice polyphonic mode with per-channel gate control
- Unison mode — 8 voices with configurable detune spread (0–100 cents)
- Per-voice ASR envelope modulating wavetable position
- Global ASR envelope mode (single envelope for all voices)
- Wavetable start position control with CV input
- Internal sine LFO for vibrato (0.1–20 Hz) with depth control
- Glide (portamento) with per-voice exponential response
- User wavetable loading from WAV files (single-cycle and Serum-style multi-frame)
- JSON persistence for loaded wavetable file paths
- Right-click context menu (load wavetable, revert to PPG ROM)
- 28HP dark-themed panel SVG (text as paths, VCV Library compliant)
- 8 individual per-voice audio output jacks
- 8 individual per-voice gate output jacks
- Polyphonic velocity CV passthrough
- 163 unit tests across 5 test suites
