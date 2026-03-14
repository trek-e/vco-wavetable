/*
 * Hurricane 8 — User Wavetable Unit Tests
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Tests for WTD-02..05:
 *   WTD-02: Load single-cycle WAV files as custom wavetables
 *   WTD-03: Load multi-frame WAV files (Serum-style concatenated cycles)
 *   WTD-04: Right-click context menu (tested at integration level, not here)
 *   WTD-05: JSON persistence (tested at integration level, not here)
 *
 * Standalone tests for WAV parsing, frame detection, resampling, and mip-mapping.
 *
 * Build: g++ -std=c++17 -O2 -I../src -o test_user_wavetables test_user_wavetables.cpp -lm
 * Run:   ./test_user_wavetables
 */

#include "wav_loader.hpp"
#include "wavetable_engine.hpp"
#include "ppg_wavetables.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <functional>
#include <memory>

// ─────────────────────────────────────────────────────────
// Test framework (same pattern as other test suites)
// ─────────────────────────────────────────────────────────

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

struct TestFailure {
    std::string testName;
    std::string message;
    int line;
};

static std::vector<TestFailure> failures;
static std::string currentTestName;

#define TEST(name) \
    static void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { registerTest(#name, test_##name); } \
    } testRegistrar_##name; \
    static void test_##name()

#define EXPECT(cond, msg) \
    do { \
        if (!(cond)) { \
            throw TestFailure{currentTestName, msg, __LINE__}; \
        } \
    } while(0)

#define EXPECT_NEAR(val, expected, tolerance, msg) \
    do { \
        float v_ = (val); float e_ = (expected); float t_ = (tolerance); \
        if (std::abs(v_ - e_) > t_) { \
            char buf_[256]; \
            snprintf(buf_, sizeof(buf_), "%s (got %.6f, expected %.6f ±%.6f)", msg, v_, e_, t_); \
            throw TestFailure{currentTestName, buf_, __LINE__}; \
        } \
    } while(0)

struct TestEntry {
    std::string name;
    std::function<void()> fn;
};

static std::vector<TestEntry>& getTests() {
    static std::vector<TestEntry> tests;
    return tests;
}

static void registerTest(const std::string& name, std::function<void()> fn) {
    getTests().push_back({name, fn});
}

// ─────────────────────────────────────────────────────────
// WAV file generation helpers (create test files in memory)
// ─────────────────────────────────────────────────────────

static void writeU16LE(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(val & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
}

static void writeU32LE(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(val & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 24) & 0xFF);
}

// Create a valid 16-bit mono WAV file in memory and write to disk
static std::string createTestWav16(const std::string& filename,
                                    const std::vector<float>& samples,
                                    int sampleRate = 44100) {
    std::vector<uint8_t> buf;

    int numSamples = (int)samples.size();
    int bitsPerSample = 16;
    int bytesPerSample = bitsPerSample / 8;
    int dataSize = numSamples * bytesPerSample;
    int fmtChunkSize = 16;
    int fileSize = 4 + (8 + fmtChunkSize) + (8 + dataSize);

    // RIFF header
    buf.insert(buf.end(), {'R','I','F','F'});
    writeU32LE(buf, fileSize);
    buf.insert(buf.end(), {'W','A','V','E'});

    // fmt chunk
    buf.insert(buf.end(), {'f','m','t',' '});
    writeU32LE(buf, fmtChunkSize);
    writeU16LE(buf, 1);                              // PCM
    writeU16LE(buf, 1);                              // mono
    writeU32LE(buf, sampleRate);                     // sample rate
    writeU32LE(buf, sampleRate * bytesPerSample);    // byte rate
    writeU16LE(buf, bytesPerSample);                 // block align
    writeU16LE(buf, bitsPerSample);                  // bits per sample

    // data chunk
    buf.insert(buf.end(), {'d','a','t','a'});
    writeU32LE(buf, dataSize);
    for (float s : samples) {
        s = std::max(-1.f, std::min(1.f, s));
        int16_t val = (int16_t)(s * 32767.f);
        writeU16LE(buf, (uint16_t)val);
    }

    // Write to temp file
    std::string path = "/tmp/" + filename;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return "";
    fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
    return path;
}

// Create a 32-bit float WAV
static std::string createTestWavFloat(const std::string& filename,
                                       const std::vector<float>& samples,
                                       int sampleRate = 44100) {
    std::vector<uint8_t> buf;

    int numSamples = (int)samples.size();
    int bitsPerSample = 32;
    int bytesPerSample = bitsPerSample / 8;
    int dataSize = numSamples * bytesPerSample;
    int fmtChunkSize = 16;
    int fileSize = 4 + (8 + fmtChunkSize) + (8 + dataSize);

    // RIFF header
    buf.insert(buf.end(), {'R','I','F','F'});
    writeU32LE(buf, fileSize);
    buf.insert(buf.end(), {'W','A','V','E'});

    // fmt chunk
    buf.insert(buf.end(), {'f','m','t',' '});
    writeU32LE(buf, fmtChunkSize);
    writeU16LE(buf, 3);                              // IEEE float
    writeU16LE(buf, 1);                              // mono
    writeU32LE(buf, sampleRate);
    writeU32LE(buf, sampleRate * bytesPerSample);
    writeU16LE(buf, bytesPerSample);
    writeU16LE(buf, bitsPerSample);

    // data chunk
    buf.insert(buf.end(), {'d','a','t','a'});
    writeU32LE(buf, dataSize);
    for (float s : samples) {
        uint8_t bytes[4];
        memcpy(bytes, &s, 4);
        buf.insert(buf.end(), bytes, bytes + 4);
    }

    std::string path = "/tmp/" + filename;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return "";
    fwrite(buf.data(), 1, buf.size(), f);
    fclose(f);
    return path;
}

// Generate a sine wave with N samples
static std::vector<float> generateSine(int numSamples, float amplitude = 1.f) {
    std::vector<float> samples(numSamples);
    for (int i = 0; i < numSamples; i++) {
        samples[i] = amplitude * std::sin(2.f * PPG_PI * i / numSamples);
    }
    return samples;
}

// Generate a sawtooth wave with N samples
static std::vector<float> generateSaw(int numSamples, float amplitude = 1.f) {
    std::vector<float> samples(numSamples);
    for (int i = 0; i < numSamples; i++) {
        samples[i] = amplitude * (2.f * i / numSamples - 1.f);
    }
    return samples;
}


// ═════════════════════════════════════════════════════════
// WAV FILE PARSING TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd02_load_16bit_wav) {
    // Generate a 256-sample sine wave and save as 16-bit WAV
    auto sine = generateSine(256);
    std::string path = createTestWav16("test_sine_256.wav", sine);
    EXPECT(!path.empty(), "Failed to create test WAV file");

    WavData wav = loadWavFile(path);
    EXPECT(wav.valid, "WAV file should be valid");
    EXPECT(wav.numChannels == 1, "Should be mono");
    EXPECT(wav.bitsPerSample == 16, "Should be 16-bit");
    EXPECT(wav.numFrames == 256, "Should have 256 sample frames");
    EXPECT(wav.sampleRate == 44100, "Sample rate should be 44100");
}

TEST(wtd02_load_float_wav) {
    auto sine = generateSine(256);
    std::string path = createTestWavFloat("test_sine_float.wav", sine);
    EXPECT(!path.empty(), "Failed to create test WAV file");

    WavData wav = loadWavFile(path);
    EXPECT(wav.valid, "WAV file should be valid");
    EXPECT(wav.bitsPerSample == 32, "Should be 32-bit float");
    EXPECT(wav.numFrames == 256, "Should have 256 sample frames");

    // Float WAV should have better precision than 16-bit
    for (int i = 0; i < 256; i++) {
        EXPECT_NEAR(wav.samples[i], sine[i], 0.0001f, "Float sample precision");
    }
}

TEST(wtd02_load_nonexistent_file) {
    WavData wav = loadWavFile("/tmp/nonexistent_wavetable_test.wav");
    EXPECT(!wav.valid, "Should fail for nonexistent file");
    EXPECT(!wav.error.empty(), "Should have an error message");
}

TEST(wtd02_load_invalid_file) {
    // Write garbage data to a file
    std::string path = "/tmp/test_invalid.wav";
    FILE* f = fopen(path.c_str(), "wb");
    const char* garbage = "This is not a WAV file";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    WavData wav = loadWavFile(path);
    EXPECT(!wav.valid, "Should fail for invalid file");
}

TEST(wtd02_16bit_sample_accuracy) {
    // Verify 16-bit quantization round-trip
    auto sine = generateSine(256);
    std::string path = createTestWav16("test_accuracy.wav", sine);

    WavData wav = loadWavFile(path);
    EXPECT(wav.valid, "Should load");

    // 16-bit quantization error: max ~1/32768 ≈ 3e-5
    for (int i = 0; i < 256; i++) {
        EXPECT_NEAR(wav.samples[i], sine[i], 0.001f, "16-bit accuracy");
    }
}


// ═════════════════════════════════════════════════════════
// SINGLE-CYCLE WAVETABLE LOADING (WTD-02)
// ═════════════════════════════════════════════════════════

TEST(wtd02_single_cycle_256_samples) {
    // Exactly 256 samples = single cycle at native frame size
    auto sine = generateSine(256);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 256);

    EXPECT(ut.valid, "Should load successfully");
    EXPECT(ut.numFrames == 1, "256 samples = 1 frame");
}

TEST(wtd02_single_cycle_512_as_multiframe) {
    // 512 samples = 2 × 256 — auto-detected as 2-frame wavetable
    // (This matches Serum-style behavior where 256 is the base cycle size)
    auto sine = generateSine(512);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 512);

    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 2, "512 samples = 2 frames of 256");
}

TEST(wtd02_single_cycle_forced) {
    // Users can force single-cycle interpretation with explicit cycle length
    auto sine = generateSine(512);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 512, 512);

    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 1, "Forced 512-sample cycle = 1 frame");
}

TEST(wtd02_single_cycle_1024_as_multiframe) {
    auto sine = generateSine(1024);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 1024);

    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 4, "1024 samples = 4 frames of 256");
}

TEST(wtd02_single_cycle_2048_as_multiframe) {
    auto sine = generateSine(2048);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 2048);

    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 8, "2048 samples = 8 frames of 256");
}

TEST(wtd02_resampled_waveform_shape) {
    // Load a 512-sample sine as a single cycle (forced), verify resampled shape
    auto sine = generateSine(512);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 512, 512);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 1, "Forced single cycle");

    // Read samples from mip level 0 (full bandwidth), frame 0
    // Phase 0.25 (quarter cycle) should be near +1.0 for a sine
    float atQuarter = ut.table.frames[0][0].samples[PPG_SAMPLES_PER_FRAME / 4];
    EXPECT_NEAR(atQuarter, 1.0f, 0.05f, "Resampled sine at quarter cycle should be ~1.0");

    // Phase 0.75 (three-quarter cycle) should be near -1.0
    float atThreeQuarter = ut.table.frames[0][0].samples[3 * PPG_SAMPLES_PER_FRAME / 4];
    EXPECT_NEAR(atThreeQuarter, -1.0f, 0.05f, "Resampled sine at 3/4 cycle should be ~-1.0");
}

TEST(wtd02_single_cycle_wav_file) {
    // End-to-end: WAV file → user wavetable
    auto sine = generateSine(256);
    std::string path = createTestWav16("test_single_cycle.wav", sine);

    UserWavetable ut = loadUserWavetable(path);
    EXPECT(ut.valid, "Should load from WAV file");
    EXPECT(ut.numFrames == 1, "Single cycle");
    EXPECT(ut.sourcePath == path, "Source path should be preserved");
}


// ═════════════════════════════════════════════════════════
// MULTI-FRAME WAVETABLE LOADING (WTD-03)
// ═════════════════════════════════════════════════════════

TEST(wtd03_multi_frame_2_cycles) {
    // 512 samples = 2 × 256 = 2 frames
    auto sine = generateSine(256);
    auto saw = generateSaw(256);
    std::vector<float> combined;
    combined.insert(combined.end(), sine.begin(), sine.end());
    combined.insert(combined.end(), saw.begin(), saw.end());

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 512, 256);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 2, "2 × 256 = 2 frames");
}

TEST(wtd03_multi_frame_4_cycles) {
    // 4 × 256 = 1024 samples, with forced cycle length of 256
    std::vector<float> combined;
    for (int f = 0; f < 4; f++) {
        auto wave = generateSine(256, 1.f - 0.2f * f);  // decreasing amplitude
        combined.insert(combined.end(), wave.begin(), wave.end());
    }

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 1024, 256);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 4, "4 × 256 = 4 frames");
}

TEST(wtd03_multi_frame_64_cycles_max) {
    // 64 × 256 = max frames per table
    std::vector<float> combined;
    for (int f = 0; f < 64; f++) {
        float morph = (float)f / 63.f;
        std::vector<float> frame(256);
        for (int s = 0; s < 256; s++) {
            float phase = 2.f * PPG_PI * s / 256.f;
            // Morph from sine to saw
            frame[s] = (1.f - morph) * std::sin(phase) +
                       morph * (2.f * s / 256.f - 1.f);
        }
        combined.insert(combined.end(), frame.begin(), frame.end());
    }

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 64 * 256, 256);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 64, "64 frames = PPG_FRAMES_PER_TABLE");
}

TEST(wtd03_multi_frame_clamps_to_64) {
    // More than 64 frames — should clamp
    std::vector<float> combined;
    for (int f = 0; f < 128; f++) {
        auto wave = generateSine(256);
        combined.insert(combined.end(), wave.begin(), wave.end());
    }

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 128 * 256, 256);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 64, "Should clamp to 64 frames");
}

TEST(wtd03_serum_style_wav_file) {
    // Serum-style: 8 concatenated 256-sample cycles in a WAV file
    std::vector<float> combined;
    for (int f = 0; f < 8; f++) {
        float morph = (float)f / 7.f;
        for (int s = 0; s < 256; s++) {
            float phase = 2.f * PPG_PI * s / 256.f;
            combined.push_back((1.f - morph) * std::sin(phase) +
                               morph * std::sin(phase * 3.f) * 0.5f);
        }
    }

    std::string path = createTestWav16("test_serum_style.wav", combined);
    UserWavetable ut = loadUserWavetable(path);
    EXPECT(ut.valid, "Should load Serum-style WAV");
    // 2048 samples / 256 = 8 frames
    EXPECT(ut.numFrames == 8, "Should detect 8 frames (2048/256)");
}

TEST(wtd03_multi_frame_different_waveforms) {
    // Verify that frame 0 and frame 1 actually contain different data
    auto sine = generateSine(256);
    auto saw = generateSaw(256);
    std::vector<float> combined;
    combined.insert(combined.end(), sine.begin(), sine.end());
    combined.insert(combined.end(), saw.begin(), saw.end());

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 512, 256);
    EXPECT(ut.valid, "Should load");

    // Frame 0 (sine) and frame 1 (saw) should differ at mip level 0
    float frame0_mid = ut.table.frames[0][0].samples[PPG_SAMPLES_PER_FRAME / 2];
    float frame1_mid = ut.table.frames[0][1].samples[PPG_SAMPLES_PER_FRAME / 2];

    // Sine at 0.5 cycle ≈ 0, Saw at 0.5 cycle ≈ 0 too (both cross zero)
    // Check at quarter cycle instead: sine ≈ 1.0, saw ≈ -0.5
    float frame0_quarter = ut.table.frames[0][0].samples[PPG_SAMPLES_PER_FRAME / 4];
    float frame1_quarter = ut.table.frames[0][1].samples[PPG_SAMPLES_PER_FRAME / 4];

    float diff = std::abs(frame0_quarter - frame1_quarter);
    EXPECT(diff > 0.3f, "Frames should contain different waveform data");
}


// ═════════════════════════════════════════════════════════
// RESAMPLING TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd02_resample_identity) {
    // 256 samples → 256 samples (no resampling)
    float src[256], dst[256];
    for (int i = 0; i < 256; i++) src[i] = std::sin(2.f * PPG_PI * i / 256.f);
    resampleLinear(src, 256, dst, 256);

    for (int i = 0; i < 256; i++) {
        EXPECT_NEAR(dst[i], src[i], 0.0001f, "Identity resampling");
    }
}

TEST(wtd02_resample_downsample) {
    // 512 → 256 (downsample 2:1)
    auto src = generateSine(512);
    float dst[256];
    resampleLinear(src.data(), 512, dst, 256);

    // Should still look like a sine
    EXPECT_NEAR(dst[0], 0.f, 0.01f, "Start near zero");
    EXPECT_NEAR(dst[64], 1.f, 0.05f, "Peak at quarter cycle");
    EXPECT_NEAR(dst[128], 0.f, 0.05f, "Zero crossing at half");
    EXPECT_NEAR(dst[192], -1.f, 0.05f, "Trough at three-quarter");
}

TEST(wtd02_resample_upsample) {
    // 128 → 256 (upsample 1:2)
    auto src = generateSine(128);
    float dst[256];
    resampleLinear(src.data(), 128, dst, 256);

    // Should still look like a sine
    EXPECT_NEAR(dst[0], 0.f, 0.01f, "Start near zero");
    EXPECT_NEAR(dst[64], 1.f, 0.05f, "Peak at quarter");
}


// ═════════════════════════════════════════════════════════
// MIP-MAP GENERATION TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd02_mipmap_level0_preserves_waveform) {
    // Mip level 0 should closely match the original waveform
    auto sine = generateSine(256);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 256);
    EXPECT(ut.valid, "Should load");

    for (int i = 0; i < 256; i++) {
        EXPECT_NEAR(ut.table.frames[0][0].samples[i], sine[i], 0.05f,
                    "Mip level 0 should match original");
    }
}

TEST(wtd02_mipmap_highest_level_is_sinusoidal) {
    // Highest mip level (level 7) should be essentially a sinusoid
    // regardless of the input waveform (only fundamental remains)
    auto saw = generateSaw(256);
    UserWavetable ut = loadUserWavetableFromBuffer(saw.data(), 256);
    EXPECT(ut.valid, "Should load");

    // Level 7: only 1 harmonic (fundamental)
    // Verify it's smooth and has non-zero amplitude
    const float* data = ut.table.frames[MIP_LEVELS - 1][0].samples;

    float maxVal = 0.f;
    for (int i = 0; i < 256; i++) {
        maxVal = std::max(maxVal, std::abs(data[i]));
    }
    EXPECT(maxVal > 0.1f, "Highest mip level should have non-zero amplitude");

    // Verify smoothness: adjacent sample differences should be small
    // (a sinusoid at 1 cycle/256 samples has max derivative ≈ 2π/256 ≈ 0.025)
    float maxDiff = 0.f;
    for (int i = 0; i < 255; i++) {
        maxDiff = std::max(maxDiff, std::abs(data[i+1] - data[i]));
    }
    // Max diff for a sinusoid of amplitude A: A * 2π/N ≈ 0.025*A
    // Allow generous margin for non-unity amplitude
    EXPECT(maxDiff < maxVal * 0.1f,
           "Highest mip level should be smooth (sinusoidal)");
}

TEST(wtd02_mipmap_decreasing_energy) {
    // Higher mip levels should have equal or less energy (fewer harmonics)
    auto saw = generateSaw(256);
    UserWavetable ut = loadUserWavetableFromBuffer(saw.data(), 256);
    EXPECT(ut.valid, "Should load");

    float prevEnergy = 1e30f;
    for (int level = 0; level < MIP_LEVELS; level++) {
        float energy = 0.f;
        for (int i = 0; i < 256; i++) {
            float s = ut.table.frames[level][0].samples[i];
            energy += s * s;
        }
        // Energy should decrease or stay roughly the same as mip level increases
        // (Allow small tolerance for rounding)
        if (level > 0) {
            EXPECT(energy <= prevEnergy + 0.01f,
                   "Mip level energy should decrease with level");
        }
        prevEnergy = energy;
    }
}


// ═════════════════════════════════════════════════════════
// FRAME FILL / SCANNING TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd02_remaining_frames_filled) {
    // A 1-frame wavetable should fill remaining 63 frames with the same data
    auto sine = generateSine(256);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 256);
    EXPECT(ut.valid, "Should load");

    // Frame 0 and frame 63 (last) should be identical at mip level 0
    for (int i = 0; i < 256; i++) {
        float f0 = ut.table.frames[0][0].samples[i];
        float f63 = ut.table.frames[0][63].samples[i];
        EXPECT_NEAR(f0, f63, 0.0001f, "Last frame should match first for single-cycle table");
    }
}

TEST(wtd03_multi_frame_scanning) {
    // Load 4 frames, verify we can scan through them
    std::vector<float> combined;
    for (int f = 0; f < 4; f++) {
        // Each frame is a sine with different amplitude
        auto wave = generateSine(256, 0.25f * (f + 1));
        combined.insert(combined.end(), wave.begin(), wave.end());
    }

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 1024, 256);
    EXPECT(ut.valid, "Should load");
    EXPECT(ut.numFrames == 4, "Should have 4 frames");

    // Check amplitude increases across frames at the quarter-cycle point
    for (int f = 0; f < 4; f++) {
        float expected = 0.25f * (f + 1);
        float actual = ut.table.frames[0][f].samples[PPG_SAMPLES_PER_FRAME / 4];
        EXPECT_NEAR(actual, expected, 0.05f, "Frame amplitude should increase");
    }
}


// ═════════════════════════════════════════════════════════
// PLAYBACK INTEGRATION TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd02_playback_through_voice) {
    // Load a user wavetable and play it through a WavetableVoice
    auto sine = generateSine(256);
    UserWavetable ut = loadUserWavetableFromBuffer(sine.data(), 256);
    EXPECT(ut.valid, "Should load");

    // Create a bank on the heap (too large for stack)
    auto bank = std::make_unique<WavetableBank>();
    bank->tables[0] = ut.table;
    bank->initialized = true;

    WavetableVoice voice;
    voice.currentTable = 0;
    voice.scanMode = ScanMode::Interpolated;
    voice.dacDepth = DacEmulator::BIT_16;  // High resolution for accuracy

    // Run for one cycle at 256 Hz with 256 * 256 = 65536 Hz sample rate
    // (so each sample advances 1/256 of a cycle)
    float sampleRate = 256.f * 256.f;
    float freq = 256.f;
    bool producedNonZero = false;

    for (int i = 0; i < 256; i++) {
        float out = voice.process(*bank, 0.f, freq, sampleRate);
        if (std::abs(out) > 0.01f) producedNonZero = true;
    }

    EXPECT(producedNonZero, "Playback should produce non-zero output");
}

TEST(wtd03_scanning_through_multiframe_playback) {
    // Verify scanning through a multi-frame user wavetable
    std::vector<float> combined;
    // Frame 0: sine, Frame 1: all zeros (silent)
    auto sine = generateSine(256);
    std::vector<float> silent(256, 0.f);
    combined.insert(combined.end(), sine.begin(), sine.end());
    combined.insert(combined.end(), silent.begin(), silent.end());

    UserWavetable ut = loadUserWavetableFromBuffer(combined.data(), 512, 256);
    EXPECT(ut.valid, "Should load");

    auto bank = std::make_unique<WavetableBank>();
    bank->tables[0] = ut.table;
    bank->initialized = true;

    WavetableVoice voice;
    voice.currentTable = 0;
    voice.scanMode = ScanMode::Interpolated;
    voice.dacDepth = DacEmulator::BIT_16;

    float sampleRate = 44100.f;
    float freq = 440.f;

    // Position 0.0 = frame 0 (sine) — should produce output
    float sumPos0 = 0.f;
    for (int i = 0; i < 100; i++) {
        float out = voice.process(*bank, 0.f, freq, sampleRate);
        sumPos0 += std::abs(out);
    }

    // Position ~0.016 (1/63) = frame 1 (silent) — should produce ~zero
    WavetableVoice voice2;
    voice2.currentTable = 0;
    voice2.scanMode = ScanMode::Interpolated;
    voice2.dacDepth = DacEmulator::BIT_16;

    float sumPos1 = 0.f;
    float pos1 = 1.f / 63.f;  // Second frame position
    for (int i = 0; i < 100; i++) {
        float out = voice2.process(*bank, pos1, freq, sampleRate);
        sumPos1 += std::abs(out);
    }

    EXPECT(sumPos0 > sumPos1 * 2.f,
           "Position 0 (sine) should be louder than position 1/63 (silent)");
}


// ═════════════════════════════════════════════════════════
// CYCLE DETECTION TESTS
// ═════════════════════════════════════════════════════════

TEST(wtd03_detect_cycle_256) {
    int result = detectCycleLength(nullptr, 256);
    EXPECT(result == 256, "256 samples = 1 cycle of 256");
}

TEST(wtd03_detect_cycle_512) {
    // 512 = 2 × 256, so auto-detection picks 256-sample cycles
    int result = detectCycleLength(nullptr, 512);
    EXPECT(result == 256, "512 samples = 2 cycles of 256");
}

TEST(wtd03_detect_cycle_multiples_of_256) {
    // 8 * 256 = 2048 — should detect 256-sample cycles
    int result = detectCycleLength(nullptr, 2048);
    EXPECT(result == 256, "2048 samples should detect 256-sample cycles (8 frames)");
}

TEST(wtd03_detect_cycle_multiples_of_512) {
    // 4 * 512 = 2048 — but 256 is tried first and 2048/256=8 which is valid
    int result = detectCycleLength(nullptr, 2048);
    // 2048 / 256 = 8 frames (≤64), so 256 wins
    EXPECT(result == 256, "2048 = 256×8, should pick 256-sample cycles");
}

TEST(wtd03_detect_cycle_odd_size) {
    // 1000 samples — not a common size, not divisible by common sizes → single cycle
    int result = detectCycleLength(nullptr, 1000);
    EXPECT(result == 1000, "Odd size should be treated as single cycle");
}


// ═════════════════════════════════════════════════════════
// EDGE CASES
// ═════════════════════════════════════════════════════════

TEST(wtd02_empty_buffer) {
    UserWavetable ut = loadUserWavetableFromBuffer(nullptr, 0);
    EXPECT(!ut.valid, "Should fail for empty buffer");
}

TEST(wtd02_very_short_buffer) {
    // 4 samples — way too short for a normal wavetable but should still work
    float samples[4] = {0.f, 1.f, 0.f, -1.f};
    UserWavetable ut = loadUserWavetableFromBuffer(samples, 4);
    EXPECT(ut.valid, "Should load even very short buffers");
    EXPECT(ut.numFrames == 1, "Should be treated as 1 frame");
}

TEST(wtd02_dc_offset_preserved) {
    // A waveform with DC offset should preserve it
    float samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i] = 0.5f;  // Pure DC
    }
    UserWavetable ut = loadUserWavetableFromBuffer(samples, 256);
    EXPECT(ut.valid, "Should load");

    // Mip level 0 should have the DC component
    float avg = 0.f;
    for (int i = 0; i < 256; i++) {
        avg += ut.table.frames[0][0].samples[i];
    }
    avg /= 256.f;
    EXPECT_NEAR(avg, 0.5f, 0.05f, "DC offset should be preserved");
}


// ═════════════════════════════════════════════════════════
// MAIN
// ═════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    printf("\n══════════════════════════════════════════════════\n");
    printf("  Hurricane 8 — User Wavetable Tests\n");
    printf("══════════════════════════════════════════════════\n\n");

    auto& tests = getTests();
    for (auto& t : tests) {
        testsRun++;
        currentTestName = t.name;
        try {
            t.fn();
            testsPassed++;
            printf("  ✓ %s\n", t.name.c_str());
        } catch (const TestFailure& f) {
            testsFailed++;
            printf("  ✗ %s\n    FAILED: %s (line %d)\n", t.name.c_str(), f.message.c_str(), f.line);
            failures.push_back(f);
        }
    }

    printf("\n────────────────────────────────────────────────────\n");
    printf("  %d tests run, %d passed, %d failed\n\n", testsRun, testsPassed, testsFailed);

    if (!failures.empty()) {
        printf("Failures:\n");
        for (auto& f : failures) {
            printf("  - %s: %s (line %d)\n", f.testName.c_str(), f.message.c_str(), f.line);
        }
        printf("\n");
    }

    // Clean up temp files
    remove("/tmp/test_sine_256.wav");
    remove("/tmp/test_sine_float.wav");
    remove("/tmp/test_invalid.wav");
    remove("/tmp/test_accuracy.wav");
    remove("/tmp/test_single_cycle.wav");
    remove("/tmp/test_serum_style.wav");

    return testsFailed > 0 ? 1 : 0;
}
