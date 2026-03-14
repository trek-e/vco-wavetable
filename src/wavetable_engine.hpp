/*
 * Hurricane 8 — Mip-Mapped Wavetable Engine
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Band-limited wavetable playback using mip-mapping.
 *
 * The engine pre-generates multiple octave-reduced versions of each wavetable
 * frame, progressively removing harmonics that would alias at higher playback
 * frequencies. At runtime, the appropriate mip level is selected based on the
 * oscillator's current frequency relative to the sample rate.
 *
 * This is the same approach used in hardware wavetable synths and modern
 * software like Serum — just adapted for VCV Rack's real-time constraints.
 */

#pragma once

#include "ppg_wavetables.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

// Mip-map levels: level 0 = full bandwidth, level N = Nyquist/2^N max harmonic
// With 256-sample frames, max useful harmonic = 128
// Level 0: 128 harmonics (for low notes)
// Level 1: 64 harmonics
// Level 2: 32 harmonics
// ...
// Level 7: 1 harmonic (sine wave, for very high notes)
static constexpr int MIP_LEVELS = 8;

struct MipMapFrame {
    float samples[PPG_SAMPLES_PER_FRAME];
};

// A single wavetable with mip-mapped versions of each frame
struct MipMappedTable {
    // [mipLevel][frame][sample]
    MipMapFrame frames[MIP_LEVELS][PPG_FRAMES_PER_TABLE];

    // Build mip maps from source data using additive synthesis
    // This is the correct approach: rebuild each mip level from harmonics,
    // not by low-pass filtering (which introduces phase errors).
    void buildFromSource(const float source[PPG_FRAMES_PER_TABLE][PPG_SAMPLES_PER_FRAME]) {
        const int N = PPG_SAMPLES_PER_FRAME;
        const float twoPiOverN = 2.f * PPG_PI / N;

        for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
            // Step 1: DFT analysis of source frame to get harmonic amplitudes
            // Using direct DFT (N=256 is small enough, and this runs at init only)
            float cosCoeffs[N / 2 + 1];
            float sinCoeffs[N / 2 + 1];
            std::memset(cosCoeffs, 0, sizeof(cosCoeffs));
            std::memset(sinCoeffs, 0, sizeof(sinCoeffs));

            for (int h = 0; h <= N / 2; h++) {
                float sumCos = 0.f, sumSin = 0.f;
                for (int s = 0; s < N; s++) {
                    float angle = twoPiOverN * h * s;
                    sumCos += source[f][s] * std::cos(angle);
                    sumSin += source[f][s] * std::sin(angle);
                }
                cosCoeffs[h] = sumCos * (2.f / N);
                sinCoeffs[h] = sumSin * (2.f / N);
            }
            // DC component doesn't get the 2x factor
            cosCoeffs[0] *= 0.5f;

            // Step 2: Build each mip level by synthesizing with limited harmonics
            for (int level = 0; level < MIP_LEVELS; level++) {
                // Max harmonic for this mip level
                // Level 0: 128 harmonics (full)
                // Level 1: 64
                // Level 7: 1 (sine only)
                int maxHarmonic = (N / 2) >> level;
                if (maxHarmonic < 1) maxHarmonic = 1;

                for (int s = 0; s < N; s++) {
                    float sample = cosCoeffs[0]; // DC
                    float phase = twoPiOverN * s;
                    for (int h = 1; h <= maxHarmonic; h++) {
                        float angle = phase * h;
                        sample += cosCoeffs[h] * std::cos(angle) + sinCoeffs[h] * std::sin(angle);
                    }
                    frames[level][f].samples[s] = sample;
                }
            }
        }
    }
};

// Complete mip-mapped wavetable bank (all PPG tables)
struct WavetableBank {
    MipMappedTable tables[PPG_NUM_TABLES];
    bool initialized = false;

    void buildFromRom(const PpgWavetableRom& rom) {
        if (initialized) return;
        for (int t = 0; t < PPG_NUM_TABLES; t++) {
            tables[t].buildFromSource(rom.data[t]);
        }
        initialized = true;
    }

    // Select mip level based on frequency and sample rate
    // Returns 0 for low frequencies (full harmonics), higher for higher frequencies
    static int selectMipLevel(float freq, float sampleRate) {
        if (freq <= 0.f || sampleRate <= 0.f) return 0;

        // How many samples per cycle?
        float samplesPerCycle = sampleRate / freq;

        // Max safe harmonic = samplesPerCycle / 2 (Nyquist)
        float maxSafeHarmonic = samplesPerCycle * 0.5f;

        // Mip level 0 has 128 harmonics, level 1 has 64, etc.
        // We want the level where maxHarmonic <= maxSafeHarmonic
        // maxHarmonic at level L = 128 >> L
        // So: 128 >> L <= maxSafeHarmonic
        //     L >= log2(128 / maxSafeHarmonic)
        if (maxSafeHarmonic >= 128.f) return 0;
        if (maxSafeHarmonic < 1.f) return MIP_LEVELS - 1;

        float level = std::log2(128.f / maxSafeHarmonic);
        int iLevel = (int)level;
        return std::max(0, std::min(iLevel, MIP_LEVELS - 1));
    }

    // Read a sample with linear interpolation, from the appropriate mip level
    float readSample(int table, int frame, float phase, int mipLevel) const {
        table = std::max(0, std::min(table, PPG_NUM_TABLES - 1));
        frame = std::max(0, std::min(frame, PPG_FRAMES_PER_TABLE - 1));
        mipLevel = std::max(0, std::min(mipLevel, MIP_LEVELS - 1));

        const float* data = tables[table].frames[mipLevel][frame].samples;

        float pos = phase * PPG_SAMPLES_PER_FRAME;
        int idx0 = (int)pos;
        float frac = pos - (float)idx0;
        idx0 = idx0 & (PPG_SAMPLES_PER_FRAME - 1);
        int idx1 = (idx0 + 1) & (PPG_SAMPLES_PER_FRAME - 1);

        return data[idx0] * (1.f - frac) + data[idx1] * frac;
    }

    // Read with crossfade between two adjacent mip levels for smooth transitions
    float readSampleSmooth(int table, int frame, float phase, float freq, float sampleRate) const {
        if (freq <= 0.f || sampleRate <= 0.f)
            return readSample(table, frame, phase, 0);

        float samplesPerCycle = sampleRate / freq;
        float maxSafeHarmonic = samplesPerCycle * 0.5f;

        if (maxSafeHarmonic >= 128.f)
            return readSample(table, frame, phase, 0);
        if (maxSafeHarmonic < 1.f)
            return readSample(table, frame, phase, MIP_LEVELS - 1);

        float levelF = std::log2(128.f / maxSafeHarmonic);
        int level0 = (int)levelF;
        int level1 = level0 + 1;
        float blend = levelF - (float)level0;

        level0 = std::max(0, std::min(level0, MIP_LEVELS - 1));
        level1 = std::max(0, std::min(level1, MIP_LEVELS - 1));

        float s0 = readSample(table, frame, phase, level0);
        float s1 = readSample(table, frame, phase, level1);

        return s0 * (1.f - blend) + s1 * blend;
    }
};

// Scanning modes for wavetable position control
enum class ScanMode {
    Stepped,       // PPG-authentic: switch frames at zero crossings
    Interpolated   // Modern: crossfade between adjacent frames
};

// Per-voice wavetable scanning state
struct WavetableScanner {
    int currentFrame = 0;         // Current frame index (for stepped mode)
    int targetFrame = 0;          // Target frame (may differ from current in stepped mode)
    float lastSample = 0.f;       // Previous output sample (for zero-crossing detection)
    bool waitingForZeroCross = false;  // Stepped mode: waiting for zero crossing to switch

    // Update target frame from position control (0..1 maps to frame range)
    void setPosition(float position, int numFrames) {
        position = std::max(0.f, std::min(1.f, position));
        targetFrame = (int)(position * (numFrames - 1) + 0.5f);
        targetFrame = std::max(0, std::min(targetFrame, numFrames - 1));
    }

    // Process one sample in stepped mode (PPG-authentic)
    // Switches frame only at zero crossings for click-free transitions
    float processStepped(const WavetableBank& bank, int table, float phase,
                         int mipLevel, float currentSample) {
        // Check if we need to change frame
        if (currentFrame != targetFrame) {
            if (!waitingForZeroCross) {
                waitingForZeroCross = true;
            }

            // Detect zero crossing: sign change between last and current sample
            if (waitingForZeroCross) {
                bool zeroCross = (lastSample <= 0.f && currentSample > 0.f) ||
                                 (lastSample >= 0.f && currentSample < 0.f) ||
                                 (lastSample == 0.f);

                if (zeroCross) {
                    currentFrame = targetFrame;
                    waitingForZeroCross = false;
                }
            }
        }

        float sample = bank.readSample(table, currentFrame, phase, mipLevel);
        lastSample = sample;
        return sample;
    }

    // Process one sample in interpolated mode (modern smooth scanning)
    // Crossfades between adjacent frames based on fractional position
    float processInterpolated(const WavetableBank& bank, int table, float position,
                              float phase, int mipLevel) {
        int numFrames = PPG_FRAMES_PER_TABLE;
        position = std::max(0.f, std::min(1.f, position));

        float framePos = position * (numFrames - 1);
        int frame0 = (int)framePos;
        float blend = framePos - (float)frame0;
        int frame1 = std::min(frame0 + 1, numFrames - 1);

        float s0 = bank.readSample(table, frame0, phase, mipLevel);
        float s1 = bank.readSample(table, frame1, phase, mipLevel);

        float sample = s0 * (1.f - blend) + s1 * blend;
        // Update state for potential mode switching
        currentFrame = frame0;
        lastSample = sample;
        return sample;
    }
};

// DAC bit-depth emulation — quantizes output to emulate vintage converter resolution
struct DacEmulator {
    enum Depth { BIT_8, BIT_12, BIT_16 };

    static float quantize(float sample, Depth depth) {
        float levels;
        switch (depth) {
            case BIT_8:  levels = 256.f;   break;  // 2^8
            case BIT_12: levels = 4096.f;  break;  // 2^12
            case BIT_16: levels = 65536.f; break;  // 2^16
            default:     levels = 65536.f; break;
        }

        // Quantize: scale to integer range, round, scale back
        float halfLevels = levels * 0.5f;
        return std::round(sample * halfLevels) / halfLevels;
    }

    // Convenience: quantize from depth index (0=8bit, 1=12bit, 2=16bit)
    static float quantize(float sample, int depthIndex) {
        Depth d;
        switch (depthIndex) {
            case 0: d = BIT_8; break;
            case 1: d = BIT_12; break;
            case 2: d = BIT_16; break;
            default: d = BIT_16; break;
        }
        return quantize(sample, d);
    }
};

// Phase accumulator with 1V/oct pitch tracking
struct PhaseAccumulator {
    float phase = 0.f;

    // Advance phase for one sample. Returns the new phase (0..1).
    // freq: oscillator frequency in Hz
    // sampleRate: audio sample rate in Hz
    float advance(float freq, float sampleRate) {
        if (sampleRate <= 0.f) return phase;
        float delta = freq / sampleRate;
        // Clamp delta to prevent instability at extreme frequencies
        delta = std::max(0.f, std::min(delta, 0.49f));
        phase += delta;
        // Wrap to 0..1 (handles large jumps from FM)
        phase -= std::floor(phase);
        return phase;
    }

    // Convert 1V/oct voltage to frequency
    // 0V = C4 (261.626 Hz), each volt = one octave
    static float voltToFreq(float volts) {
        // f = 261.626 * 2^v
        return 261.626f * std::pow(2.f, volts);
    }

    void reset() { phase = 0.f; }
};

// Complete single-voice wavetable oscillator
// Combines all DSP components for one voice
struct WavetableVoice {
    PhaseAccumulator phaseAccum;
    WavetableScanner scanner;
    int currentTable = 0;
    ScanMode scanMode = ScanMode::Stepped;
    DacEmulator::Depth dacDepth = DacEmulator::BIT_8;

    // Process one sample for this voice
    // position: wavetable frame position (0..1)
    // freq: oscillator frequency in Hz
    // sampleRate: audio sample rate in Hz
    float process(const WavetableBank& bank, float position, float freq, float sampleRate) {
        float phase = phaseAccum.advance(freq, sampleRate);
        int mipLevel = WavetableBank::selectMipLevel(freq, sampleRate);

        float sample;
        if (scanMode == ScanMode::Stepped) {
            // In stepped mode, we need a preliminary read to detect zero crossings
            float preliminary = bank.readSample(currentTable, scanner.currentFrame, phase, mipLevel);
            scanner.setPosition(position, PPG_FRAMES_PER_TABLE);
            sample = scanner.processStepped(bank, currentTable, phase, mipLevel, preliminary);
        } else {
            sample = scanner.processInterpolated(bank, currentTable, position, phase, mipLevel);
        }

        return DacEmulator::quantize(sample, dacDepth);
    }
};
