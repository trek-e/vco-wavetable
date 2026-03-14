# Hurricane 8

**8-voice polyphonic PPG-style wavetable oscillator for [VCV Rack](https://vcvrack.com/)**

by [Synth-etic Intelligence](https://github.com/trek-e)

![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)
![Rack 2](https://img.shields.io/badge/VCV%20Rack-2.x-green)

---

Hurricane 8 brings the gritty, stepped, digital-analog hybrid sound of the PPG Wave synthesizers into VCV Rack. Eight independent voices, authentic wavetable scanning, vintage DAC emulation, and full modulation — all in a 28HP module.

## Features

### Wavetable Engine
- **32 PPG-style wavetable banks** (64 frames × 256 samples each), generated via additive synthesis
- **Mip-mapped band-limited playback** — DFT analysis with 8 mip levels eliminates aliasing across the full frequency range
- **Stepped scanning** — authentic PPG zero-crossing frame switching for that classic digital transition sound
- **Interpolated scanning** — smooth crossfading between adjacent frames for modern timbral animation
- **DAC bit-depth emulation** — selectable 8-bit, 12-bit, or 16-bit quantization reproduces vintage converter character

### Voices
- **8-voice polyphonic** — each input channel drives an independent voice with gate control
- **Unison mode** — all 8 voices stacked on a single pitch with configurable detune spread (0–100 cents)
- Switchable at runtime between polyphonic and unison modes

### Modulation
- **ASR envelope** — per-voice or global modes, modulates wavetable position from start position to end of table
- **Start position** — sets the base wavetable position before envelope modulation, with CV input
- **Vibrato LFO** — internal sine oscillator (0.1–20 Hz) with depth control
- **Glide** — per-voice portamento with exponential response (constant time per octave)
- All modulation parameters accept CV input

### User Wavetables
- Load custom wavetables from **WAV files** (8/16/24/32-bit PCM, 32-bit float)
- Supports **single-cycle** and **multi-frame** formats (Serum-style concatenated cycles)
- Automatic cycle detection, resampling to 256 samples, and mip-map generation
- Wavetable file path persists across patch save/load
- Right-click context menu: load wavetable or revert to PPG ROM

### I/O
- **V/Oct** input (polyphonic, 8 channels)
- **Gate** input (polyphonic)
- **Polyphonic audio** output (up to 8 channels)
- **Polyphonic gate** passthrough
- **Mono mix** output (normalized average of active voices)
- **8 individual audio** output jacks (one per voice)
- **8 individual gate** output jacks (one per voice)
- **Velocity CV** passthrough (polyphonic input → output)
- **CV inputs** for position, start position, LFO rate, LFO amount, and glide

## Panel Layout

28HP dark-themed panel:

| Section | Controls |
|---------|----------|
| **Oscillator** | Wavetable bank (0–31), Position knob + CV, Scan mode (Stepped/Interpolated), DAC depth (8/12/16-bit) |
| **Voice** | Mode (Polyphonic/Unison), Detune (0–100 cents) |
| **Modulation** | Start position + CV, Attack, Release, Envelope mode (Per-Voice/Global), LFO rate + CV, LFO amount + CV, Glide + CV |
| **Inputs** | V/Oct, Gate, Velocity, Position CV, Start Pos CV, LFO Rate CV, LFO Amount CV, Glide CV |
| **Poly Outputs** | Audio (poly), Gate (poly), Mix (mono), Velocity (poly) |
| **Individual Outputs** | 8× Audio, 8× Gate (right side columns) |

## Installation

### From source

Requires the [VCV Rack SDK](https://vcvrack.com/manual/PluginDevelopmentTutorial).

```bash
# Clone into your Rack plugins source directory
git clone https://github.com/trek-e/vco-wavetable.git Hurricane8
cd Hurricane8

# Build (Rack SDK must be one level up, or set RACK_DIR)
make

# Install to your Rack plugins directory
make install
```

### Development

```bash
# Run all 163 unit tests (no Rack SDK required)
make test

# Run individual test suites
make test-dsp         # 42 DSP foundation tests
make test-poly        # 26 polyphonic routing tests
make test-mod         # 35 modulation tests
make test-user-wt     # 36 user wavetable tests
make test-panel       # 24 panel/submission tests
```

## Architecture

The DSP engine is split into four standalone headers with **no VCV Rack SDK dependency** — they compile and test with plain `g++ -std=c++17`:

| Header | Purpose |
|--------|---------|
| `ppg_wavetables.hpp` | PPG-style wavetable ROM generation (additive synthesis) |
| `wavetable_engine.hpp` | Mip-mapped band-limited playback, phase accumulator, scanning, DAC emulation |
| `polyphonic_engine.hpp` | 8-voice polyphonic/unison engine with detune and mix |
| `modulation_engine.hpp` | ASR envelope, sine LFO, glide, start position |
| `wav_loader.hpp` | Self-contained WAV parser with multi-frame detection and resampling |

`Hurricane8VCO.cpp` orchestrates these engines into the VCV Rack module.

## Quick Start

1. Add Hurricane 8 to your patch
2. Connect a **V/Oct** source (e.g., MIDI-CV) to the V/Oct input
3. Connect the **Audio** output to a mixer or audio module
4. Turn the **Position** knob to sweep through the wavetable frames
5. Try **8-bit DAC** mode for authentic PPG grit
6. Switch **Scan Mode** to Stepped for classic PPG frame transitions

### Tips

- Use the **ASR envelope** to automatically sweep wavetable position on each note — set Start Position low, and the envelope sweeps up through the table
- **Unison mode** with 20–50 cents detune creates massive supersaw-style sounds from any wavetable
- The **individual voice outputs** let you process each voice separately — try different effects per voice
- Load a **Serum-compatible .wav** wavetable via right-click for thousands of available timbres

## License

[GPL-3.0-or-later](LICENSE.md)

Copyright © 2026 Synth-etic Intelligence
