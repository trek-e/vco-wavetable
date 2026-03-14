/*
 * Hurricane 8 — Plugin Registration
 * Copyright (C) 2026 Synth-etic Intelligence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "plugin.hpp"

using namespace rack;

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelHurricane8VCO);
}
