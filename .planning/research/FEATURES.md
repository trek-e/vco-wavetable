# Feature Research

**Domain:** VCV Rack wavetable oscillator module (PPG-inspired, polyphonic)
**Researched:** 2026-03-01
**Confidence:** MEDIUM — VCV Rack ecosystem well-documented; PPG-authentic feature expectations cross-referenced from hardware, community, and competitor analysis. No single authoritative source covers all feature expectations for this specific niche.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete or incompatible with standard VCV Rack workflow.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 1V/oct pitch tracking | Every VCV oscillator has this; without it the module won't integrate with any patch | LOW | VCV voltage standard: ±5V audio, 0-10V or ±5V CV. Precision required — logarithmic conversion must be accurate across 10+ octaves |
| Polyphonic cable output | VCV Rack 2 norm; users expect to drive the module from a MIDI-CV module with a single poly cable | MEDIUM | Up to 16 channels per Rack cable. Hurricane 8 targets 8 voices — must advertise channel count correctly |
| Wavetable position control with CV input | Core identity of any wavetable VCO — without CV-controllable position, there's no "wavetable scanning" | LOW | WT POS knob + attenuator + CV input is the minimum expected by any user familiar with Fundamental VCO-2 or Surge XT WT |
| User-loadable wavetables | Expected since VCV Fundamental VCO-2 supports it; users have libraries of .WAV files | MEDIUM | Must support at minimum: single-cycle WAV files. Multi-frame (Serum-style 2048-sample tagged) is strongly preferred |
| Mix (summed) output | Standard on all polyphonic voice modules; needed to route through a filter or effects chain | LOW | Simple sum of all active voices; clip-guard at output helpful |
| Polyphonic gate output | Required for downstream envelope generators when used in a full voice patch | LOW | VCV convention: gate high = 10V, low = 0V |
| Right-click context menu with wavetable load | VCV Rack UI convention; users look here first | LOW | Standard pattern from VCV Fundamental VCO-2 |
| Panel SVG that meets library standards | Required for VCV Library submission | MEDIUM | Must follow VCV Panel Guide: text converted to paths, readable at 100% on non-HiDPI, inverted backgrounds on outputs |
| Plugin manifest with correct versioning | Required for library submission; MAJOR version must match Rack SDK major | LOW | Format: MAJOR.MINOR.REVISION, no "v" prefix, MAJOR = 2 for Rack 2 |

### Differentiators (Competitive Advantage)

Features that set Hurricane 8 apart. These align with the core value: "Authentic PPG wavetable character accessible as a polyphonic VCV Rack module with modern flexibility."

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| PPG stepped vs interpolated scanning modes | The PPG Wave did NOT crossfade between wavetable entries — it switched at zero-crossing only. This stepped behavior is the defining sonic character. No VCV module currently replicates this authentically | HIGH | Stepped = switch wave at start of next cycle (zero-crossing). Interpolated = linear crossfade. Switchable gives users both authentic character and modern smoothness. Confirmed from PPG technical analysis: "the PPG ones didn't crossfade at all, they just switched waves at the beginning/end of the next cycle" |
| Configurable DAC bit depth emulation (8/12/16-bit) | The PPG Wave 2 used 8-bit DACs producing quantization artifacts that define its sound. Wave 2.3 upgraded to 12-bit. Emulating this gives users the ability to dial in the exact era of PPG character | HIGH | 8-bit = authentic early PPG grit and aliasing; 12-bit = Wave 2.3 character; 16-bit = transparent/modern. No existing VCV wavetable module offers this tiered DAC emulation specifically targeting PPG authenticity |
| Original PPG wavetable set bundled | Users who want the PPG sound need the original waveforms. Sourcing these is non-trivial for most users | MEDIUM | Hermann Seib's PPG Wave ROM documentation provides wavetable structure reference. 30 wavetables of 64 waves each. Licensing must be considered — original ROM data may be in the public domain given age |
| Multi-frame WAV support (Serum-style) | Serum, Bitwig, and WaveEdit users have large libraries in multi-frame format. Supporting this dramatically expands usable content | HIGH | Serum format: 2048-sample tagged WAV. Must parse WAV loop markers or metadata tags to detect frame boundaries. Surge XT already supports this — sets user expectation |
| Per-voice ASR envelope for wavetable position | The authentic PPG had envelope-controlled wavetable scanning (Env3 was the dedicated "wavetable position" envelope). Per-voice tracking enables expressive polyphony where each note independently scans the table | HIGH | Per-voice = each of the 8 voices has its own envelope phase. Global = all voices share one envelope. Switchable gives flexibility. ASR (Attack-Sustain-Release) matches PPG's original design |
| Wavetable start position control | Sets where in the wavetable scanning begins — allows tuning a specific wave character as the "base" timbre | LOW | Independent of the scan envelope — the start position sets the ground state before the envelope moves it |
| Internal LFO for vibrato | The PPG Wave had dedicated vibrato circuitry. Having it onboard reduces patch complexity for a common use case | MEDIUM | Rate and amount controls with CV inputs for each. Sine wave LFO routed to pitch. External LFO can always be patched, but internal saves two modules for a typical live performance patch |
| Glide (portamento) with CV input | Essential for PPG-style expressive playing — the original PPG had portamento between note pitches | LOW | Per-voice slew limiting on the pitch signal. CV input for dynamic glide amount (e.g., from mod wheel) |
| Unison mode with detune | Stacking all 8 voices on a single monophonic pitch with detuning gives the characteristic "super saw" / wall-of-sound effect that PPG players use for pads | MEDIUM | Unison = all 8 voices receive the same 1V/oct; spread detune across ±N cents. Polyphonic mode = standard voice allocation. Switchable per VCV convention |
| 8 individual voice audio outputs | Allows each voice to be processed independently (different filter depths, panning, effects per voice) — power-user capability | MEDIUM | Both as individual jacks AND packed into a polyphonic cable. Individual jacks are unusual in VCV — most modules use poly cables + Split utility. Having both is the differentiator |
| 8 individual gate outputs | Enables per-voice envelope triggering with external envelope generators, bypassing the internal ASR | LOW | Both individual jacks and polyphonic cable. Same pattern as audio outputs |
| Velocity CV output (polyphonic) | Passes velocity data from upstream MIDI-CV module through — enables downstream dynamics without losing velocity information | LOW | Polyphonic only (16 channels max from MIDI-CV); pass-through rather than generation |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems for this module's scope and quality.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Built-in filter (VCF) | PPG Wave had a 24dB/oct analog filter — users want the "complete PPG voice" | Adds massive complexity, doubles scope; VCV's modular philosophy means filters belong in separate modules (Vult Freak, Bogaudio VCF, etc). Scope creep destroys timeline | Pair with external filter modules; document recommended patch topologies in presets |
| Built-in VCA/amplitude envelope | Complete voice = OSC + VCF + VCA; users want one-module voice | Same scope problem; VCA and amplitude envelopes are well-served by existing VCV modules (ADSR + VCA) | External envelope + VCA; the mix output routes directly to any VCA |
| Built-in reverb/delay/chorus | "PPG with effects" requests will come | Completely outside oscillator scope; adds CPU cost for effects most users don't want | Effect modules exist in abundance; out of scope by project definition |
| MIDI input | Easier patching for keyboard players | VCV Rack's MIDI-CV module handles this; duplicating it adds maintenance burden and reduces interoperability | Upstream MIDI-CV module feeds poly cables into Hurricane 8's inputs |
| 2D XY wavetable morphing | E370-style XY morphing is requested by power users | PPG's wavetable structure is 1D (linear scan through a 64-wave table); 2D morphing is a different synthesis paradigm that changes the fundamental PPG character | The 1D scan IS the PPG. Stick to 1D. Users wanting XY morphing should use E370 |
| Sequencer or arpeggiator | "Complete instrument" requests | Out of scope; VCV has excellent sequencers (Geodesics, Impromptu, Count Modula) | External sequencer modules |
| Preset management UI | Save/load sound presets inside the module | VCV Rack patches ARE the preset system. In-module presets duplicate this and add complexity | VCV patch save/load serves this need |
| 16-voice mode | More voices = more CPU, hitting Rack's polyphonic cable max | 8-voice is the PPG's voice count and maps cleanly to module design; 16 voices doubles CPU and individual jack count | Document that 8 voices is intentional PPG fidelity |

---

## Feature Dependencies

```
1V/Oct Pitch Input (per voice)
    └──requires──> Phase Accumulator (per voice)
                       └──requires──> Wavetable Lookup
                                          └──requires──> Wavetable Data (ROM PPG tables or user WAV)

Polyphonic Cable Output
    └──requires──> Voice Allocation (maps poly channels to voices)
                       └──requires──> Gate Input (polyphonic)

Wavetable Position CV
    └──enhances──> Wavetable Lookup

Per-Voice ASR Envelope (wavetable position)
    └──requires──> Gate Input (per voice)
    └──enhances──> Wavetable Position CV (envelope drives position)

Wavetable Start Position
    └──enhances──> Per-Voice ASR Envelope (defines envelope start point)

PPG Stepped Scanning Mode
    └──requires──> Wavetable Lookup (zero-crossing detection logic)
    └──conflicts──> Interpolated Scanning Mode (mutually exclusive per-voice modes)

DAC Bit Depth Emulation
    └──requires──> Wavetable Lookup output (post-lookup quantization step)

Multi-frame WAV Support
    └──requires──> WAV File Parser (frame boundary detection)
    └──enhances──> User-Loadable Wavetables

Unison Mode
    └──requires──> Voice Allocation
    └──conflicts──> Polyphonic Mode (mutually exclusive modes)

Internal LFO (vibrato)
    └──enhances──> 1V/Oct Pitch (adds pitch modulation)

Glide (portamento)
    └──enhances──> 1V/Oct Pitch (slew limits pitch transitions)

Individual Voice Audio Outputs (8 jacks)
    └──requires──> Per-Voice Audio Generation

Individual Gate Outputs (8 jacks)
    └──requires──> Voice Allocation (knows which voice is on gate)

Velocity CV Output (polyphonic)
    └──requires──> Gate/Velocity Input from upstream MIDI-CV

Mix Output
    └──requires──> Per-Voice Audio Generation (sum of all voices)
```

### Dependency Notes

- **Per-Voice ASR Envelope requires Gate Input:** The envelope must know when a note starts and ends. Each polyphonic channel carries its own gate signal.
- **PPG Stepped Scanning conflicts with Interpolated Mode:** These are mutually exclusive per-voice (or global switch). Cannot crossfade AND step-switch simultaneously.
- **DAC Bit Depth Emulation is post-lookup:** The quantization happens AFTER wavetable lookup — it's a processing stage on the output, not part of the table storage.
- **Unison Mode conflicts with Polyphonic Mode:** In unison, all 8 voices track a single monophonic pitch. In polyphonic mode, voices are independently allocated. A panel switch or right-click menu controls this.
- **Wavetable Start Position enhances ASR Envelope:** The start position is the envelope's "zero point" — where the table scan begins before the attack stage moves it forward.

---

## MVP Definition

### Launch With (v1)

Minimum viable product — what validates the PPG polyphonic wavetable concept.

- [ ] 1V/oct pitch tracking (polyphonic, 8 channels) — fundamental oscillator function
- [ ] Phase accumulator per voice — the DSP core
- [ ] PPG wavetable data bundled (ROM set) — gives immediate PPG identity
- [ ] Wavetable scanning with stepped mode — the defining PPG character
- [ ] Wavetable position control with CV input — essential wavetable feature
- [ ] Per-voice ASR envelope for wavetable position — the PPG "Env3" equivalent
- [ ] DAC bit depth emulation (8/12/16-bit) — differentiator, builds in from day one
- [ ] Mix (summed) output — required for basic patching
- [ ] Polyphonic audio output (8 channels) — standard VCV polyphonic output
- [ ] Polyphonic gate output — downstream envelope generators need this
- [ ] Basic panel SVG — required for any use in VCV Rack
- [ ] User-loadable single-cycle WAV — expected by all users

### Add After Validation (v1.x)

Features to add once core DSP is working and validated.

- [ ] Multi-frame WAV support (Serum-style) — expands usable wavetable libraries; add when user feedback confirms demand
- [ ] Interpolated scanning mode (switchable with stepped) — modern fallback; add after stepped is validated
- [ ] Glide (portamento) with CV input — expressive feature, add in v1.1
- [ ] Internal LFO for vibrato — convenience feature; add once core patch is stable
- [ ] Unison mode with detune — high-value differentiator; requires voice allocation redesign; add after polyphonic mode is stable
- [ ] 8 individual voice audio outputs (jacks) — power user feature; HP-constrained, requires panel redesign
- [ ] 8 individual gate outputs (jacks) — accompanies individual voice audio outputs

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Wavetable start position as separate control — nuance feature; users may not discover it; add if requested
- [ ] Velocity CV output pass-through — low complexity but low priority; add when available
- [ ] Additional bundled wavetable sets beyond original PPG ROM — content work; defer until core is stable
- [ ] Wavetable editor or display visualization — complex UI; out of scope for v1

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 1V/oct pitch tracking (polyphonic) | HIGH | LOW | P1 |
| PPG wavetable ROM bundled | HIGH | MEDIUM | P1 |
| Stepped scanning mode (PPG authentic) | HIGH | HIGH | P1 |
| Wavetable position control + CV | HIGH | LOW | P1 |
| Per-voice ASR envelope for WT position | HIGH | HIGH | P1 |
| DAC bit depth emulation (8/12/16-bit) | HIGH | HIGH | P1 |
| Mix output | HIGH | LOW | P1 |
| Polyphonic audio output | HIGH | LOW | P1 |
| Polyphonic gate output | HIGH | LOW | P1 |
| User-loadable single-cycle WAV | HIGH | MEDIUM | P1 |
| Panel SVG + library submission | HIGH | MEDIUM | P1 |
| Interpolated scanning mode (switchable) | MEDIUM | MEDIUM | P2 |
| Multi-frame WAV support | HIGH | HIGH | P2 |
| Glide (portamento) with CV | MEDIUM | LOW | P2 |
| Internal LFO for vibrato | MEDIUM | MEDIUM | P2 |
| Unison mode with detune | HIGH | MEDIUM | P2 |
| 8 individual voice audio outputs (jacks) | MEDIUM | HIGH | P2 |
| 8 individual gate outputs (jacks) | MEDIUM | LOW | P2 |
| Wavetable start position control | MEDIUM | LOW | P2 |
| Velocity CV output pass-through | LOW | LOW | P3 |
| Additional bundled wavetable sets | MEDIUM | MEDIUM | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

The VCV Rack wavetable space has several occupants; none precisely overlap with Hurricane 8's combination of PPG authenticity, 8-voice polyphony, and individual voice outputs.

| Feature | VCV Fundamental VCO-2 | Surge XT Wavetable VCO | Valley Terrorform | E370 Quad Morphing VCO | Hurricane 8 (proposed) |
|---------|----------------------|------------------------|-------------------|------------------------|------------------------|
| Wavetable scanning | Interpolated crossfade | Interpolated (2 modes) | FM + wavetable hybrid | 2D XY morph, smooth/glitched | Stepped + interpolated (switchable) |
| DAC bit depth emulation | None | "WaveTable mode" simulates early DAC (undocumented fidelity) | None | None | 8/12/16-bit explicit emulation |
| PPG ROM tables bundled | No | No | No | No | Yes |
| User-loadable wavetables | Single-cycle WAV | Serum, Bitwig, WaveEdit formats | Yes (format TBD) | Via WaveEdit tool | Single-cycle + multi-frame WAV |
| Polyphonic | Yes | Yes | Yes | No (quad mono voices) | Yes (8-voice) |
| Voice count | Up to 16 (Rack limit) | Up to 16 | Up to 16 | 4 mono voices | 8 fixed (PPG authentic) |
| Unison mode | No (external module needed) | Via separate UnisonHelper module | No | No | Yes (built-in) |
| Glide/portamento | No | No | No | No | Yes (built-in) |
| Internal vibrato LFO | No | Via modulation matrix | No | No | Yes (built-in) |
| Per-voice ASR envelope | No | No | No | No | Yes (built-in) |
| Individual voice outputs | No (poly cable only) | No (poly cable only) | No (poly cable only) | Yes (4 mono outs) | Yes (8 jacks + poly cable) |
| Individual gate outputs | No | No | No | No | Yes (8 jacks + poly cable) |

**Key gap Hurricane 8 fills:** No existing VCV Rack module combines authentic PPG stepped scanning, explicit DAC bit depth emulation, the original PPG wavetable set, built-in PPG-style voice envelope, AND 8-voice polyphony in a single module. Users currently need to assemble a complex patch (wavetable VCO + envelope + poly unison utility + glide module + LFO) to approximate this. Hurricane 8 makes it a single drop.

---

## Sources

- [VCV Library - Oscillator tag](https://library.vcvrack.com/?tag=Oscillator) — MEDIUM confidence (paginated, not exhaustive)
- [VCV Fundamental VCO-2 (Wavetable VCO)](https://library.vcvrack.com/Fundamental/VCO2) — HIGH confidence (official VCV module)
- [Surge XT Wavetable VCO documentation](https://surge-synthesizer.github.io/rack_xt_manual/) — MEDIUM confidence (official Surge docs)
- [PPG Wave Wikipedia](https://en.wikipedia.org/wiki/PPG_Wave) — MEDIUM confidence (well-sourced Wikipedia article)
- [PPG Wave ROM Wavetable documentation - Hermann Seib](https://www.hermannseib.com/documents/PPGWTbl.pdf) — HIGH confidence (primary source researcher)
- [Gearspace PPG Wave interpolation discussion](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1273412-ppg-wave-doest-interpolate-between-single-cycles.html) — MEDIUM confidence (community with technical detail)
- [Groove Synthesis 3rd Wave 8M - PPG-style polyphonic](https://groovesynthesis.com/8m/) — MEDIUM confidence (contemporary commercial analog)
- [PPG W2.2x4 Eurorack module](https://synthanatomy.com/2025/09/ppg-w2-2x4-legendary-german-brand-makes-a-comeback-with-a-new-dual-wavetable-oscillator.html) — HIGH confidence (official PPG product, 2025)
- [WAEVE82 PPG-inspired AE Modular polysynth](https://www.tangiblewaves.com/store/p235/WAEVE82.html) — HIGH confidence (official product page, 2025/2026)
- [VCV Rack Polyphony manual](https://vcvrack.com/manual/Polyphony) — HIGH confidence (official VCV documentation)
- [VCV Rack Voltage Standards](https://vcvrack.com/manual/VoltageStandards) — HIGH confidence (official VCV documentation)
- [Erica Synths Black Wavetable VCO - bitcrush features](https://gearnews.com/erica-synths-ports-black-wavetable-vco-and-black-octasource-to-vcv-rack/) — MEDIUM confidence (product announcement)
- [VCV Community wavetable VCO discussion](https://community.vcvrack.com/t/wave-table-vco/14759) — LOW confidence (community forum, limited to browsed portion)
- [Synthesis Technology E370 features](https://vcvrack.com/SynthesisTechnology) — MEDIUM confidence (official product page)

---

*Feature research for: PPG-inspired polyphonic wavetable oscillator VCV Rack module (Hurricane 8)*
*Researched: 2026-03-01*
