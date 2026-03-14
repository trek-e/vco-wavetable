# Hurricane 8 — Architecture

## Design Principles

1. **DSP headers are Rack-SDK-independent.** All engine code compiles and tests with plain `g++ -std=c++17`. This keeps the DSP math portable and enables fast standalone testing without the full Rack build environment.

2. **No allocations in the audio path.** All wavetable data is pre-generated at plugin load. The `process()` method uses only stack variables and pre-allocated arrays.

3. **Shared immutable data.** The PPG wavetable ROM and mip-mapped bank are static singletons — generated once at plugin init, read-only thereafter, shared across all module instances.

## Module Structure

```
┌─────────────────────────────────────────────────────┐
│                  Hurricane8VCO.cpp                    │
│              (VCV Rack Module + Widget)               │
│                                                       │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │  Modulation   │  │  Polyphonic  │  │    WAV     │ │
│  │   Engine      │──│   Engine     │  │   Loader   │ │
│  │              │  │  (8 voices)  │  │            │ │
│  └──────────────┘  └──────┬───────┘  └────────────┘ │
│                           │                           │
│              ┌────────────┴────────────┐             │
│              │    Wavetable Engine      │             │
│              │  (mip-map, phase, scan)  │             │
│              └────────────┬────────────┘             │
│                           │                           │
│              ┌────────────┴────────────┐             │
│              │    PPG Wavetable ROM     │             │
│              │  (32 banks, static)      │             │
│              └─────────────────────────┘             │
└─────────────────────────────────────────────────────┘
```

## File Map

| File | Lines | Dependencies | Purpose |
|------|-------|-------------|---------|
| `ppg_wavetables.hpp` | ~300 | none | Generates 32 wavetable banks via additive synthesis. Each bank: 64 frames × 256 samples. Normalized to ±1.0. |
| `wavetable_engine.hpp` | ~350 | none | `MipMappedTable` (DFT analysis, 8 mip levels), `WavetableBank`, `WavetableScanner` (stepped/interpolated), `DacEmulator`, `PhaseAccumulator`, `WavetableVoice` |
| `polyphonic_engine.hpp` | ~150 | `wavetable_engine.hpp` | `PolyphonicEngine` — 8 `WavetableVoice` instances, polyphonic/unison routing, detune, gate control, mix output |
| `modulation_engine.hpp` | ~280 | none | `AsrEnvelope`, `SineLfo`, `GlideFilter`, `ModulationEngine` — per-voice and global modulation state |
| `wav_loader.hpp` | ~320 | `wavetable_engine.hpp` | WAV file parser (PCM 8/16/24/32, float 32), cycle detection, resampling, mip-map generation |
| `Hurricane8VCO.cpp` | ~460 | all of the above + Rack SDK | Module definition (params, inputs, outputs), `process()`, JSON serialization, context menu, widget layout |
| `plugin.cpp` | ~10 | Rack SDK | Plugin registration |
| `plugin.hpp` | ~10 | Rack SDK | Plugin/model declarations |

## Data Flow (per sample)

```
V/Oct input ──→ Glide filter ──→ + LFO vibrato ──→ pitch (V/Oct)
                                                        │
Gate input ──→ ASR envelope ──────────────────────→ position modulation
                                                        │
Start Pos + CV ──→ start ──→ start + env*(1-start) ──→ position (0..1)
                                                        │
Wavetable Bank ──→ Scanner (stepped/interpolated) ──→ raw sample
                                                        │
                         DAC emulator ──→ quantized sample ──→ output × 5V
```

## Key Data Structures

### WavetableVoice
Per-voice state: phase accumulator, wavetable scanner, DAC emulator. Processes one sample at a time given a V/Oct pitch, wavetable position, scan mode, DAC depth, and sample rate.

### PolyphonicEngine
Array of 8 `WavetableVoice`. In polyphonic mode, voice N uses input channel N. In unison mode, all voices use channel 0 with symmetric detune offsets in V/Oct space. Outputs: per-voice audio, per-voice gates, normalized mix.

### ModulationEngine
Per-voice arrays of `AsrEnvelope`, `GlideFilter`, and a shared `SineLfo`. `getModulatedPitch()` applies glide then vibrato. `getModulatedPosition()` applies envelope to start position. Global envelope mode uses a single envelope triggered by OR of all gates.

### MipMappedTable
One wavetable frame stored at 8 resolution levels. Level 0 = full bandwidth (128 harmonics for 256-sample frame). Each subsequent level halves the harmonic count. `readSampleSmooth()` crossfades between adjacent levels based on frequency.

### WavetableBank
32 `MipMappedTable` arrays of 64 frames each. Built once from ROM data at plugin load (~670ms). User wavetable loading creates a second bank per instance.

## Testing Strategy

All tests are standalone C++ binaries using a custom macro framework (`TEST`, `EXPECT`, `EXPECT_NEAR`). No external test dependencies.

```
tests/
├── test_dsp_foundation.cpp     — 42 tests (DSP-01..07, WTD-01)
├── test_polyphonic_routing.cpp — 26 tests (VOC-01..03, OUT-01..03)
├── test_modulation.cpp         — 35 tests (MOD-01..07)
├── test_user_wavetables.cpp    — 36 tests (WTD-02..03)
└── test_panel_submission.cpp   — 24 tests (OUT-04..06, PNL-01..03)
```

Each test file compiles independently: `g++ -std=c++17 -O2 -I src -o test_binary test_file.cpp -lm`

Tests create synthetic data in-memory (generated WAV buffers, test signals) rather than reading external files. The PPG ROM is generated at test startup (~650ms).

## Decisions Register

See [DECISIONS.md](../.gsd/DECISIONS.md) for the full register of 20 architectural decisions (D001–D020) with rationale.
