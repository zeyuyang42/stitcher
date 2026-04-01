# Stitcher — Concatenative Synthesizer JUCE Plugin Design

**Date:** 2026-04-01
**Source reference:** `../concatenative-synthesizer/CSS.sc` (SuperCollider implementation)
**Target:** JUCE plugin (VST3 / AU / Standalone), project name `Stitcher`

---

## 1. Overview

Port the CSS.sc SuperCollider concatenative sound synthesizer into a fully self-contained JUCE audio plugin. The plugin performs real-time corpus-based concatenative synthesis: it listens to a **control** signal (sidechain input) and a **source** signal (main input), builds a rolling corpus of analyzed source frames, and continuously outputs the source frame that best matches the current control signal's audio features.

Post-matching processing: 3-band EQ → reverb → output gain/mix.

---

## 2. Signal Chain

```
Sidechain in ──→ FeatureExtractor ──→ features_ctrl [zcr, rms, sc, st]
                                                │
Main in ──→ FeatureExtractor ──→ features_src ──→ CorpusStore.push({audio, features})
                                                │
                          ConcatenativeMatcher ←┘ (iterates corpus, weighted L2 distance)
                                    │
                               EQProcessor (3-band: low shelf, mid peak, high shelf)
                                    │
                            ReverbProcessor (juce::dsp::Reverb)
                                    │
                            Output (gain_out + dry/wet mix)
                                    │
                              Stereo output
```

**Key constraint:** `FeatureExtractor` is called twice per block — once on the source block (at corpus store time) and once on the control block (at match time). Corpus frames hold pre-computed features so the matching step is a cheap distance search with no FFT.

---

## 3. Component Design

### 3.1 FeatureExtractor

Extracts 4 scalar features from a mono audio buffer of `frameSize` samples.

| Feature | Abbreviation | Method |
|---|---|---|
| Zero-crossing rate | ZCR | Count sign changes / N |
| RMS loudness | RMS | `sqrt(mean(x²))` |
| Spectral centroid | SC | FFT → weighted mean frequency bin |
| Spectral tilt | ST | FFT → energy in top half of spectrum / total energy, normalized 0–1 |

Uses `juce::dsp::FFT` for SC and ST. Frame size: 1024 samples (fixed). Returns `struct Features { float zcr, rms, sc, st; }`.

Interface:
```cpp
class FeatureExtractor {
public:
    void prepare(int frameSize);
    Features extract(const float* samples, int numSamples);
};
```

### 3.2 CorpusStore

A fixed-capacity circular buffer of frames. Each slot holds audio samples and pre-extracted features.

```cpp
struct CorpusFrame {
    std::vector<float> audio;   // frameSize samples
    Features features;
};

class CorpusStore {
public:
    void prepare(int frameSize, int maxFrames);
    void push(const float* audio, const Features& features); // no-op if frozen
    int size() const;
    const CorpusFrame& getFrame(int index) const;
    void setFrozen(bool frozen);
};
```

`maxFrames` = `seek_time * sampleRate / frameSize`. When the buffer is full, oldest frames are overwritten (circular). Freeze stops new frames from being written.

### 3.3 ConcatenativeMatcher

Runs once per block. Takes current control features, searches corpus, outputs best-matching frame's audio.

```cpp
class ConcatenativeMatcher {
public:
    void prepare(int frameSize);
    void setWeights(float zcr, float rms, float sc, float st);
    void setRand(float rand);       // 0 = deterministic best, 1 = pick from top-k
    const float* match(const Features& controlFeatures, const CorpusStore& corpus);
};
```

Distance metric: `d = sqrt(w_zcr*(Δzcr)² + w_rms*(Δrms)² + w_sc*(Δsc)² + w_st*(Δst)²)`

Rand: when `rand > 0`, collect all frames within `rand * d_min` distance and select one at random. Output is crossfaded with the previous frame over 64 samples to avoid clicks.

### 3.4 EQProcessor

Three `juce::dsp::IIR::Filter<float>` instances in series:
- **Low shelf** — centre freq 200 Hz, `eq_low` gain in dB
- **Mid peak** — centre freq 1 kHz, `eq_mid` gain in dB
- **High shelf** — centre freq 6 kHz, `eq_high` gain in dB

```cpp
class EQProcessor {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setGains(float lowDb, float midDb, float highDb);
    void process(juce::dsp::AudioBlock<float>& block);
};
```

Coefficients recalculated whenever gains change via `juce::dsp::IIR::Coefficients`.

### 3.5 ReverbProcessor

Thin wrapper around `juce::dsp::Reverb`.

```cpp
class ReverbProcessor {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void setParams(float roomSize, float damping, float wetLevel);
    void process(juce::dsp::AudioBlock<float>& block);
};
```

---

## 4. Parameters

All parameters registered in `AudioProcessorValueTreeState` (APVTS).

| ID | Label | Range | Default | Group |
|---|---|---|---|---|
| `zcr_weight` | ZCR | 0.0–1.0 | 0.0 | Concatenative |
| `rms_weight` | RMS | 0.0–1.0 | 1.0 | Concatenative |
| `sc_weight` | S.Centroid | 0.0–1.0 | 0.0 | Concatenative |
| `st_weight` | S.Tilt | 0.0–1.0 | 0.0 | Concatenative |
| `match_len` | Match Len | 10–100 ms | 50 ms | Concatenative |
| `seek_time` | Seek Time | 1.0–5.0 s | 1.0 s | Concatenative |
| `rand` | Rand | 0.0–1.0 | 0.0 | Concatenative |
| `freeze` | Freeze | bool | false | Concatenative |
| `gain_ctrl` | Ctrl Gain | -24–12 dB | 0 dB | Concatenative |
| `gain_src` | Src Gain | -24–12 dB | 0 dB | Concatenative |
| `eq_low` | Low | -12–12 dB | 0 dB | EQ |
| `eq_mid` | Mid | -12–12 dB | 0 dB | EQ |
| `eq_high` | High | -12–12 dB | 0 dB | EQ |
| `reverb_room` | Room | 0.0–1.0 | 0.5 | Reverb |
| `reverb_damp` | Damp | 0.0–1.0 | 0.5 | Reverb |
| `reverb_wet` | Wet | 0.0–1.0 | 0.0 | Reverb |
| `gain_out` | Output | -36–12 dB | 0 dB | Output |
| `mix` | Mix | 0–100% | 100% | Output |

---

## 5. UI Layout

Partial custom UI — `juce::Slider` (rotary) with `SliderAttachment` for all knobs. No oscilloscopes.

```
┌──────────────────────────┬──────────────┬──────────────┬──────────┐
│      CONCATENATOR        │      EQ      │    REVERB    │  OUTPUT  │
│                          │              │              │          │
│  [zcr] [rms] [sc] [st]  │ [low][mid]   │ [room][damp] │  [gain]  │
│  [time][len][rand]       │    [high]    │   [wet]      │  [mix]   │
│  [gain_ctrl][gain_src]   │              │              │          │
│  [ FREEZE toggle btn ]   │              │              │          │
└──────────────────────────┴──────────────┴──────────────┴──────────┘
```

---

## 6. Source File Layout

```
Source/
  FeatureExtractor.h/.cpp     — feature extraction (ZCR, RMS, SC, ST)
  CorpusStore.h/.cpp          — rolling circular frame buffer
  ConcatenativeMatcher.h/.cpp — weighted distance matching + crossfade
  EQProcessor.h/.cpp          — 3-band IIR EQ
  ReverbProcessor.h/.cpp      — juce::dsp::Reverb wrapper
  Parameters.h                — APVTS parameter layout (replaces current)
  PluginProcessor.h/.cpp      — orchestrator (renamed to StitcherProcessor)
  PluginEditor.h/.cpp         — partial custom UI (renamed to StitcherEditor)
  ProtectYourEars.h           — kept as-is

tests/
  FeatureExtractorTest.cpp
  CorpusStoreTest.cpp
  ConcatenativeMatcherTest.cpp
  EQProcessorTest.cpp
```

---

## 7. Build & Test Pipeline

**CMake dependencies (all via CPM):**
```cmake
# JUCE (already present)
CPMAddPackage(NAME juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG origin/master)

# Catch2 for unit tests
CPMAddPackage("gh:catchorg/Catch2@3.7.1")

# pluginval for plugin format validation (requires JUCE 8, after juce_add_plugin)
CPMAddPackage("gh:Tracktion/pluginval#develop")
```

**Targets:**
```
cmake --build . --target StitcherTests          # Catch2 unit tests
cmake --build . --target Stitcher_pluginval_cli # pluginval format validation
cmake --build . --target Stitcher_All           # build VST3 + AU + Standalone
```

**Unit test coverage:**

| File | Tests |
|---|---|
| `FeatureExtractorTest.cpp` | Silence → ZCR=0, RMS=0; sine wave → known SC; DC signal → SC=0 |
| `CorpusStoreTest.cpp` | Push N frames → size correct; circular overwrite; freeze prevents writes |
| `ConcatenativeMatcherTest.cpp` | Identical features → distance=0; weight=0 ignores feature; rand range |
| `EQProcessorTest.cpp` | Flat input with 0 dB gains → unity pass-through |

**pluginval:** `PLUGINVAL_STRICTNESS_LEVEL 5`, runs against `Stitcher_VST3` on all platforms, `Stitcher_AU` on macOS.

---

## 8. Naming

| Old | New |
|---|---|
| `ChaoticSonicStitcher` | `Stitcher` |
| `ChaoticSonicStitcherProcessor` | `StitcherProcessor` |
| `ChaoticSonicStitcherEditor` | `StitcherEditor` |
| `PROJECT_NAME ChaoticSonicStitcher` | `PROJECT_NAME Stitcher` |
| `PRODUCT_NAME Stitcher` | unchanged |
