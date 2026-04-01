# Stitcher

Real-time corpus-based concatenative synthesizer — JUCE VST3 / AU / Standalone plugin.

Listens to a **sidechain (control)** signal and a **main (source)** signal, builds a rolling corpus of analyzed source frames, and continuously outputs the source frame whose audio features best match the current control signal. Followed by 3-band EQ, reverb, and output gain/mix.

## Signal Chain

```
Sidechain in ──→ FeatureExtractor ──→ features_ctrl
                                             │
Main in ──→ FeatureExtractor ──→ features_src ──→ CorpusStore
                                             │
                          ConcatenativeMatcher (weighted L2 distance)
                                    │
                               EQProcessor (low shelf / mid peak / high shelf)
                                    │
                            ReverbProcessor (juce::dsp::Reverb)
                                    │
                            Output (gain + dry/wet mix)
```

## Features

- **Feature matching** — ZCR, RMS, spectral centroid, spectral tilt (independent weights per feature)
- **Corpus** — rolling circular buffer, configurable seek time (1–5 s), freeze toggle
- **Randomness** — blend between deterministic best-match and random near-match selection
- **3-band EQ** — low shelf (200 Hz), mid peak (1 kHz), high shelf (6 kHz)
- **Reverb** — room size, damping, wet level
- **Output** — gain and dry/wet mix

## Parameters

| Group | Parameters |
|---|---|
| Concatenative | ZCR / RMS / S.Centroid / S.Tilt weights, Match Len, Seek Time, Rand, Freeze, Ctrl Gain, Src Gain |
| EQ | Low, Mid, High (±12 dB) |
| Reverb | Room, Damp, Wet |
| Output | Gain, Mix |

## Building

Requires CMake 3.27+ and a C++17 compiler. Dependencies are fetched automatically via CPM.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Stitcher_All     # VST3 + AU + Standalone
cmake --build build --target StitcherTests    # Catch2 unit tests
cmake --build build --target Stitcher_pluginval_cli  # pluginval format validation
```

## Testing

- **Unit tests** — 20 Catch2 tests covering FeatureExtractor, CorpusStore, ConcatenativeMatcher, EQProcessor
- **Plugin validation** — pluginval at strictness level 5 against VST3 and AU

## Usage

The plugin requires two stereo inputs:

- **Input 1 (Main)** — source audio to build the corpus from
- **Input 2 (Sidechain)** — control signal that drives the feature matching

In most DAWs, route the sidechain via the plugin's side-chain input. If the sidechain is unconnected, the plugin outputs silence (corpus still accumulates from the main input).
