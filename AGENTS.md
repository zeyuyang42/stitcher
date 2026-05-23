# AGENTS.md — Stitcher

## Build
```bash
# Configure (once)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build plugin (VST3 + AU + Standalone)
cmake --build build --target Stitcher_All

# Build individual formats
cmake --build build --target Stitcher_VST3
cmake --build build --target Stitcher_AU          # macOS only
cmake --build build --target Stitcher_Standalone
```
Requires CMake 3.27+, C++17. Dependencies (JUCE, Catch2, pluginval) are fetched automatically via CPM.cmake. No vcpkg, no Conan.

## Test
```bash
# Build + run all tests
cmake --build build --target StitcherTests && ./build/tests/StitcherTests

# Run one test by name substring (Catch2)
./build/tests/StitcherTests "ZCR is 0 for silence"

# Plugin validation
cmake --build build --target Stitcher_pluginval_cli
```
Tests compile Source `.cpp` files directly (not via a library target). Test binary is `build/tests/StitcherTests`, **not** `build/StitcherTests`.

## Architecture

### Plugin layout
- Single-project (not a monorepo). One JUCE plugin → VST3 + AU + Standalone.
- Entrypoint: `Source/PluginProcessor.cpp` → `createPluginFilter()` returns `new StitcherProcessor`.
- Editor is `StitcherEditor` (custom 4-section UI in `Source/PluginEditor.h/.cpp`).
- Company: Absinthismus, Plugin code: CSS1, Manufacturer code: Ab42.
- LookAndFeel: `StitcherLookAndFeel` (dark theme, amber accents, Inter font via BinaryData).
- Preset bar: `PresetBar` component at top — save/load/next/prev/init, A/B state slots.
- Factory presets: 51 XMLs embedded via `juce_add_binary_data`; `makeFactoryPresets()` in `FactoryPresets.h` registers them with `PresetManager`.
- MIDI learn: `MidiLearn` in `Source/MidiLearn.h/.cpp` — right-click any knob → bind CC.
- Morph pad: `MorphPad` in `Source/UI/MorphPad.h/.cpp` — 2D bilinear blending of the four feature weights (TL=ZCR, TR=RMS, BL=SC, BR=ST); replaces four separate knobs.
- Match visualizer: `MatchVisualizer` in `Source/UI/MatchVisualizer.h/.cpp` — live 2-track strip showing ctrl RMS history (top) and corpus slots (bottom) with a connection line to the matched slot.

### Source layout
```
Source/
  LookAndFeel/StitcherLookAndFeel.h/.cpp  — custom JUCE LookAndFeel_V4
  UI/PresetBar.h/.cpp          — top-strip preset nav + A/B slots
  UI/FeatureMeter.h/.cpp       — horizontal bargraph (used for corpus fill indicator)
  UI/LevelMeter.h/.cpp         — stereo peak meter
  UI/MorphPad.h/.cpp           — 4-corner bilinear morph pad for feature weights
  UI/MatchVisualizer.h/.cpp    — live corpus/ctrl match animation strip
  UI/SettingsPopover.h/.cpp    — gear popover: Seek/MatchLen/Sync/Div/CtrlGain/SrcGain
  Assets/Fonts/            — Inter-Regular.ttf (embedded via BinaryData)
  Assets/Presets/          — 51 factory preset XMLs (embedded via BinaryData)
  PluginEditor.h/.cpp
  PluginProcessor.h/.cpp
  Parameters.h             — 23 APVTS parameter IDs (added eqTilt + reverbSpace in v0.4)
  FeatureExtractor.h/.cpp
  CorpusStore.h/.cpp
  ConcatenativeMatcher.h/.cpp
  EQProcessor.h/.cpp
  ReverbProcessor.h/.cpp
  PresetManager.h/.cpp     — save/load/factory preset management
  FactoryPresets.h         — makeFactoryPresets() — owned by editor, not processor
  MidiLearn.h/.cpp         — lock-free MIDI CC → parameter binding
```

### Signal chain (order matters)
1. Stereo→mono mix of both inputs → `ctrlAccum_` / `srcAccum_` (for features); raw stereo → `srcAccumL_`/`srcAccumR_` (for corpus audio)
2. Feature extraction (ZCR, RMS, spectral centroid, tilt) from the mono mixes
3. `gainSrc_` applied to stereo corpus audio **after** feature extraction (does not affect RMS matching)
4. CorpusStore push (stereo L+R) + ConcatenativeMatcher match → stereo grain pointers
5. Grain playback with position-aligned crossfade on L and R independently
6. EQ applied to grain only (via `grainMixBuf_` staging)
7. Dry/wet blend: `dry * mainInput + wet * grainEQ'd`
8. Reverb → Output gain → Limiter (−1 dBFS)

`gainCtrl_` is applied **before** feature extraction (normalises ctrl signal for matching). `gainSrc_` is applied **after** feature extraction (affects playback level, not RMS matching).

### I/O requirements
- **Two stereo inputs required**: Main (bus 0) + Sidechain (bus 1).
- If sidechain is unconnected → plugin outputs silence (corpus still accumulates from main).
- Guard: `sidechain.getNumChannels() >= 2` check before processing.

### Constants and load-time parameters
- Frame size: driven by `matchLen` parameter (10–100 ms, nearest power of two, min 64, max 8192). When `matchLenSync=true`, resolved from host BPM + `matchLenDiv` subdivision at `prepareToPlay`. Requires plugin reload to take effect.
- Crossfade length: **256 samples** (~5.8 ms at 44.1 kHz), constant.
- `seekTime` drives corpus capacity (`seekTime * sampleRate / frameSize + 1` frames) at `prepareToPlay`.

## Thread safety
Parameters modified on the message thread, consumed on the audio thread. Use the **atomic dirty-flag pattern**:
- `std::atomic<bool>` dirty flags: `eqDirty_`, `reverbDirty_`, `matcherDirty_`
- `std::atomic<float>` / `std::atomic<bool>` for direct reads: `gainCtrl_`, `gainSrc_`, `gainOut_`, `mix_`, `freeze_`
- New parameters must follow this pattern — never read `juce::AudioParameterFloat` values on the audio thread.

**MidiLearn thread model:**
- `midiLearn_.handleCC()` — audio thread only; reads `ccToParam_[cc]` atomically.
- `midiLearn_.processCapture()` — message thread only (called from 30 Hz timer); finalizes bindings after a learn capture by draining `capturedCC_` atomic.
- `ccToParam_[128]` is `std::array<std::atomic<RangedAudioParameter*>, 128>` — lock-free.

**Live meter atomics** (processor → editor, 30 Hz timer):
- `lastCtrlZcr_`, `lastCtrlRms_`, `lastCtrlSc_`, `lastCtrlSt_` — latest ctrl feature values
- `lastCorpusFill_` — corpus fill ratio (0–1)
- `lastOutPeakL_`, `lastOutPeakR_` — post-limiter peak levels

## Docs
- `docs/dsp-review.md` — canonical self-review with tracked issues and design decisions.
- `docs/superpowers/specs/` — design specs.
- `docs/superpowers/plans/` — implementation plans produced by Claude Code. Plans are step-by-step guides with checkboxes.
- README.md — user-facing overview and parameter table.

## Working with implementation plans
Plans in `docs/superpowers/plans/` (or specified by the user) **may be stale** — they were designed against a snapshot of the codebase that may have diverged. Before executing any plan:

1. **Read the full plan** to understand the goal and architecture.
2. **Verify each task against the current codebase** — check whether the stated starting state, file list, and code snippets still match reality. Plans often contain inline code blocks that are no longer accurate.
3. **Skip already-completed steps.** Don't blindly re-execute code that's already in place.
4. **Execute one task at a time**, following the plan's checkbox tracking. Build and run tests after each commit.
5. **If a plan step contradicts current code**, trust the current code over the plan. The plan is a guide, not authority.

## Linting & CI
- No CI, no linter, no formatter configured. Only compile-time warnings via `juce_recommended_warning_flags`.
- No GitHub Actions, no pre-commit hooks.
