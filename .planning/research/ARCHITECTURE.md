# Architecture Research

**Domain:** VCV Rack polyphonic wavetable oscillator module (C++ / VCV Rack SDK)
**Researched:** 2026-03-01
**Confidence:** HIGH (VCV Rack SDK documentation + official API reference + verified open-source patterns)

---

## Standard Architecture

### System Overview

VCV Rack enforces a hard two-layer split between DSP and UI. The engine thread runs `process()` at audio rate. The UI thread runs the widget rendering. They share state only through the Module struct's params/inputs/outputs/lights arrays, which the engine mediates safely.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        UI THREAD (App Layer)                         │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  ModuleWidget (Hurricane8Widget)                              │   │
│  │  - SVG panel rendering                                        │   │
│  │  - ParamWidget placement (knobs, switches, buttons)           │   │
│  │  - PortWidget placement (inputs, outputs)                     │   │
│  │  - LightWidget placement                                      │   │
│  │  - appendContextMenu() for user wavetable loading             │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │  reads params[], lights[] (safe)           │
├─────────────────────────┼───────────────────────────────────────────┤
│                  VCV Rack Engine Boundary                            │
├─────────────────────────┼───────────────────────────────────────────┤
│                        AUDIO THREAD (Engine Layer)                   │
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  Module (Hurricane8)                                          │   │
│  │  process(const ProcessArgs& args)                             │   │
│  │                                                               │   │
│  │  ┌────────────────────────────────────────────────────────┐  │   │
│  │  │  Voice Manager                                          │  │   │
│  │  │  - Reads polyphonic input channels (up to 16)           │  │   │
│  │  │  - Routes to Voice[0..7] or routes unison               │  │   │
│  │  │  - Reads params, applies CV modulation                  │  │   │
│  │  └────────────────────────────────────────────────────────┘  │   │
│  │                                                               │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ... ┌──────────┐   │   │
│  │  │ Voice[0] │ │ Voice[1] │ │ Voice[2] │     │ Voice[7] │   │   │
│  │  │          │ │          │ │          │     │          │   │   │
│  │  │ - Phase  │ │ - Phase  │ │ - Phase  │     │ - Phase  │   │   │
│  │  │ - Glide  │ │ - Glide  │ │ - Glide  │     │ - Glide  │   │   │
│  │  │ - WT Env │ │ - WT Env │ │ - WT Env │     │ - WT Env │   │   │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘     └────┬─────┘   │   │
│  │       │             │             │                 │         │   │
│  │  ┌────▼─────────────▼─────────────▼─────────────────▼─────┐ │   │
│  │  │  WavetableEngine (shared read-only tables)               │ │   │
│  │  │  - Built-in PPG tables (ROM, static)                     │ │   │
│  │  │  - User-loaded tables (heap, swapped off-thread)         │ │   │
│  │  │  - Sample lookup + interpolation                         │ │   │
│  │  └─────────────────────────────────────────────────────────┘ │   │
│  │                                                               │   │
│  │  ┌─────────────────────────────────────────────────────────┐ │   │
│  │  │  Output Mixer                                            │ │   │
│  │  │  - Per-voice jacks (8x audio, 8x gate)                  │ │   │
│  │  │  - Polyphonic cable outputs (audio, gate, velocity)      │ │   │
│  │  │  - Summed mix output                                     │ │   │
│  │  └─────────────────────────────────────────────────────────┘ │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

| Component | Responsibility | Communicates With |
|-----------|----------------|-------------------|
| `Hurricane8Widget` | SVG panel, widget placement, context menu (load wavetable) | Module via params[], lights[], createMenuItem callbacks |
| `Hurricane8` (Module) | Owns all DSP state, process() entry point, voice routing | Rack engine (inputs/outputs), WavetableBank, Voice[8] |
| `Voice` struct | Single-voice DSP: phase accumulator, glide slew, WT position ASR envelope | WavetableEngine (read-only table access), SlewLimiter, AHREnvelope |
| `WavetableBank` | Stores all tables (PPG built-in + user-loaded); provides sample lookup | Voice (read-only during process), Widget (write during load, off-thread) |
| `WavetableLoader` | Loads WAV files off the audio thread, decodes frames, validates format | WavetableBank (writes new table pointer via atomic swap) |
| Output router | Distributes per-voice output to individual jacks, poly cable, and mix bus | Module inputs/outputs array |

---

## Recommended Project Structure

```
src/
├── Hurricane8.hpp          # Module and Widget declarations, param/input/output enums
├── Hurricane8.cpp          # ModuleWidget: panel layout, context menus, widget creation
├── Hurricane8Module.cpp    # Module: constructor, process(), dataToJson/dataFromJson
│
├── dsp/
│   ├── Voice.hpp           # Voice struct: all per-voice state and process()
│   ├── Voice.cpp           # Voice DSP implementation
│   ├── WavetableBank.hpp   # Table storage, sample lookup interface
│   ├── WavetableBank.cpp   # Built-in PPG ROM tables, user table management
│   ├── WavetableLoader.hpp # Off-thread WAV file loading
│   ├── WavetableLoader.cpp # WAV decode (libsamplerate or stb_vorbis-style)
│   ├── AHREnvelope.hpp     # ASR/AHR envelope for wavetable position modulation
│   └── DacEmulator.hpp     # Bit-depth quantization (8/12/16-bit DAC emulation)
│
├── ppg_tables/
│   └── PPGWavetables.hpp   # PPG Wave 2.3 ROM data as static constexpr arrays
│
└── plugin.cpp              # Plugin registration, createModel<Hurricane8>()
```

### Structure Rationale

- **`dsp/` subfolder:** Keeps all audio-thread code isolated from UI code. The `Voice` and `WavetableBank` are audio-thread-only; the `WavetableLoader` explicitly crosses the thread boundary with care.
- **`ppg_tables/` subfolder:** ROM data is large and should be in its own header to keep it out of the way. Declared `constexpr` so it lives in read-only segment, not heap.
- **Split Module/Widget files:** `Hurricane8.cpp` (widget) can change without touching DSP. `Hurricane8Module.cpp` (DSP) can change without recompiling widget code. This matches how well-structured VCV modules like BogaudioModules are organized.

---

## Architectural Patterns

### Pattern 1: Engine-per-Voice Array

**What:** Declare a fixed-size array of Voice structs (`Voice voices[8]`), one per polyphonic channel. Each voice is fully self-contained with its own phase, glide, and envelope state.

**When to use:** Always, for polyphonic modules. This is the canonical VCV Rack pattern (confirmed in official polyphony documentation and multiple open-source modules including BogaudioModules VCO).

**Trade-offs:** Fixed 8-voice limit avoids heap allocation in process(); all voice state lives on the stack frame of the Module struct. CPU cost scales linearly with active voice count.

**Example:**
```cpp
struct Hurricane8 : Module {
    Voice voices[8];   // fixed, no allocation

    void process(const ProcessArgs& args) override {
        int channels = inputs[VOCT_INPUT].getChannels();
        channels = clamp(channels, 1, 8);

        for (int c = 0; c < channels; c++) {
            float pitch = inputs[VOCT_INPUT].getPolyVoltage(c);
            float gate  = inputs[GATE_INPUT].getPolyVoltage(c);
            float wtPos = params[WTPOS_PARAM].getValue()
                        + inputs[WTPOS_CV_INPUT].getPolyVoltage(c) * 0.1f;

            voices[c].process(args.sampleTime, pitch, gate, wtPos, bank);

            outputs[AUDIO_OUTPUT].setVoltage(voices[c].output * 5.f, c);
            outputs[GATE_POLY_OUTPUT].setVoltage(voices[c].gateOut, c);
        }

        outputs[AUDIO_OUTPUT].setChannels(channels);
        outputs[GATE_POLY_OUTPUT].setChannels(channels);
    }
};
```

### Pattern 2: Phase Accumulator with Fractional Index

**What:** Each voice maintains a `double phase` in [0, 1). Each sample, advance by `freq / sampleRate`. Multiply by `tableSize` to get a fractional table index. Apply linear interpolation between adjacent samples.

**When to use:** Always for wavetable oscillators. This is standard practice.

**Trade-offs:** `double` phase prevents drift over long sustains at low pitches. Fractional index enables smooth timbre at all pitches. PPG hardware used no interpolation at the wavecycle level (integer index only), so the stepped/interpolated switch maps cleanly onto this: interpolated = linear interp, stepped = truncate to integer index.

**Example:**
```cpp
struct Voice {
    double phase = 0.0;    // [0, 1)
    float  output = 0.f;

    void process(float sampleTime, float pitchVolts, const WavetableBank& bank,
                 int tableIndex, int waveIndex, bool stepped) {
        float freq = dsp::FREQ_C4 * std::pow(2.f, pitchVolts);
        phase += freq * sampleTime;
        if (phase >= 1.0) phase -= 1.0;

        float tablePos = phase * WAVE_SIZE;   // WAVE_SIZE = 64 (PPG) or 2048 (modern)
        int   idx0 = (int)tablePos;
        float frac = tablePos - idx0;

        if (stepped) {
            output = bank.sample(tableIndex, waveIndex, idx0);
        } else {
            float s0 = bank.sample(tableIndex, waveIndex, idx0);
            float s1 = bank.sample(tableIndex, waveIndex, (idx0 + 1) % WAVE_SIZE);
            output = s0 + frac * (s1 - s0);
        }
    }
};
```

### Pattern 3: Glide via SlewLimiter

**What:** Use `rack::dsp::SlewLimiter` (or equivalent first-order lag) per voice to smooth pitch transitions. The slew rate is set from the GLIDE knob + CV.

**When to use:** Any module with portamento. The VCV Rack DSP library provides `SlewLimiter` directly. MEDIUM confidence — confirmed present in `rack::dsp` namespace from official API docs.

**Trade-offs:** Per-voice slew limiter means each voice can glide independently (correct for polyphonic portamento). Shared global glide time is simpler but sounds wrong in poly mode.

**Example:**
```cpp
struct Voice {
    dsp::SlewLimiter pitchSlew;

    void setGlide(float glideTime) {
        // SlewLimiter uses rise/fall in units/second
        float slewRate = (glideTime > 0.001f) ? (10.f / glideTime) : 1e6f;
        pitchSlew.setRiseFall(slewRate, slewRate);
    }

    float getSlewedPitch(float targetPitch, float sampleTime) {
        return pitchSlew.process(sampleTime, targetPitch);
    }
};
```

### Pattern 4: Off-Thread Wavetable Loading via Atomic Swap

**What:** WAV file I/O must never happen on the audio thread. Load the new table on a worker thread (std::thread or VCV's worker), then swap the active table pointer atomically so the audio thread sees it safely.

**When to use:** Any time user files must be loaded at runtime. This is required by VCV Rack's real-time constraints — file I/O in process() causes audio dropouts and crashes.

**Trade-offs:** Adds complexity. The swap must be atomic or protected. A simple approach: allocate new table, store in `std::atomic<WavetableData*>`, audio thread reads it. A double-buffer approach (ping-pong) avoids any lock. The old table must be freed off the audio thread too.

**Example:**
```cpp
// In WavetableBank
std::atomic<WavetableData*> userTable{nullptr};

// Called from UI thread (context menu action)
void loadFromFile(const std::string& path) {
    std::thread([this, path]() {
        WavetableData* newData = WavetableLoader::load(path);
        WavetableData* old = userTable.exchange(newData);
        delete old;  // safe: audio thread no longer holds old pointer
    }).detach();
}

// In process() — audio thread reads atomically
WavetableData* tbl = userTable.load(std::memory_order_relaxed);
if (tbl) { /* use tbl->samples */ }
```

### Pattern 5: ASR Envelope for Wavetable Position

**What:** Each voice has an Attack-Sustain-Release envelope whose output drives the wavetable scan position. Gate high = attack then sustain at top; gate low = release. In "per-voice" mode, each voice runs its own envelope triggered by its gate input. In "global" mode, all voices share the same envelope driven by any active gate.

**When to use:** PPG-style wavetable scanning. The envelope controls position in the wavetable (which waveform is active), not amplitude.

**Trade-offs:** Per-voice is more expressive but uses 8x the CPU for envelope computation. Given 8 voices max (not 16), this is fine.

**Example:**
```cpp
struct Voice {
    AHREnvelope wtEnvelope;   // drives wavetable position
    float wtPosition = 0.f;   // 0.0 = first wave, 1.0 = last wave

    void process(float sampleTime, float gate, float wtStart, float wtEnvAmt) {
        wtEnvelope.process(sampleTime, gate > 1.f);
        // wtStart sets the base position; envelope adds to it
        wtPosition = clamp(wtStart + wtEnvelope.out * wtEnvAmt, 0.f, 1.f);
    }
};
```

### Pattern 6: DAC Emulation via Quantization

**What:** After computing a float sample in [-1, 1], apply bit-depth quantization to emulate the PPG's 8-bit DACs or intermediate depths. `floor(sample * levels + 0.5f) / levels`.

**When to use:** After all interpolation, before writing to output. This is a post-process step, not part of the oscillator core.

**Trade-offs:** 8-bit quantization is audibly gritty (32 steps per half-cycle at standard tuning). 12-bit is warmer. 16-bit is effectively clean. The `levels = pow(2, bits) - 1`.

**Example:**
```cpp
float emulateDac(float sample, int bits) {
    float levels = (float)((1 << bits) - 1);
    return std::round(sample * levels) / levels;
}
```

---

## Data Flow

### Per-Sample Audio Flow (process() called ~44,100 times/sec)

```
inputs[VOCT_INPUT].getPolyVoltage(c)   ← 1V/oct pitch per channel
inputs[GATE_INPUT].getPolyVoltage(c)   ← 10V gate per channel
params[WTPOS_PARAM].getValue()         ← global wavetable position knob
inputs[WTPOS_CV].getPolyVoltage(c)     ← per-voice wavetable position CV
params[GLIDE_PARAM].getValue()         ← glide time
params[DACBITS_PARAM].getValue()       ← DAC bit depth (8/12/16)
         │
         ▼
  Voice[c].process()
    │  SlewLimiter → slewedPitch
    │  phase += freq(slewedPitch) * sampleTime
    │  wtEnvelope.process(gate) → wtPosition
    │  WavetableBank.lookup(tableIndex, wtPosition, phase, stepped) → rawSample
    │  DacEmulator.quantize(rawSample, dacBits) → quantizedSample
    │
    ▼
  outputs[AUDIO_POLY].setVoltage(quantizedSample * 5.f, c)    ← poly cable
  outputs[AUDIO_VOICE_0 + c].setVoltage(quantizedSample * 5.f)  ← individual jacks
  outputs[MIX_OUTPUT].addVoltage(quantizedSample * 5.f / channels)  ← summed mix
  outputs[GATE_POLY].setVoltage(gateOut * 10.f, c)
  outputs[VELOCITY_POLY].setVoltage(velocity * 10.f, c)
```

### Wavetable Load Flow (off audio thread)

```
User right-clicks panel
    → appendContextMenu() shows "Load wavetable…"
    → std::filesystem::path selection (platform file dialog or path input)
    → WavetableLoader::loadAsync(path, &bank)
        → std::thread: open file, read WAV headers, decode samples
        → Validate: single-cycle (N samples) or multi-frame (N * frameCount)
        → Normalize to float [-1, 1]
        → Resize frames to consistent WAVE_SIZE if needed
        → atomic swap into bank.userTable
    → Audio thread picks up new table on next process() call
```

### Unison Mode vs Polyphonic Mode

```
POLY MODE:
    voct_channels = inputs[VOCT_INPUT].getChannels()  → up to 8
    Voice[0..voct_channels-1] each get their own pitch/gate
    Outputs: poly cable has voct_channels channels

UNISON MODE:
    One pitch (channel 0 of VOCT input) → spread to all 8 voices
    Each voice gets pitch + detune[i] offset
    detune[i] = (i - (voices/2)) * detuneCents / 100.f * (1/12.f) volts
    Poly cable output still carries 8 channels (all at similar pitch)
    Individual voice outputs still active
```

### JSON State Persistence

```
dataToJson():
    → save: wavetableBankPath (path to user WAV, if loaded)
    → save: dacBits, stepped mode, polyphonic/unison switch
    → save: wtStartPos, glideAmount, LFO rate/amount
    (param values saved automatically by Rack)

dataFromJson():
    → restore: wavetableBankPath → trigger async reload
    → restore: mode switches (atomic flags)
```

---

## Build Order (Component Dependencies)

The architecture has clear dependency ordering. Build bottom-up.

```
1. PPGWavetables.hpp        — static ROM data, no dependencies
        ↓
2. WavetableBank            — owns tables, provides sample lookup
        ↓
3. AHREnvelope              — standalone DSP, no module dependencies
4. DacEmulator              — pure function, no dependencies
        ↓
5. Voice struct             — depends on WavetableBank, AHREnvelope, DacEmulator
        ↓
6. Hurricane8 Module        — depends on Voice[], WavetableBank
        ↓
7. WavetableLoader          — depends on WavetableBank (writes to it), std::thread
        ↓
8. Hurricane8Widget         — depends on Hurricane8 Module (reads params/lights)
        ↓
9. plugin.cpp               — registers Hurricane8 with Rack
```

### Phase Build Implications

| Build Phase | Components | Can Stand Alone? |
|-------------|------------|-----------------|
| Phase 1: Core oscillator | PPGWavetables, WavetableBank, Voice (basic), Hurricane8 Module (mono) | YES — test with single voice |
| Phase 2: Polyphony | Voice array, poly input routing, setChannels() on outputs | YES — builds on Phase 1 |
| Phase 3: Modulation | AHREnvelope, SlewLimiter (glide), LFO | YES — adds to Voice struct |
| Phase 4: DAC emulation | DacEmulator, bit-depth param | YES — post-process step |
| Phase 5: User wavetables | WavetableLoader, off-thread swap, dataToJson/From | YES — optional feature |
| Phase 6: Panel + outputs | Hurricane8Widget, individual jacks, mix output | YES — final assembly |

---

## Anti-Patterns

### Anti-Pattern 1: File I/O in process()

**What people do:** Load a WAV file when they detect a changed parameter in `process()`.
**Why it's wrong:** process() is called on the audio thread. File I/O blocks for milliseconds to seconds. This causes audio dropouts, Rack stalls, and is explicitly forbidden by VCV Rack's real-time rules.
**Do this instead:** Trigger file loading from the UI thread (context menu action). Use std::thread + atomic pointer swap. Audio thread only reads the pre-loaded data.

### Anti-Pattern 2: Heap Allocation in process()

**What people do:** `new`, `malloc`, `std::vector::push_back`, or any dynamic allocation inside `process()`.
**Why it's wrong:** Memory allocators use mutexes that can block the audio thread unpredictably, causing audio dropouts. Also forbidden by Rack's real-time rules.
**Do this instead:** Pre-allocate all buffers in the Module constructor. Fixed arrays (`Voice voices[8]`) on the struct. No dynamic containers in the hot path.

### Anti-Pattern 3: Sharing Mutable State Between Threads Without Atomics

**What people do:** Write to a `bool` flag in the UI thread, read it in `process()` without synchronization.
**Why it's wrong:** Data races cause undefined behavior (torn reads, cache incoherence, compiler reordering). On ARM/x86 you may get away with it briefly, but it is undefined behavior.
**Do this instead:** Use `std::atomic<bool>` or `std::atomic<int>` for flags that cross the thread boundary. For wavetable pointer swaps, use `std::atomic<WavetableData*>` with `memory_order_relaxed` (x86 TSO gives this for free; atomic ensures correctness on ARM too).

### Anti-Pattern 4: Using getVoltage() Without getPolyVoltage() for CV Inputs

**What people do:** Read `inputs[WTPOS_CV].getVoltage()` (channel 0 only) for a CV input that could carry per-voice polyphonic data.
**Why it's wrong:** Silently discards channels 1-7, making poly CV inputs behave as mono.
**Do this instead:** Use `inputs[WTPOS_CV].getPolyVoltage(c)` inside the voice loop. This returns channel 0 voltage if the input is monophonic, and the per-channel voltage if polyphonic — correct behavior in both cases.

### Anti-Pattern 5: Fixed 16-Voice Loop for an 8-Voice Module

**What people do:** Loop `for (int c = 0; c < 16; c++)` always, even though the module only has 8 voices.
**Why it's wrong:** Wastes ~2x CPU. The loop should run only for the number of active channels on the input, clamped to the module's voice count maximum.
**Do this instead:** `int channels = clamp(inputs[VOCT_INPUT].getChannels(), 1, 8);` then `for (int c = 0; c < channels; c++)`.

---

## Integration Points

### VCV Rack Engine Boundary

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Module ↔ Rack Engine | params[], inputs[], outputs[], lights[] arrays | Engine-managed, safe across threads |
| ModuleWidget ↔ Module | params[] (read), lights[] (read), custom JSON (save/load) | Widget reads; Module writes lights |
| Module ↔ User filesystem | WAV file paths in dataToJson(); async load via std::thread | Never in process() |
| Module ↔ Other Rack Modules | Polyphonic cables: VOCT, GATE in; AUDIO, GATE, VELOCITY out | Standard VCV voltage conventions |

### Voltage Conventions (HIGH confidence — official VCV docs)

| Signal | Convention | Notes |
|--------|------------|-------|
| 1V/oct pitch | `f = 261.626 * 2^V` Hz, 0V = C4 | Use `dsp::FREQ_C4 * pow(2.f, pitch)` |
| Gate | 0V = off, 10V = on; Schmitt trigger thresholds ~0.1V low, ~1.0V high | Read with `> 1.f` threshold |
| Audio output | ±5V (10Vpp) | Multiply normalized float output by 5.f |
| CV modulation | 0–10V unipolar or ±5V bipolar | Wavetable position CV: 0–10V makes sense |
| Velocity | 0–10V | Polyphonic VELOCITY output to downstream envelopes |
| Polyphonic channels | Up to 16 channels per cable; module limits to 8 | Set with `setChannels(count)` per output |

---

## Scaling Considerations

This is a DSP module, not a server. "Scaling" means CPU budget, not user load.

| Concern | Approach |
|---------|----------|
| 8 voices × sample rate | Fixed array of Voices; no allocation. Linear cost ~8x mono. Acceptable. |
| SIMD optimization | If CPU is tight: `dsp::float_4` processes 4 voices per SIMD op. Reduces 8-voice cost to ~2x mono. Worth doing in Phase 2 if profiling shows need. |
| Wavetable size | PPG ROM: 64-sample wavecycles are tiny. User tables up to 2048 samples/frame × N frames. Keep in flat float array; pointer stays constant during process(). |
| Wavetable table-switch transition | Crossfade between old/new table pointer over N samples to avoid clicks. Simple linear fade of ~256 samples suffices. |

---

## Sources

- VCV Rack Plugin Development Tutorial: https://vcvrack.com/manual/PluginDevelopmentTutorial (HIGH confidence)
- VCV Rack Plugin API Guide: https://vcvrack.com/manual/PluginGuide (HIGH confidence)
- VCV Rack Voltage Standards: https://vcvrack.com/manual/VoltageStandards (HIGH confidence)
- VCV Rack Polyphony Manual: https://vcvrack.com/manual/Polyphony (HIGH confidence)
- VCV Rack DSP Manual: https://vcvrack.com/manual/DSP (HIGH confidence)
- rack::dsp Namespace Reference: https://vcvrack.com/docs-v2/namespacerack_1_1dsp (HIGH confidence)
- rack::engine::Port API: https://vcvrack.com/docs-v2/structrack_1_1engine_1_1Port.html (HIGH confidence)
- VCV Community: Polyphony reminders for plugin developers: https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572 (MEDIUM confidence)
- VCV Community: Making your monophonic module polyphonic: https://community.vcvrack.com/t/making-your-monophonic-module-polyphonic/6926 (MEDIUM confidence)
- BogaudioModules VCO (reference implementation): https://github.com/bogaudio/BogaudioModules (MEDIUM confidence)
- WvTable-logue PPG Wave implementation (PPG table structure reference): https://github.com/vuki/WvTable-logue (MEDIUM confidence)
- EarLevel Engineering — Wavetable oscillator C++ patterns: https://www.earlevel.com/main/2012/05/25/a-wavetable-oscillator-the-code/ (MEDIUM confidence)

---
*Architecture research for: VCV Rack polyphonic wavetable oscillator (Hurricane 8)*
*Researched: 2026-03-01*
