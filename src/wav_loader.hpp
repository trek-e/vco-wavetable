/*
 * Hurricane 8 — WAV File Wavetable Loader
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Loads single-cycle and multi-frame (Serum-style concatenated) WAV files
 * as custom wavetables. Resamples to PPG_SAMPLES_PER_FRAME (256) samples
 * per frame and builds mip-mapped versions for band-limited playback.
 *
 * Supports: 8/16/24/32-bit integer PCM and 32-bit float WAV files, mono only.
 * Multi-channel files use channel 0 only.
 *
 * No VCV Rack SDK dependency — testable standalone.
 */

#pragma once

#include "wavetable_engine.hpp"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────────────────
// WAV file parsing (minimal, self-contained)
// ─────────────────────────────────────────────────────────

struct WavData {
    std::vector<float> samples;   // Interleaved if multi-channel, but we only use ch0
    int sampleRate = 0;
    int numChannels = 0;
    int numFrames = 0;            // Total sample frames (samples.size() / numChannels)
    int bitsPerSample = 0;
    bool valid = false;
    std::string error;
};

// Read a little-endian uint16/uint32 from a byte buffer
static inline uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static WavData loadWavFile(const std::string& path) {
    WavData wav;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        wav.error = "Cannot open file: " + path;
        return wav;
    }

    // Read entire file into memory (wavetable files are small)
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize < 44) {
        wav.error = "File too small to be a WAV file";
        fclose(f);
        return wav;
    }

    std::vector<uint8_t> buf(fileSize);
    size_t bytesRead = fread(buf.data(), 1, fileSize, f);
    fclose(f);

    if ((long)bytesRead != fileSize) {
        wav.error = "Failed to read entire file";
        return wav;
    }

    const uint8_t* data = buf.data();

    // RIFF header
    if (memcmp(data, "RIFF", 4) != 0) {
        wav.error = "Not a RIFF file";
        return wav;
    }
    if (memcmp(data + 8, "WAVE", 4) != 0) {
        wav.error = "Not a WAVE file";
        return wav;
    }

    // Find fmt and data chunks
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    const uint8_t* dataChunk = nullptr;
    uint32_t dataSize = 0;

    size_t pos = 12;
    while (pos + 8 <= (size_t)fileSize) {
        uint32_t chunkSize = readU32LE(data + pos + 4);

        if (memcmp(data + pos, "fmt ", 4) == 0) {
            if (pos + 8 + 16 > (size_t)fileSize) {
                wav.error = "Truncated fmt chunk";
                return wav;
            }
            audioFormat = readU16LE(data + pos + 8);
            numChannels = readU16LE(data + pos + 10);
            sampleRate = readU32LE(data + pos + 12);
            // bytes 16-19: byte rate (skip)
            // bytes 20-21: block align (skip)
            bitsPerSample = readU16LE(data + pos + 22);
        }
        else if (memcmp(data + pos, "data", 4) == 0) {
            dataChunk = data + pos + 8;
            dataSize = chunkSize;
        }

        // Advance to next chunk (pad to even boundary)
        pos += 8 + chunkSize;
        if (pos % 2 != 0) pos++;
    }

    if (audioFormat == 0) {
        wav.error = "No fmt chunk found";
        return wav;
    }
    if (audioFormat != 1 && audioFormat != 3) {
        // 1 = PCM integer, 3 = IEEE float
        wav.error = "Unsupported audio format (only PCM and IEEE float supported)";
        return wav;
    }
    if (!dataChunk || dataSize == 0) {
        wav.error = "No data chunk found";
        return wav;
    }
    if (numChannels == 0) {
        wav.error = "Zero channels";
        return wav;
    }

    int bytesPerSample = bitsPerSample / 8;
    int blockAlign = bytesPerSample * numChannels;
    int totalFrames = dataSize / blockAlign;

    if (totalFrames == 0) {
        wav.error = "No audio data";
        return wav;
    }

    // Decode samples (extract channel 0 only)
    wav.samples.resize(totalFrames);
    wav.numChannels = numChannels;
    wav.sampleRate = sampleRate;
    wav.bitsPerSample = bitsPerSample;
    wav.numFrames = totalFrames;

    for (int i = 0; i < totalFrames; i++) {
        const uint8_t* samplePtr = dataChunk + (size_t)i * blockAlign;

        // Bounds check
        if (samplePtr + bytesPerSample > data + fileSize) break;

        float value = 0.f;

        if (audioFormat == 3 && bitsPerSample == 32) {
            // IEEE 32-bit float
            float fval;
            memcpy(&fval, samplePtr, 4);
            value = fval;
        }
        else if (audioFormat == 1) {
            switch (bitsPerSample) {
                case 8: {
                    // Unsigned 8-bit
                    value = ((float)samplePtr[0] - 128.f) / 128.f;
                    break;
                }
                case 16: {
                    int16_t s = (int16_t)readU16LE(samplePtr);
                    value = (float)s / 32768.f;
                    break;
                }
                case 24: {
                    // Sign-extend 24-bit to 32-bit
                    int32_t s = (int32_t)samplePtr[0] | ((int32_t)samplePtr[1] << 8) | ((int32_t)samplePtr[2] << 16);
                    if (s & 0x800000) s |= 0xFF000000;  // Sign extend
                    value = (float)s / 8388608.f;
                    break;
                }
                case 32: {
                    int32_t s = (int32_t)readU32LE(samplePtr);
                    value = (float)s / 2147483648.f;
                    break;
                }
                default:
                    wav.error = "Unsupported bit depth: " + std::to_string(bitsPerSample);
                    return wav;
            }
        }

        wav.samples[i] = value;
    }

    wav.valid = true;
    return wav;
}

// ─────────────────────────────────────────────────────────
// Wavetable frame extraction and resampling
// ─────────────────────────────────────────────────────────

// Result of loading a user wavetable
struct UserWavetable {
    MipMappedTable table;
    int numFrames = 0;           // Actual number of frames loaded (≤ PPG_FRAMES_PER_TABLE)
    std::string sourcePath;
    bool valid = false;
    std::string error;
};

// Resample a source buffer of arbitrary length to exactly targetLen samples
// Uses linear interpolation
static void resampleLinear(const float* src, int srcLen, float* dst, int dstLen) {
    if (srcLen <= 0 || dstLen <= 0) return;
    if (srcLen == 1) {
        for (int i = 0; i < dstLen; i++) dst[i] = src[0];
        return;
    }

    float ratio = (float)(srcLen - 1) / (float)(dstLen - 1);
    for (int i = 0; i < dstLen; i++) {
        float pos = (float)i * ratio;
        int idx0 = (int)pos;
        float frac = pos - (float)idx0;
        int idx1 = std::min(idx0 + 1, srcLen - 1);
        dst[i] = src[idx0] * (1.f - frac) + src[idx1] * frac;
    }
}

// Detect the cycle length of a single-cycle waveform
// Returns the full sample count if no obvious cycle is detected
static int detectCycleLength(const float* samples, int numSamples) {
    // Common single-cycle sizes used by wavetable synths (smallest first)
    static const int commonSizes[] = { 256, 512, 1024, 2048, 4096, 8192 };

    // First check: is this an exact multiple of a smaller common size?
    // This catches Serum-style concatenated wavetables (e.g., 2048 = 8×256)
    for (int sz : commonSizes) {
        if (numSamples > sz && numSamples % sz == 0) {
            int numCycles = numSamples / sz;
            if (numCycles >= 2 && numCycles <= PPG_FRAMES_PER_TABLE) return sz;
        }
    }

    // If the total length matches a common size exactly (and wasn't caught
    // as a multi-frame above), treat it as a single cycle
    for (int sz : commonSizes) {
        if (numSamples == sz) return numSamples;
    }

    // Fallback: treat the entire file as a single cycle
    return numSamples;
}

// Load a user wavetable from a WAV file
// - Single-cycle files (≤ 8192 samples) become a 1-frame wavetable
// - Multi-frame files are split into frames based on detected cycle length
// - All frames are resampled to PPG_SAMPLES_PER_FRAME (256) samples
static UserWavetable loadUserWavetable(const std::string& path) {
    UserWavetable result;
    result.sourcePath = path;

    WavData wav = loadWavFile(path);
    if (!wav.valid) {
        result.error = wav.error;
        return result;
    }

    int totalSamples = (int)wav.samples.size();
    if (totalSamples == 0) {
        result.error = "WAV file contains no samples";
        return result;
    }

    // Determine cycle length and number of frames
    int cycleLen = detectCycleLength(wav.samples.data(), totalSamples);
    int numFrames = totalSamples / cycleLen;

    // Clamp to max frames per table
    if (numFrames > PPG_FRAMES_PER_TABLE) numFrames = PPG_FRAMES_PER_TABLE;
    if (numFrames < 1) numFrames = 1;

    result.numFrames = numFrames;

    // Extract and resample each frame to PPG_SAMPLES_PER_FRAME
    float sourceFrames[PPG_FRAMES_PER_TABLE][PPG_SAMPLES_PER_FRAME];
    std::memset(sourceFrames, 0, sizeof(sourceFrames));

    for (int f = 0; f < numFrames; f++) {
        int srcOffset = f * cycleLen;
        int srcAvail = std::min(cycleLen, totalSamples - srcOffset);
        if (srcAvail <= 0) break;

        if (srcAvail == PPG_SAMPLES_PER_FRAME) {
            // Perfect match — no resampling needed
            std::memcpy(sourceFrames[f], wav.samples.data() + srcOffset,
                       PPG_SAMPLES_PER_FRAME * sizeof(float));
        } else {
            // Resample to target frame size
            resampleLinear(wav.samples.data() + srcOffset, srcAvail,
                          sourceFrames[f], PPG_SAMPLES_PER_FRAME);
        }
    }

    // If fewer frames than PPG_FRAMES_PER_TABLE, fill remaining with last frame
    // This ensures smooth scanning doesn't fall off the end
    for (int f = numFrames; f < PPG_FRAMES_PER_TABLE; f++) {
        std::memcpy(sourceFrames[f], sourceFrames[numFrames - 1],
                   PPG_SAMPLES_PER_FRAME * sizeof(float));
    }

    // Build mip-mapped table from the source frames
    result.table.buildFromSource(sourceFrames);
    result.valid = true;
    return result;
}

// Load a user wavetable from raw sample data (for testing without file I/O)
static UserWavetable loadUserWavetableFromBuffer(const float* samples, int numSamples,
                                                   int forceCycleLen = 0) {
    UserWavetable result;
    result.sourcePath = "<buffer>";

    if (numSamples == 0 || samples == nullptr) {
        result.error = "Empty buffer";
        return result;
    }

    int cycleLen = (forceCycleLen > 0) ? forceCycleLen
                                       : detectCycleLength(samples, numSamples);
    int numFrames = numSamples / cycleLen;
    if (numFrames > PPG_FRAMES_PER_TABLE) numFrames = PPG_FRAMES_PER_TABLE;
    if (numFrames < 1) numFrames = 1;

    result.numFrames = numFrames;

    float sourceFrames[PPG_FRAMES_PER_TABLE][PPG_SAMPLES_PER_FRAME];
    std::memset(sourceFrames, 0, sizeof(sourceFrames));

    for (int f = 0; f < numFrames; f++) {
        int srcOffset = f * cycleLen;
        int srcAvail = std::min(cycleLen, numSamples - srcOffset);
        if (srcAvail <= 0) break;

        if (srcAvail == PPG_SAMPLES_PER_FRAME) {
            std::memcpy(sourceFrames[f], samples + srcOffset,
                       PPG_SAMPLES_PER_FRAME * sizeof(float));
        } else {
            resampleLinear(samples + srcOffset, srcAvail,
                          sourceFrames[f], PPG_SAMPLES_PER_FRAME);
        }
    }

    for (int f = numFrames; f < PPG_FRAMES_PER_TABLE; f++) {
        std::memcpy(sourceFrames[f], sourceFrames[numFrames - 1],
                   PPG_SAMPLES_PER_FRAME * sizeof(float));
    }

    result.table.buildFromSource(sourceFrames);
    result.valid = true;
    return result;
}
