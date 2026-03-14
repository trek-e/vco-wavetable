#include "plugin.hpp"

using namespace rack;

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	p->addModel(modelHurricane8VCO);
}
