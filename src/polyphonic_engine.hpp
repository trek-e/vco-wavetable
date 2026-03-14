/*
 * Hurricane 8 — Polyphonic Voice Engine
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Routes polyphonic V/Oct and gate signals to 8 independent WavetableVoice
 * instances. Supports polyphonic mode (one voice per input channel) and
 * unison mode (all 8 voices stacked on a single pitch with configurable detune).
 *
 * No VCV Rack SDK dependency — testable standalone.
 */

#pragma once

#include "wavetable_engine.hpp"
#include <cmath>
#include <algorithm>

static constexpr int MAX_POLY_VOICES = 8;

// Voice modes
enum class VoiceMode {
    Polyphonic,   // Each input channel drives one voice
    Unison        // All voices stack on channel 0's pitch with detune spread
};

// Polyphonic voice engine — manages 8 WavetableVoice instances
struct PolyphonicEngine {
    WavetableVoice voices[MAX_POLY_VOICES];
    float outputs[MAX_POLY_VOICES] = {};   // Per-voice output buffer (raw, pre-scaling)
    bool  active[MAX_POLY_VOICES] = {};     // Which voices are active this frame
    int   activeCount = 0;                  // Number of active voices

    VoiceMode voiceMode = VoiceMode::Polyphonic;

    // Shared voice parameters (set before processing)
    int currentTable = 0;
    ScanMode scanMode = ScanMode::Stepped;
    DacEmulator::Depth dacDepth = DacEmulator::BIT_8;

    // Unison detune spread in cents (0 = no detune, 100 = ±1 semitone spread)
    float unisonDetuneCents = 20.f;

    // Process all voices for one sample
    // voctInputs: per-channel V/Oct values (up to MAX_POLY_VOICES)
    // gateInputs: per-channel gate values (>0 = active)
    // numInputChannels: number of active polyphonic channels from input
    // position: wavetable frame position (0..1)
    // sampleRate: audio sample rate in Hz
    void process(const WavetableBank& bank,
                 const float* voctInputs,
                 const float* gateInputs,
                 int numInputChannels,
                 float position,
                 float sampleRate) {

        // Apply shared params to all voices
        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            voices[i].currentTable = currentTable;
            voices[i].scanMode = scanMode;
            voices[i].dacDepth = dacDepth;
        }

        if (voiceMode == VoiceMode::Polyphonic) {
            processPolyphonic(bank, voctInputs, gateInputs, numInputChannels, position, sampleRate);
        } else {
            processUnison(bank, voctInputs, gateInputs, numInputChannels, position, sampleRate);
        }
    }

    // Get the mix output (sum of all active voices, normalized)
    float getMixOutput() const {
        if (activeCount <= 0) return 0.f;

        float sum = 0.f;
        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            if (active[i]) {
                sum += outputs[i];
            }
        }

        // Normalize by number of active voices to prevent clipping
        return sum / (float)activeCount;
    }

    // Get the mix output without normalization (raw sum)
    float getMixOutputRaw() const {
        float sum = 0.f;
        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            if (active[i]) {
                sum += outputs[i];
            }
        }
        return sum;
    }

    // Reset all voices
    void reset() {
        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            voices[i].phaseAccum.reset();
            voices[i].scanner = WavetableScanner{};
            outputs[i] = 0.f;
            active[i] = false;
        }
        activeCount = 0;
    }

private:
    // Polyphonic mode: each input channel drives one voice
    void processPolyphonic(const WavetableBank& bank,
                           const float* voctInputs,
                           const float* gateInputs,
                           int numInputChannels,
                           float position,
                           float sampleRate) {
        activeCount = 0;

        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            if (i < numInputChannels && gateInputs[i] > 0.f) {
                float freq = PhaseAccumulator::voltToFreq(voctInputs[i]);
                outputs[i] = voices[i].process(bank, position, freq, sampleRate);
                active[i] = true;
                activeCount++;
            } else {
                outputs[i] = 0.f;
                active[i] = false;
            }
        }
    }

    // Unison mode: all 8 voices on one pitch with detune spread
    void processUnison(const WavetableBank& bank,
                       const float* voctInputs,
                       const float* gateInputs,
                       int numInputChannels,
                       float position,
                       float sampleRate) {

        // Use channel 0 for pitch and gate
        float baseVoct = (numInputChannels > 0) ? voctInputs[0] : 0.f;
        bool gateActive = (numInputChannels > 0) ? (gateInputs[0] > 0.f) : false;

        if (!gateActive) {
            for (int i = 0; i < MAX_POLY_VOICES; i++) {
                outputs[i] = 0.f;
                active[i] = false;
            }
            activeCount = 0;
            return;
        }

        activeCount = MAX_POLY_VOICES;

        // Distribute detune symmetrically across voices
        // Voice 0 gets -maxDetune, voice 7 gets +maxDetune
        // Detune in cents: 100 cents = 1 semitone = 1/12 volt
        float maxDetuneVolts = unisonDetuneCents / 100.f / 12.f;

        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            // Spread: -1..+1 across voices (linear)
            float spread = (MAX_POLY_VOICES > 1)
                ? (2.f * i / (MAX_POLY_VOICES - 1) - 1.f)
                : 0.f;

            float voct = baseVoct + spread * maxDetuneVolts;
            float freq = PhaseAccumulator::voltToFreq(voct);

            outputs[i] = voices[i].process(bank, position, freq, sampleRate);
            active[i] = true;
        }
    }
};
