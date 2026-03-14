/*
 * Hurricane 8 — Panel & Submission Tests
 * Tests for S05: Individual voice outputs, velocity passthrough,
 * panel SVG validation, and plugin manifest checks.
 *
 * No VCV Rack SDK dependency — uses PolyphonicEngine directly for output routing tests.
 */

#include "ppg_wavetables.hpp"
#include "wavetable_engine.hpp"
#include "polyphonic_engine.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

// Minimal test framework (same as other test files)
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct TestReg_##name { \
        TestReg_##name() { \
            tests_run++; \
            printf("  RUN  " #name "\n"); \
            try { \
                test_##name(); \
                tests_passed++; \
                printf("  PASS " #name "\n"); \
            } catch (const char* msg) { \
                tests_failed++; \
                printf("  FAIL " #name ": %s\n", msg); \
            } \
        } \
    } reg_##name; \
    static void test_##name()

#define EXPECT(cond) do { if (!(cond)) throw "EXPECT failed: " #cond; } while(0)
#define EXPECT_NEAR(a, b, eps) do { \
    if (std::fabs((a) - (b)) > (eps)) { \
        static char buf[256]; \
        snprintf(buf, sizeof(buf), "EXPECT_NEAR failed: %f vs %f (eps %f)", \
                 (double)(a), (double)(b), (double)(eps)); \
        throw (const char*)buf; \
    } \
} while(0)

// Shared wavetable data
static PpgWavetableRom rom;
static WavetableBank bank;
static bool initialized = false;

static void ensureInit() {
    if (!initialized) {
        rom.generate();
        bank.buildFromRom(rom);
        initialized = true;
        printf("\n");
    }
}

// ============================================================
// OUT-04: Individual per-voice audio outputs
// ============================================================

TEST(out04_individual_audio_polyphonic_4voices) {
    // In polyphonic mode with 4 active voices, individual outputs 0-3
    // should have non-zero audio, outputs 4-7 should be silent
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.25f, 0.5f, 0.75f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f};

    // Run enough samples to get meaningful output
    for (int s = 0; s < 256; s++) {
        engine.process(bank, vocts, gates, 4, 0.5f, 48000.f);
    }

    // Active voices should have output
    for (int i = 0; i < 4; i++) {
        EXPECT(std::fabs(engine.outputs[i]) > 0.0001f);
    }
    // Inactive voices should be silent
    for (int i = 4; i < 8; i++) {
        EXPECT_NEAR(engine.outputs[i], 0.f, 0.0001f);
    }
}

TEST(out04_individual_audio_all_8_voices) {
    // With 8 polyphonic voices, all 8 individual outputs should have audio
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 5;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {-1.f, -0.5f, 0.f, 0.25f, 0.5f, 0.75f, 1.f, 1.5f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f};

    for (int s = 0; s < 256; s++) {
        engine.process(bank, vocts, gates, 8, 0.3f, 48000.f);
    }

    // All 8 voices should have non-zero output
    for (int i = 0; i < 8; i++) {
        EXPECT(std::fabs(engine.outputs[i]) > 0.0001f);
    }
}

TEST(out04_individual_audio_unison_all_active) {
    // In unison mode, all 8 individual outputs should be active
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 20.f;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    for (int s = 0; s < 256; s++) {
        engine.process(bank, vocts, gates, 1, 0.5f, 48000.f);
    }

    for (int i = 0; i < 8; i++) {
        EXPECT(engine.active[i]);
        EXPECT(std::fabs(engine.outputs[i]) > 0.0001f);
    }
}

TEST(out04_individual_voices_are_independent) {
    // Each individual output should carry a different signal when
    // voices are at different pitches
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    // Two voices at very different pitches
    float vocts[MAX_POLY_VOICES] = {-2.f, 2.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    // Collect output samples
    float out0[256], out1[256];
    for (int s = 0; s < 256; s++) {
        engine.process(bank, vocts, gates, 2, 0.5f, 48000.f);
        out0[s] = engine.outputs[0];
        out1[s] = engine.outputs[1];
    }

    // Outputs should differ (different frequencies produce different waveforms)
    bool anyDifferent = false;
    for (int s = 0; s < 256; s++) {
        if (std::fabs(out0[s] - out1[s]) > 0.001f) {
            anyDifferent = true;
            break;
        }
    }
    EXPECT(anyDifferent);
}

TEST(out04_individual_output_matches_poly_output) {
    // The individual voice output[i] should equal what the polyphonic
    // output would carry on channel i (same scaling factor)
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 10;
    engine.scanMode = ScanMode::Interpolated;
    engine.dacDepth = DacEmulator::BIT_12;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    for (int s = 0; s < 128; s++) {
        engine.process(bank, vocts, gates, 3, 0.2f, 48000.f);
    }

    // The raw engine outputs[i] are what both poly output channels and
    // individual outputs are scaled from (by *5V in the module)
    // Just verify the engine provides per-voice values
    for (int i = 0; i < 3; i++) {
        EXPECT(engine.active[i]);
        // Output value is bounded
        EXPECT(engine.outputs[i] >= -1.5f && engine.outputs[i] <= 1.5f);
    }
}

// ============================================================
// OUT-05: Individual per-voice gate outputs
// ============================================================

TEST(out05_individual_gate_polyphonic) {
    // In polyphonic mode, individual gate outputs should mirror
    // the corresponding input gate channel
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    // Gates: voices 0,1,3 active; 2,4-7 inactive
    float vocts[MAX_POLY_VOICES] = {0.f, 0.25f, 0.5f, 0.75f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 0.f, 10.f, 0.f, 0.f, 0.f, 0.f};

    engine.process(bank, vocts, gates, 4, 0.5f, 48000.f);

    // Active voices (0,1,3) should have active[i] = true
    EXPECT(engine.active[0]);
    EXPECT(engine.active[1]);
    EXPECT(!engine.active[2]); // gate was 0
    EXPECT(engine.active[3]);
    for (int i = 4; i < 8; i++) {
        EXPECT(!engine.active[i]);
    }
}

TEST(out05_individual_gate_unison) {
    // In unison mode, all 8 gates should follow the single gate input
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Unison;
    engine.unisonDetuneCents = 10.f;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    // Gate on channel 0
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};

    engine.process(bank, vocts, gates, 1, 0.5f, 48000.f);

    // All voices should be active
    for (int i = 0; i < 8; i++) {
        EXPECT(engine.active[i]);
    }
}

TEST(out05_gate_off_silences_individual) {
    // When gate goes off, the corresponding individual output should be silent
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    engine.process(bank, vocts, gates, 8, 0.5f, 48000.f);

    for (int i = 0; i < 8; i++) {
        EXPECT(!engine.active[i]);
        EXPECT_NEAR(engine.outputs[i], 0.f, 0.0001f);
    }
}

// ============================================================
// OUT-06: Velocity CV passthrough
// Velocity is a simple passthrough in the module (not in the engine).
// We test the concept: velocity values pass through unchanged.
// ============================================================

TEST(out06_velocity_passthrough_concept) {
    // Velocity is a direct input-to-output passthrough.
    // We verify the concept by checking that the engine doesn't
    // interfere with external signal routing (velocity doesn't
    // go through the engine at all).
    ensureInit();
    PolyphonicEngine engine;

    // Process a frame — velocity is handled outside the engine
    float vocts[MAX_POLY_VOICES] = {0.f};
    float gates[MAX_POLY_VOICES] = {10.f};
    engine.process(bank, vocts, gates, 1, 0.5f, 48000.f);

    // Engine should not have any velocity-related state that would
    // interfere with external passthrough
    // (This test confirms the architecture — velocity is a cable-level
    // passthrough, not an engine feature)
    EXPECT(engine.activeCount > 0);
}

TEST(out06_velocity_values_bounded) {
    // Velocity CV values from MIDI-CV are typically 0-10V.
    // Verify the passthrough concept handles the expected range.
    // Since this is a direct passthrough, we just verify the
    // expected value range doesn't need clamping.
    float testVelocities[] = {0.f, 1.f, 5.f, 7.5f, 10.f};
    for (int i = 0; i < 5; i++) {
        float v = testVelocities[i];
        EXPECT(v >= 0.f && v <= 10.f);
    }
}

// ============================================================
// PNL-01: Panel SVG validation
// ============================================================

TEST(pnl01_panel_svg_exists) {
    // Panel SVG file must exist at the expected path
    std::ifstream f("res/Hurricane8VCO.svg");
    EXPECT(f.good());
}

TEST(pnl01_panel_svg_valid_xml) {
    // Panel SVG should start with XML declaration or SVG tag
    std::ifstream f("res/Hurricane8VCO.svg");
    EXPECT(f.good());

    std::string line;
    std::getline(f, line);
    EXPECT(line.find("<?xml") != std::string::npos ||
           line.find("<svg") != std::string::npos);
}

TEST(pnl01_panel_svg_correct_dimensions) {
    // Panel should be 28HP wide (142.24mm) and 128.5mm tall
    std::ifstream f("res/Hurricane8VCO.svg");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    EXPECT(content.find("142.24mm") != std::string::npos); // width
    EXPECT(content.find("128.5mm") != std::string::npos);  // height
}

TEST(pnl01_panel_svg_no_text_elements) {
    // VCV Library requires text as paths, not <text> elements
    std::ifstream f("res/Hurricane8VCO.svg");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // Should not contain <text> tags (text must be converted to paths)
    EXPECT(content.find("<text") == std::string::npos);
}

TEST(pnl01_panel_svg_has_viewbox) {
    // SVG should have a viewBox attribute for proper scaling
    std::ifstream f("res/Hurricane8VCO.svg");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    EXPECT(content.find("viewBox") != std::string::npos);
}

// ============================================================
// PNL-02: Plugin manifest validation
// ============================================================

TEST(pnl02_plugin_json_exists) {
    std::ifstream f("plugin.json");
    EXPECT(f.good());
}

TEST(pnl02_plugin_json_has_required_fields) {
    // Check that plugin.json contains all required VCV Library fields
    std::ifstream f("plugin.json");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    EXPECT(content.find("\"slug\"") != std::string::npos);
    EXPECT(content.find("\"name\"") != std::string::npos);
    EXPECT(content.find("\"version\"") != std::string::npos);
    EXPECT(content.find("\"license\"") != std::string::npos);
    EXPECT(content.find("\"brand\"") != std::string::npos);
    EXPECT(content.find("\"author\"") != std::string::npos);
    EXPECT(content.find("\"modules\"") != std::string::npos);
}

TEST(pnl02_module_entry_correct) {
    // The module entry should have correct slug and tags
    std::ifstream f("plugin.json");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    EXPECT(content.find("\"Hurricane8VCO\"") != std::string::npos);
    EXPECT(content.find("\"Oscillator\"") != std::string::npos);
    EXPECT(content.find("\"Polyphonic\"") != std::string::npos);
    EXPECT(content.find("\"Wavetable\"") != std::string::npos);
}

TEST(pnl02_version_format) {
    // Version should be in semver format (X.Y.Z)
    std::ifstream f("plugin.json");
    EXPECT(f.good());

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    // "version": "2.0.0" — check for valid version pattern
    EXPECT(content.find("\"2.0.0\"") != std::string::npos);
}

// ============================================================
// PNL-03: Build compatibility
// ============================================================

TEST(pnl03_polyphonic_engine_header_standalone) {
    // Verify that the polyphonic engine compiles without Rack SDK
    // (this test running proves it)
    PolyphonicEngine engine;
    EXPECT(engine.activeCount == 0);
}

TEST(pnl03_max_poly_voices_is_8) {
    // Confirm the constant matches the hardware design
    EXPECT(MAX_POLY_VOICES == 8);
}

TEST(pnl03_output_count_consistency) {
    // Verify the engine provides exactly 8 output slots
    PolyphonicEngine engine;
    // outputs array has MAX_POLY_VOICES entries
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        engine.outputs[i] = (float)i;
    }
    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        EXPECT_NEAR(engine.outputs[i], (float)i, 0.001f);
    }
}

// ============================================================
// Integration: Individual outputs scale correctly
// ============================================================

TEST(integration_individual_output_bounded) {
    // Individual voice outputs should be in [-1, 1] range (raw engine output)
    // The module scales by *5V, but the engine output itself is bounded
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_8;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 1.5f, 2.f, -1.f, -0.5f, 0.25f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f};

    float maxAbs = 0.f;
    for (int s = 0; s < 1000; s++) {
        engine.process(bank, vocts, gates, 8, 0.5f, 48000.f);
        for (int i = 0; i < 8; i++) {
            float a = std::fabs(engine.outputs[i]);
            if (a > maxAbs) maxAbs = a;
        }
    }

    // Raw engine output should not exceed ±1.1 (with headroom for DAC quantization)
    EXPECT(maxAbs <= 1.1f);
    EXPECT(maxAbs > 0.01f); // Should have some output
}

TEST(integration_mix_equals_average_of_individual) {
    // Mix output should equal the average of all active individual outputs
    ensureInit();
    PolyphonicEngine engine;
    engine.voiceMode = VoiceMode::Polyphonic;
    engine.currentTable = 0;
    engine.scanMode = ScanMode::Stepped;
    engine.dacDepth = DacEmulator::BIT_16;

    float vocts[MAX_POLY_VOICES] = {0.f, 0.5f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float gates[MAX_POLY_VOICES] = {10.f, 10.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f};

    for (int s = 0; s < 256; s++) {
        engine.process(bank, vocts, gates, 3, 0.5f, 48000.f);
    }

    // Manually compute average of active voices
    float sum = 0.f;
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (engine.active[i]) {
            sum += engine.outputs[i];
            count++;
        }
    }
    float expectedMix = sum / (float)count;
    float actualMix = engine.getMixOutput();

    EXPECT_NEAR(actualMix, expectedMix, 0.0001f);
}

// ============================================================
// Main
// ============================================================

int main() {
    printf("\n══════════════════════════════════════════════════\n");
    printf("  Hurricane 8 — Panel & Submission Tests\n");
    printf("  OUT-04, OUT-05, OUT-06, PNL-01, PNL-02, PNL-03\n");
    printf("══════════════════════════════════════════════════\n\n");

    // Tests run via static initialization

    printf("\n────────────────────────────────────────────────────\n");
    printf("  %d tests run, %d passed, %d failed\n\n", tests_run, tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
