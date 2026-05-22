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
- **Position-aligned crossfade** — 256-sample crossfade between consecutive grains eliminates boundary clicks without resetting playback position
- **Grain-only EQ** — 3-band EQ shapes the grain signal before the dry/wet blend; dry path is unaffected
- **Randomness** — blend between deterministic best-match and random near-match selection
- **Reverb** — room size, damping, wet level
- **Freeze** — lock corpus to prevent new frames from being written

## Parameters

| Group | Parameter | Range | Notes |
|---|---|---|---|
| Concatenative | ZCR Weight | 0–1 | Zero-crossing rate matching weight |
| | RMS Weight | 0–1 | Loudness matching weight |
| | S.Centroid Weight | 0–1 | Brightness matching weight |
| | S.Tilt Weight | 0–1 | High-frequency energy matching weight |
| | Match Len | 10–100 ms | Analysis/grain frame size (load-time) |
| | Seek Time | 1–5 s | Corpus rolling-window depth (load-time) |
| | Rand | 0–1 | Randomness among near-best matches |
| | Ctrl Gain | −24–+24 dB | Scales sidechain before feature extraction |
| | Src Gain | −24–+24 dB | Scales corpus audio after feature extraction |
| | Freeze | on/off | Locks corpus (no new frames written) |
| EQ | Low | −12–+12 dB | Low shelf at 200 Hz |
| | Mid | −12–+12 dB | Mid peak at 1 kHz |
| | High | −12–+12 dB | High shelf at 6 kHz |
| Reverb | Room | 0–1 | Room size |
| | Damp | 0–1 | High-frequency damping |
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

- **Unit tests** — 31 Catch2 tests covering FeatureExtractor, CorpusStore, ConcatenativeMatcher, EQProcessor
- **Plugin validation** — pluginval at strictness level 5 against AU

## Usage

The plugin requires two stereo inputs:

- **Input 1 (Main)** — source audio to build the corpus from
- **Input 2 (Sidechain)** — control signal that drives the feature matching

In most DAWs, route the sidechain via the plugin's side-chain input. If the sidechain is unconnected, the plugin outputs silence (corpus still accumulates from the main input).

## Changelog

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
