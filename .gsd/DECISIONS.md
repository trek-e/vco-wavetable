# Decisions

<!-- Append-only register of architectural and pattern decisions -->

| ID | Decision | Rationale | Date |
|----|----------|-----------|------|
| D001 | Wavetable data generated via additive synthesis, not PPG ROM dumps | Avoids copyright issues with original ROM data while producing authentic PPG-style timbral progressions through harmonic series | 2026-03-13 |
| D002 | Mip-mapping built from DFT analysis + band-limited resynthesis (not low-pass filtering) | Low-pass filtering introduces phase errors at octave boundaries; DFT resynthesis with harmonic truncation is mathematically exact | 2026-03-13 |
| D003 | DSP engine headers are VCV Rack SDK-independent | Enables standalone unit testing with g++ without requiring full Rack build environment; keeps DSP math portable | 2026-03-13 |
| D004 | Wavetable ROM and bank are static singletons shared across module instances | ~2MB one-time allocation, ~670ms init at plugin load; avoids per-instance duplication of identical data | 2026-03-13 |
| D005 | 256 samples per frame (power of 2) | Allows bitwise AND wrapping in lookup hot path instead of branch/modulo; standard wavetable size | 2026-03-13 |
| D006 | Standalone test binary with custom TEST/EXPECT macros (no external test framework) | DSP tests must be Rack-SDK-independent; avoiding Catch2/GTest keeps the test build trivial (`g++ -std=c++17`) | 2026-03-13 |
