/*
 * Hurricane 8 — Modulation Unit Tests
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Tests for MOD-01..07:
 *   MOD-01: Per-voice ASR envelope modulates wavetable position
 *   MOD-02: Global ASR envelope mode
 *   MOD-03: Per-voice vs global envelope mode switch
 *   MOD-04: Wavetable start position with CV
 *   MOD-05: Internal sine LFO for vibrato
 *   MOD-06: LFO amount (depth) control
 *   MOD-07: Glide (portamento)
 *
 * Build: g++ -std=c++17 -O2 -I../src -o test_modulation test_modulation.cpp -lm
 * Run:   ./test_modulation
 */

#include "modulation_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <string>
#include <functional>

// ─────────────────────────────────────────────────────────
// Test framework (same pattern as DSP and polyphonic tests)
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
            throw TestFailure{currentTestName, msg, __LINE__}; \
        } \
    } while(0)

#define EXPECT_NEAR(val, expected, tolerance, msg) \
    do { \
        float v_ = (val); float e_ = (expected); float t_ = (tolerance); \
        if (std::abs(v_ - e_) > t_) { \
            char buf_[512]; \
            std::snprintf(buf_, sizeof(buf_), "%s (got %.6f, expected %.6f ± %.6f)", \
                         msg, v_, e_, t_); \
            throw TestFailure{currentTestName, buf_, __LINE__}; \
        } \
    } while(0)

struct TestEntry {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestEntry> allTests;
static std::string currentTestName;

static void registerTest(const char* name, std::function<void()> func) {
    allTests.push_back({name, func});
}

static constexpr float SAMPLE_RATE = 44100.f;

// ─────────────────────────────────────────────────────────
// MOD-01: Per-voice ASR envelope modulates wavetable position
// ─────────────────────────────────────────────────────────

TEST(mod01_asr_idle_at_zero) {
    AsrEnvelope env;
    // Without any gate, envelope should stay at 0
    for (int i = 0; i < 100; i++) {
        float val = env.process(false);
        EXPECT_NEAR(val, 0.f, 0.0001f, "Idle envelope should be 0");
    }
}

TEST(mod01_asr_attack_rises) {
    AsrEnvelope env;
    env.setAttack(0.01f, SAMPLE_RATE);  // 10ms attack
    env.setRelease(0.01f, SAMPLE_RATE);

    // Trigger gate
    float first = env.process(true);
    EXPECT(first > 0.f, "First attack sample should be > 0");

    // Keep gate high — level should keep rising
    float prev = first;
    for (int i = 0; i < 100; i++) {
        float val = env.process(true);
        EXPECT(val >= prev, "Attack should monotonically rise");
        prev = val;
    }
}

TEST(mod01_asr_reaches_sustain) {
    AsrEnvelope env;
    env.setAttack(0.001f, SAMPLE_RATE);  // 1ms attack
    env.setRelease(0.1f, SAMPLE_RATE);

    // Run attack to completion (1ms = ~44 samples)
    for (int i = 0; i < 200; i++) {
        env.process(true);
    }

    EXPECT_NEAR(env.level, 1.f, 0.001f, "Should reach sustain at 1.0");
    EXPECT(env.stage == EnvelopeStage::Sustain, "Should be in sustain stage");
}

TEST(mod01_asr_release_falls) {
    AsrEnvelope env;
    env.setAttack(0.001f, SAMPLE_RATE);
    env.setRelease(0.01f, SAMPLE_RATE);  // 10ms release

    // Attack to sustain
    for (int i = 0; i < 200; i++) {
        env.process(true);
    }
    EXPECT_NEAR(env.level, 1.f, 0.001f, "Should be at sustain");

    // Release gate
    float val = env.process(false);
    EXPECT(val < 1.f, "Should start falling on release");

    // Keep processing — should continue falling
    float prev = val;
    for (int i = 0; i < 100; i++) {
        val = env.process(false);
        EXPECT(val <= prev + 0.0001f, "Release should monotonically fall");
        prev = val;
    }
}

TEST(mod01_asr_returns_to_idle) {
    AsrEnvelope env;
    env.setAttack(0.001f, SAMPLE_RATE);
    env.setRelease(0.001f, SAMPLE_RATE);

    // Full cycle: attack → sustain → release → idle
    for (int i = 0; i < 200; i++) env.process(true);
    for (int i = 0; i < 200; i++) env.process(false);

    EXPECT_NEAR(env.level, 0.f, 0.001f, "Should return to 0 after release");
    EXPECT(env.stage == EnvelopeStage::Idle, "Should be idle after full release");
}

TEST(mod01_per_voice_independent) {
    // Two voices with different gates should have independent envelopes
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::PerVoice;
    mod.attackTime = 0.01f;
    mod.releaseTime = 0.01f;
    mod.startPosition = 0.f;

    // Voice 0 gate on, voice 1 gate off
    for (int i = 0; i < 500; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
        mod.getModulatedPosition(1, false, SAMPLE_RATE);
    }

    float pos0 = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    float pos1 = mod.getModulatedPosition(1, false, SAMPLE_RATE);

    EXPECT(pos0 > 0.5f, "Voice 0 (gated) should have high position from envelope");
    EXPECT_NEAR(pos1, 0.f, 0.01f, "Voice 1 (no gate) should stay at start position");
}

TEST(mod01_per_voice_different_timing) {
    // Voices triggered at different times should be at different envelope stages
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::PerVoice;
    mod.attackTime = 0.05f;  // 50ms attack
    mod.releaseTime = 0.05f;
    mod.startPosition = 0.f;

    // Voice 0 starts first
    for (int i = 0; i < 500; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
    }
    float pos0_early = mod.getModulatedPosition(0, true, SAMPLE_RATE);

    // Voice 1 just started
    float pos1_start = mod.getModulatedPosition(1, true, SAMPLE_RATE);

    EXPECT(pos0_early > pos1_start,
           "Voice 0 (longer gate) should have higher envelope than voice 1 (just triggered)");
}

// ─────────────────────────────────────────────────────────
// MOD-02: Global ASR envelope mode
// ─────────────────────────────────────────────────────────

TEST(mod02_global_envelope_all_voices_same) {
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::Global;
    mod.attackTime = 0.01f;
    mod.releaseTime = 0.01f;
    mod.startPosition = 0.f;

    // Run global envelope with gate for a while
    for (int i = 0; i < 500; i++) {
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
    }

    // All voices should read the same position regardless of per-voice gate
    float pos0 = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    float pos1 = mod.getModulatedPosition(1, false, SAMPLE_RATE);
    float pos2 = mod.getModulatedPosition(2, true, SAMPLE_RATE);

    EXPECT_NEAR(pos0, pos1, 0.001f, "Global mode: all voices should have same position");
    EXPECT_NEAR(pos1, pos2, 0.001f, "Global mode: all voices should have same position");
}

TEST(mod02_global_envelope_triggered_by_any_gate) {
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::Global;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.001f;
    mod.startPosition = 0.f;

    // Advance global envelope as if "any gate active"
    for (int i = 0; i < 200; i++) {
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
    }

    EXPECT_NEAR(mod.globalEnvelope.level, 1.f, 0.001f,
                "Global envelope should reach sustain");
}

TEST(mod02_global_envelope_releases_when_all_gates_off) {
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::Global;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.001f;
    mod.startPosition = 0.f;

    // Attack to sustain
    for (int i = 0; i < 200; i++) {
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
    }
    EXPECT_NEAR(mod.globalEnvelope.level, 1.f, 0.001f, "Should be at sustain");

    // Release
    for (int i = 0; i < 200; i++) {
        mod.processGlobalEnvelope(false, SAMPLE_RATE);
    }
    EXPECT_NEAR(mod.globalEnvelope.level, 0.f, 0.001f,
                "Global envelope should return to 0 when all gates off");
}

// ─────────────────────────────────────────────────────────
// MOD-03: Per-voice vs global mode switching
// ─────────────────────────────────────────────────────────

TEST(mod03_switch_pervoice_to_global) {
    ModulationEngine mod;
    mod.attackTime = 0.01f;
    mod.releaseTime = 0.01f;
    mod.startPosition = 0.f;

    // Start in per-voice mode
    mod.envelopeMode = EnvelopeMode::PerVoice;
    for (int i = 0; i < 500; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
        mod.getModulatedPosition(1, false, SAMPLE_RATE);
    }

    // Voices should differ
    float pv0 = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    float pv1 = mod.getModulatedPosition(1, false, SAMPLE_RATE);
    EXPECT(std::abs(pv0 - pv1) > 0.1f, "Per-voice mode: voices should differ");

    // Switch to global mode
    mod.envelopeMode = EnvelopeMode::Global;
    for (int i = 0; i < 500; i++) {
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
    }

    // Now voices should be the same
    float g0 = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    float g1 = mod.getModulatedPosition(1, false, SAMPLE_RATE);
    EXPECT_NEAR(g0, g1, 0.001f, "Global mode: voices should be equal");
}

TEST(mod03_switch_global_to_pervoice) {
    ModulationEngine mod;
    mod.attackTime = 0.01f;
    mod.releaseTime = 0.01f;
    mod.startPosition = 0.f;

    // Start in global mode
    mod.envelopeMode = EnvelopeMode::Global;
    for (int i = 0; i < 500; i++) {
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
    }

    // Switch to per-voice mode
    mod.envelopeMode = EnvelopeMode::PerVoice;

    // Gate only voice 0, not voice 1
    for (int i = 0; i < 500; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
        mod.getModulatedPosition(1, false, SAMPLE_RATE);
    }

    float pv0 = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    float pv1 = mod.getModulatedPosition(1, false, SAMPLE_RATE);

    // Voice 0 should be up (attack), voice 1 should be low (no gate → idle/release)
    EXPECT(pv0 > pv1 + 0.1f, "After switch to per-voice: voices should diverge");
}

// ─────────────────────────────────────────────────────────
// MOD-04: Wavetable start position with CV
// ─────────────────────────────────────────────────────────

TEST(mod04_start_position_zero) {
    ModulationEngine mod;
    mod.startPosition = 0.f;
    mod.startPositionCV = 0.f;
    mod.envelopeMode = EnvelopeMode::PerVoice;

    // No gate → envelope idle → position = startPosition
    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(pos, 0.f, 0.001f, "Start position 0 with no envelope should give 0");
}

TEST(mod04_start_position_mid) {
    ModulationEngine mod;
    mod.startPosition = 0.5f;
    mod.startPositionCV = 0.f;
    mod.envelopeMode = EnvelopeMode::PerVoice;

    // No gate → position = 0.5
    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(pos, 0.5f, 0.001f, "Start position 0.5 with no envelope should give 0.5");
}

TEST(mod04_start_position_full) {
    ModulationEngine mod;
    mod.startPosition = 1.f;
    mod.startPositionCV = 0.f;
    mod.envelopeMode = EnvelopeMode::PerVoice;

    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(pos, 1.f, 0.001f, "Start position 1.0 should give 1.0");
}

TEST(mod04_start_position_cv_positive) {
    ModulationEngine mod;
    mod.startPosition = 0.f;
    mod.startPositionCV = 5.f;  // +5V → +0.5 position offset
    mod.envelopeMode = EnvelopeMode::PerVoice;

    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(pos, 0.5f, 0.01f, "+5V CV should add 0.5 to position");
}

TEST(mod04_start_position_cv_negative) {
    ModulationEngine mod;
    mod.startPosition = 0.5f;
    mod.startPositionCV = -5.f;  // -5V → -0.5 position offset
    mod.envelopeMode = EnvelopeMode::PerVoice;

    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(pos, 0.f, 0.01f, "-5V CV at start=0.5 should clamp to 0");
}

TEST(mod04_start_position_cv_clamped) {
    ModulationEngine mod;
    mod.startPosition = 0.9f;
    mod.startPositionCV = 10.f;  // Would push past 1.0
    mod.envelopeMode = EnvelopeMode::PerVoice;

    float pos = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT(pos <= 1.f, "Position should never exceed 1.0");
    EXPECT(pos >= 0.f, "Position should never go below 0.0");
}

TEST(mod04_start_plus_envelope) {
    // Start at 0.5, envelope should sweep from 0.5 to 1.0
    ModulationEngine mod;
    mod.startPosition = 0.5f;
    mod.startPositionCV = 0.f;
    mod.envelopeMode = EnvelopeMode::PerVoice;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.1f;

    // Before gate: position = 0.5
    float before = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(before, 0.5f, 0.01f, "Before gate: position at start");

    // Gate on, run attack
    for (int i = 0; i < 200; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
    }

    // After full attack: position should be near 1.0 (start + env*(1-start) = 0.5 + 1*0.5 = 1.0)
    float after = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    EXPECT_NEAR(after, 1.f, 0.01f, "After full attack: should sweep to 1.0");
}

// ─────────────────────────────────────────────────────────
// MOD-05: Internal sine LFO for vibrato
// ─────────────────────────────────────────────────────────

TEST(mod05_lfo_produces_sine) {
    SineLfo lfo;
    float rateHz = 10.f;
    int samplesPerCycle = (int)(SAMPLE_RATE / rateHz);

    // Collect one cycle
    std::vector<float> samples(samplesPerCycle);
    for (int i = 0; i < samplesPerCycle; i++) {
        samples[i] = lfo.process(rateHz, SAMPLE_RATE);
    }

    // Check it's bipolar
    float minVal = *std::min_element(samples.begin(), samples.end());
    float maxVal = *std::max_element(samples.begin(), samples.end());

    EXPECT(maxVal > 0.9f, "LFO should reach near +1");
    EXPECT(minVal < -0.9f, "LFO should reach near -1");
}

TEST(mod05_lfo_frequency_accuracy) {
    SineLfo lfo;
    float rateHz = 5.f;  // 5 Hz LFO

    // Count positive-going zero crossings in 1 second
    int crossings = 0;
    float prev = 0.f;
    int totalSamples = (int)SAMPLE_RATE;

    for (int i = 0; i < totalSamples; i++) {
        float val = lfo.process(rateHz, SAMPLE_RATE);
        if (prev <= 0.f && val > 0.f) {
            crossings++;
        }
        prev = val;
    }

    EXPECT(crossings == 5, "5 Hz LFO should have 5 positive crossings in 1 second");
}

TEST(mod05_lfo_zero_rate) {
    SineLfo lfo;
    float val = lfo.process(0.f, SAMPLE_RATE);
    EXPECT_NEAR(val, 0.f, 0.0001f, "Zero rate LFO should output 0");
}

TEST(mod05_vibrato_pitch_modulation) {
    ModulationEngine mod;
    mod.lfoAmount = 1.f;  // Full depth (1 semitone)
    mod.lfoRateHz = 100.f;  // Fast for testing

    // Run LFO for a while and check vibrato range
    float maxVibrato = 0.f;
    float minVibrato = 0.f;
    for (int i = 0; i < (int)SAMPLE_RATE; i++) {
        mod.processLfo(SAMPLE_RATE);
        float v = mod.getVibrato();
        if (v > maxVibrato) maxVibrato = v;
        if (v < minVibrato) minVibrato = v;
    }

    // 1 semitone = 1/12 V/Oct
    float expectedDepth = 1.f / 12.f;
    EXPECT_NEAR(maxVibrato, expectedDepth, 0.005f, "Max vibrato should be ~1/12 V");
    EXPECT_NEAR(minVibrato, -expectedDepth, 0.005f, "Min vibrato should be ~-1/12 V");
}

// ─────────────────────────────────────────────────────────
// MOD-06: LFO amount (depth) control
// ─────────────────────────────────────────────────────────

TEST(mod06_zero_amount_no_vibrato) {
    ModulationEngine mod;
    mod.lfoAmount = 0.f;
    mod.lfoRateHz = 10.f;

    for (int i = 0; i < 1000; i++) {
        mod.processLfo(SAMPLE_RATE);
        float v = mod.getVibrato();
        EXPECT_NEAR(v, 0.f, 0.0001f, "Zero amount should produce no vibrato");
    }
}

TEST(mod06_half_amount_half_depth) {
    ModulationEngine mod;
    mod.lfoAmount = 0.5f;
    mod.lfoRateHz = 100.f;

    float maxVibrato = 0.f;
    for (int i = 0; i < (int)SAMPLE_RATE; i++) {
        mod.processLfo(SAMPLE_RATE);
        float v = mod.getVibrato();
        if (std::abs(v) > maxVibrato) maxVibrato = std::abs(v);
    }

    float expectedMax = 0.5f / 12.f;  // Half a semitone
    EXPECT_NEAR(maxVibrato, expectedMax, 0.005f,
                "Half amount should give half-semitone vibrato depth");
}

TEST(mod06_amount_scales_linearly) {
    // Check that doubling the amount doubles the depth
    float depths[3] = {};
    float amounts[3] = {0.25f, 0.5f, 1.0f};

    for (int a = 0; a < 3; a++) {
        ModulationEngine mod;
        mod.lfoAmount = amounts[a];
        mod.lfoRateHz = 100.f;

        float maxV = 0.f;
        for (int i = 0; i < (int)SAMPLE_RATE; i++) {
            mod.processLfo(SAMPLE_RATE);
            float v = std::abs(mod.getVibrato());
            if (v > maxV) maxV = v;
        }
        depths[a] = maxV;
    }

    // depths[1] should be ~2x depths[0], depths[2] should be ~2x depths[1]
    float ratio1 = depths[1] / depths[0];
    float ratio2 = depths[2] / depths[1];
    EXPECT_NEAR(ratio1, 2.f, 0.1f, "Doubling amount should double depth (0.25→0.5)");
    EXPECT_NEAR(ratio2, 2.f, 0.1f, "Doubling amount should double depth (0.5→1.0)");
}

// ─────────────────────────────────────────────────────────
// MOD-07: Glide (portamento)
// ─────────────────────────────────────────────────────────

TEST(mod07_glide_zero_amount_instant) {
    GlideProcessor glide;

    float v1 = glide.process(1.f, 0.f);  // First value, instant init
    EXPECT_NEAR(v1, 1.f, 0.0001f, "First value should initialize immediately");

    float v2 = glide.process(3.f, 0.f);  // Jump, amount=0 → instant
    EXPECT_NEAR(v2, 3.f, 0.0001f, "Zero glide should track instantly");
}

TEST(mod07_glide_nonzero_smooths) {
    GlideProcessor glide;
    glide.process(0.f, 0.5f);  // Initialize at 0V

    // Jump to 5V with glide
    float first = glide.process(5.f, 0.5f);
    EXPECT(first > 0.f && first < 5.f,
           "Glide should start moving toward target but not reach it instantly");
}

TEST(mod07_glide_converges) {
    GlideProcessor glide;
    glide.process(0.f, 0.5f);  // Init at 0

    // Apply glide toward 3V for many samples
    float val = 0.f;
    for (int i = 0; i < 10000; i++) {
        val = glide.process(3.f, 0.5f);
    }

    EXPECT_NEAR(val, 3.f, 0.01f, "Glide should converge to target");
}

TEST(mod07_glide_higher_amount_slower) {
    // Compare convergence speed at two different amounts
    GlideProcessor slowGlide, fastGlide;
    slowGlide.process(0.f, 0.9f);
    fastGlide.process(0.f, 0.3f);

    float slowVal = 0.f, fastVal = 0.f;
    for (int i = 0; i < 100; i++) {
        slowVal = slowGlide.process(5.f, 0.9f);
        fastVal = fastGlide.process(5.f, 0.3f);
    }

    EXPECT(fastVal > slowVal, "Lower glide amount should converge faster");
}

TEST(mod07_glide_per_voice_independent) {
    ModulationEngine mod;
    mod.glideAmount = 0.8f;

    // Initialize both voices at 0
    mod.getGlidedVoct(0, 0.f);
    mod.getGlidedVoct(1, 0.f);

    // Voice 0 targets 5V, voice 1 targets -2V
    float v0 = 0.f, v1 = 0.f;
    for (int i = 0; i < 1000; i++) {
        v0 = mod.getGlidedVoct(0, 5.f);
        v1 = mod.getGlidedVoct(1, -2.f);
    }

    EXPECT(v0 > 0.f, "Voice 0 should glide toward positive");
    EXPECT(v1 < 0.f, "Voice 1 should glide toward negative");
    EXPECT(std::abs(v0 - v1) > 1.f, "Voices should be independent");
}

TEST(mod07_glide_reset) {
    GlideProcessor glide;
    glide.process(5.f, 0.5f);  // Init at 5V

    glide.reset();

    // After reset, should reinitialize at new value
    float val = glide.process(0.f, 0.5f);
    EXPECT_NEAR(val, 0.f, 0.001f, "After reset, should init at new target");
}

// ─────────────────────────────────────────────────────────
// Integration: ModulationEngine combined behavior
// ─────────────────────────────────────────────────────────

TEST(integration_position_bounded_0_1) {
    // Position should always be in [0, 1] no matter what
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::PerVoice;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.001f;

    // Extreme CV values
    float cvValues[] = {-10.f, -5.f, 0.f, 5.f, 10.f};
    float startValues[] = {0.f, 0.25f, 0.5f, 0.75f, 1.f};
    bool gates[] = {false, true};

    for (float cv : cvValues) {
        for (float start : startValues) {
            for (bool gate : gates) {
                mod.startPosition = start;
                mod.startPositionCV = cv;
                for (int i = 0; i < 100; i++) {
                    float pos = mod.getModulatedPosition(0, gate, SAMPLE_RATE);
                    EXPECT(pos >= 0.f && pos <= 1.f, "Position must always be in [0, 1]");
                }
            }
        }
    }
}

TEST(integration_reset_clears_all) {
    ModulationEngine mod;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.001f;
    mod.lfoAmount = 1.f;
    mod.lfoRateHz = 10.f;
    mod.glideAmount = 0.5f;

    // Warm up everything
    for (int i = 0; i < 500; i++) {
        mod.processLfo(SAMPLE_RATE);
        mod.processGlobalEnvelope(true, SAMPLE_RATE);
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
        mod.getGlidedVoct(0, 5.f);
    }

    // Reset
    mod.reset();

    // All envelopes should be idle
    for (int i = 0; i < MOD_MAX_VOICES; i++) {
        EXPECT(mod.perVoiceEnvelopes[i].stage == EnvelopeStage::Idle,
               "Per-voice envelope should be idle after reset");
        EXPECT_NEAR(mod.perVoiceEnvelopes[i].level, 0.f, 0.0001f,
                    "Per-voice envelope level should be 0 after reset");
    }
    EXPECT(mod.globalEnvelope.stage == EnvelopeStage::Idle,
           "Global envelope should be idle after reset");

    // LFO phase should be 0
    EXPECT_NEAR(mod.lfo.phase, 0.f, 0.0001f, "LFO phase should be 0 after reset");
}

TEST(integration_envelope_sweeps_position_range) {
    // Verify that envelope correctly sweeps from startPosition to 1.0
    ModulationEngine mod;
    mod.envelopeMode = EnvelopeMode::PerVoice;
    mod.startPosition = 0.25f;
    mod.startPositionCV = 0.f;
    mod.attackTime = 0.001f;
    mod.releaseTime = 0.5f;

    // No gate → position at start
    float idle = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(idle, 0.25f, 0.01f, "Idle position should be start");

    // Full attack → position at 1.0
    for (int i = 0; i < 200; i++) {
        mod.getModulatedPosition(0, true, SAMPLE_RATE);
    }
    float peak = mod.getModulatedPosition(0, true, SAMPLE_RATE);
    EXPECT_NEAR(peak, 1.f, 0.01f, "Full envelope should reach 1.0");

    // Release → position returns to start
    for (int i = 0; i < (int)(0.5f * SAMPLE_RATE + 100); i++) {
        mod.getModulatedPosition(0, false, SAMPLE_RATE);
    }
    float released = mod.getModulatedPosition(0, false, SAMPLE_RATE);
    EXPECT_NEAR(released, 0.25f, 0.02f, "After release, should return near start position");
}

// ─────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║     Hurricane 8 — Modulation Unit Tests                 ║\n");
    printf("║     MOD-01..07: ASR, LFO, Glide, Start Position        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    for (auto& t : allTests) {
        currentTestName = t.name;
        testsRun++;
        try {
            t.func();
            testsPassed++;
            printf("  ✓ %s\n", t.name.c_str());
        } catch (const TestFailure& f) {
            testsFailed++;
            failures.push_back(f);
            printf("  ✗ %s — %s (line %d)\n", t.name.c_str(), f.message.c_str(), f.line);
        }
    }

    printf("\n────────────────────────────────────────────────────\n");
    printf("  %d tests run, %d passed, %d failed\n", testsRun, testsPassed, testsFailed);

    if (!failures.empty()) {
        printf("\n  Failures:\n");
        for (auto& f : failures) {
            printf("    • %s: %s (line %d)\n", f.testName.c_str(), f.message.c_str(), f.line);
        }
    }

    printf("\n");
    return testsFailed > 0 ? 1 : 0;
}
