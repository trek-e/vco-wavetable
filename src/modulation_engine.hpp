/*
 * Hurricane 8 — Modulation Engine
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * Per-voice and global modulation sources for wavetable position control:
 *   - ASR (Attack-Sustain-Release) envelope → wavetable position
 *   - Start position control with CV
 *   - Internal sine LFO → vibrato (pitch modulation)
 *   - Glide (portamento) → pitch smoothing
 *
 * No VCV Rack SDK dependency — testable standalone.
 */

#pragma once

#include <cmath>
#include <algorithm>

static constexpr float MOD_PI = 3.14159265358979323846f;
static constexpr int MOD_MAX_VOICES = 8;

// ─────────────────────────────────────────────────────────
// ASR Envelope — Attack / Sustain / Release
// ─────────────────────────────────────────────────────────
// Output: 0..1 envelope value for wavetable position modulation.
// Sustain is full (1.0) — the envelope holds at peak while gate is high.

enum class EnvelopeStage {
    Idle,
    Attack,
    Sustain,
    Release
};

struct AsrEnvelope {
    EnvelopeStage stage = EnvelopeStage::Idle;
    float level = 0.f;        // Current envelope output (0..1)
    float attackRate = 0.f;    // Per-sample increment during attack
    float releaseRate = 0.f;   // Per-sample decrement during release
    bool lastGate = false;     // Edge detection

    // Set attack time in seconds. Minimum 1 sample to prevent division by zero.
    void setAttack(float seconds, float sampleRate) {
        if (seconds <= 0.f || sampleRate <= 0.f) {
            attackRate = 1.f; // instant
        } else {
            float samples = seconds * sampleRate;
            attackRate = 1.f / std::max(1.f, samples);
        }
    }

    // Set release time in seconds
    void setRelease(float seconds, float sampleRate) {
        if (seconds <= 0.f || sampleRate <= 0.f) {
            releaseRate = 1.f; // instant
        } else {
            float samples = seconds * sampleRate;
            releaseRate = 1.f / std::max(1.f, samples);
        }
    }

    // Process one sample. gate=true while key is held.
    float process(bool gate) {
        // Edge detection: gate rising
        if (gate && !lastGate) {
            stage = EnvelopeStage::Attack;
        }
        // Gate falling
        if (!gate && lastGate) {
            stage = EnvelopeStage::Release;
        }
        lastGate = gate;

        switch (stage) {
            case EnvelopeStage::Idle:
                level = 0.f;
                break;

            case EnvelopeStage::Attack:
                level += attackRate;
                if (level >= 1.f) {
                    level = 1.f;
                    stage = EnvelopeStage::Sustain;
                }
                break;

            case EnvelopeStage::Sustain:
                level = 1.f;
                if (!gate) {
                    stage = EnvelopeStage::Release;
                }
                break;

            case EnvelopeStage::Release:
                level -= releaseRate;
                if (level <= 0.f) {
                    level = 0.f;
                    stage = EnvelopeStage::Idle;
                }
                break;
        }

        return level;
    }

    void reset() {
        stage = EnvelopeStage::Idle;
        level = 0.f;
        lastGate = false;
    }
};

// ─────────────────────────────────────────────────────────
// Sine LFO — vibrato source
// ─────────────────────────────────────────────────────────
// Output: bipolar (-1..+1) sine wave at configurable rate.
// Applied as pitch modulation (vibrato) in V/Oct space.

struct SineLfo {
    float phase = 0.f;   // 0..1 phase accumulator

    // Process one sample. Returns bipolar output (-1..+1).
    // rateHz: LFO frequency in Hz
    // sampleRate: audio sample rate
    float process(float rateHz, float sampleRate) {
        if (rateHz <= 0.f || sampleRate <= 0.f) return 0.f;

        float delta = rateHz / sampleRate;
        phase += delta;
        phase -= std::floor(phase);

        return std::sin(2.f * MOD_PI * phase);
    }

    void reset() { phase = 0.f; }
};

// ─────────────────────────────────────────────────────────
// Glide (Portamento) — exponential pitch smoothing
// ─────────────────────────────────────────────────────────
// Uses a one-pole filter on V/Oct input. Exponential response
// gives constant time per octave traversal — musically correct.

struct GlideProcessor {
    float currentVoct = 0.f;
    bool initialized = false;

    // glideAmount: 0..1 where 0 = instant, 1 = very slow
    // Internally mapped to a coefficient: higher amount = slower glide
    float process(float targetVoct, float glideAmount) {
        if (!initialized) {
            currentVoct = targetVoct;
            initialized = true;
            return currentVoct;
        }

        if (glideAmount <= 0.f) {
            currentVoct = targetVoct;
            return currentVoct;
        }

        // One-pole filter coefficient
        // glideAmount 0..1 maps to coefficient 0..0.9999
        // coeff near 1 = slow glide, coeff near 0 = fast glide
        float coeff = glideAmount * 0.9999f;
        currentVoct = currentVoct * coeff + targetVoct * (1.f - coeff);

        return currentVoct;
    }

    void reset() {
        currentVoct = 0.f;
        initialized = false;
    }
};

// ─────────────────────────────────────────────────────────
// Envelope Mode — per-voice vs global
// ─────────────────────────────────────────────────────────

enum class EnvelopeMode {
    PerVoice,   // Each voice has its own ASR envelope
    Global      // One ASR envelope applied to all voices
};

// ─────────────────────────────────────────────────────────
// ModulationEngine — combines all modulation sources
// ─────────────────────────────────────────────────────────
// One instance per module. Manages per-voice and global modulation.

struct ModulationEngine {
    // Per-voice ASR envelopes (MOD-01)
    AsrEnvelope perVoiceEnvelopes[MOD_MAX_VOICES];

    // Global ASR envelope (MOD-02)
    AsrEnvelope globalEnvelope;

    // Envelope mode switch (MOD-03)
    EnvelopeMode envelopeMode = EnvelopeMode::PerVoice;

    // Wavetable start position (MOD-04) — base position before envelope
    float startPosition = 0.f;     // 0..1 from knob
    float startPositionCV = 0.f;   // ±5V CV, scaled to ±0.5

    // Internal LFO (MOD-05, MOD-06)
    SineLfo lfo;
    float lfoRateHz = 5.f;         // LFO rate in Hz
    float lfoRateCV = 0.f;         // CV for rate (not used in DSP — set externally)
    float lfoAmount = 0.f;         // LFO depth (0..1), maps to V/Oct modulation depth
    float lfoAmountCV = 0.f;       // CV for amount

    // Glide (MOD-07)
    GlideProcessor glideProcessors[MOD_MAX_VOICES];
    float glideAmount = 0.f;       // 0..1 (0 = off, 1 = maximum glide)
    float glideAmountCV = 0.f;     // CV for glide amount

    // ASR time parameters
    float attackTime = 0.1f;       // seconds
    float releaseTime = 0.3f;      // seconds

    // ─── Process modulation for all voices ───

    // Call once per sample to advance the LFO (shared across voices)
    void processLfo(float sampleRate) {
        lfoOutput = lfo.process(lfoRateHz, sampleRate);
    }

    // Get the effective wavetable position for a voice after envelope modulation
    // basePosition: the "raw" position from knob + position CV (already combined externally)
    // voiceIndex: which voice (0..7)
    // gate: is this voice's gate active?
    // sampleRate: audio sample rate
    float getModulatedPosition(int voiceIndex, bool gate, float sampleRate) {
        voiceIndex = std::max(0, std::min(voiceIndex, MOD_MAX_VOICES - 1));

        // Start position (MOD-04): base + CV
        float base = startPosition + startPositionCV * 0.1f; // ±5V → ±0.5
        base = std::max(0.f, std::min(1.f, base));

        // Envelope contribution
        float envValue = 0.f;
        if (envelopeMode == EnvelopeMode::PerVoice) {
            // Per-voice: update and use this voice's envelope
            perVoiceEnvelopes[voiceIndex].setAttack(attackTime, sampleRate);
            perVoiceEnvelopes[voiceIndex].setRelease(releaseTime, sampleRate);
            envValue = perVoiceEnvelopes[voiceIndex].process(gate);
        } else {
            // Global: envelope was already advanced — just read the value
            envValue = globalEnvelope.level;
        }

        // Envelope modulates position: start + envelope * (1 - start)
        // At env=0, position = start. At env=1, position = 1.0.
        // This sweeps from start position to the end of the wavetable.
        float position = base + envValue * (1.f - base);

        return std::max(0.f, std::min(1.f, position));
    }

    // Advance the global envelope. Call once per sample with the "any gate active" signal.
    void processGlobalEnvelope(bool anyGateActive, float sampleRate) {
        globalEnvelope.setAttack(attackTime, sampleRate);
        globalEnvelope.setRelease(releaseTime, sampleRate);
        globalEnvelope.process(anyGateActive);
    }

    // Get pitch modulation from LFO (vibrato) in V/Oct offset
    // lfoAmount 0..1 maps to 0..1 semitones of vibrato depth
    float getVibrato() const {
        // Max depth: 1 semitone = 1/12 volt
        float depth = lfoAmount / 12.f;
        return lfoOutput * depth;
    }

    // Get glide-smoothed V/Oct for a voice
    float getGlidedVoct(int voiceIndex, float targetVoct) {
        voiceIndex = std::max(0, std::min(voiceIndex, MOD_MAX_VOICES - 1));
        float effectiveGlide = std::max(0.f, std::min(1.f, glideAmount));
        return glideProcessors[voiceIndex].process(targetVoct, effectiveGlide);
    }

    // Reset all modulation state
    void reset() {
        for (int i = 0; i < MOD_MAX_VOICES; i++) {
            perVoiceEnvelopes[i].reset();
            glideProcessors[i].reset();
        }
        globalEnvelope.reset();
        lfo.reset();
        lfoOutput = 0.f;
    }

private:
    float lfoOutput = 0.f;  // Cached LFO output for the current sample
};
