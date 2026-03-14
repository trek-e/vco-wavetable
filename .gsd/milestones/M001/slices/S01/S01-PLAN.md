# S01: Dsp Foundation

**Goal:** unit tests prove dsp-foundation works
**Demo:** unit tests prove dsp-foundation works

## Must-Haves

- PPG-style wavetable ROM data generation (32 tables × 64 frames × 256 samples)
- Mip-mapped band-limited wavetable engine (8 octave levels via DFT analysis)
- Phase accumulator with 1V/oct pitch tracking across 10+ octaves
- PPG stepped scanning mode (frame switching at zero crossings)
- Interpolated scanning mode (smooth crossfade between adjacent frames)
- Switchable scanning modes
- DAC bit-depth emulation (8-bit, 12-bit, 16-bit)
- Complete voice pipeline integrating all DSP components
- 42 unit tests covering all requirements
- VCV Rack module stub with build system

## Tasks

- [x] **T01: Build complete DSP foundation** `est:1h`

## Files Likely Touched

- `src/ppg_wavetables.hpp`
- `src/wavetable_engine.hpp`
- `src/Hurricane8VCO.cpp`
- `src/plugin.cpp`
- `src/plugin.hpp`
- `tests/test_dsp_foundation.cpp`
- `plugin.json`
- `Makefile`
