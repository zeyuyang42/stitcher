# Stitcher DSP Implementation Review

**Date:** 2026-04-01 (updated 2026-04-02)  
**Branch:** dsp-finetune  
**Scope:** Full self-review of every DSP component against the design spec and real-time audio constraints.

**Status:** All 3 🔴 bugs fixed as of 2026-04-02. See Issue Summary table for current state.

---

## How to Use This Document

Each section covers one component. For each, you'll find:
- **What it does** — a plain-language summary of the algorithm
- **What to check** — specific lines/behaviors to verify when inspecting the code
- **Issues found** — bugs or concerns, rated by severity

Severity ratings:
- 🔴 **Bug** — incorrect behavior or real-time safety violation
- 🟡 **Quality** — works but has a meaningful flaw or design gap
- 🟢 **Minor** — cosmetic, misleading comment, or very-edge-case risk

---

## 1. Signal Chain Overview

```
Sidechain in ──[stereo→mono mix]──→ ctrlAccum_
                                          │
Main in ──[stereo→mono mix]──→ srcAccum_ ─┤
                                          │
                              (every kFrameSize=1024 samples)
                                          │
                              FeatureExtractor.extract(src) → push → CorpusStore
                              FeatureExtractor.extract(ctrl) ──────→ ConcatenativeMatcher.match()
                                                                            │
                                                                      grainBuf_ (1024 samples)
                                                                            │
                              dry*mainInput + wet*grain ─────────────────→ output
                                                                            │
                                                                       EQProcessor
                                                                            │
                                                                      ReverbProcessor
                                                                            │
                                                                    output * gainOut_
                                                                            │
                                                                     Limiter (-1 dBFS)
                                                                            │
                                                                       Stereo out
```

**Key observation:** EQ and Reverb sit after the dry/wet blend. They process the already-mixed `dry*main + wet*grain` signal — not the grain alone. This is worth knowing when using EQ to shape the grain output.

---

## 2. FeatureExtractor (`Source/FeatureExtractor.h/.cpp`)

### What it does

Extracts four scalar features from a mono buffer of exactly `kFrameSize` (1024) samples:

| Feature | Algorithm | Range |
|---|---|---|
| ZCR | sign changes / (N−1) | [0, 1] |
| RMS | sqrt(mean(x²)) | [0, ∞) |
| SC | weighted mean bin / (N/2), bins 1..N/2−1 | [0, 1] |
| ST | energy in bins N/4..N/2 / total energy, bins 1..N/2−1 | [0, 1] |

FFT: `juce::dsp::FFT` of order 10 (1024-point). Buffer is 2048 floats. `performFrequencyOnlyForwardTransform` fills bins 0..511 with magnitudes. SC and ST read bins 1..511.

### What to check

- `fftBuffer_.assign(frameSize * 2, 0.f)` at line — verifies the 2048-element buffer matches JUCE's `2 * 2^order` requirement.
- `std::fill` before every extract call — verifies no stale data from a previous frame leaks into the FFT.
- `computeSc`: loop `k=1` to `k < halfN` — DC bin 0 is correctly skipped.
- `computeSt`: `mid = halfN / 2 = 256` — confirms "high half" means bins 256..511 (~12 kHz and above at 44.1 kHz), which is quite high. This differs from a typical "spectral tilt" definition. Worth being aware of when using `st_weight`.

### Issues

🟡 **No windowing before FFT.** A rectangular window (no windowing) causes spectral leakage. For a 1024-sample frame of a 440 Hz sine, energy bleeds across many bins, making SC and ST less accurate than they could be. A Hann window would improve measurement quality. Not a crash, but affects matching character.

🟢 **Magnitudes are not normalized.** SC and ST are computed as ratios so normalization doesn't matter. ZCR and RMS are computed directly from time-domain, unaffected. No impact on correctness.

---

## 3. CorpusStore (`Source/CorpusStore.h/.cpp`)

### What it does

A fixed-capacity circular buffer of `CorpusFrame` objects. Each frame holds 1024 audio samples and a `Features` struct. `push()` writes to `writeIndex_` then advances it. `getFrame(index)` maps logical index [0 = oldest, size()−1 = newest] to the correct circular slot:

```
oldestSlot = (writeIndex_ - count_ + maxFrames_) % maxFrames_
slot       = (oldestSlot + index)  % maxFrames_
```

Capacity at 44.1 kHz, 1024-sample frames: 1 s ≈ 43 frames, 5 s ≈ 215 frames.

### What to check

- After filling to `maxFrames_` and pushing one more: verify `size()` stays at `maxFrames_` and `getFrame(0)` returns what was pushed second (oldest = second push, not first).
- `setFrozen(true)` → push → `size()` must not change.
- `newestIndex()` returns `count_ - 1` — confirm this is used with `getFrame()`, not as a raw slot index.

### Issues

🟢 **`newestIndex()` header comment says "slot index" but returns a logical index.** The implementation is correct and matches test usage (`getFrame(store.newestIndex())`), but the comment in `CorpusStore.h` says "slot index, not logical index" — the opposite of the truth. Low risk (only affects readability), but worth correcting.

🟢 **`corpus_.setFrozen(freeze_.load())` is called every block.** A dirty flag would be marginally more efficient, but functionally fine.

---

## 4. ConcatenativeMatcher (`Source/ConcatenativeMatcher.h/.cpp`)

### What it does

Each call to `match()`:
1. Iterates the full corpus, computes weighted L2 distance to each frame, finds `bestIdx`.
2. If `rand_ > 0`: re-iterates the corpus, collects all frames within `(1 + rand_) * minDist + 1e-6f`, picks one at random.
3. Crossfades the previous 1024-sample output into the new matched frame over 64 samples.
4. Returns a pointer to the internal `outputBuffer_`.

**Distance formula:**
```cpp
sqrt((wZcr * ΔZcr)² + (wRms * ΔRMS)² + (wSc * ΔSC)² + (wSt * ΔST)²)
```
The weight is multiplied with the delta before squaring, so the effective contribution of a feature scales as `w²`. The spec describes `w * (Δf)²`, which scales as `w`. Both are valid monotonic distance metrics, but the response to weight changes is quadratic here rather than linear.

### What to check

- With `rand_=0`: verify only one corpus pass happens (the min-search loop).
- With `rand_=0` and two frames of identical features: both have distance 0 → `bestIdx` should be 0 (first match wins). Confirm no undefined behavior.
- Crossfade end: at `i = kCrossfadeLen - 1 = 63`, `t = 63/64 ≈ 0.984`. The output is 98.4% new frame, 1.6% previous. Fully fades into new frame by sample 64 onward.
- `corpus.size() == 0`: `match()` returns `nullptr`. Verify `processBlock` checks this and leaves `grainBuf_` unchanged.

### Issues

✅ ~~🔴~~ **Heap allocation on the audio thread when `rand_ > 0`.** **FIXED** (commit a2a64f8)

`candidates_` is now a `std::vector<int>` member, reserved to capacity 512 in `prepare()`. The audio-thread path uses `candidates_.clear()` + `candidates_.push_back()` which never allocates within that capacity.

✅ ~~🔴~~ **Data race on matcher weight members.** **FIXED** (commit 5d4ba60)

`parameterChanged()` now sets `matcherDirty_ = true` (same pattern as EQ/reverb). `processBlock` calls `updateMatcherFromParams()` at the start of the next audio block via `matcherDirty_.exchange(false)`.

🟡 **`grainPos_` wraps but grows unboundedly between grain updates.** 

```cpp
float grain = grainReady_ ? grainBuf_[grainPos_ % kFrameSize] : 0.f;
++grainPos_;
```

`grainPos_` resets to 0 on each new grain but otherwise increments every sample. Between grain updates (every 1024 samples at 44.1 kHz, ~43 Hz), it wraps via `% kFrameSize` correctly. However, if a grain update is skipped or delayed, `grainPos_` keeps growing. An `int` would overflow after ~13.6 hours of continuous use. Low real-world risk, fixable with `grainPos_ %= kFrameSize` or `grainPos_ = 0` every update.

🟡 **O(2n) distance computations when rand > 0.** The rand path does two full corpus passes per block: one to find `minDist`, one to collect candidates. At maximum corpus size (~215 frames), that's 430 distance calculations per block (~43 times per second). Each distance computation is cheap (4 multiplies, sqrt). Not a performance problem at this scale, but worth noting.

---

## 5. EQProcessor (`Source/EQProcessor.h/.cpp`)

### What it does

Three `juce::dsp::IIR::Filter<float>` in series:
- Low shelf at 200 Hz, Q = 0.707
- Mid peak at 1 kHz, Q = 0.707
- High shelf at 6 kHz, Q = 0.707

Coefficients are updated on demand (called from `processBlock` via `eqDirty_` flag). Filter state is not reset on coefficient change — this is correct behavior for smooth parameter response.

### What to check

- `spec.numChannels = 2` in `prepareToPlay`: `IIR::Filter::prepare(spec)` allocates state for 2 channels, so L and R are processed independently. ✓
- `eqDirty_` set on message thread, read via `exchange(false)` on audio thread: thread-safe because `std::atomic<bool>`. ✓
- Coefficient update happens at block boundary, once per block at most. Avoids mid-block coefficient change artifacts.

### Issues

🟡 **Q is fixed at 0.707 for all three bands.** For the mid peak filter especially, Q = 0.707 gives a wide bandwidth (~1.4 octaves). Users have no way to tighten it. This limits EQ precision but is a design choice, not a bug.

🟡 **EQ processes the dry+wet blend, not the grain alone.** When `mix_=0.5`, EQ shapes both the original main input signal and the grain simultaneously. You cannot EQ only the concatenative output.

---

## 6. ReverbProcessor (`Source/ReverbProcessor.h/.cpp`)

### What it does

Thin wrapper around `juce::dsp::Reverb`. `prepare()` initializes with default params (room=0.5, damp=0.5, wet=0.0, dry=1.0). `setParams()` maps the three user parameters to `juce::dsp::Reverb::Parameters`.

Note: `dryLevel = 1.f - wetLevel` inside `setParams`. So at `reverbWet=1.0`, dry=0.0 (full wet, no dry signal through reverb). The plugin's separate `mix` parameter then controls how much of this entire processed signal reaches the output.

### What to check

- At `reverbWet=0`: dry=1.0 → reverb unit passes signal unchanged. EQ output passes through as-is.
- Stereo processing: `juce::dsp::Reverb` internally uses `processStereo` for a 2-channel block. Confirm by checking that L and R outputs differ when room > 0. (The reverb does process stereo natively.)
- `setParameters` is called inside `processBlock` (via `reverbDirty_` flag). Thread-safe for the same reason as EQ.

### Issues

✅ ~~🔴~~ **`getTailLengthSeconds()` returns 0.0 despite reverb being active.** **FIXED** (commit 3077a72)

Now returns `5.0` seconds. The DAW will keep processing the plugin for 5 seconds after audio stops, allowing the reverb tail to decay naturally.

---

## 7. PluginProcessor (`Source/PluginProcessor.h/.cpp`)

### What to check — prepareToPlay

- `spec.numChannels = 2` passed to all DSP units — matches the stereo output. ✓
- `maxFrames = seekTime * sampleRate / kFrameSize + 1` — at 1.0s/44100 Hz: 44 frames. At 5.0s: 216 frames. ✓
- All member vectors (`ctrlAccum_`, `srcAccum_`, `ctrlMono_`, `srcMono_`, `grainBuf_`) resized here, not in `processBlock`. ✓

### What to check — processBlock

- **Sidechain guard** (`hasSidechain = sidechain.getNumChannels() >= 2`): prevents crash when sidechain is unconnected. When no sidechain, ctrl signal is silence → corpus still grows from main input, matcher always outputs the lowest-energy frame (since RMS will be near zero for ctrl). Effectively passes main input through.
- **Output block scoping** (`juce::dsp::AudioBlock<float> outputBlock(output)`): EQ and Reverb only process the 2-channel output block, not the full buffer (which in VST3 includes sidechain channels 2-3). ✓
- **`applyGain` then `limiter_.process`**: `outputBlock` is a view into `output`'s memory. `output.applyGain(0, numSamples, gainOut_)` modifies the underlying data in place. `limiter_.process(outputBlock)` then sees the gain-applied data. The pointer in `outputBlock` remains valid. ✓
- **In-place buffer**: `mainInput` and `output` share channels 0-1 of `buffer`. The grain write loop reads `inL[i]`/`inR[i]` and writes `outL[i]`/`outR[i]` at the same index — safe because the read happens before the write at each position. ✓

### Issues

🟡 **`gainCtrl_` and `gainSrc_` are applied before feature extraction.**

```cpp
ctrlAccum_[accumPos_] = ctrlMono_[i] * gainCtrl_.load();
srcAccum_[accumPos_]  = srcMono_[i]  * gainSrc_.load();
```

These gains affect feature values — especially RMS. Boosting `gainSrc_` by +6 dB doubles the RMS of every source frame in the corpus. The control RMS must be similarly boosted to match. This is consistent with the original SuperCollider behavior, but it means the two gain knobs interact non-obviously with RMS-weighted matching. Worth documenting.

🟡 **`matchLen` and `seekTime` parameters are non-functional at runtime.**

`matchLen` (10–100ms) is displayed in the UI but has no effect. Frame size is permanently `kFrameSize = 1024` samples (~23ms at 44.1kHz). Similarly, changing `seekTime` during a session doesn't resize the corpus — capacity is fixed at `prepareToPlay`. Changing `seekTime` requires reloading the plugin. Both parameters are listed as "reserved for future use" in the code comments, but a user seeing these knobs would expect them to do something.

🟡 **Wet grain is mono (L = R).**

```cpp
outL[i] = dry * inL[i] + wet * grain;
outR[i] = dry * inR[i] + wet * grain;
```

`grain` is a single float, so at `mix=1.0` (full wet), L and R are identical. The corpus is built from a mono mix of the main input, so stereo information from the source is discarded. Dry signal retains stereo. This is expected given the mono corpus design, but users may be surprised that increasing Mix collapses the stereo image.

🟢 **`protectYourEars(buffer)` operates on the full buffer** (DEBUG only). In VST3, `buffer` has 4 channels (main + sidechain). If sidechain channels happen to have high amplitude, `protectYourEars` may mute them. Since the sidechain has already been read before this call, there's no functional impact — just a note for debugging.

---

## 8. Parameters (`Source/Parameters.h`)

### What to check

- `gainOut` has `NormalisableRange` with skew factor `2.4f` — this gives finer control in the negative dB range. ✓
- `mix` is 0–100% normalized to 0–1 inside `parameterChanged`: `mix_ = newValue / 100.f`. ✓
- All 18 parameters are registered as listeners in the constructor loop, including `freeze` which requires its own `addParameterListener` call. ✓

### Issues

🟡 **`matchLen` parameter range is 10–100ms but actual frame size is fixed at ~23ms.** Misleading to the user.

---

## 9. Test Coverage Gaps

| Component | Missing Tests |
|---|---|
| FeatureExtractor | SC value for a known single-frequency sine (bin k → SC ≈ k/512). ST near 0 for a low-frequency signal, near 1 for a high-frequency signal. |
| ConcatenativeMatcher | Rand behavior: when rand=1.0, verify multiple distinct frames can be selected. No test currently exercises the `candidates` vector path. |
| EQProcessor | Negative gains (attenuation). Mid and high bands. Verify mid peak actually peaks at ~1kHz. |
| PluginProcessor | No integration test covering the full processBlock chain. |

---

## 10. Issue Summary

| # | Severity | Component | Description |
|---|---|---|---|
| 1 | ✅ Fixed | ConcatenativeMatcher | ~~Heap allocation (`std::vector<int>`) inside `match()` when rand > 0~~ — pre-allocated as member (commit a2a64f8) |
| 2 | ✅ Fixed | ConcatenativeMatcher / PluginProcessor | ~~Data race on matcher weight members~~ — atomic dirty flag pattern applied (commit 5d4ba60) |
| 3 | ✅ Fixed | PluginProcessor | ~~`getTailLengthSeconds()` returns 0.0~~ — now returns 5.0s (commit 3077a72) |
| 4 | 🟡 Quality | FeatureExtractor | No FFT windowing — spectral leakage reduces SC/ST accuracy |
| 5 | 🟡 Quality | ConcatenativeMatcher | Distance formula squares weights (`(w*Δf)²`) — spec says `w*(Δf)²`; semantically different but internally consistent |
| 6 | 🟡 Quality | PluginProcessor | `matchLen` and `seekTime` do nothing at runtime; misleading to users |
| 7 | 🟡 Quality | PluginProcessor | Wet grain is always mono — `mix` > 0 collapses stereo image |
| 8 | 🟡 Quality | PluginProcessor | `gainCtrl_` / `gainSrc_` applied before feature extraction — affects RMS matching in non-obvious ways |
| 9 | 🟡 Quality | EQProcessor | EQ processes blended dry+wet signal, not grain alone |
| 10 | 🟢 Minor | CorpusStore | `newestIndex()` comment says "slot index" but returns logical index |
| 11 | 🟢 Minor | ConcatenativeMatcher | `grainPos_` grows unboundedly (overflows after ~13h continuous use) |
| 12 | 🟢 Minor | Tests | Missing SC/ST precision tests, rand path test, negative EQ gain tests |

---

## Fix Status

**✅ Completed (2026-04-02):**
- #3: `getTailLengthSeconds()` → 5.0s
- #2: Matcher weight data race → atomic dirty flag
- #1: Audio-thread heap allocation → pre-allocated `candidates_` member

**Remaining (quality/minor, open for future work):**
- #11: `grainPos_` overflow — add `grainPos_ %= kFrameSize` or clamp in processBlock
- #4: FFT windowing — Hann window would improve SC/ST accuracy (changes matching character)
- #6: `matchLen` / `seekTime` — make them functional or remove from UI to avoid confusion
- #10: Fix `newestIndex()` comment in `CorpusStore.h`
- #12: Add missing Catch2 tests for SC/ST precision, rand path, negative EQ gains
