# Hurricane 8 — User Manual

## Overview

Hurricane 8 is a polyphonic wavetable oscillator inspired by the PPG Wave 2.2 and 2.3 synthesizers. It reproduces the characteristic "digital but warm" sound created by low-resolution DACs and discrete wavetable stepping, while adding modern features like smooth interpolated scanning, per-voice envelopes, and user-loadable wavetables.

---

## Controls

### Oscillator Section

#### Wavetable Bank (0–31)
Selects which wavetable bank to use. The 32 bundled banks cover a range of timbral territories:

| Banks | Character |
|-------|-----------|
| 0–3 | Sawtooth harmonic series |
| 4–7 | Square and pulse waves |
| 8–11 | Pulse width modulation sweeps |
| 12–15 | Triangle morphing with wavefolding |
| 16–19 | Vocal and formant waveforms |
| 20–23 | Digital noise textures |
| 24–27 | Oscillator sync emulation |
| 28–31 | Organ drawbar combinations |

Each bank contains 64 frames that progress from simple to complex timbres.

#### Position (0–100%)
Controls which frame within the current wavetable bank is playing. At 0% you hear the first frame; at 100% the last. Accepts CV input (±5V mapped to ±50% offset).

This is the primary timbral control — sweeping position through a bank creates the signature wavetable animation sound.

#### Scan Mode
- **Stepped** — Authentic PPG behavior. Frame changes only occur at zero crossings of the waveform, preventing clicks. This produces the characteristic "stepped" digital quality of the original hardware.
- **Interpolated** — Smoothly crossfades between adjacent frames for continuous timbral morphing. More modern-sounding, useful for pads and evolving textures.

#### DAC Bit Depth
Emulates vintage digital-to-analog converter quantization:
- **8-bit** — 256 levels. Heavy staircase distortion, the authentic PPG Wave sound. Adds grit and harmonic content, especially noticeable on simple waveforms.
- **12-bit** — 4,096 levels. Subtle quantization. Good middle ground between character and cleanliness.
- **16-bit** — 65,536 levels. Essentially clean output. Use when you want the wavetable timbres without DAC coloration.

### Voice Section

#### Voice Mode
- **Polyphonic** — Each channel of the polyphonic V/Oct input drives an independent voice (up to 8). Each voice has its own phase accumulator, envelope, and glide state.
- **Unison** — All 8 voices play the same pitch (from channel 0) with a configurable detune spread. Creates thick, chorused sounds from any wavetable.

#### Detune (0–100 cents)
Only active in Unison mode. Sets the total detuning spread across all 8 voices. Voice 0 is detuned down by the full amount, voice 7 up by the full amount, with the others spaced linearly between. The detuning is applied in V/Oct space, so the frequency spread is wider at higher pitches (musically correct behavior).

**Suggested settings:**
- 5–10 cents: subtle thickening
- 15–30 cents: classic unison chorus
- 50–100 cents: extreme detuned "supersaw" effect

### Modulation Section

#### Start Position (0–100%)
Sets the base wavetable position before envelope modulation is applied. The ASR envelope sweeps from the start position to the end of the table:

```
effective position = start + envelope × (1 - start)
```

So with Start at 25%, the envelope sweeps through the upper 75% of the table. With Start at 0%, the full table range is swept.

Accepts CV input (±5V scaled to ±50% offset, clamped to 0–1).

#### Attack (0.001–2 seconds)
The attack time of the ASR envelope. How long the envelope takes to rise from 0 to 1 after a gate-on event.

#### Release (0.001–2 seconds)
The release time of the ASR envelope. How long the envelope takes to fall from 1 back to 0 after a gate-off event.

#### Envelope Mode
- **Per-Voice** — Each voice runs its own independent ASR envelope, triggered by its own gate. Useful for expressive polyphonic play where each note has its own timbral sweep.
- **Global** — A single ASR envelope applies to all voices simultaneously. Triggered by the OR of all active gates (any note held keeps it open). Useful for uniform ensemble movement.

#### LFO Rate (0.1–20 Hz)
Frequency of the internal sine LFO used for vibrato. Accepts CV input (±5V offset).

#### LFO Amount (0–100%)
Depth of the vibrato. At 100%, the LFO modulates pitch by ±1 semitone. The depth scales linearly — 50% gives ±0.5 semitones. Set to 0% to disable vibrato. Accepts CV input.

#### Glide (0–100%)
Per-voice portamento. At 0%, pitch changes are instant. At higher values, pitch slides smoothly between notes using a one-pole filter on the V/Oct signal. The response is exponential — constant time per octave, which is musically natural (a slide from C3 to C4 takes the same time as C4 to C5).

Accepts CV input (±5V offset to the glide amount).

---

## Inputs

| Input | Signal | Notes |
|-------|--------|-------|
| **V/Oct** | Polyphonic (1–8 ch) | 1V/octave pitch. Each channel drives one voice in Polyphonic mode. Only channel 0 is used in Unison mode. |
| **Gate** | Polyphonic (1–8 ch) | Gate signals. Controls voice activation and triggers ASR envelopes. If disconnected, all voices receiving V/Oct are active. |
| **Velocity** | Polyphonic | Passed through to the Velocity output unchanged. Not processed by the oscillator — route to a VCA downstream for velocity sensitivity. |
| **Position CV** | Monophonic | ±5V offset added to the Position knob. |
| **Start Pos CV** | Monophonic | ±5V offset added to the Start Position knob. |
| **LFO Rate CV** | Monophonic | ±5V offset added to the LFO Rate knob. |
| **LFO Amount CV** | Monophonic | ±5V offset added to the LFO Amount knob. |
| **Glide CV** | Monophonic | ±5V offset added to the Glide knob. |

---

## Outputs

| Output | Signal | Notes |
|--------|--------|-------|
| **Audio** | Polyphonic (1–8 ch) | Main polyphonic output. Channel count matches V/Oct input in Polyphonic mode, always 8 in Unison mode. |
| **Gate** | Polyphonic (1–8 ch) | Passthrough of gate input signals. |
| **Mix** | Monophonic | Normalized average of all active voices. Sum divided by active count, scaled to ±5V. Safe from clipping regardless of voice count. |
| **Velocity** | Polyphonic | Direct passthrough of the Velocity input. |
| **Voice 1–8 Audio** | Monophonic (×8) | Individual per-voice audio outputs. Each carries the same signal as the corresponding polyphonic Audio channel, but as a mono jack. Use for per-voice processing (separate effects, panning, etc). |
| **Voice 1–8 Gate** | Monophonic (×8) | Individual per-voice gate outputs. In Polyphonic mode, each mirrors the corresponding gate input channel. In Unison mode, all mirror channel 0. |

---

## User Wavetables

### Loading a Custom Wavetable

1. Right-click the module panel
2. Select **"Load wavetable..."**
3. Choose a `.wav` file from the file dialog
4. The module switches to the loaded wavetable immediately

The status line in the context menu shows the current source: the loaded filename and frame count, or "PPG ROM" if using the built-in tables.

To return to the bundled PPG tables, right-click and select **"Revert to PPG ROM wavetables"**.

### Supported Formats

- **Sample formats:** 8-bit, 16-bit, 24-bit, or 32-bit PCM; 32-bit IEEE float
- **Channels:** Mono or multi-channel (only channel 0 is used)
- **Single-cycle:** A WAV file containing one waveform cycle. Any length is resampled to 256 samples.
- **Multi-frame (Serum-style):** A WAV file containing multiple cycles concatenated end-to-end. The loader detects cycle boundaries at common wavetable sizes (256, 512, 1024, 2048 samples) and splits accordingly. Up to 64 frames are loaded.

### Compatibility

Most wavetable `.wav` files from Serum, WaveEdit, and similar tools will load correctly. The auto-detection uses smallest-first cycle matching (256 samples before 512 before 1024), which works well for the standard Serum format of many short cycles in one file.

### Persistence

The file path to your loaded wavetable is saved in the VCV Rack patch file. When you reload the patch, the wavetable is re-loaded from disk automatically. If the file has been moved or deleted, the module falls back to the PPG ROM tables.

---

## Patch Ideas

### Classic PPG Pad
- Bank 16–19 (vocal/formant), Interpolated scan mode, 12-bit DAC
- Start Position at 10%, Attack 0.5s, Release 1.0s, Per-Voice envelope
- Slow LFO (1–2 Hz) with subtle amount (10–20%)
- Great for evolving, breathy pad sounds

### Aggressive Bass
- Bank 0–3 (sawtooth series), Stepped scan mode, 8-bit DAC
- Position at 30–50% (higher harmonics), no envelope
- Glide at 20–30% for sliding bass lines
- The 8-bit quantization adds aggressive harmonics in the low end

### Massive Unison Lead
- Any bank, Unison mode, 30 cents detune
- 16-bit DAC for cleanliness (the detuning adds enough movement)
- Short attack (10ms), medium release (300ms)
- Use the 8 individual voice outputs to pan voices across the stereo field

### Evolving Texture
- Load a Serum wavetable with many frames
- Interpolated scan mode, 16-bit DAC
- Start Position at 0%, long Attack (2s), long Release (2s)
- Global envelope mode — all voices sweep together
- Slow position LFO creates additional movement

---

## Technical Notes

### CPU Usage
At 48kHz sample rate, 8-voice polyphonic processing takes approximately 3ms per buffer — well under the ~21ms budget for real-time audio. CPU usage scales linearly with active voice count.

### Wavetable Initialization
The 32 PPG wavetable banks are generated and mip-mapped at plugin load time. This takes approximately 650–700ms on modern hardware. The data is shared across all Hurricane 8 instances — only one copy exists in memory (~2MB).

### Band Limiting
Each wavetable frame is analyzed via DFT and resynthesized at 8 mip levels with progressively fewer harmonics. At runtime, the appropriate mip level is selected based on the ratio of oscillator frequency to sample rate, with smooth crossfading between adjacent levels. This eliminates aliasing at all playable frequencies without audible filtering artifacts.

### Signal Levels
- Audio outputs: approximately ±5V (±1.0 internal × 5V scaling)
- Mix output: normalized to prevent clipping regardless of voice count
- CV inputs: ±5V range, mapped to appropriate parameter offsets
- Gate threshold: standard VCV Rack convention (> 0V = on)
