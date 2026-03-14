# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../Rack-SDK

# FLAGS will be passed to both the C and C++ compiler
FLAGS +=
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

# Installation directories for VCV Rack 2
PLUGIN_SLUG := $(shell jq -r .slug plugin.json)
ifeq ($(ARCH_OS),mac)
    RACK_PLUGINS_DIR ?= $(HOME)/Library/Application Support/Rack2/plugins-mac-$(ARCH_CPU)
else ifeq ($(ARCH_OS),win)
    RACK_PLUGINS_DIR ?= $(USERPROFILE)/Documents/Rack2/plugins-win-$(ARCH_CPU)
else
    RACK_PLUGINS_DIR ?= $(HOME)/.Rack2/plugins-lin-$(ARCH_CPU)
endif

.PHONY: install
install: dist
	mkdir -p "$(RACK_PLUGINS_DIR)"
	rm -rf "$(RACK_PLUGINS_DIR)/$(PLUGIN_SLUG)"
	cp -r dist/$(PLUGIN_SLUG) "$(RACK_PLUGINS_DIR)/"
	@echo "Installed $(PLUGIN_SLUG) to $(RACK_PLUGINS_DIR)"

# Test target: build and run all standalone DSP tests (no Rack SDK dependency)
.PHONY: test test-dsp test-poly
test: test-dsp test-poly

test-dsp:
	g++ -std=c++17 -O2 -I src -o tests/test_dsp_foundation tests/test_dsp_foundation.cpp -lm
	./tests/test_dsp_foundation

test-poly:
	g++ -std=c++17 -O2 -I src -o tests/test_polyphonic_routing tests/test_polyphonic_routing.cpp -lm
	./tests/test_polyphonic_routing
