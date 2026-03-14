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
#include "modulation_engine.hpp"
#include "wav_loader.hpp"
#include <osdialog.h>

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
		// Modulation params (MOD-01..07)
		START_POSITION_PARAM,  // Wavetable start position (0-1)
		ATTACK_PARAM,          // ASR attack time (0-2s)
		RELEASE_PARAM,         // ASR release time (0-2s)
		ENV_MODE_PARAM,        // Per-voice (0) / Global (1)
		LFO_RATE_PARAM,        // LFO rate (0.1-20 Hz)
		LFO_AMOUNT_PARAM,      // LFO depth (0-1)
		GLIDE_PARAM,           // Glide amount (0-1)
		PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,
		GATE_INPUT,
		VELOCITY_INPUT,     // Polyphonic velocity CV input (OUT-06)
		POSITION_CV_INPUT,
		// Modulation CV inputs
		START_POSITION_CV_INPUT,
		LFO_RATE_CV_INPUT,
		LFO_AMOUNT_CV_INPUT,
		GLIDE_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,       // Polyphonic audio (up to 8 channels)
		GATE_OUTPUT,        // Polyphonic gate passthrough
		MIX_OUTPUT,         // Mono mix of all voices
		VELOCITY_OUTPUT,    // Polyphonic velocity CV passthrough (OUT-06)
		// Individual per-voice audio outputs (OUT-04)
		VOICE1_OUTPUT, VOICE2_OUTPUT, VOICE3_OUTPUT, VOICE4_OUTPUT,
		VOICE5_OUTPUT, VOICE6_OUTPUT, VOICE7_OUTPUT, VOICE8_OUTPUT,
		// Individual per-voice gate outputs (OUT-05)
		GATE1_OUTPUT, GATE2_OUTPUT, GATE3_OUTPUT, GATE4_OUTPUT,
		GATE5_OUTPUT, GATE6_OUTPUT, GATE7_OUTPUT, GATE8_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	PolyphonicEngine polyEngine;
	ModulationEngine modEngine;

	// User wavetable state (WTD-02..05)
	std::string userWavetablePath;         // Empty = using PPG ROM tables
	WavetableBank userBank;                // User wavetable bank (all slots = same table)
	bool userTableLoaded = false;          // True when a user wavetable is active
	int userTableNumFrames = 0;            // Number of frames in the user wavetable

	Hurricane8VCO() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configParam(WAVETABLE_PARAM, 0.f, 31.f, 0.f, "Wavetable Bank");
		configParam(POSITION_PARAM, 0.f, 1.f, 0.f, "Wavetable Position", "%", 0.f, 100.f);
		configSwitch(SCAN_MODE_PARAM, 0.f, 1.f, 0.f, "Scan Mode", {"Stepped", "Interpolated"});
		configSwitch(DAC_DEPTH_PARAM, 0.f, 2.f, 0.f, "DAC Bit Depth", {"8-bit", "12-bit", "16-bit"});
		configSwitch(VOICE_MODE_PARAM, 0.f, 1.f, 0.f, "Voice Mode", {"Polyphonic", "Unison"});
		configParam(DETUNE_PARAM, 0.f, 100.f, 20.f, "Unison Detune", " cents");

		// Modulation controls (MOD-01..07)
		configParam(START_POSITION_PARAM, 0.f, 1.f, 0.f, "Start Position", "%", 0.f, 100.f);
		configParam(ATTACK_PARAM, 0.001f, 2.f, 0.1f, "Envelope Attack", " s");
		configParam(RELEASE_PARAM, 0.001f, 2.f, 0.3f, "Envelope Release", " s");
		configSwitch(ENV_MODE_PARAM, 0.f, 1.f, 0.f, "Envelope Mode", {"Per-Voice", "Global"});
		configParam(LFO_RATE_PARAM, 0.1f, 20.f, 5.f, "LFO Rate", " Hz");
		configParam(LFO_AMOUNT_PARAM, 0.f, 1.f, 0.f, "LFO Amount (Vibrato)", "%", 0.f, 100.f);
		configParam(GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(VELOCITY_INPUT, "Velocity");
		configInput(POSITION_CV_INPUT, "Position CV");
		configInput(START_POSITION_CV_INPUT, "Start Position CV");
		configInput(LFO_RATE_CV_INPUT, "LFO Rate CV");
		configInput(LFO_AMOUNT_CV_INPUT, "LFO Amount CV");
		configInput(GLIDE_CV_INPUT, "Glide CV");

		configOutput(AUDIO_OUTPUT, "Polyphonic Audio");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(MIX_OUTPUT, "Mix");
		configOutput(VELOCITY_OUTPUT, "Velocity");

		// Individual per-voice outputs (OUT-04, OUT-05)
		const char* voiceNames[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
		for (int i = 0; i < 8; i++) {
			configOutput(VOICE1_OUTPUT + i, std::string("Voice ") + voiceNames[i]);
			configOutput(GATE1_OUTPUT + i, std::string("Gate ") + voiceNames[i]);
		}

		ensureWavetables();
	}

	// Load a user wavetable from a WAV file path (WTD-02, WTD-03)
	void loadUserWavetableFromFile(const std::string& path) {
		UserWavetable loaded = loadUserWavetable(path);
		if (loaded.valid) {
			// Fill all 32 bank slots with the user table so any table index works
			for (int t = 0; t < PPG_NUM_TABLES; t++) {
				userBank.tables[t] = loaded.table;
			}
			userBank.initialized = true;
			userTableNumFrames = loaded.numFrames;
			userWavetablePath = path;
			userTableLoaded = true;
		}
	}

	// Clear user wavetable — revert to PPG ROM tables
	void clearUserWavetable() {
		userWavetablePath.clear();
		userTableLoaded = false;
		userTableNumFrames = 0;
	}

	// JSON serialization — persist user wavetable path (WTD-05)
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		if (!userWavetablePath.empty()) {
			json_object_set_new(rootJ, "userWavetablePath",
				json_string(userWavetablePath.c_str()));
		}
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* pathJ = json_object_get(rootJ, "userWavetablePath");
		if (pathJ && json_is_string(pathJ)) {
			std::string path = json_string_value(pathJ);
			if (!path.empty()) {
				loadUserWavetableFromFile(path);
			}
		}
	}

	void process(const ProcessArgs& args) override {
		// Read parameters
		int table = (int)std::round(params[WAVETABLE_PARAM].getValue());
		float position = params[POSITION_PARAM].getValue();
		int scanMode = (int)std::round(params[SCAN_MODE_PARAM].getValue());
		int dacDepth = (int)std::round(params[DAC_DEPTH_PARAM].getValue());
		int voiceMode = (int)std::round(params[VOICE_MODE_PARAM].getValue());
		float detune = params[DETUNE_PARAM].getValue();

		// Modulation parameters
		float startPos = params[START_POSITION_PARAM].getValue();
		float attack = params[ATTACK_PARAM].getValue();
		float release = params[RELEASE_PARAM].getValue();
		int envMode = (int)std::round(params[ENV_MODE_PARAM].getValue());
		float lfoRate = params[LFO_RATE_PARAM].getValue();
		float lfoAmount = params[LFO_AMOUNT_PARAM].getValue();
		float glide = params[GLIDE_PARAM].getValue();

		// Apply modulation CVs
		if (inputs[START_POSITION_CV_INPUT].isConnected()) {
			modEngine.startPositionCV = inputs[START_POSITION_CV_INPUT].getVoltage();
		} else {
			modEngine.startPositionCV = 0.f;
		}
		if (inputs[LFO_RATE_CV_INPUT].isConnected()) {
			// ±5V adds ±10 Hz to rate
			lfoRate += inputs[LFO_RATE_CV_INPUT].getVoltage() * 2.f;
			lfoRate = std::max(0.1f, std::min(20.f, lfoRate));
		}
		if (inputs[LFO_AMOUNT_CV_INPUT].isConnected()) {
			// ±5V adds ±0.5 to amount
			lfoAmount += inputs[LFO_AMOUNT_CV_INPUT].getVoltage() * 0.1f;
			lfoAmount = std::max(0.f, std::min(1.f, lfoAmount));
		}
		if (inputs[GLIDE_CV_INPUT].isConnected()) {
			// ±5V adds ±0.5 to glide
			glide += inputs[GLIDE_CV_INPUT].getVoltage() * 0.1f;
			glide = std::max(0.f, std::min(1.f, glide));
		}

		// Configure modulation engine
		modEngine.startPosition = startPos;
		modEngine.attackTime = attack;
		modEngine.releaseTime = release;
		modEngine.envelopeMode = (envMode == 0) ? EnvelopeMode::PerVoice : EnvelopeMode::Global;
		modEngine.lfoRateHz = lfoRate;
		modEngine.lfoAmount = lfoAmount;
		modEngine.glideAmount = glide;

		// Position CV (±5V adds ±0.5 to position — legacy, still works alongside start position)
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

		// Process modulation (LFO runs once per sample, shared across voices)
		modEngine.processLfo(args.sampleRate);

		// Global envelope: triggered by any active gate
		bool anyGateActive = false;
		for (int i = 0; i < numInputChannels && i < MAX_POLY_VOICES; i++) {
			if (gateInputs[i] > 0.f) { anyGateActive = true; break; }
		}
		modEngine.processGlobalEnvelope(anyGateActive, args.sampleRate);

		// Apply per-voice modulation: glide, vibrato, envelope → position
		float modulatedVocts[MAX_POLY_VOICES] = {};
		float modulatedPositions[MAX_POLY_VOICES] = {};

		for (int i = 0; i < numInputChannels && i < MAX_POLY_VOICES; i++) {
			// Glide + vibrato on pitch
			float glidedVoct = modEngine.getGlidedVoct(i, voctInputs[i]);
			modulatedVocts[i] = glidedVoct + modEngine.getVibrato();

			// Envelope → wavetable position
			bool gate = gateInputs[i] > 0.f;
			modulatedPositions[i] = modEngine.getModulatedPosition(i, gate, args.sampleRate);
		}

		// Configure engine
		polyEngine.currentTable = table;
		polyEngine.scanMode = (scanMode == 0) ? ScanMode::Stepped : ScanMode::Interpolated;
		polyEngine.dacDepth = static_cast<DacEmulator::Depth>(dacDepth);
		polyEngine.voiceMode = (voiceMode == 0) ? VoiceMode::Polyphonic : VoiceMode::Unison;
		polyEngine.unisonDetuneCents = detune;

		// Select wavetable bank: user wavetable if loaded, otherwise PPG ROM
		const WavetableBank& activeBank = userTableLoaded ? userBank : wavetableBank;

		// Process all voices with modulated inputs
		// Note: we pass the modulated position of voice 0 as the "base" position
		// since the polyEngine expects a single position. Per-voice position
		// modulation happens via the envelope which is already applied.
		float effectivePosition = modulatedPositions[0];
		polyEngine.process(activeBank, modulatedVocts, gateInputs,
		                   numInputChannels, effectivePosition, args.sampleRate);

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

		// Velocity passthrough (OUT-06)
		if (inputs[VELOCITY_INPUT].isConnected()) {
			int velChannels = inputs[VELOCITY_INPUT].getChannels();
			outputs[VELOCITY_OUTPUT].setChannels(velChannels);
			for (int i = 0; i < velChannels; i++) {
				outputs[VELOCITY_OUTPUT].setVoltage(
					inputs[VELOCITY_INPUT].getPolyVoltage(i), i);
			}
		} else {
			outputs[VELOCITY_OUTPUT].setChannels(1);
			outputs[VELOCITY_OUTPUT].setVoltage(0.f, 0);
		}

		// Individual per-voice audio outputs (OUT-04)
		for (int i = 0; i < MAX_POLY_VOICES; i++) {
			outputs[VOICE1_OUTPUT + i].setChannels(1);
			outputs[VOICE1_OUTPUT + i].setVoltage(polyEngine.outputs[i] * 5.f, 0);
		}

		// Individual per-voice gate outputs (OUT-05)
		for (int i = 0; i < MAX_POLY_VOICES; i++) {
			outputs[GATE1_OUTPUT + i].setChannels(1);
			if (inputs[GATE_INPUT].isConnected()) {
				if (polyEngine.voiceMode == VoiceMode::Unison) {
					// Unison: all individual gates mirror channel 0
					outputs[GATE1_OUTPUT + i].setVoltage(
						inputs[GATE_INPUT].getPolyVoltage(0), 0);
				} else {
					outputs[GATE1_OUTPUT + i].setVoltage(
						inputs[GATE_INPUT].getPolyVoltage(i), 0);
				}
			} else {
				outputs[GATE1_OUTPUT + i].setVoltage(0.f, 0);
			}
		}
	}
};

struct Hurricane8VCOWidget : ModuleWidget {
	Hurricane8VCOWidget(Hurricane8VCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Hurricane8VCO.svg")));

		// Screws (28HP panel)
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// ===== Oscillator Section (y≈22) =====
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(15, 22)), module, Hurricane8VCO::WAVETABLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 22)), module, Hurricane8VCO::POSITION_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(52, 22)), module, Hurricane8VCO::SCAN_MODE_PARAM));
		addParam(createParamCentered<RoundBlackSnapKnob>(mm2px(Vec(67, 22)), module, Hurricane8VCO::DAC_DEPTH_PARAM));

		// ===== Voice Section (y≈37) =====
		addParam(createParamCentered<CKSS>(mm2px(Vec(15, 37)), module, Hurricane8VCO::VOICE_MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 37)), module, Hurricane8VCO::DETUNE_PARAM));

		// ===== Modulation Section (y≈52, 67) =====
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 52)), module, Hurricane8VCO::START_POSITION_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 52)), module, Hurricane8VCO::ATTACK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52, 52)), module, Hurricane8VCO::RELEASE_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(67, 52)), module, Hurricane8VCO::ENV_MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 67)), module, Hurricane8VCO::LFO_RATE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35, 67)), module, Hurricane8VCO::LFO_AMOUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(52, 67)), module, Hurricane8VCO::GLIDE_PARAM));

		// ===== Inputs (y≈84, 97) =====
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 84)), module, Hurricane8VCO::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 84)), module, Hurricane8VCO::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40, 84)), module, Hurricane8VCO::VELOCITY_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 84)), module, Hurricane8VCO::POSITION_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 97)), module, Hurricane8VCO::START_POSITION_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25, 97)), module, Hurricane8VCO::LFO_RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40, 97)), module, Hurricane8VCO::LFO_AMOUNT_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(55, 97)), module, Hurricane8VCO::GLIDE_CV_INPUT));

		// ===== Polyphonic Outputs (y≈115) =====
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 115)), module, Hurricane8VCO::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25, 115)), module, Hurricane8VCO::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(40, 115)), module, Hurricane8VCO::MIX_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(55, 115)), module, Hurricane8VCO::VELOCITY_OUTPUT));

		// ===== Individual Voice Outputs (right side, OUT-04 / OUT-05) =====
		// 8 audio jacks in a column at x=100, 8 gate jacks at x=125
		// Spaced evenly from y=19 to y=92.5 (10.5mm apart)
		for (int i = 0; i < 8; i++) {
			float y = 19.f + i * 10.5f;
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(100, y)), module, Hurricane8VCO::VOICE1_OUTPUT + i));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(125, y)), module, Hurricane8VCO::GATE1_OUTPUT + i));
		}
	}

	// Right-click context menu for loading user wavetables (WTD-04)
	void appendContextMenu(ui::Menu* menu) override {
		Hurricane8VCO* module = dynamic_cast<Hurricane8VCO*>(this->module);
		if (!module) return;

		menu->addChild(new ui::MenuSeparator);

		// Show current wavetable source
		if (module->userTableLoaded) {
			// Extract filename from path for display
			std::string filename = module->userWavetablePath;
			size_t lastSlash = filename.find_last_of("/\\");
			if (lastSlash != std::string::npos)
				filename = filename.substr(lastSlash + 1);

			menu->addChild(createMenuLabel("Wavetable: " + filename +
				" (" + std::to_string(module->userTableNumFrames) + " frames)"));
		} else {
			menu->addChild(createMenuLabel("Wavetable: PPG ROM (built-in)"));
		}

		// Load wavetable item
		menu->addChild(createMenuItem("Load wavetable...", "",
			[=]() {
				osdialog_filters* filters = osdialog_filters_parse("WAV files:wav");
				char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
				if (pathC) {
					module->loadUserWavetableFromFile(std::string(pathC));
					free(pathC);
				}
				osdialog_filters_free(filters);
			}
		));

		// Revert to built-in wavetables
		if (module->userTableLoaded) {
			menu->addChild(createMenuItem("Revert to PPG ROM wavetables", "",
				[=]() {
					module->clearUserWavetable();
				}
			));
		}
	}
};

Model* modelHurricane8VCO = createModel<Hurricane8VCO, Hurricane8VCOWidget>("Hurricane8VCO");
