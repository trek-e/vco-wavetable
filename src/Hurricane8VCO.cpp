/*
 * Hurricane 8 VCO - 8-voice polyphonic PPG-style wavetable oscillator for VCV Rack
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "plugin.hpp"
#include "ppg_wavetables.hpp"
#include "wavetable_engine.hpp"
#include "polyphonic_engine.hpp"

using namespace rack;

// Shared wavetable data — generated once at plugin init, used by all instances
static PpgWavetableRom ppgRom;
static WavetableBank wavetableBank;
static bool wavetablesReady = false;

static void ensureWavetables() {
	if (!wavetablesReady) {
		ppgRom.generate();
		wavetableBank.buildFromRom(ppgRom);
		wavetablesReady = true;
	}
}

struct Hurricane8VCO : Module {
	enum ParamId {
		WAVETABLE_PARAM,    // Which wavetable bank (0-31)
		POSITION_PARAM,     // Wavetable frame position (0-1)
		SCAN_MODE_PARAM,    // Stepped (0) / Interpolated (1)
		DAC_DEPTH_PARAM,    // 8-bit (0) / 12-bit (1) / 16-bit (2)
		VOICE_MODE_PARAM,   // Polyphonic (0) / Unison (1)
		DETUNE_PARAM,       // Unison detune spread in cents (0-100)
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		GATE_INPUT,
		POSITION_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,       // Polyphonic audio (up to 8 channels)
		GATE_OUTPUT,        // Polyphonic gate passthrough
		MIX_OUTPUT,         // Mono mix of all voices
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	PolyphonicEngine polyEngine;

	Hurricane8VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(WAVETABLE_PARAM, 0.f, 31.f, 0.f, "Wavetable Bank");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Wavetable Position", "%", 0.f, 100.f);
		configSwitch(SCAN_MODE_PARAM, 0.f, 1.f, 0.f, "Scan Mode", {"Stepped", "Interpolated"});
		configSwitch(DAC_DEPTH_PARAM, 0.f, 2.f, 0.f, "DAC Bit Depth", {"8-bit", "12-bit", "16-bit"});
		configSwitch(VOICE_MODE_PARAM, 0.f, 1.f, 0.f, "Voice Mode", {"Polyphonic", "Unison"});
		configParam(DETUNE_PARAM, 0.f, 100.f, 20.f, "Unison Detune", " cents");

		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(POSITION_CV_INPUT, "Position CV");

		configOutput(AUDIO_OUTPUT, "Polyphonic Audio");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(MIX_OUTPUT, "Mix");

		ensureWavetables();
	}

	void process(const ProcessArgs& args) override {
		// Read parameters
		int table = (int)std::round(params[WAVETABLE_PARAM].getValue());
		float position = params[POSITION_PARAM].getValue();
		int scanMode = (int)std::round(params[SCAN_MODE_PARAM].getValue());
		int dacDepth = (int)std::round(params[DAC_DEPTH_PARAM].getValue());
		int voiceMode = (int)std::round(params[VOICE_MODE_PARAM].getValue());
		float detune = params[DETUNE_PARAM].getValue();

		// Position CV (±5V adds ±0.5 to position)
		if (inputs[POSITION_CV_INPUT].isConnected()) {
			position += inputs[POSITION_CV_INPUT].getVoltage() * 0.1f;
			position = std::max(0.f, std::min(1.f, position));
		}

		// Read polyphonic inputs
		int numVoctChannels = inputs[VOCT_INPUT].getChannels();
		int numGateChannels = inputs[GATE_INPUT].getChannels();
		int numInputChannels = std::max(numVoctChannels, numGateChannels);
		if (numInputChannels < 1) numInputChannels = 1;  // Always at least 1 voice

		float voctInputs[MAX_POLY_VOICES] = {};
		float gateInputs[MAX_POLY_VOICES] = {};

		for (int i = 0; i < numInputChannels && i < MAX_POLY_VOICES; i++) {
			voctInputs[i] = inputs[VOCT_INPUT].getPolyVoltage(i);
			// If gate is not connected, treat all V/Oct channels as active
			if (inputs[GATE_INPUT].isConnected()) {
				gateInputs[i] = inputs[GATE_INPUT].getPolyVoltage(i);
			} else {
				gateInputs[i] = 10.f;  // Default active
			}
		}

		// Configure engine
		polyEngine.currentTable = table;
		polyEngine.scanMode = (scanMode == 0) ? ScanMode::Stepped : ScanMode::Interpolated;
		polyEngine.dacDepth = static_cast<DacEmulator::Depth>(dacDepth);
		polyEngine.voiceMode = (voiceMode == 0) ? VoiceMode::Polyphonic : VoiceMode::Unison;
		polyEngine.unisonDetuneCents = detune;

		// Process all voices
		polyEngine.process(wavetableBank, voctInputs, gateInputs,
		                   numInputChannels, position, args.sampleRate);

		// Set polyphonic audio output
		int numOutputChannels;
		if (polyEngine.voiceMode == VoiceMode::Unison) {
			numOutputChannels = MAX_POLY_VOICES;
		} else {
			numOutputChannels = numInputChannels;
		}

		outputs[AUDIO_OUTPUT].setChannels(numOutputChannels);
		for (int i = 0; i < numOutputChannels; i++) {
			outputs[AUDIO_OUTPUT].setVoltage(polyEngine.outputs[i] * 5.f, i);
		}

		// Gate passthrough (polyphonic)
		if (inputs[GATE_INPUT].isConnected()) {
			int gateOutChannels = (polyEngine.voiceMode == VoiceMode::Unison)
				? 1  // Unison: single gate from channel 0
				: numGateChannels;
			outputs[GATE_OUTPUT].setChannels(gateOutChannels);
			for (int i = 0; i < gateOutChannels; i++) {
				outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getPolyVoltage(i), i);
			}
		} else {
			outputs[GATE_OUTPUT].setChannels(1);
			outputs[GATE_OUTPUT].setVoltage(0.f, 0);
		}

		// Mix output (mono sum of all active voices)
		float mix = polyEngine.getMixOutput();
		outputs[MIX_OUTPUT].setChannels(1);
		outputs[MIX_OUTPUT].setVoltage(mix * 5.f, 0);
	}
};

struct Hurricane8VCOWidget : ModuleWidget {
	Hurricane8VCOWidget(Hurricane8VCO* module) {
		setModule(module);
		// Panel SVG will be created in S05
		// For now, use a blank panel at 20HP
		box.size = Vec(20 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

		// Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Controls
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(15, 30)), module, Hurricane8VCO::WAVETABLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 30)), module, Hurricane8VCO::POSITION_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(55, 30)), module, Hurricane8VCO::SCAN_MODE_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(75, 30)), module, Hurricane8VCO::DAC_DEPTH_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(15, 50)), module, Hurricane8VCO::VOICE_MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 50)), module, Hurricane8VCO::DETUNE_PARAM));

		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15, 80)), module, Hurricane8VCO::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35, 80)), module, Hurricane8VCO::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 80)), module, Hurricane8VCO::POSITION_CV_INPUT));

		// Outputs
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 110)), module, Hurricane8VCO::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35, 110)), module, Hurricane8VCO::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(55, 110)), module, Hurricane8VCO::MIX_OUTPUT));
	}
};

Model* modelHurricane8VCO = createModel<Hurricane8VCO, Hurricane8VCOWidget>("Hurricane8VCO");
