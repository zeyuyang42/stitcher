# Stitcher

> This is my pure vibe coded fun plugin project, derived from the previous [concatenative synthesizer in SuperCollider](https://github.com/zeyuyang42/concatenative-synthesizer).

Real-time corpus-based concatenative synthesizer — JUCE VST3 / AU / Standalone plugin.

Listens to a **sidechain (control)** signal and a **main (source)** signal, builds a rolling stereo corpus of analyzed source frames, and continuously outputs the source frame whose audio features best match the current control signal. Grain output is shaped by grain-only EQ, then blended with the dry signal before reverb and output gain.

## Signal Chain

```
Sidechain in ──[stereo→mono]──→ FeatureExtractor ──→ ctrl_features
                                                            │
Main in ──[stereo→mono]──→ FeatureExtractor ──→ src_features ──→ CorpusStore (stereo L+R)
                                                            │
                              ConcatenativeMatcher (weighted L2 distance)
                                        │
                                  stereo grain (L+R, position-aligned crossfade)
                                        │
                              EQProcessor (grain only — low shelf / mid peak / high shelf)
                                        │
                              dry * mainIn + wet * EQ'd grain
                                        │
                              ReverbProcessor (juce::dsp::Reverb)
                                        │
                              Output gain → Limiter (−1 dBFS)
```

## Features

- **Stereo corpus** — source audio is stored as stereo L+R frames; grain output preserves the stereo field of the matched frame
- **Feature matching** — ZCR, RMS, spectral centroid, spectral tilt (independent weights per feature)
- **Hann-windowed FFT** — accurate spectral features with no leakage from the analysis window
- **Configurable frame size** — `matchLen` (10–100 ms) sets the analysis/grain size at load time (nearest power of two); `seekTime` (1–5 s) sets corpus depth
- **Tempo-sync** — toggle sync on the Match Len knob to lock grain size to a host BPM subdivision (1/16 through 2/1)
- **Variable crossfade** — Xfade knob (0–1) controls grain-boundary crossfade length; dial to 0 for audible clicks, 1 for smooth transitions
- **Grain-only EQ (Tilt)** — single Tilt knob shapes grain brightness: negative = dark (low boost / high cut), positive = bright (high boost / low cut)
- **Randomness** — blend between deterministic best-match and random near-match selection
- **Reverb** — single Space knob drives room size + damping together (small = tight/damped, large = open/airy); separate Wet level
- **Freeze** — lock corpus to prevent new frames from being written
- **Center-focus layout** — MorphPad (hero) + MatchVisualizer stacked in center; 3 live controls (Rand/Xfade/Freeze) on the left, 5 tone controls (Tilt/Space/Wet/Mix/Output) on the right
- **Morph pad** — 2D pad replaces four weight knobs; drag the thumb to blend ZCR (TL), RMS (TR), SC (BL), ST (BR) via bilinear weighting; corner glow dots show live feature levels
- **Match visualizer** — live 2-track animation showing ctrl RMS history (top) and corpus slots (bottom); connection line highlights the matched corpus block each grain cycle
- **Settings popover** — gear button (⚙) exposes load-time params (Seek/MatchLen/Sync/Div) and trim gains (Ctrl/Src) in a compact overlay
- **Custom UI** — black/white with mint green accents, Inter font, value-arc rotary knobs, stereo output level meter
- **Preset bank** — 51 factory presets across 8 categories (Utility, Drums, Texture, Glitch, Vocal, Lo-Fi, Tonal, FX) plus user preset save/load
- **A/B state slots** — capture and recall two parameter snapshots for instant comparison
- **MIDI learn** — right-click any knob and assign a MIDI CC; bindings persist with the DAW project

## Parameters

| Group | Parameter | Range | Notes |
|---|---|---|---|
| Concatenative | ZCR Weight | 0–1 | Zero-crossing rate matching weight |
| | RMS Weight | 0–1 | Loudness matching weight |
| | S.Centroid Weight | 0–1 | Brightness matching weight |
| | S.Tilt Weight | 0–1 | High-frequency energy matching weight |
| | Match Len | 10–100 ms | Analysis/grain frame size (load-time) |
| | Match Len Sync | on/off | Lock Match Len to host BPM subdivision |
| | Match Len Div | 1/16–2/1 | Subdivision when sync is on |
| | Seek Time | 1–5 s | Corpus rolling-window depth (load-time) |
| | Rand | 0–1 | Randomness among near-best matches |
| | Xfade | 0–1 | Grain boundary crossfade length (0 = click, 1 = 256 samples) |
| | Ctrl Gain | −24–+24 dB | Scales sidechain before feature extraction |
| | Src Gain | −24–+24 dB | Scales corpus audio after feature extraction |
| | Freeze | on/off | Locks corpus (no new frames written) |
| EQ | Tilt | −1–+1 | Negative = dark (low boost/high cut), positive = bright (high boost/low cut) |
| Reverb | Space | 0–1 | 0 = tight/damped (room 0.20, damp 0.90), 1 = open/airy (room 0.95, damp 0.20) |
| | Wet | 0–1 | Reverb wet level |
| Output | Gain | −24–+24 dB | Final output gain |
| | Mix | 0–100% | Dry/wet blend |

## Building

Requires CMake 3.27+ and a C++17 compiler. Dependencies are fetched automatically via CPM.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Stitcher_All     # VST3 + AU + Standalone
cmake --build build --target StitcherTests    # Catch2 unit tests
cmake --build build --target Stitcher_pluginval_cli  # pluginval format validation
```

## Testing

- **Unit tests** — 62 Catch2 tests covering FeatureExtractor, CorpusStore, ConcatenativeMatcher, EQProcessor, EQTilt, ReverbSpace, PresetManager, MidiLearn, MorphPad
- **Plugin validation** — pluginval at strictness level 5 against AU

## Usage

The plugin requires two stereo inputs:

- **Input 1 (Main)** — source audio to build the corpus from
- **Input 2 (Sidechain)** — control signal that drives the feature matching

In most DAWs, route the sidechain via the plugin's side-chain input. If the sidechain is unconnected, the plugin outputs silence (corpus still accumulates from the main input).

## Changelog

### v0.4.0 — center-focus UI cycle (2026-05-23)

- **Center-focus layout** — removed 4-panel grid; MorphPad + MatchVisualizer occupy the center; 3 live controls (Rand/Xfade/Freeze) on the left, 5 tone controls (Tilt/Space/Wet/Mix/Output) on the right
- **EQ 3→1 Tilt** — single Tilt knob (-1..+1) replaces Low/Mid/High EQ; maps to ±12 dB inverse low/high shelves; old band params kept in APVTS for preset compatibility
- **Reverb 3→2 Space+Wet** — Space knob (0..1) drives room size and damping together; Wet stays separate; old Room/Damp kept in APVTS
- **Settings popover** — gear button (⚙) in preset bar reveals Seek, Match Len, Sync, Div, Ctrl Gain, Src Gain (load-time + trim controls no longer cluttering main face)
- **APVTS grows to 23 params** (eqTilt + reverbSpace added; 21 existing kept for backward preset compatibility)

### v0.3.0 — ui-v3 cycle (2026-05-23)

- **Black/white + mint palette** — Background #000000, Panel #0F0F0F, Accent #7FD8A8 mint green; replaces dark amber theme
- **Xfade parameter** — 0–1 knob maps to 0–256 sample crossfade; at 0 grain edges click (textural choice), at 1 fully smooth
- **4-corner morph pad** — replaces ZCR/RMS/SC/ST weight knobs; drag thumb to bilinearly blend all four weights; corner glow dots reflect live ctrl feature levels
- **Match visualizer** — live 2-track animation: top scrolls ctrl RMS history, bottom shows 32 corpus slots with matched slot highlighted; diagonal connection line follows each grain match

### v0.2.0 — ui-presets cycle (2026-05-22 – 2026-05-23)

- **Custom LookAndFeel** — dark theme (#1A1A1F background, amber #FFA84C accents), value-arc rotary knobs, Inter font embedded via BinaryData
- **Preset bar** — save / save-as / load / init / prev / next controls across the full top strip; user presets stored as XML in `~/Library/Application Support/Stitcher/Presets/User/`
- **51 factory presets** — embedded in binary across 8 categories: Utility, Drums, Texture, Glitch, Vocal, Lo-Fi, Tonal, FX
- **Live feature meters** — per-feature (ZCR/RMS/SC/ST) horizontal bargraphs alongside weight knobs, dimmed when weight = 0
- **Corpus fill indicator** — thin progress bar shows how full the rolling corpus buffer is
- **Stereo output level meter** — peak meter in the Output section driven by post-limiter atomics
- **Tempo-sync match len** — Sync toggle + subdivision dropdown (1/16–2/1) locks grain size to host BPM
- **A/B state slots** — four buttons in the preset bar (A+ / A / B+ / B) for instant parameter snapshot comparison (session-local)
- **MIDI learn** — right-click any knob → "Learn MIDI CC" → bind next incoming CC; bindings persist with DAW project state
- **Crash fix** — null `Typeface::Ptr` guard in `StitcherLookAndFeel` constructor prevents SIGSEGV in LTO builds

### dsp-finetune cycle (2026-04-01 – 2026-05-22)

- **Custom UI** — 4-section rotary layout (Concatenator / EQ / Reverb / Output) replacing the JUCE generic editor
- **Stereo grain pipeline** — corpus stores L/R audio per frame; matched grains play back in stereo preserving the source's stereo field
- **Functional `matchLen`** — analysis frame size is now driven by the Match Len knob (nearest power of two, resolved at load time)
- **Grain-only EQ** — EQ now shapes the grain before the dry/wet blend; the dry signal is unaffected at Mix = 0
- **Decoupled Src Gain** — `gainSrc_` is applied after feature extraction; boosting source volume no longer shifts RMS matching
- **Position-aligned crossfade** — 256-sample crossfade at grain boundaries eliminates clicks without resetting playback position
- **Hann windowing** — FFT feature extraction uses a Hann window for accurate spectral measurements
- **Audio-thread safety** — matcher weight data race fixed with atomic dirty-flag pattern; `candidates_` pre-allocated to avoid audio-thread heap allocation
- **Reverb tail** — `getTailLengthSeconds()` returns 5.0 s so DAWs keep processing the reverb decay after audio stops
