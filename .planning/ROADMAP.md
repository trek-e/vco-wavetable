# Roadmap: Hurricane 8

## Overview

Hurricane 8 is built bottom-up, driven by a hard architectural dependency graph. The DSP engine must exist before voices, voices before polyphonic routing, routing before modulation, and the full jack count must be locked before the panel SVG can be finalized. Five phases map directly from the WavetableBank → Voice → Module → Widget build order: establish an anti-aliased, real-time-safe monophonic engine with PPG ROM; layer in correct 8-voice polyphonic routing; attach modulation (envelope, glide, LFO, unison); add user wavetable loading via off-thread WAV decode; then finalize the panel, individual voice outputs, and submit to the VCV Library.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: DSP Foundation** - Monophonic wavetable engine with PPG ROM, mip-mapped anti-aliasing, stepped/interpolated scanning, DAC emulation, and correct 1V/oct tracking
- [ ] **Phase 2: Polyphonic Routing** - 8-voice polyphonic channel handling, poly audio/gate/mix outputs, voice allocation, and polyphonic compliance
- [ ] **Phase 3: Modulation** - Per-voice ASR envelope for wavetable position, glide, internal vibrato LFO, and unison mode with detune
- [ ] **Phase 4: User Wavetables** - Off-thread WAV loading (single-cycle and multi-frame), right-click file picker, and patch-persistent state serialization
- [ ] **Phase 5: Panel and Submission** - Full SVG panel, 8 individual voice and gate outputs, velocity CV output, and VCV Library submission package

## Phase Details

### Phase 1: DSP Foundation
**Goal**: A playable monophonic wavetable oscillator that produces authentic PPG character — correct pitch, correct scanning modes, correct DAC quantization, no aliasing — validates the core sound before polyphonic plumbing is added
**Depends on**: Nothing (first phase)
**Requirements**: DSP-01, DSP-02, DSP-03, DSP-04, DSP-05, DSP-06, DSP-07, WTD-01
**Success Criteria** (what must be TRUE):
  1. Patching a V/OCT signal produces correct pitch across 10+ octaves with no drift
  2. A wavetable position knob sweeps audibly through the PPG ROM table set in real time
  3. Switching between stepped and interpolated scanning modes produces audibly different transitions between wavetables
  4. The DAC bit-depth selector produces audibly distinct quantization character at 8-bit, 12-bit, and 16-bit settings
  5. High-frequency output shows no aliasing artifacts visible on a spectrum analyzer
**Plans**: TBD

### Phase 2: Polyphonic Routing
**Goal**: All 8 voices routed correctly from polyphonic V/OCT and gate inputs to polyphonic audio, gate, and mix outputs, with compliant channel handling that passes VCV Rack polyphony requirements
**Depends on**: Phase 1
**Requirements**: VOC-01, VOC-03, OUT-01, OUT-02, OUT-03
**Success Criteria** (what must be TRUE):
  1. Connecting an 8-channel polyphonic V/OCT cable produces 8 independent pitched voices on the polyphonic audio output
  2. Each voice triggers and releases independently based on its polyphonic gate channel
  3. The mix output sums all active voices to a single signal that responds as voices are added or removed
  4. The module handles 1, 2, 4, and 8 polyphonic channels without clicks, stuck notes, or incorrect channel counts
**Plans**: TBD

### Phase 3: Modulation
**Goal**: Per-voice expressive control — wavetable position driven by ASR envelope per voice or globally, glide between pitches, vibrato from internal LFO, and wall-of-sound unison mode — making the module a complete PPG-style voice
**Depends on**: Phase 2
**Requirements**: MOD-01, MOD-02, MOD-03, MOD-04, MOD-05, MOD-06, MOD-07, VOC-02
**Success Criteria** (what must be TRUE):
  1. In per-voice envelope mode, each voice's wavetable position rises and falls independently with its own gate, producing a sweep unique to each voice
  2. In global envelope mode, all voices' wavetable positions move together from a single gate trigger
  3. Glide (portamento) causes pitch transitions to slide smoothly between notes, with CV-controllable glide time
  4. The internal LFO applies pitch modulation (vibrato) with independently CV-controllable rate and depth
  5. Unison mode stacks all 8 voices on one pitch with audible detuning spread between voices
**Plans**: TBD

### Phase 4: User Wavetables
**Goal**: Users can load their own wavetable files — both single-cycle and multi-frame Serum-format WAVs — via a right-click menu, with the loaded file persisting across patch save and reload
**Depends on**: Phase 3
**Requirements**: WTD-02, WTD-03, WTD-04, WTD-05
**Success Criteria** (what must be TRUE):
  1. Right-clicking the module shows a "Load wavetable" menu item that opens a file picker
  2. Selecting a single-cycle WAV file replaces the active wavetable set and the module plays it immediately
  3. Selecting a multi-frame WAV file (Serum-style) loads all frames and the wavetable position knob scans through them
  4. Saving a VCV Rack patch and reloading it restores the same custom wavetable file without user action
**Plans**: TBD

### Phase 5: Panel and Submission
**Goal**: A complete, submission-ready VCV Rack plugin — finalized SVG panel at correct HP width with all controls labeled and accessible, 8 individual voice and gate output jacks, velocity CV output, correct plugin.json manifest, and a clean library submission package
**Depends on**: Phase 4
**Requirements**: OUT-04, OUT-05, OUT-06, PNL-01, PNL-02, PNL-03
**Success Criteria** (what must be TRUE):
  1. All 8 individual voice audio jacks and 8 individual gate jacks produce the correct per-voice signals
  2. The polyphonic velocity CV output passes velocity data from upstream MIDI-CV correctly
  3. The panel SVG renders cleanly at 100% with all text as paths and passes VCV Library visual review
  4. The plugin builds from a clean checkout for VCV Rack 2 with no deprecated API warnings
  5. The plugin.json manifest has correct versioning, tags, and metadata required for VCV Library submission
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. DSP Foundation | 0/TBD | Not started | - |
| 2. Polyphonic Routing | 0/TBD | Not started | - |
| 3. Modulation | 0/TBD | Not started | - |
| 4. User Wavetables | 0/TBD | Not started | - |
| 5. Panel and Submission | 0/TBD | Not started | - |
