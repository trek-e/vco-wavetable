/*
 * Hurricane 8 — PPG-style Wavetable ROM Data
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Mathematically generated wavetable set inspired by the PPG Wave 2.2/2.3.
 * These are original waveforms created by additive synthesis, NOT ROM dumps.
 *
 * Structure:
 *   - PPG_NUM_TABLES wavetables (banks)
 *   - PPG_FRAMES_PER_TABLE frames (single-cycle waveforms) per table
 *   - PPG_SAMPLES_PER_FRAME samples per frame
 *
 * The tables progress from pure harmonics through increasingly complex
 * spectral content, mimicking the PPG's characteristic timbral evolution.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

// Wavetable dimensions
static constexpr int PPG_NUM_TABLES = 32;
static constexpr int PPG_FRAMES_PER_TABLE = 64;
static constexpr int PPG_SAMPLES_PER_FRAME = 256;

static constexpr float PPG_PI = 3.14159265358979323846f;

// All PPG data stored flat: [table][frame][sample]
// Total: 32 * 64 * 256 = 524,288 floats (~2MB)
// Generated once at plugin init, not at audio rate.

struct PpgWavetableRom {
    float data[PPG_NUM_TABLES][PPG_FRAMES_PER_TABLE][PPG_SAMPLES_PER_FRAME];
    bool initialized = false;

    void generate() {
        if (initialized) return;

        // Table 0-3: Sawtooth harmonic series (fundamental → full saw)
        // Frame 0 = sine, Frame 63 = full sawtooth
        for (int t = 0; t < 4; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                // Number of harmonics increases with frame index
                // Tables 0-3 have progressively faster harmonic addition
                int maxHarmonic = 1 + (f * (4 + t * 12)) / PPG_FRAMES_PER_TABLE;
                if (maxHarmonic > 128) maxHarmonic = 128;

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;
                    for (int h = 1; h <= maxHarmonic; h++) {
                        // Sawtooth: sum of sin(2*pi*h*phase) / h, alternating sign
                        float sign = (h % 2 == 0) ? -1.f : 1.f;
                        sample += sign * std::sin(2.f * PPG_PI * h * phase) / (float)h;
                    }
                    // Normalize
                    data[t][f][s] = sample * (2.f / PPG_PI);
                }
            }
        }

        // Table 4-7: Square/pulse wave series
        // Frame 0 = sine, Frame 63 = full square with odd harmonics
        for (int t = 4; t < 8; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                int maxHarmonic = 1 + (f * (4 + (t - 4) * 12)) / PPG_FRAMES_PER_TABLE;
                if (maxHarmonic > 128) maxHarmonic = 128;

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;
                    for (int h = 1; h <= maxHarmonic; h += 2) {
                        // Square: sum of sin(2*pi*h*phase) / h, odd harmonics only
                        sample += std::sin(2.f * PPG_PI * h * phase) / (float)h;
                    }
                    data[t][f][s] = sample * (4.f / PPG_PI);
                }
            }
        }

        // Table 8-11: Pulse width modulation sweep
        // Each table is a different base timbre, frames sweep duty cycle
        for (int t = 8; t < 12; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                float duty = 0.1f + 0.8f * (float)f / (PPG_FRAMES_PER_TABLE - 1);
                int maxHarmonics = 16 + (t - 8) * 16;

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;
                    for (int h = 1; h <= maxHarmonics; h++) {
                        // Pulse wave via Fourier: (2/(h*pi)) * sin(h*pi*duty) * cos(2*pi*h*phase - h*pi*duty)
                        float coeff = std::sin(PPG_PI * h * duty) / (float)h;
                        sample += coeff * std::cos(2.f * PPG_PI * h * phase - PPG_PI * h * duty);
                    }
                    data[t][f][s] = sample * (2.f / PPG_PI);
                }
            }
        }

        // Table 12-15: Triangle to sine morphing with harmonic foldback
        for (int t = 12; t < 16; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                float morph = (float)f / (PPG_FRAMES_PER_TABLE - 1);
                int maxH = 1 + (t - 12) * 8 + (int)(morph * 32);

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;
                    for (int h = 1; h <= maxH; h += 2) {
                        // Triangle: (-1)^((h-1)/2) * sin(2*pi*h*phase) / h^2
                        float sign = ((h / 2) % 2 == 0) ? 1.f : -1.f;
                        float triCoeff = sign / ((float)h * (float)h);
                        // Blend between triangle and modified harmonic
                        float foldback = 1.f + morph * std::sin(PPG_PI * h * 0.1f);
                        sample += triCoeff * std::sin(2.f * PPG_PI * h * phase) * foldback;
                    }
                    data[t][f][s] = sample * (8.f / (PPG_PI * PPG_PI));
                }
            }
        }

        // Table 16-19: Vocal/formant-like waveforms
        // Simulate formant peaks by boosting specific harmonic ranges
        for (int t = 16; t < 20; t++) {
            float formant1 = 3.f + (t - 16) * 2.f;    // First formant harmonic
            float formant2 = formant1 * 2.5f;            // Second formant
            float formantWidth = 1.5f + (t - 16) * 0.5f;

            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                float formantShift = (float)f / (PPG_FRAMES_PER_TABLE - 1);

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;

                    for (int h = 1; h <= 64; h++) {
                        // Base amplitude decays with harmonic number
                        float amp = 1.f / (float)h;

                        // Boost near formant frequencies (Gaussian-like peaks)
                        float f1Center = formant1 + formantShift * 4.f;
                        float f2Center = formant2 + formantShift * 6.f;
                        float d1 = (h - f1Center) / formantWidth;
                        float d2 = (h - f2Center) / formantWidth;
                        float formantBoost = 3.f * std::exp(-0.5f * d1 * d1) +
                                             2.f * std::exp(-0.5f * d2 * d2);
                        amp *= (1.f + formantBoost);

                        sample += amp * std::sin(2.f * PPG_PI * h * phase);
                    }
                    // Normalize to prevent clipping
                    data[t][f][s] = sample * 0.3f;
                }
            }
        }

        // Table 20-23: Digital noise / bit-crushed textures
        // These create the harsh digital character PPG is known for
        for (int t = 20; t < 24; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                // Start with a harmonic base, then fold/wrap to create digital texture
                float foldAmount = 1.f + (float)f / (PPG_FRAMES_PER_TABLE - 1) * (3.f + (t - 20));

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;

                    // Additive base with inharmonic components
                    for (int h = 1; h <= 32; h++) {
                        float freq = (float)h * (1.f + 0.01f * (t - 20) * (h % 3));
                        sample += std::sin(2.f * PPG_PI * freq * phase) / (float)h;
                    }

                    // Wavefold for digital grit
                    sample *= foldAmount;
                    // Fold: keep within -1..1 by triangle folding
                    while (sample > 1.f || sample < -1.f) {
                        if (sample > 1.f) sample = 2.f - sample;
                        if (sample < -1.f) sample = -2.f - sample;
                    }

                    data[t][f][s] = sample;
                }
            }
        }

        // Table 24-27: Sync-sweep emulation
        // Simulates oscillator sync with increasing slave frequency
        for (int t = 24; t < 28; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                // Slave/master frequency ratio increases with frame
                float ratio = 1.f + (float)f / (PPG_FRAMES_PER_TABLE - 1) * (4.f + (t - 24) * 2.f);

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float masterPhase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float slavePhase = masterPhase * ratio;
                    slavePhase -= std::floor(slavePhase);

                    // Slave waveform is sawtooth
                    float sample = 2.f * slavePhase - 1.f;

                    // Window with master cycle to avoid discontinuities at frame boundaries
                    float window = std::sin(PPG_PI * masterPhase);
                    data[t][f][s] = sample * window;
                }
            }
        }

        // Table 28-31: Organ / additive harmonic drawbar emulation
        // Frames sweep through different drawbar combinations
        for (int t = 28; t < 32; t++) {
            // Drawbar harmonics: 1, 2, 3, 4, 5, 6, 8 (organ registration)
            const int drawbarHarmonics[] = {1, 2, 3, 4, 5, 6, 8};
            const int numDrawbars = 7;

            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                float pos = (float)f / (PPG_FRAMES_PER_TABLE - 1);

                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float phase = (float)s / PPG_SAMPLES_PER_FRAME;
                    float sample = 0.f;

                    for (int d = 0; d < numDrawbars; d++) {
                        // Each drawbar fades in and out as position sweeps
                        float drawbarPhase = pos * numDrawbars - d + (t - 28) * 0.5f;
                        float drawbarLevel = std::max(0.f, 1.f - std::abs(drawbarPhase));

                        int h = drawbarHarmonics[d];
                        sample += drawbarLevel * std::sin(2.f * PPG_PI * h * phase);
                    }
                    data[t][f][s] = sample;
                }
            }
        }

        // Final pass: normalize all tables to [-1, 1] range
        for (int t = 0; t < PPG_NUM_TABLES; t++) {
            for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
                float maxAbs = 0.f;
                for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                    float v = std::abs(data[t][f][s]);
                    if (v > maxAbs) maxAbs = v;
                }
                if (maxAbs > 1e-6f) {
                    float scale = 1.f / maxAbs;
                    // Only scale down, never up from very quiet frames
                    if (maxAbs > 1.f) {
                        for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                            data[t][f][s] *= scale;
                        }
                    }
                }
            }
        }

        initialized = true;
    }

    // Lookup with linear interpolation within a frame
    float lookup(int table, int frame, float phase) const {
        table = std::max(0, std::min(table, PPG_NUM_TABLES - 1));
        frame = std::max(0, std::min(frame, PPG_FRAMES_PER_TABLE - 1));

        // Phase is 0..1, map to sample index with linear interpolation
        float pos = phase * PPG_SAMPLES_PER_FRAME;
        int idx0 = (int)pos;
        float frac = pos - idx0;
        idx0 = idx0 & (PPG_SAMPLES_PER_FRAME - 1);  // Wrap (power of 2)
        int idx1 = (idx0 + 1) & (PPG_SAMPLES_PER_FRAME - 1);

        return data[table][frame][idx0] * (1.f - frac) + data[table][frame][idx1] * frac;
    }
};
