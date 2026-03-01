# Pitfalls Research

**Domain:** VCV Rack polyphonic wavetable oscillator module (PPG Wave emulation)
**Researched:** 2026-03-01
**Confidence:** HIGH (multiple official sources + verified community patterns)

---

## Critical Pitfalls

### Pitfall 1: Memory Allocation or File I/O Inside process()

**What goes wrong:**
Any call to `new`, `delete`, `malloc`, `free`, or file I/O inside the `process()` method causes non-deterministic stalls. The OS memory allocator acquires locks and may swap to disk. A single stall of even 10ms in the audio thread causes a buffer underrun (audible click or dropout). User wavetable loading (WAV parsing) is a common trigger for this.

**Why it happens:**
Developers port non-realtime patterns (load on demand, lazy-init) into the audio callback. WAV file loading in particular is tempting to do inline when a user patches a new file, but `process()` is on the audio thread. The VCV Rack docs explicitly warn: "accessing patch files in Module::process() can block the DSP thread, resulting in audio hiccups."

**How to avoid:**
- Pre-allocate all per-voice state arrays at module construction time (8 voices fixed)
- Use a worker thread or `onAdd()`/`onReset()` events for file loading
- Communicate between UI/file-loading thread and audio thread via lock-free mechanisms (e.g., `dsp::RingBuffer`, atomic flags, or double-buffered ping-pong approach)
- Wavetables should be fully loaded into a pre-allocated buffer before any audio thread reads them; use an atomic "ready" flag to switch

**Warning signs:**
- Occasional audio pops when changing wavetable files
- `valgrind` or sanitizers detecting allocations in process()
- CPU spikes on file load coinciding with audio dropouts

**Phase to address:** Wavetable engine core (Phase 1) and user wavetable loading (whichever phase introduces WAV import)

---

### Pitfall 2: No Band-Limited Mip-Map Tables — Aliasing at High Pitches

**What goes wrong:**
A single wavetable used across all pitches produces severe aliasing above roughly C5. Harmonics that exceed the Nyquist limit fold back as audible, dissonant mirror frequencies. The problem is worst with complex waveforms (sawtooth, PPG tables with many harmonics) and is invisible at C3 but catastrophic at C6+.

**Why it happens:**
Developers build a working oscillator at middle frequencies, test it there, and don't notice the problem until testing high-register patches. The PPG waveforms contain many high-order harmonics that alias immediately at high pitches without mip-mapping.

**How to avoid:**
- Build a mip-map set at module initialization: generate one band-limited version per octave (or semi-tone region) by taking an FFT, zeroing harmonics above Nyquist for that pitch range, and inverse-FFTing
- Select the appropriate mip-map level based on the current playback frequency per voice
- Table size of 2048 samples is a common baseline; linear interpolation between samples is sufficient if tables are large enough
- Rebuild mip-map set after any user wavetable is loaded (on the worker thread)

**Warning signs:**
- Metallic, dissonant ringing at high pitches that disappears at lower notes
- High-frequency notes sound "wrong" compared to the reference PPG sound
- Pitch-dependent timbre changes that go beyond the intended character

**Phase to address:** Wavetable engine core (Phase 1) — must be designed in from the start, not retrofitted

---

### Pitfall 3: Non-Compliant Polyphonic Channel Handling

**What goes wrong:**
The module produces incorrect or inconsistent behavior when receiving polyphonic cables. Common failures:
- Reading only channel 0 from a polyphonic V/OCT input (silently ignoring voices 1–7)
- Setting output channel count to a hardcoded 8 instead of querying the connected polyphonic input
- Not calling `setChannels()` on output ports, leaving downstream modules confused about active voice count

**Why it happens:**
Developers test with a single voice and never validate polyphonic behavior. The VCV Rack polyphony model is not enforced at compile time — incorrect code compiles and runs, it just does the wrong thing silently.

**How to avoid:**
- For the polyphonic V/OCT input: read all channels up to `inputs[VOCT_INPUT].getChannels()`, capped at 8
- Use `std::max(inputs[VOCT_INPUT].getChannels(), 1)` to handle unpatched case (returns 0 channels when unpatched)
- Call `outputs[POLY_OUT].setChannels(numVoices)` every process() (or at least when voice count changes) so downstream modules know how many channels are valid
- For audio inputs on mono paths, use `getVoltageSum()` not `getVoltage()`
- Test with VCV Core's MIDI-CV module set to 2, 4, 8, and 16 channels

**Warning signs:**
- Only one voice sounds when MIDI-CV is in polyphonic mode
- Downstream modules (like poly-to-mono mixers) show wrong channel counts
- Works in unison mode but not polyphonic mode

**Phase to address:** Core module scaffold (earliest phase) — the module's process() loop must be written for polyphony from day one

---

### Pitfall 4: Floating-Point Phase Accumulator Drift and Denormal Numbers

**What goes wrong:**
Single-precision float phase accumulators accumulate rounding error over long sustained notes. At very low frequencies (sub-audio LFO rates or very slow wavetable scanning), the phase increment becomes a subnormal (denormal) floating-point number. Denormal arithmetic on x86 CPUs is 10–100x slower than normal FP operations, causing sudden CPU spikes that can manifest as audio dropouts.

**Why it happens:**
It is natural to use `float` for phase values since Rack's audio samples are `float`. But accumulated phase drift is real at 44100 Hz over minutes of runtime, and nobody tests for it. The denormal trap is especially insidious because it appears only at very low LFO rates or extreme portamento.

**How to avoid:**
- Use `double` for phase accumulators (double-precision eliminates drift for all practical note lengths)
- Flush-to-zero (FTZ) mode: VCV Rack's engine enables FTZ/DAZ CPU flags in the audio thread, which converts denormals to zero automatically. Verify this is active (it is in Rack 2's engine) and don't disable it
- Add a small DC offset trick or clamp phase to [0.0, 1.0] after each wrap to prevent accumulation
- For wavetable scan position, clamp to [0.0, numWaves-1] after each increment

**Warning signs:**
- CPU usage spikes unpredictably at slow LFO rates
- Very slow portamento glides producing audio dropouts
- Phase-related glitches after long sustained notes

**Phase to address:** DSP engine core (Phase 1) — use double for all phase accumulators from the start

---

### Pitfall 5: Wavetable Position Stepping Transition Clicks (The Wrong Kind of PPG Character)

**What goes wrong:**
When scanning between wavetable positions in stepped mode (authentic PPG behavior), abrupt transitions between adjacent waveforms create discontinuities that produce loud clicks at audio rate. This is different from the intended "digital stepped" character — the real PPG's stepping artifacts are at the *sample level*, not at the waveform boundary level. Badly implemented stepping causes click artifacts that are purely bugs, not character.

**Why it happens:**
Developers implement stepping by directly substituting the new waveform table index on every new period or on every incoming CV change. If the waveform transition happens at a non-zero-crossing point, an instantaneous voltage jump creates a click. The PPG's stepping was between fixed wave positions on the wavetable — the waveform itself loops continuously.

**How to avoid:**
- Stepped mode changes the target wavetable table index but the phase accumulator runs continuously — never reset the phase on a table change
- Apply hysteresis to the stepped table selection: don't switch tables faster than the current audio period
- Optional: crossfade over 1–2 samples when switching table index even in "stepped" mode (imperceptible but click-free)
- Separate the authenticity of stepping (integer table indices, no interpolation between tables) from click-free implementation (continuous phase, smooth enough to not introduce new artifacts)

**Warning signs:**
- Audible clicks when sweeping the wavetable position knob quickly
- Clicks at low wavetable positions in stepped mode but not in interpolated mode
- Users report "glitchy" sound that isn't the intended PPG character

**Phase to address:** Wavetable engine core (Phase 1), specifically when implementing the stepped/interpolated mode switch

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Single wavetable (no mip-map) | Faster to build, simpler code | Severe aliasing above C4; users will reject the module | Never — must be in Phase 1 |
| `float` phase accumulators | Trivially easy | Drift on long notes; denormal risk at slow rates | Never for final product |
| `getVoltage(0)` instead of per-channel loop | Simpler process() | Module silently monophonic; polyphonic cables ignored | Never — defeats module purpose |
| Blocking file load in process() | Simpler WAV loading code | Audio dropouts on file change | Never |
| Hardcoded 44100 Hz assumption | Simpler frequency calculation | Incorrect pitch tracking at 48000/96000 Hz | Never — use `APP->engine->getSampleRate()` |
| Single wavetable buffer shared across voices | Less memory | Race condition if voices read while file-loading thread writes | Never without proper sync |
| All voice state on stack per-sample | Avoids heap, simple | Stack overflow with 8 voices of deep state | Never — allocate in constructor |

---

## Integration Gotchas

Common mistakes when connecting to external VCV Rack systems.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| MIDI-CV → V/OCT input | Assuming mono V/OCT; reading only channel 0 | Loop over `inputs[VOCT_INPUT].getChannels()` per process() call |
| Polyphonic gate input | Treating gate as mono; all 8 voices trigger simultaneously | Read gate per channel: `inputs[GATE_INPUT].getVoltage(voiceIndex)` |
| Mix output | Summing voice outputs without normalization; very loud output | Sum then divide by `numVoices` or use a fixed attenuation |
| Polyphonic output channel count | Not setting `setChannels()` on output | Call `outputs[POLY_OUT].setChannels(numActiveVoices)` every frame |
| CV inputs for wavetable position | Unpatched CV reads as 0V — correct | But ensure 0V maps to a musically useful wavetable position (first table), not silence |
| Velocity CV output | Outputting velocity as raw MIDI 0–127 | VCV standard is 0–10V; map velocity: `velocity / 127.0f * 10.0f` |
| SVG panel components layer | Leaving component markers visible in shipped SVG | Hide the "components" layer before exporting final SVG |

---

## Performance Traps

Patterns that work for 1 voice but fail at 8 voices under load.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-sample `std::pow(2, cv/12)` for V/oct | Correct but slow | Pre-compute or use lookup table; `dsp::approxExp2_taylor5()` from Rack DSP | Any time; 8 voices × 44100 Hz = significant overhead |
| Linear interpolation across all voices naively | CPU spikes at 8 voices | Vectorize inner loop with SIMD; profile before optimizing | Above 4–6 active voices at 44100 Hz |
| Wavetable mip-map rebuild on every process() call | Constant ~100% CPU | Rebuild only when wavetable changes (triggered by worker thread flag) | Immediately |
| Complex widget `draw()` called every frame | High GPU/CPU usage on panel redraw | Wrap in `FramebufferWidget` so it only redraws on change | Any time the Rack window is animating at 60fps |
| FFT-based mip-map generation on module add | Long stall when adding module to rack | Generate mip-maps asynchronously; play silence until ready | Tables larger than ~1024 samples |
| Unison mode: 8 voices summed without level compensation | Output clips when unison enabled | Scale output by `1.0f / numUnisonVoices` in mix output | Immediately on first unison note |

---

## UX Pitfalls

Common user experience mistakes specific to wavetable oscillator modules.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Wavetable position knob jumps between tables with no visual feedback | Users can't see which wave they're on; confusing | Display current table index in panel label or LED display |
| Stepped and interpolated modes produce identical-sounding output | Users can't tell modes apart; feature appears broken | Ensure stepped mode has audibly distinct stepping artifacts vs. smooth crossfade |
| Per-voice ASR envelopes all reset on ANY gate | Voices not playing sustain inappropriately | Only trigger ASR on the gate for *that specific voice index* |
| All 8 individual jacks always present but unlabelled | Users confused which jack is which voice | Clear labels V1–V8 on panel; label gates G1–G8 |
| User wavetable loading with no progress feedback | Long WAV files stall UI; user thinks app is frozen | Show a loading state (LED blink or label change) while file loads |
| Module too narrow for 8+8 jacks | Jacks too close to patch without catching adjacent jack | Plan HP width early: 8 voice outs + 8 gate outs + poly outs + controls requires ~30–40 HP minimum |
| Glide applies to all voices equally without per-voice tracking | Playing polyphonic chord with glide produces wrong pitch slides | Track glide target independently per voice index |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Pitch tracking:** 1V/oct formula uses `APP->engine->getSampleRate()` not hardcoded 44100 — verify at 48kHz
- [ ] **Polyphony:** Module produces correct output with 1, 2, 4, AND 8 polyphonic channels (not just 1 and 8)
- [ ] **Wavetable loading:** WAV files with sample rates other than the module's rate are handled (resample or reject gracefully)
- [ ] **Multi-frame WAV:** Frame boundary detection handles files where total samples is not an exact multiple of the frame size (truncate last partial frame, don't crash)
- [ ] **Unison mode:** Switching from polyphonic to unison and back mid-patch does not leave orphan voices sounding
- [ ] **Stepped mode:** Changing wavetable position at audio rate does not produce clicks louder than the intended PPG stepping character
- [ ] **State serialization:** `dataToJson`/`dataFromJson` correctly round-trips the wavetable file path AND the loaded wavetable data reference — patch save/load restores the correct sound without re-showing a file picker
- [ ] **Mip-map rebuild:** Loading a new user wavetable correctly rebuilds mip-maps for ALL voices, not just voice 0
- [ ] **Library compliance:** All module parameters have descriptions, names, and units configured; module has tags and description in plugin.json
- [ ] **NaN safety:** Outputs are checked with `std::isfinite()`; any NaN from bad CV inputs outputs 0V rather than propagating NaN to downstream modules
- [ ] **Cross-platform build:** Header `#include` paths match case of actual filenames — compiles on Linux (case-sensitive filesystem) not just macOS

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Aliasing discovered after wavetable engine is built | HIGH | Retrofit mip-map system; requires rethinking table storage and lookup; touches all voice code |
| Memory allocation in process() | MEDIUM | Identify all allocation sites with sanitizer; move to constructor or worker thread; redesign data ownership |
| Polyphonic channel handling wrong | MEDIUM | Systematic rewrite of process() loop from mono to per-channel; test with poly MIDI-CV |
| State serialization broken | LOW-MEDIUM | Add version field to JSON; implement migration path; re-test save/load |
| Panel HP too narrow | HIGH | Redesign SVG and all component coordinates; must adjust C++ widget positions to match |
| Click on wavetable stepping | LOW | Add crossfade or hysteresis to table index selection; isolated change |
| CPU too high at 8 voices | MEDIUM | Profile with VTune or Instruments; likely the V/oct conversion or mip-map lookup; vectorize hot path |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Memory allocation in process() | Phase 1: Wavetable DSP engine | No allocations reported by sanitizer; no audio dropouts on WAV load |
| No band-limited mip-maps | Phase 1: Wavetable DSP engine | No aliasing at C6 and above (spectrogram verification) |
| Polyphonic channel handling | Phase 1 or 2: VCV Rack integration | Module behaves correctly with 1, 4, 8 poly channels from MIDI-CV |
| Float phase drift / denormals | Phase 1: Wavetable DSP engine | 1-hour sustained note test; CPU stable; no pitch drift measurable |
| Wavetable position clicking | Phase 1: Wavetable DSP engine | Stepped mode sweeps produce no clicks above the intended stepping character |
| Missing mip-map rebuild on user load | Phase with WAV loading | Load new wavetable while sustaining notes; no aliasing introduced mid-note |
| Wrong panel HP | Pre-Phase 1: Planning + SVG design | All 16 individual jacks + polyphonic jacks + controls fit with thumb clearance |
| State serialization | Final Phase: Library prep | Save patch, quit, reopen → identical sound; preset system works |
| Cross-platform build failures | Every phase | Plugin toolchain CI builds for Windows, Mac, Linux on each merge |
| NaN propagation | Throughout | Stress test with no cables patched; extreme CV values (±10V); verify no NaN on outputs |

---

## Sources

- [VCV Rack Plugin API Guide](https://vcvrack.com/manual/PluginGuide) — thread safety, file I/O, bypass, expander messaging (HIGH confidence)
- [VCV Rack DSP Manual](https://vcvrack.com/manual/DSP) — aliasing, bandlimiting, minBLEP, SIMD optimization (HIGH confidence)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) — audio levels, gate thresholds, Schmitt triggers, NaN handling (HIGH confidence)
- [VCV Rack Polyphony Manual](https://vcvrack.com/manual/Polyphony) — channel count rules, getChannels(), setChannels() (HIGH confidence)
- [VCV Community: Polyphony Reminders for Plugin Developers](https://community.vcvrack.com/t/polyphony-reminders-for-plugin-developers/9572) — getVoltageSum() for audio, channel count with std::max (HIGH confidence, official community post)
- [VCV Community: Common Issues for Plugin Developers](https://community.vcvrack.com/t/help-wanted-for-reporting-common-issues-to-rack-plugin-developers/11031) — metadata, polyphony compliance, crash patterns (HIGH confidence)
- [VCV Rack Panel Guide](https://vcvrack.com/manual/Panel) — 128.5mm height, mm units only, component color coding, text-to-paths (HIGH confidence)
- [EarLevel Engineering: Wavetable Oscillator Series](https://www.earlevel.com/main/2012/11/18/a-wavetable-oscillator-end-notes/) — mip-mapping, band-limiting, interpolation tradeoffs (HIGH confidence, authoritative DSP reference)
- [KVR Audio: Wavetable oscillator implementation](https://www.kvraudio.com/forum/viewtopic.php?t=509014) — mip-map spacing, linear vs cubic interpolation (MEDIUM confidence)
- [KVR Audio: Comparing phase truncation and interpolation](https://www.kvraudio.com/forum/viewtopic.php?t=546829) — interpolation vs stepping modes (MEDIUM confidence)
- [Ross Bencina: Real-time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) — malloc, blocking, pre-allocation rules (HIGH confidence, canonical real-time audio reference)
- [PPG Wave Wikipedia](https://en.wikipedia.org/wiki/PPG_Wave) — original hardware bit depth, stepping architecture (MEDIUM confidence)
- [VCV Community: Glide/portamento subtleties](https://community.vcvrack.com/t/some-subtleties-in-glide-portamento/16136) — per-voice gate tracking, rate vs time glide (MEDIUM confidence)
- [VCV Community: Lights and thread safety](https://community.vcvrack.com/t/lights-buttons-and-thread-safety/21797) — thread model in Rack 2 (HIGH confidence)

---
*Pitfalls research for: VCV Rack polyphonic wavetable oscillator (PPG Wave-style, 8-voice, Hurricane 8)*
*Researched: 2026-03-01*
