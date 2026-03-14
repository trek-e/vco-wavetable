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
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		POSITION_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	// Single voice for now — polyphonic routing comes in S02
	WavetableVoice voice;

	Hurricane8VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(WAVETABLE_PARAM, 0.f, 31.f, 0.f, "Wavetable Bank");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Wavetable Position", "%", 0.f, 100.f);
		configSwitch(SCAN_MODE_PARAM, 0.f, 1.f, 0.f, "Scan Mode", {"Stepped", "Interpolated"});
		configSwitch(DAC_DEPTH_PARAM, 0.f, 2.f, 0.f, "DAC Bit Depth", {"8-bit", "12-bit", "16-bit"});

		configInput(VOCT_INPUT, "V/Oct");
		configInput(POSITION_CV_INPUT, "Position CV");

		configOutput(AUDIO_OUTPUT, "Audio");

		ensureWavetables();
	}

	void process(const ProcessArgs& args) override {
		// Read parameters
		int table = (int)std::round(params[WAVETABLE_PARAM].getValue());
		float position = params[POSITION_PARAM].getValue();
		int scanMode = (int)std::round(params[SCAN_MODE_PARAM].getValue());
		int dacDepth = (int)std::round(params[DAC_DEPTH_PARAM].getValue());

		// Position CV (±5V adds ±0.5 to position)
		if (inputs[POSITION_CV_INPUT].isConnected()) {
			position += inputs[POSITION_CV_INPUT].getVoltage() * 0.1f;
			position = std::max(0.f, std::min(1.f, position));
		}

		// 1V/oct pitch
		float voct = inputs[VOCT_INPUT].getVoltage();
		float freq = PhaseAccumulator::voltToFreq(voct);

		// Configure voice
		voice.currentTable = table;
		voice.scanMode = (scanMode == 0) ? ScanMode::Stepped : ScanMode::Interpolated;
		voice.dacDepth = static_cast<DacEmulator::Depth>(dacDepth);

		// Process
		float out = voice.process(wavetableBank, position, freq, args.sampleRate);

		// Output at ±5V
		outputs[AUDIO_OUTPUT].setVoltage(out * 5.f);
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

		// Inputs
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15, 80)), module, Hurricane8VCO::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35, 80)), module, Hurricane8VCO::POSITION_CV_INPUT));

		// Output
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(55, 80)), module, Hurricane8VCO::AUDIO_OUTPUT));
	}
};

Model* modelHurricane8VCO = createModel<Hurricane8VCO, Hurricane8VCOWidget>("Hurricane8VCO");
