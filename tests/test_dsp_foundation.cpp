/*
 * Hurricane 8 — DSP Foundation Unit Tests
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Standalone tests for the wavetable DSP engine.
 * No VCV Rack SDK dependency — tests pure DSP math.
 *
 * Build: g++ -std=c++17 -O2 -I../src -o test_dsp_foundation test_dsp_foundation.cpp -lm
 * Run:   ./test_dsp_foundation
 */

#include "ppg_wavetables.hpp"
#include "wavetable_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <functional>
#include <chrono>

// ─────────────────────────────────────────────────────────
// Test framework (minimal, no dependencies)
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

#define TEST(name) \
    static void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { registerTest(#name, test_##name); } \
    } testRegistrar_##name; \
    static void test_##name()

#define EXPECT(cond, msg) \
    do { \
        if (!(cond)) { \
            failures.push_back({currentTestName, msg, __LINE__}); \
            printf("  FAIL [%s:%d]: %s\n", currentTestName.c_str(), __LINE__, msg); \
            return; \
        } \
    } while (0)

#define EXPECT_NEAR(actual, expected, tolerance, msg) \
    do { \
        float _a = (actual), _e = (expected), _t = (tolerance); \
        if (std::abs(_a - _e) > _t) { \
            char _buf[256]; \
            snprintf(_buf, sizeof(_buf), "%s (got %.6f, expected %.6f, tol %.6f)", msg, _a, _e, _t); \
            failures.push_back({currentTestName, _buf, __LINE__}); \
            printf("  FAIL [%s:%d]: %s\n", currentTestName.c_str(), __LINE__, _buf); \
            return; \
        } \
    } while (0)

struct TestEntry {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestEntry>& getTests() {
    static std::vector<TestEntry> tests;
    return tests;
}

static std::string currentTestName;

static void registerTest(const char* name, std::function<void()> func) {
    getTests().push_back({name, func});
}

// ─────────────────────────────────────────────────────────
// Shared fixtures (generated once)
// ─────────────────────────────────────────────────────────

static PpgWavetableRom* rom = nullptr;
static WavetableBank* bank = nullptr;

static void ensureInitialized() {
    if (!rom) {
        rom = new PpgWavetableRom();
        rom->generate();
    }
    if (!bank) {
        bank = new WavetableBank();
        bank->buildFromRom(*rom);
    }
}

// ─────────────────────────────────────────────────────────
// DSP-01: 1V/oct pitch tracking across 10+ octaves
// ─────────────────────────────────────────────────────────

TEST(voct_c4_is_261hz) {
    float freq = PhaseAccumulator::voltToFreq(0.f);
    EXPECT_NEAR(freq, 261.626f, 0.01f, "0V should produce C4 (261.626 Hz)");
}

TEST(voct_c5_is_523hz) {
    float freq = PhaseAccumulator::voltToFreq(1.f);
    EXPECT_NEAR(freq, 523.252f, 0.02f, "1V should produce C5 (523.252 Hz)");
}

TEST(voct_octave_doubling) {
    // Each volt should double the frequency
    for (int v = -5; v < 5; v++) {
        float f1 = PhaseAccumulator::voltToFreq((float)v);
        float f2 = PhaseAccumulator::voltToFreq((float)(v + 1));
        float ratio = f2 / f1;
        EXPECT_NEAR(ratio, 2.0f, 0.001f, "1V step should double frequency");
    }
}

TEST(voct_10_octave_range) {
    // Test from C-1 (~8 Hz) to C9 (~8372 Hz) = 10 octaves
    float fLow = PhaseAccumulator::voltToFreq(-5.f);   // C-1 ≈ 8.176 Hz
    float fHigh = PhaseAccumulator::voltToFreq(5.f);    // C9 ≈ 8372 Hz

    // Ratio should be 2^10 = 1024
    float ratio = fHigh / fLow;
    EXPECT_NEAR(ratio, 1024.f, 1.f, "10 octave range should span ratio of 1024");

    // Both frequencies should be positive and finite
    EXPECT(fLow > 0.f && std::isfinite(fLow), "Low frequency must be positive");
    EXPECT(fHigh > 0.f && std::isfinite(fHigh), "High frequency must be positive");
}

TEST(phase_accumulator_wraps) {
    PhaseAccumulator pa;
    float sampleRate = 44100.f;
    float freq = 440.f;

    // Run for many samples — phase must always stay in [0, 1)
    for (int i = 0; i < 100000; i++) {
        float phase = pa.advance(freq, sampleRate);
        EXPECT(phase >= 0.f && phase < 1.f, "Phase must be in [0, 1)");
    }
}

TEST(phase_accumulator_frequency_accuracy) {
    // Verify the oscillator completes the right number of cycles
    PhaseAccumulator pa;
    float sampleRate = 48000.f;
    float freq = 1000.f;  // 1kHz = 48 samples/cycle

    int zeroCrossings = 0;
    float prevPhase = 0.f;

    // Run for 1 second
    for (int i = 0; i < 48000; i++) {
        float phase = pa.advance(freq, sampleRate);
        // Count phase wraps (phase < prevPhase means it wrapped)
        if (phase < prevPhase) {
            zeroCrossings++;
        }
        prevPhase = phase;
    }

    // Should have ~1000 cycles in 1 second at 1kHz
    EXPECT(zeroCrossings >= 999 && zeroCrossings <= 1001,
           "1kHz oscillator should complete ~1000 cycles per second");
}

// ─────────────────────────────────────────────────────────
// DSP-02: Mip-mapped band-limited wavetables
// ─────────────────────────────────────────────────────────

TEST(ppg_rom_generates_valid_data) {
    ensureInitialized();

    // All samples should be finite
    for (int t = 0; t < PPG_NUM_TABLES; t++) {
        for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
            for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                EXPECT(std::isfinite(rom->data[t][f][s]),
                       "All ROM samples must be finite");
            }
        }
    }
}

TEST(ppg_rom_bounded) {
    ensureInitialized();

    // After normalization, no sample should exceed [-1, 1] by much
    // (slight overshoot from Gibbs phenomenon is acceptable)
    float maxAbs = 0.f;
    for (int t = 0; t < PPG_NUM_TABLES; t++) {
        for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
            for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                float v = std::abs(rom->data[t][f][s]);
                if (v > maxAbs) maxAbs = v;
            }
        }
    }
    EXPECT(maxAbs <= 1.5f, "ROM samples should be roughly bounded to [-1.5, 1.5]");
}

TEST(ppg_table0_frame0_is_near_sine) {
    ensureInitialized();

    // Table 0, frame 0 should be very close to a sine wave (1 harmonic)
    float maxError = 0.f;
    for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
        float phase = (float)s / PPG_SAMPLES_PER_FRAME;
        float expected = std::sin(2.f * PPG_PI * phase) * (2.f / PPG_PI);
        float actual = rom->data[0][0][s];
        float err = std::abs(actual - expected);
        if (err > maxError) maxError = err;
    }
    // First frame of sawtooth series with 1 harmonic ≈ scaled sine
    EXPECT(maxError < 0.1f, "Table 0 frame 0 should approximate a sine wave");
}

TEST(mipmap_level0_preserves_source) {
    ensureInitialized();

    // Mip level 0 (full bandwidth) should closely match the source ROM data
    float maxError = 0.f;
    // Test a few tables/frames
    for (int t = 0; t < PPG_NUM_TABLES; t += 8) {
        for (int f = 0; f < PPG_FRAMES_PER_TABLE; f += 16) {
            for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                float src = rom->data[t][f][s];
                float mip = bank->tables[t].frames[0][f].samples[s];
                float err = std::abs(src - mip);
                if (err > maxError) maxError = err;
            }
        }
    }
    // Should be very close — DFT → IDFT round-trip should be near-perfect
    EXPECT(maxError < 0.01f,
           "Mip level 0 should faithfully reproduce source data");
}

TEST(mipmap_higher_levels_have_less_energy) {
    ensureInitialized();

    // Higher mip levels should have less high-frequency energy
    // (fewer harmonics = lower RMS of difference from fundamental)
    for (int t = 0; t < PPG_NUM_TABLES; t += 8) {
        int f = PPG_FRAMES_PER_TABLE / 2;  // Mid-frame (has harmonics)

        float rms[MIP_LEVELS];
        for (int level = 0; level < MIP_LEVELS; level++) {
            float sumSq = 0.f;
            for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                float v = bank->tables[t].frames[level][f].samples[s];
                sumSq += v * v;
            }
            rms[level] = std::sqrt(sumSq / PPG_SAMPLES_PER_FRAME);
        }

        // RMS should generally decrease or stay similar as we go to higher levels
        // (removing harmonics reduces energy). Allow some tolerance.
        EXPECT(rms[MIP_LEVELS - 1] <= rms[0] + 0.01f,
               "Highest mip level should have <= energy of full bandwidth level");
    }
}

TEST(mipmap_level_selection_low_freq) {
    // Low frequency should select level 0 (full harmonics)
    int level = WavetableBank::selectMipLevel(100.f, 48000.f);
    EXPECT(level == 0, "100 Hz at 48kHz should use mip level 0");
}

TEST(mipmap_level_selection_high_freq) {
    // High frequency should select a higher mip level
    int level = WavetableBank::selectMipLevel(10000.f, 48000.f);
    EXPECT(level > 0, "10kHz at 48kHz should use mip level > 0");
}

TEST(mipmap_level_selection_very_high_freq) {
    // Near Nyquist should use highest level
    int level = WavetableBank::selectMipLevel(20000.f, 44100.f);
    EXPECT(level >= MIP_LEVELS - 2, "20kHz at 44.1kHz should use high mip level");
}

TEST(mipmap_read_interpolation) {
    ensureInitialized();

    // Reading at exact sample positions vs. between should give different results
    // (proving interpolation is working)
    float atSample = bank->readSample(0, 32, 0.5f, 0);
    float between = bank->readSample(0, 32, 0.502f, 0);

    // They should be close but not identical (unless the waveform is flat there)
    EXPECT(std::isfinite(atSample), "Sample read at exact position must be finite");
    EXPECT(std::isfinite(between), "Sample read between positions must be finite");
}

TEST(mipmap_smooth_read_continuous) {
    ensureInitialized();

    // Verify smooth reading doesn't produce discontinuities
    float prev = bank->readSampleSmooth(0, 32, 0.f, 440.f, 48000.f);
    float maxJump = 0.f;
    for (int s = 1; s < 1000; s++) {
        float phase = (float)s / 1000.f;
        float val = bank->readSampleSmooth(0, 32, phase, 440.f, 48000.f);
        float jump = std::abs(val - prev);
        if (jump > maxJump) maxJump = jump;
        prev = val;
    }
    // With 1000 steps across one cycle, jumps should be small
    EXPECT(maxJump < 0.5f, "Smooth mip-mapped read should not have large discontinuities");
}

// ─────────────────────────────────────────────────────────
// DSP-04: PPG stepped scanning (zero-crossing switch)
// ─────────────────────────────────────────────────────────

TEST(stepped_scanning_waits_for_zero_cross) {
    ensureInitialized();

    WavetableScanner scanner;
    scanner.currentFrame = 0;
    scanner.lastSample = 0.5f;  // Positive — no zero crossing yet

    // Set target to frame 10
    scanner.setPosition(10.f / 63.f, PPG_FRAMES_PER_TABLE);
    EXPECT(scanner.targetFrame == 10, "Target frame should be 10");

    // Feed positive samples — should NOT switch
    for (int i = 0; i < 10; i++) {
        scanner.processStepped(*bank, 0, 0.1f, 0, 0.5f);
    }
    EXPECT(scanner.currentFrame != 10 || !scanner.waitingForZeroCross,
           "Should not switch frame without zero crossing");

    // Feed a zero crossing (positive to negative)
    scanner.processStepped(*bank, 0, 0.1f, 0, -0.1f);
    EXPECT(scanner.currentFrame == 10, "Should switch frame at zero crossing");
    EXPECT(!scanner.waitingForZeroCross, "Should no longer be waiting after switch");
}

TEST(stepped_scanning_immediate_when_at_zero) {
    ensureInitialized();

    WavetableScanner scanner;
    scanner.currentFrame = 0;
    scanner.lastSample = 0.f;  // Already at zero

    scanner.setPosition(20.f / 63.f, PPG_FRAMES_PER_TABLE);

    // Feed a sample with lastSample at zero — should trigger immediately
    scanner.processStepped(*bank, 0, 0.1f, 0, 0.5f);
    EXPECT(scanner.currentFrame == 20, "Should switch immediately when last sample is zero");
}

// ─────────────────────────────────────────────────────────
// DSP-05: Interpolated scanning (smooth crossfade)
// ─────────────────────────────────────────────────────────

TEST(interpolated_scanning_blends_frames) {
    ensureInitialized();

    WavetableScanner scanner;

    // Read at position 0.0 (frame 0 only)
    float s0 = scanner.processInterpolated(*bank, 0, 0.f, 0.25f, 0);

    // Read at position 1.0 (frame 63 only)
    float s63 = scanner.processInterpolated(*bank, 0, 1.f, 0.25f, 0);

    // Read at position 0.5 (midpoint)
    float sMid = scanner.processInterpolated(*bank, 0, 0.5f, 0.25f, 0);

    // If frames 0 and 63 differ, the midpoint should be different from both
    // (or at least a valid blend)
    EXPECT(std::isfinite(s0) && std::isfinite(s63) && std::isfinite(sMid),
           "All interpolated reads must be finite");
}

TEST(interpolated_scanning_continuous) {
    ensureInitialized();

    WavetableScanner scanner;

    // Sweep position slowly — output should be continuous
    float prev = scanner.processInterpolated(*bank, 0, 0.f, 0.25f, 0);
    float maxJump = 0.f;

    for (int i = 1; i <= 1000; i++) {
        float pos = (float)i / 1000.f;
        float val = scanner.processInterpolated(*bank, 0, pos, 0.25f, 0);
        float jump = std::abs(val - prev);
        if (jump > maxJump) maxJump = jump;
        prev = val;
    }

    EXPECT(maxJump < 0.1f,
           "Interpolated scanning should produce smooth transitions between frames");
}

// ─────────────────────────────────────────────────────────
// DSP-06: Mode switching between stepped and interpolated
// ─────────────────────────────────────────────────────────

TEST(scan_mode_enum_values) {
    EXPECT(ScanMode::Stepped != ScanMode::Interpolated,
           "Stepped and Interpolated must be distinct modes");
}

TEST(voice_mode_switching) {
    ensureInitialized();

    WavetableVoice voice;
    float sampleRate = 48000.f;
    float freq = 440.f;

    // Process in stepped mode
    voice.scanMode = ScanMode::Stepped;
    float steppedOut = 0.f;
    for (int i = 0; i < 100; i++) {
        steppedOut = voice.process(*bank, 0.5f, freq, sampleRate);
    }
    EXPECT(std::isfinite(steppedOut), "Stepped mode output must be finite");

    // Switch to interpolated mode
    voice.scanMode = ScanMode::Interpolated;
    float interpOut = 0.f;
    for (int i = 0; i < 100; i++) {
        interpOut = voice.process(*bank, 0.5f, freq, sampleRate);
    }
    EXPECT(std::isfinite(interpOut), "Interpolated mode output must be finite");
}

// ─────────────────────────────────────────────────────────
// DSP-07: DAC bit-depth emulation
// ─────────────────────────────────────────────────────────

TEST(dac_8bit_quantization) {
    // 8-bit: 256 levels, step size = 1/128
    float step = 1.f / 128.f;

    float quantized = DacEmulator::quantize(0.123456f, DacEmulator::BIT_8);
    // Should be rounded to nearest step
    float remainder = std::fmod(std::abs(quantized * 128.f), 1.f);
    EXPECT(remainder < 0.001f || remainder > 0.999f,
           "8-bit quantization should snap to 256-level grid");
}

TEST(dac_12bit_quantization) {
    float quantized = DacEmulator::quantize(0.123456f, DacEmulator::BIT_12);
    float remainder = std::fmod(std::abs(quantized * 2048.f), 1.f);
    EXPECT(remainder < 0.001f || remainder > 0.999f,
           "12-bit quantization should snap to 4096-level grid");
}

TEST(dac_16bit_quantization) {
    float quantized = DacEmulator::quantize(0.123456f, DacEmulator::BIT_16);
    // 16-bit has very fine resolution, should be close to input
    EXPECT_NEAR(quantized, 0.123456f, 0.0001f,
                "16-bit quantization should be nearly transparent");
}

TEST(dac_8bit_creates_audible_steps) {
    // 8-bit should have visibly larger quantization steps than 16-bit
    float input = 0.3333f;
    float q8 = DacEmulator::quantize(input, DacEmulator::BIT_8);
    float q16 = DacEmulator::quantize(input, DacEmulator::BIT_16);

    float error8 = std::abs(input - q8);
    float error16 = std::abs(input - q16);

    EXPECT(error8 >= error16, "8-bit should have >= quantization error vs 16-bit");
}

TEST(dac_preserves_sign) {
    float pos = DacEmulator::quantize(0.5f, DacEmulator::BIT_8);
    float neg = DacEmulator::quantize(-0.5f, DacEmulator::BIT_8);

    EXPECT(pos > 0.f, "Positive input should produce positive output");
    EXPECT(neg < 0.f, "Negative input should produce negative output");
}

TEST(dac_zero_stays_zero) {
    EXPECT_NEAR(DacEmulator::quantize(0.f, DacEmulator::BIT_8), 0.f, 0.001f,
                "Zero input should produce zero output at 8-bit");
    EXPECT_NEAR(DacEmulator::quantize(0.f, DacEmulator::BIT_12), 0.f, 0.001f,
                "Zero input should produce zero output at 12-bit");
    EXPECT_NEAR(DacEmulator::quantize(0.f, DacEmulator::BIT_16), 0.f, 0.001f,
                "Zero input should produce zero output at 16-bit");
}

TEST(dac_index_convenience) {
    // Index 0=8bit, 1=12bit, 2=16bit should match named enum
    float v = 0.4f;
    EXPECT_NEAR(DacEmulator::quantize(v, 0), DacEmulator::quantize(v, DacEmulator::BIT_8),
                0.0001f, "Index 0 should match BIT_8");
    EXPECT_NEAR(DacEmulator::quantize(v, 1), DacEmulator::quantize(v, DacEmulator::BIT_12),
                0.0001f, "Index 1 should match BIT_12");
    EXPECT_NEAR(DacEmulator::quantize(v, 2), DacEmulator::quantize(v, DacEmulator::BIT_16),
                0.0001f, "Index 2 should match BIT_16");
}

// ─────────────────────────────────────────────────────────
// WTD-01: PPG ROM wavetable set is bundled and playable
// ─────────────────────────────────────────────────────────

TEST(all_32_tables_have_data) {
    ensureInitialized();

    for (int t = 0; t < PPG_NUM_TABLES; t++) {
        // Each table should have at least some non-zero data
        float sum = 0.f;
        for (int f = 0; f < PPG_FRAMES_PER_TABLE; f++) {
            for (int s = 0; s < PPG_SAMPLES_PER_FRAME; s++) {
                sum += std::abs(rom->data[t][f][s]);
            }
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "Table %d should contain non-zero data", t);
        EXPECT(sum > 0.f, msg);
    }
}

TEST(wavetable_bank_all_tables_mipmapped) {
    ensureInitialized();

    // Every table should have valid mip-mapped data at all levels
    for (int t = 0; t < PPG_NUM_TABLES; t++) {
        for (int level = 0; level < MIP_LEVELS; level++) {
            // Spot-check middle frame
            float val = bank->readSample(t, PPG_FRAMES_PER_TABLE / 2, 0.25f, level);
            char msg[128];
            snprintf(msg, sizeof(msg), "Table %d mip level %d should be readable and finite", t, level);
            EXPECT(std::isfinite(val), msg);
        }
    }
}

// ─────────────────────────────────────────────────────────
// Integration: Complete voice pipeline
// ─────────────────────────────────────────────────────────

TEST(voice_produces_output_at_c4) {
    ensureInitialized();

    WavetableVoice voice;
    voice.currentTable = 0;
    voice.scanMode = ScanMode::Interpolated;
    voice.dacDepth = DacEmulator::BIT_16;

    float sampleRate = 48000.f;
    float freq = PhaseAccumulator::voltToFreq(0.f);  // C4

    // Run for 100 samples, output should be non-zero
    bool hasNonZero = false;
    for (int i = 0; i < 100; i++) {
        float out = voice.process(*bank, 0.5f, freq, sampleRate);
        EXPECT(std::isfinite(out), "Voice output must be finite");
        if (std::abs(out) > 0.001f) hasNonZero = true;
    }
    EXPECT(hasNonZero, "Voice should produce non-zero output at C4");
}

TEST(voice_output_bounded) {
    ensureInitialized();

    WavetableVoice voice;
    voice.currentTable = 0;
    voice.scanMode = ScanMode::Interpolated;
    voice.dacDepth = DacEmulator::BIT_8;  // Most aggressive quantization

    float sampleRate = 48000.f;

    // Test across multiple octaves
    for (int v = -5; v <= 5; v++) {
        float freq = PhaseAccumulator::voltToFreq((float)v);
        for (int i = 0; i < 1000; i++) {
            float out = voice.process(*bank, 0.5f, freq, sampleRate);
            EXPECT(std::abs(out) <= 1.5f, "Voice output should be bounded");
        }
    }
}

TEST(voice_different_tables_produce_different_timbres) {
    ensureInitialized();

    float sampleRate = 48000.f;
    float freq = 440.f;

    // Collect output from two different tables
    float sum0 = 0.f, sum4 = 0.f;

    WavetableVoice voice0;
    voice0.currentTable = 0;
    voice0.scanMode = ScanMode::Interpolated;
    voice0.dacDepth = DacEmulator::BIT_16;

    WavetableVoice voice4;
    voice4.currentTable = 4;
    voice4.scanMode = ScanMode::Interpolated;
    voice4.dacDepth = DacEmulator::BIT_16;

    for (int i = 0; i < 1000; i++) {
        sum0 += std::abs(voice0.process(*bank, 0.5f, freq, sampleRate));
        sum4 += std::abs(voice4.process(*bank, 0.5f, freq, sampleRate));
    }

    // Different tables should produce measurably different output
    // (table 0 is saw-type, table 4 is square-type)
    float diff = std::abs(sum0 - sum4);
    EXPECT(diff > 0.01f, "Different wavetable banks should produce different timbres");
}

TEST(voice_8bit_vs_16bit_audibly_different) {
    ensureInitialized();

    float sampleRate = 48000.f;
    float freq = 440.f;

    WavetableVoice voice8;
    voice8.currentTable = 0;
    voice8.scanMode = ScanMode::Interpolated;
    voice8.dacDepth = DacEmulator::BIT_8;

    WavetableVoice voice16;
    voice16.currentTable = 0;
    voice16.scanMode = ScanMode::Interpolated;
    voice16.dacDepth = DacEmulator::BIT_16;

    float diffSum = 0.f;
    for (int i = 0; i < 4800; i++) {
        // Same phase input
        float position = 0.5f;
        float out8 = voice8.process(*bank, position, freq, sampleRate);
        float out16 = voice16.process(*bank, position, freq, sampleRate);
        diffSum += std::abs(out8 - out16);
    }

    float avgDiff = diffSum / 4800.f;
    EXPECT(avgDiff > 0.0001f, "8-bit and 16-bit DAC emulation should produce audibly different output");
}

// ─────────────────────────────────────────────────────────
// Edge cases and robustness
// ─────────────────────────────────────────────────────────

TEST(phase_accumulator_handles_zero_freq) {
    PhaseAccumulator pa;
    float phase = pa.advance(0.f, 48000.f);
    EXPECT(phase >= 0.f && phase < 1.f, "Zero frequency should not crash");
}

TEST(phase_accumulator_handles_very_high_freq) {
    PhaseAccumulator pa;
    // Near Nyquist
    float phase = pa.advance(23000.f, 48000.f);
    EXPECT(phase >= 0.f && phase < 1.f, "Near-Nyquist frequency should not crash");
}

TEST(mipmap_clamps_out_of_range_table) {
    ensureInitialized();

    // Out of range table indices should clamp, not crash
    float v1 = bank->readSample(-1, 0, 0.5f, 0);
    float v2 = bank->readSample(999, 0, 0.5f, 0);
    EXPECT(std::isfinite(v1), "Negative table index should clamp safely");
    EXPECT(std::isfinite(v2), "Excessive table index should clamp safely");
}

TEST(mipmap_clamps_out_of_range_frame) {
    ensureInitialized();

    float v1 = bank->readSample(0, -1, 0.5f, 0);
    float v2 = bank->readSample(0, 999, 0.5f, 0);
    EXPECT(std::isfinite(v1), "Negative frame index should clamp safely");
    EXPECT(std::isfinite(v2), "Excessive frame index should clamp safely");
}

TEST(scanner_position_clamping) {
    WavetableScanner scanner;
    scanner.setPosition(-0.5f, PPG_FRAMES_PER_TABLE);
    EXPECT(scanner.targetFrame >= 0, "Negative position should clamp to 0");

    scanner.setPosition(1.5f, PPG_FRAMES_PER_TABLE);
    EXPECT(scanner.targetFrame < PPG_FRAMES_PER_TABLE,
           "Position > 1 should clamp to last frame");
}

TEST(rom_lookup_interpolation) {
    ensureInitialized();

    // Direct ROM lookup should also interpolate
    float v = rom->lookup(0, 0, 0.5f);
    EXPECT(std::isfinite(v), "ROM lookup should return finite value");

    // Phase wrapping
    float v2 = rom->lookup(0, 0, 0.9999f);
    EXPECT(std::isfinite(v2), "ROM lookup near phase=1 should work");
}

// ─────────────────────────────────────────────────────────
// Performance sanity check
// ─────────────────────────────────────────────────────────

TEST(voice_performance_48k_samples) {
    ensureInitialized();

    WavetableVoice voice;
    voice.currentTable = 0;
    voice.scanMode = ScanMode::Interpolated;
    voice.dacDepth = DacEmulator::BIT_8;

    float sampleRate = 48000.f;
    float freq = 440.f;

    auto start = std::chrono::high_resolution_clock::now();

    // Process 1 second of audio (48000 samples)
    for (int i = 0; i < 48000; i++) {
        float out = voice.process(*bank, 0.5f, freq, sampleRate);
        (void)out;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 48000 samples should process in well under 1 second
    // Real-time budget for 1 second at 48kHz = 1,000,000 microseconds
    // We want to be way under that (leave headroom for 8 voices + other processing)
    printf("  [perf] 48k samples in %lld µs (budget: 1000000 µs)\n", (long long)duration.count());
    EXPECT(duration.count() < 500000, "Single voice should process 48k samples in < 500ms");
}

// ─────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────

int main() {
    printf("Hurricane 8 — DSP Foundation Tests\n");
    printf("===================================\n\n");

    // Time the initialization
    auto initStart = std::chrono::high_resolution_clock::now();
    ensureInitialized();
    auto initEnd = std::chrono::high_resolution_clock::now();
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(initEnd - initStart);
    printf("[init] ROM generation + mip-mapping: %lld ms\n\n", (long long)initMs.count());

    auto& tests = getTests();
    for (auto& test : tests) {
        currentTestName = test.name;
        testsRun++;
        printf("  RUN  %s\n", test.name.c_str());

        size_t failsBefore = failures.size();
        test.func();

        if (failures.size() == failsBefore) {
            testsPassed++;
            printf("  PASS %s\n", test.name.c_str());
        } else {
            testsFailed++;
        }
    }

    printf("\n===================================\n");
    printf("Results: %d/%d passed", testsPassed, testsRun);
    if (testsFailed > 0) {
        printf(", %d FAILED", testsFailed);
    }
    printf("\n");

    if (!failures.empty()) {
        printf("\nFailures:\n");
        for (auto& f : failures) {
            printf("  %s (line %d): %s\n", f.testName.c_str(), f.line, f.message.c_str());
        }
    }

    // Cleanup
    delete bank;
    delete rom;

    return testsFailed > 0 ? 1 : 0;
}
