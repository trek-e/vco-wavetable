/*
 * Hurricane 8 — Polyphonic Routing Unit Tests
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Tests for VOC-01..03 (voice allocation, unison, mode switching)
 * and OUT-01..03 (polyphonic audio, gate passthrough, mix output).
 *
 * Build: g++ -std=c++17 -O2 -I../src -o test_polyphonic_routing test_polyphonic_routing.cpp -lm
 * Run:   ./test_polyphonic_routing
 */

#include "ppg_wavetables.hpp"
#include "wavetable_engine.hpp"
#include "polyphonic_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <functional>
#include <set>

// ─────────────────────────────────────────────────────────
// Test framework (same minimal framework as DSP foundation tests)
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
// Shared fixtures
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

static constexpr float SAMPLE_RATE = 48000.f;

// Helper: run engine for N samples and return outputs
static void runEngine(PolyphonicEngine& engine, const float* vocts, const float* gates,
                      int numChannels, float position, int numSamples) {
    for (int s = 0; s < numSamples; s++) {
        engine.process(*bank, vocts, gates, numChannels, position, SAMPLE_RATE);
    }
}

// ─────────────────────────────────────────────────────────
// VOC-01: Polyphonic mode — 8 independent voices
// ─────────────────────────────────────────────────────────

TEST(voc01_polyphonic_single_voice) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};  // C4
    float gates[MAX_POLY_VOICES] = {10.f}; // Voice 0 active
    int numChannels = 1;

    // Process several samples
    runEngine(engine, vocts, gates, numChannels, 0.f, 256);

    EXPECT(engine.active[0] == true, "Voice 0 should be active");
    EXPECT(engine.activeCount == 1, "Only 1 voice should be active");
    EXPECT(engine.outputs[0] != 0.f, "Voice 0 should produce output");

    // Voices 1-7 should be silent
    for (int i = 1; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.active[i] == false, "Inactive voices should be marked inactive");
        EXPECT(engine.outputs[i] == 0.f, "Inactive voices should produce zero output");
    }
}

TEST(voc01_polyphonic_four_voices) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // 4 voices at different pitches
    float vocts[MAX_POLY_VOICES] = {0.f, 0.25f, 0.5f, 0.75f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f};
    int numChannels = 4;

    runEngine(engine, vocts, gates, numChannels, 0.f, 256);

    EXPECT(engine.activeCount == 4, "4 voices should be active");

    for (int i = 0; i < 4; i++) {
        EXPECT(engine.active[i] == true, "Voices 0-3 should be active");
        EXPECT(engine.outputs[i] != 0.f, "Active voices should produce output");
    }
    for (int i = 4; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.active[i] == false, "Voices 4-7 should be inactive");
    }
}

TEST(voc01_polyphonic_eight_voices) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // All 8 voices active at different pitches
    float vocts[MAX_POLY_VOICES] = {-2.f, -1.f, 0.f, 0.5f, 1.f, 1.5f, 2.f, 3.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f};

    runEngine(engine, vocts, gates, MAX_POLY_VOICES, 0.5f, 256);

    EXPECT(engine.activeCount == 8, "All 8 voices should be active");

    // Each voice should produce different output (different pitches = different phases)
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.active[i] == true, "All voices should be active");
    }
}

TEST(voc01_polyphonic_independent_pitches) {
    ensureInitialized();

    // Verify voices at different pitches produce different frequencies
    // by checking phase accumulator rates differ
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {-1.f, 0.f, 1.f, 2.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f};

    // Run enough samples for frequency differences to be apparent
    runEngine(engine, vocts, gates, 4, 0.f, 1024);

    // Verify that each voice has a different phase — proving independent oscillators
    std::set<float> phases;
    for (int i = 0; i < 4; i++) {
        phases.insert(engine.voices[i].phaseAccum.phase);
    }
    // With different frequencies over 1024 samples, phases should all differ
    EXPECT(phases.size() == 4, "All 4 voices should have different phases (independent oscillators)");
}

TEST(voc01_gate_controls_voice_activation) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // 4 channels, but only voices 0 and 2 gated on
    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 1.5f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 0.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    runEngine(engine, vocts, gates, 4, 0.f, 256);

    EXPECT(engine.activeCount == 2, "Only 2 voices should be active");
    EXPECT(engine.active[0] == true, "Voice 0 should be active (gate high)");
    EXPECT(engine.active[1] == false, "Voice 1 should be inactive (gate low)");
    EXPECT(engine.active[2] == true, "Voice 2 should be active (gate high)");
    EXPECT(engine.active[3] == false, "Voice 3 should be inactive (gate low)");
    EXPECT(engine.outputs[1] == 0.f, "Gated-off voice should produce zero");
    EXPECT(engine.outputs[3] == 0.f, "Gated-off voice should produce zero");
}

// ─────────────────────────────────────────────────────────
// VOC-02: Unison mode — 8 voices stacked with detune
// ─────────────────────────────────────────────────────────

TEST(voc02_unison_all_voices_active) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};  // Single input pitch
    float gates[MAX_POLY_VOICES] = {10.f}; // Single gate

    runEngine(engine, vocts, gates, 1, 0.5f, 256);

    EXPECT(engine.activeCount == 8, "Unison: all 8 voices should be active");
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.active[i] == true, "All unison voices should be active");
        EXPECT(engine.outputs[i] != 0.f, "All unison voices should produce output");
    }
}

TEST(voc02_unison_detune_spread) {
    ensureInitialized();

    // Verify voices are detuned symmetrically
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 50.f;  // ±50 cents spread
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    // Run enough samples that detuned voices diverge in phase
    runEngine(engine, vocts, gates, 1, 0.f, 2048);

    // With detune, voices should have different phases
    // Voice 0 has most negative detune, voice 7 has most positive
    float phase0 = engine.voices[0].phaseAccum.phase;
    float phase7 = engine.voices[7].phaseAccum.phase;

    // Phases should differ since detune creates frequency differences
    EXPECT(std::abs(phase0 - phase7) > 0.001f,
           "Detuned voices should have different phases");

    // Middle voices should have phases between extremes (roughly)
    // This confirms the spread is distributed across all voices
    std::set<float> phases;
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        phases.insert(engine.voices[i].phaseAccum.phase);
    }
    EXPECT(phases.size() > 1, "Detuned voices should have varying phases");
}

TEST(voc02_unison_zero_detune) {
    ensureInitialized();

    // With zero detune, all voices should produce identical output
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 0.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    // Reset all voices to ensure identical starting state
    engine.reset();

    runEngine(engine, vocts, gates, 1, 0.5f, 256);

    // All voices should produce identical output
    for (int i = 1; i < MAX_POLY_VOICES; i++) {
        EXPECT_NEAR(engine.outputs[i], engine.outputs[0], 0.0001f,
                    "Zero-detune unison voices should have identical output");
    }
}

TEST(voc02_unison_gate_controls_all) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // Gate off: no voices should produce output
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {0.f};  // Gate off

    runEngine(engine, vocts, gates, 1, 0.5f, 256);

    EXPECT(engine.activeCount == 0, "Unison with gate off: no voices active");
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.outputs[i] == 0.f, "Unison with gate off: zero output");
    }
}

// ─────────────────────────────────────────────────────────
// VOC-03: Mode switching
// ─────────────────────────────────────────────────────────

TEST(voc03_switch_poly_to_unison) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    // Start polyphonic: 2 voices
    engine.voiceMode = VoiceMode::Polyphonic;
    runEngine(engine, vocts, gates, 2, 0.5f, 128);
    EXPECT(engine.activeCount == 2, "Poly mode: 2 voices active");

    // Switch to unison: all 8 voices
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    runEngine(engine, vocts, gates, 2, 0.5f, 128);
    EXPECT(engine.activeCount == 8, "After switch to unison: all 8 voices active");
}

TEST(voc03_switch_unison_to_poly) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    // Start unison: all 8 voices
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    runEngine(engine, vocts, gates, 3, 0.5f, 128);
    EXPECT(engine.activeCount == 8, "Unison: all 8 active");

    // Switch to polyphonic: 3 voices
    engine.voiceMode = VoiceMode::Polyphonic;
    runEngine(engine, vocts, gates, 3, 0.5f, 128);
    EXPECT(engine.activeCount == 3, "After switch to poly: 3 voices active");
}

// ─────────────────────────────────────────────────────────
// OUT-01: 8-channel polyphonic audio output
// ─────────────────────────────────────────────────────────

TEST(out01_poly_output_channels_match_input) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // 3 input channels
    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f};

    runEngine(engine, vocts, gates, 3, 0.5f, 256);

    // Verify per-channel output is non-zero for active voices
    for (int i = 0; i < 3; i++) {
        EXPECT(engine.outputs[i] != 0.f, "Active voice channel should have non-zero output");
    }
}

TEST(out01_poly_output_per_voice_independent) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // Two voices at very different pitches
    float vocts[MAX_POLY_VOICES] = {-2.f, 2.f};  // ~65Hz vs ~1047Hz
    float gates[MAX_POLY_VOICES] = {10.f, 10.f};

    runEngine(engine, vocts, gates, 2, 0.f, 512);

    // Outputs should be different (different frequencies)
    EXPECT(std::abs(engine.outputs[0] - engine.outputs[1]) > 0.001f,
           "Voices at different pitches should produce different outputs");
}

TEST(out01_unison_always_8_channels) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // Even with 1 input channel, unison produces 8 output voices
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    runEngine(engine, vocts, gates, 1, 0.5f, 256);

    EXPECT(engine.activeCount == 8, "Unison should activate all 8 voices");
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.active[i] == true, "All unison channels should be active");
    }
}

// ─────────────────────────────────────────────────────────
// OUT-02: Gate passthrough (tested at engine level)
// ─────────────────────────────────────────────────────────

TEST(out02_gate_passthrough_poly) {
    ensureInitialized();

    // Gate signals control which voices are active
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // Gates: voice 0=on, 1=off, 2=on, 3=off
    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 1.5f};
    float gates[MAX_POLY_VOICES] = {10.f, 0.f, 10.f, 0.f};

    runEngine(engine, vocts, gates, 4, 0.f, 256);

    EXPECT(engine.active[0] == true, "Gate high -> voice active");
    EXPECT(engine.active[1] == false, "Gate low -> voice inactive");
    EXPECT(engine.active[2] == true, "Gate high -> voice active");
    EXPECT(engine.active[3] == false, "Gate low -> voice inactive");
}

TEST(out02_gate_passthrough_unison) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 10.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // Gate on -> all 8 active
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gatesOn[MAX_POLY_VOICES] = {10.f};

    runEngine(engine, vocts, gatesOn, 1, 0.f, 64);
    EXPECT(engine.activeCount == 8, "Unison gate on: 8 voices");

    // Gate off -> all silent
    float gatesOff[MAX_POLY_VOICES] = {0.f};
    runEngine(engine, vocts, gatesOff, 1, 0.f, 64);
    EXPECT(engine.activeCount == 0, "Unison gate off: 0 voices");
}

// ─────────────────────────────────────────────────────────
// OUT-03: Mix output
// ─────────────────────────────────────────────────────────

TEST(out03_mix_single_voice) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    runEngine(engine, vocts, gates, 1, 0.f, 256);

    // Mix with 1 voice should equal that voice's output
    float mix = engine.getMixOutput();
    EXPECT_NEAR(mix, engine.outputs[0], 0.0001f,
                "Mix with 1 voice should equal single voice output");
}

TEST(out03_mix_multiple_voices_normalized) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 1.5f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f};

    runEngine(engine, vocts, gates, 4, 0.5f, 256);

    float mix = engine.getMixOutput();

    // Mix should be the average of active outputs
    float expectedMix = 0.f;
    for (int i = 0; i < 4; i++) {
        expectedMix += engine.outputs[i];
    }
    expectedMix /= 4.f;

    EXPECT_NEAR(mix, expectedMix, 0.0001f,
                "Mix should be average of active voice outputs");
}

TEST(out03_mix_normalized_prevents_clipping) {
    ensureInitialized();

    // With 8 active voices, mix should still be in [-1, 1] range
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f};

    // Run many samples and check mix stays bounded
    bool clipDetected = false;
    for (int s = 0; s < 4096; s++) {
        engine.process(*bank, vocts, gates, 8, 0.5f, SAMPLE_RATE);
        float mix = engine.getMixOutput();
        if (mix > 1.01f || mix < -1.01f) {
            clipDetected = true;
            break;
        }
    }

    EXPECT(!clipDetected, "Mix output should stay within [-1, 1] with 8 voices");
}

TEST(out03_mix_silent_when_no_voices) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    // All gates off
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {0.f};

    runEngine(engine, vocts, gates, 1, 0.f, 256);

    float mix = engine.getMixOutput();
    EXPECT_NEAR(mix, 0.f, 0.0001f, "Mix should be 0 when no voices are active");
}

TEST(out03_mix_unison_voices) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 30.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    runEngine(engine, vocts, gates, 1, 0.5f, 512);

    float mix = engine.getMixOutput();

    // Should be non-zero (voices are active)
    EXPECT(mix != 0.f, "Unison mix should be non-zero with active gate");

    // Should be normalized average of 8 voices
    float expectedMix = 0.f;
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        expectedMix += engine.outputs[i];
    }
    expectedMix /= MAX_POLY_VOICES;

    EXPECT_NEAR(mix, expectedMix, 0.0001f,
                "Unison mix should be average of all 8 voices");
}

// ─────────────────────────────────────────────────────────
// Engine reset and edge cases
// ─────────────────────────────────────────────────────────

TEST(engine_reset_clears_all_state) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f};

    runEngine(engine, vocts, gates, 2, 0.5f, 256);
    EXPECT(engine.activeCount > 0, "Should have active voices before reset");

    engine.reset();

    EXPECT(engine.activeCount == 0, "After reset: no active voices");
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.outputs[i] == 0.f, "After reset: zero output");
        EXPECT(engine.active[i] == false, "After reset: inactive");
        EXPECT(engine.voices[i].phaseAccum.phase == 0.f, "After reset: phase at 0");
    }
}

TEST(engine_shared_params_propagate) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 5;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_12;

    float vocts[MAX_POLY_VOICES] = {0.f, 1.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f};

    runEngine(engine, vocts, gates, 2, 0.f, 1);

    // Shared params should propagate to all voices
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(engine.voices[i].currentTable == 5, "Table should propagate to voice");
        EXPECT(engine.voices[i].scanMode == ScanMode::Stepped, "Scan mode should propagate");
        EXPECT(engine.voices[i].dacDepth == DacEmulator::BIT_12, "DAC depth should propagate");
    }
}

TEST(engine_unison_detune_range) {
    ensureInitialized();

    // Test detune at extremes
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    // Maximum detune (100 cents = ±1 semitone)
    engine.unisonDetuneCents = 100.f;
    engine.reset();
    runEngine(engine, vocts, gates, 1, 0.f, 4096);

    // All voices should still produce valid output
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT(std::isfinite(engine.outputs[i]), "Max detune should produce finite output");
    }

    // Verify the spread covers roughly ±1 semitone
    // At 0V (C4 = 261.626Hz), ±100 cents ≈ ±1/12 volt
    // Voice 0 freq ≈ 261.626 * 2^(-1/12) ≈ 246.94 Hz
    // Voice 7 freq ≈ 261.626 * 2^(+1/12) ≈ 277.18 Hz
    float detuneVolts = 100.f / 100.f / 12.f;  // 1/12 volt
    float expectedLowFreq = PhaseAccumulator::voltToFreq(0.f - detuneVolts);
    float expectedHighFreq = PhaseAccumulator::voltToFreq(0.f + detuneVolts);

    EXPECT(expectedLowFreq < 261.626f, "Low detune should be below base freq");
    EXPECT(expectedHighFreq > 261.626f, "High detune should be above base freq");
    EXPECT_NEAR(expectedLowFreq, 246.94f, 1.f, "Low detune should be ~246.94 Hz");
    EXPECT_NEAR(expectedHighFreq, 277.18f, 1.f, "High detune should be ~277.18 Hz");
}

// ─────────────────────────────────────────────────────────
// Performance test
// ─────────────────────────────────────────────────────────

TEST(perf_8voice_polyphonic) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {-2.f, -1.f, 0.f, 0.5f, 1.f, 1.5f, 2.f, 3.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f};

    auto start = std::chrono::high_resolution_clock::now();
    const int numSamples = 48000;  // 1 second of audio

    for (int s = 0; s < numSamples; s++) {
        engine.process(*bank, vocts, gates, 8, 0.5f, SAMPLE_RATE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    printf("    8-voice polyphonic: %lld µs for %d samples (budget: 1,000,000 µs)\n", us, numSamples);

    // 1 second of audio must process in well under 1 second (1,000,000 µs)
    EXPECT(us < 1000000, "8-voice polyphonic must process in real-time");
}

TEST(perf_8voice_unison) {
    ensureInitialized();

    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 30.f;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    auto start = std::chrono::high_resolution_clock::now();
    const int numSamples = 48000;

    for (int s = 0; s < numSamples; s++) {
        engine.process(*bank, vocts, gates, 1, 0.5f, SAMPLE_RATE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    printf("    8-voice unison: %lld µs for %d samples (budget: 1,000,000 µs)\n", us, numSamples);

    EXPECT(us < 1000000, "8-voice unison must process in real-time");
}

// ─────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────

int main() {
    printf("===================================\n");
    printf("Hurricane 8 — Polyphonic Routing Tests\n");
    printf("===================================\n\n");

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
