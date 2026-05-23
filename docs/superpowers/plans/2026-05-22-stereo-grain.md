# Stereo Grain Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Issue #7 — the corpus stores mono audio; increasing Mix collapses the stereo image. Make the corpus store stereo (L and R) audio, the matcher return stereo, and the grain playback preserve the original stereo field.

**Architecture:** Feature extraction remains mono (from stereo-to-mono mix) — only the stored audio becomes stereo. `CorpusFrame.audio` is split into `audioL` / `audioR`. `ConcatenativeMatcher::match()` signature changes to output L and R pointers by reference (returns bool). `PluginProcessor` accumulates stereo src audio in `srcAccumL_`/`srcAccumR_`, stores stereo corpus, and maintains stereo grain double-buffers. Memory doubles for corpus audio (~0.7–3.5 MB total — acceptable).

**Tech Stack:** JUCE, Catch2, C++17

---

## File Map

| File | Change |
|---|---|
| `Source/CorpusStore.h` | `CorpusFrame.audio` → `audioL`/`audioR`; push() takes separate L/R pointers |
| `Source/CorpusStore.cpp` | Update prepare() and push() for stereo |
| `Source/ConcatenativeMatcher.h` | `outputBuffer_` → `outputBufL_`/`outputBufR_`; match() returns bool, outL/outR by ref |
| `Source/ConcatenativeMatcher.cpp` | Update prepare() and match() for stereo |
| `Source/PluginProcessor.h` | Split grain double-buffers; add srcAccumL_/srcAccumR_ |
| `Source/PluginProcessor.cpp` | Stereo accumulation, corpus push, grain copy, output loop |
| `tests/CorpusStoreTest.cpp` | Update push() calls |
| `tests/ConcatenativeMatcherTest.cpp` | Update match() calls |

---

## Task 1: Update `CorpusStore` for stereo audio

**Files:**
- Modify: `Source/CorpusStore.h`
- Modify: `Source/CorpusStore.cpp`
- Modify: `tests/CorpusStoreTest.cpp`

- [ ] **Step 1: Rewrite `CorpusStore.h`**

Replace `CorpusFrame` and `push()` declaration:

```cpp
struct CorpusFrame {
    std::vector<float> audioL;
    std::vector<float> audioR;
    Features features;
};

class CorpusStore {
public:
    void prepare(int frameSize, int maxFrames);
    void push(const float* audioL, const float* audioR, const Features& features);
    int size() const;
    int newestIndex() const;
    const CorpusFrame& getFrame(int index) const;
    void setFrozen(bool frozen);
private:
    std::vector<CorpusFrame> frames_;
    int writeIndex_ = 0;
    int count_      = 0;
    int maxFrames_  = 0;
    int frameSize_  = 0;
    bool frozen_    = false;
};
```

- [ ] **Step 2: Update `CorpusStore.cpp`**

In `prepare()`, resize both channels:
```cpp
void CorpusStore::prepare(int frameSize, int maxFrames)
{
    frameSize_ = frameSize;
    maxFrames_ = maxFrames;
    frames_.resize(maxFrames);
    for (auto& f : frames_) {
        f.audioL.resize(frameSize, 0.f);
        f.audioR.resize(frameSize, 0.f);
    }
    writeIndex_ = 0;
    count_      = 0;
    frozen_     = false;
}
```

In `push()`, accept and copy L and R:
```cpp
void CorpusStore::push(const float* audioL, const float* audioR, const Features& features)
{
    if (frozen_) return;

    auto& slot = frames_[writeIndex_];
    std::copy(audioL, audioL + frameSize_, slot.audioL.begin());
    std::copy(audioR, audioR + frameSize_, slot.audioR.begin());
    slot.features = features;

    writeIndex_ = (writeIndex_ + 1) % maxFrames_;
    if (count_ < maxFrames_)
        ++count_;
}
```

- [ ] **Step 3: Update `tests/CorpusStoreTest.cpp` — replace mono push calls with stereo**

All existing push calls like `store.push(audio, features)` must become `store.push(audio, audio, features)` (passing same buffer as L and R for existing mono tests). Also update the test that reads `frame.audio[0]` to read `frame.audioL[0]`:

Find:
```cpp
    REQUIRE(store.getFrame(store.newestIndex()).audio[0] == Catch::Approx(2.f));
```
Replace:
```cpp
    REQUIRE(store.getFrame(store.newestIndex()).audioL[0] == Catch::Approx(2.f));
```

Find:
```cpp
    REQUIRE(frame.audio[0] == Catch::Approx(0.1f));
```
Replace:
```cpp
    REQUIRE(frame.audioL[0] == Catch::Approx(0.1f));
```

- [ ] **Step 4: Build and run tests (will fail — ConcatenativeMatcher still reads `.audio`)**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|warning:.*error|Built target|FAILED" | head -20
```

Expected: compile error mentioning `.audio` in ConcatenativeMatcher — that's fine, fix in Task 2.

---

## Task 2: Update `ConcatenativeMatcher` for stereo output

**Files:**
- Modify: `Source/ConcatenativeMatcher.h`
- Modify: `Source/ConcatenativeMatcher.cpp`
- Modify: `tests/ConcatenativeMatcherTest.cpp`

- [ ] **Step 1: Rewrite `ConcatenativeMatcher.h`**

Replace `outputBuffer_` with stereo buffers and update `match()` signature:

```cpp
#pragma once
#include "FeatureExtractor.h"
#include "CorpusStore.h"
#include <juce_core/juce_core.h>
#include <vector>

class ConcatenativeMatcher {
public:
    void prepare(int frameSize);
    void setWeights(float zcr, float rms, float sc, float st);
    void setRand(float rand);

    // Fills outL and outR with the matched frame's audio (frameSize samples each).
    // Returns false if corpus is empty; outL and outR are unchanged.
    bool match(const Features& controlFeatures, const CorpusStore& corpus,
               const float*& outL, const float*& outR);

    float distance(const Features& a, const Features& b) const;

private:
    int frameSize_ = 1024;
    float wZcr_ = 0.f, wRms_ = 1.f, wSc_ = 0.f, wSt_ = 0.f;
    float rand_ = 0.f;

    std::vector<float> outputBufL_;
    std::vector<float> outputBufR_;
    std::vector<int>   candidates_;

    juce::Random random_;
};
```

- [ ] **Step 2: Rewrite `ConcatenativeMatcher.cpp`**

Update `prepare()` and `match()`:

```cpp
void ConcatenativeMatcher::prepare(int frameSize)
{
    frameSize_ = frameSize;
    outputBufL_.assign(frameSize, 0.f);
    outputBufR_.assign(frameSize, 0.f);
    candidates_.reserve(512);
}

bool ConcatenativeMatcher::match(const Features& controlFeatures,
                                  const CorpusStore& corpus,
                                  const float*& outL, const float*& outR)
{
    if (corpus.size() == 0) return false;

    float minDist = std::numeric_limits<float>::max();
    int   bestIdx = 0;
    for (int i = 0; i < corpus.size(); ++i) {
        float d = distance(controlFeatures, corpus.getFrame(i).features);
        if (d < minDist) {
            minDist = d;
            bestIdx = i;
        }
    }

    int matchIdx = bestIdx;
    if (rand_ > 0.f && corpus.size() > 1) {
        float threshold = (1.f + rand_) * minDist + 1e-6f;
        candidates_.clear();
        for (int i = 0; i < corpus.size(); ++i)
            if (distance(controlFeatures, corpus.getFrame(i).features) <= threshold)
                candidates_.push_back(i);
        if (!candidates_.empty())
            matchIdx = candidates_[random_.nextInt(static_cast<int>(candidates_.size()))];
    }

    const auto& frame = corpus.getFrame(matchIdx);
    std::copy(frame.audioL.begin(), frame.audioL.end(), outputBufL_.begin());
    std::copy(frame.audioR.begin(), frame.audioR.end(), outputBufR_.begin());
    outL = outputBufL_.data();
    outR = outputBufR_.data();
    return true;
}
```

- [ ] **Step 3: Update all `tests/ConcatenativeMatcherTest.cpp` call sites**

Every `const float* out = matcher.match(ctrl, corpus);` and `REQUIRE(out != nullptr)` changes to:

```cpp
const float* outL = nullptr, *outR = nullptr;
bool ok = matcher.match(ctrl, corpus, outL, outR);
REQUIRE(ok);
// Then use outL[i] instead of out[i]
```

Specific replacements (9 tests need updating):

**Test "match returns nullptr when corpus is empty":**
```cpp
TEST_CASE("match returns nullptr when corpus is empty") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR) == false);
}
```

**Test "match returns audio from the single frame...":**
```cpp
TEST_CASE("match returns audio from the single frame when corpus has one frame") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);
    float audio[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    Features f{0.5f, 0.5f, 0.5f, 0.5f};
    corpus.push(audio, audio, f);

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    REQUIRE(outL[3] == Catch::Approx(0.4f));
}
```

**Test "Matcher picks frame with closest RMS...":**
```cpp
TEST_CASE("Matcher picks frame with closest RMS when only rms_weight is nonzero") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);
    float audioLow[4]  = {0.1f, 0.1f, 0.1f, 0.1f};
    float audioHigh[4] = {0.9f, 0.9f, 0.9f, 0.9f};
    Features fLow  {0.f, 0.1f, 0.f, 0.f};
    Features fHigh {0.f, 0.9f, 0.f, 0.f};
    corpus.push(audioLow, audioLow, fLow);
    corpus.push(audioHigh, audioHigh, fHigh);

    Features ctrl{0.f, 0.9f, 0.f, 0.f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += outL[i];
    avg /= 4.f;
    REQUIRE(avg > 0.05f);
}
```

**Test "rand path executes...":**
```cpp
TEST_CASE("rand path executes without crash and can return different frames") {
    ConcatenativeMatcher matcher;
    matcher.prepare(128);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);
    matcher.setRand(1.f);

    CorpusStore corpus;
    corpus.prepare(128, 20);

    for (int j = 0; j < 10; ++j) {
        std::vector<float> audio(128, float(j + 1));
        corpus.push(audio.data(), audio.data(), Features{0.f, 0.5f, 0.f, 0.f});
    }

    Features ctrl{0.f, 0.5f, 0.f, 0.f};
    bool seenDifferentOutputs = false;
    const float* outL = nullptr, *outR = nullptr;
    matcher.match(ctrl, corpus, outL, outR);
    float prevVal = outL[100];
    for (int i = 0; i < 50; ++i) {
        REQUIRE(matcher.match(ctrl, corpus, outL, outR));
        if (outL[100] != prevVal)
            seenDifferentOutputs = true;
    }
    REQUIRE(seenDifferentOutputs);
}
```

**Test "RMS matching uses stored features...":**
```cpp
TEST_CASE("RMS matching uses stored features independent of corpus audio scale") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 4);

    float audioA[4] = {0.4f, 0.4f, 0.4f, 0.4f};
    corpus.push(audioA, audioA, Features{0.f, 0.1f, 0.f, 0.f});

    float audioB[4] = {2.f, 2.f, 2.f, 2.f};
    corpus.push(audioB, audioB, Features{0.f, 0.5f, 0.f, 0.f});

    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(Features{0.f, 0.1f, 0.f, 0.f}, corpus, outL, outR));
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += outL[i];
    avg /= 4.f;
    REQUIRE(avg == Catch::Approx(0.4f));
}
```

**Test "match returns raw frame audio without crossfade modification":**
```cpp
TEST_CASE("match returns raw frame audio without crossfade modification") {
    ConcatenativeMatcher matcher;
    matcher.prepare(128);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(128, 4);
    std::vector<float> audio(128, 0.75f);
    corpus.push(audio.data(), audio.data(), Features{0.5f, 0.5f, 0.5f, 0.5f});

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    REQUIRE(outL[0] == Catch::Approx(0.75f));
    REQUIRE(outL[127] == Catch::Approx(0.75f));
}
```

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target|FAILED" && ./build/tests/StitcherTests 2>&1
```

Expected: compile error from `PluginProcessor.cpp` referencing the old mono match API — that's fine, fix in Task 3.

---

## Task 3: Update `PluginProcessor` for stereo corpus and grain playback

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Update `PluginProcessor.h` — split grain buffers, add stereo src accumulators**

Replace:
```cpp
    std::vector<float> currentGrain_;
    std::vector<float> nextGrain_;
```
With:
```cpp
    std::vector<float> currentGrainL_;
    std::vector<float> currentGrainR_;
    std::vector<float> nextGrainL_;
    std::vector<float> nextGrainR_;
```

After the existing `srcAccum_` declaration (near `ctrlAccum_`), add:
```cpp
    std::vector<float> srcAccumL_;  // stereo L for corpus audio (raw, gainSrc_ applied post-extraction)
    std::vector<float> srcAccumR_;  // stereo R for corpus audio
```

- [ ] **Step 2: Update `prepareToPlay` in `PluginProcessor.cpp`**

Replace:
```cpp
    currentGrain_.assign(frameSize_, 0.f);
    nextGrain_.assign(frameSize_, 0.f);
```
With:
```cpp
    currentGrainL_.assign(frameSize_, 0.f);
    currentGrainR_.assign(frameSize_, 0.f);
    nextGrainL_.assign(frameSize_, 0.f);
    nextGrainR_.assign(frameSize_, 0.f);
    srcAccumL_.assign(frameSize_, 0.f);
    srcAccumR_.assign(frameSize_, 0.f);
```

- [ ] **Step 3: Update the accumulation loop in `processBlock`**

After `srcAccum_[accumPos_] = srcMono_[i];`, add stereo accumulation:
```cpp
        srcAccumL_[accumPos_] = mainInput.getReadPointer(0)[i];
        srcAccumR_[accumPos_] = mainInput.getReadPointer(1)[i];
```

Replace the gainSrc_ application block:
```cpp
            const float gs = gainSrc_.load();
            if (gs != 1.f)
                for (int j = 0; j < frameSize_; ++j)
                    srcAccum_[j] *= gs;

            corpus_.push(srcAccum_.data(), srcFeatures);
```
With:
```cpp
            const float gs = gainSrc_.load();
            if (gs != 1.f) {
                for (int j = 0; j < frameSize_; ++j) {
                    srcAccumL_[j] *= gs;
                    srcAccumR_[j] *= gs;
                }
            }

            corpus_.push(srcAccumL_.data(), srcAccumR_.data(), srcFeatures);
```

Replace the grain copy blocks:
```cpp
                    // First grain: load directly and start playing from position 0
                    std::copy(matched, matched + frameSize_, currentGrain_.begin());
```
With:
```cpp
                    // First grain: load directly and start playing from position 0
                    const float* matchedL = nullptr, *matchedR = nullptr;
                    if (!matcher_.match(ctrlFeatures, corpus_, matchedL, matchedR)) break;
                    std::copy(matchedL, matchedL + frameSize_, currentGrainL_.begin());
                    std::copy(matchedR, matchedR + frameSize_, currentGrainR_.begin());
```

Wait — actually the match() call was already made before. Let me look at how the grain update logic works and restructure it properly.

The current block is:
```cpp
            const float* matched = matcher_.match(ctrlFeatures, corpus_);
            if (matched != nullptr) {
                if (!grainReady_) {
                    std::copy(matched, matched + frameSize_, currentGrain_.begin());
                    grainPos_   = 0;
                    grainReady_ = true;
                } else {
                    std::copy(matched, matched + frameSize_, nextGrain_.begin());
                    xfadePos_ = 0;
                    xfading_  = true;
                }
            }
```

This needs to become:
```cpp
            const float* matchedL = nullptr, *matchedR = nullptr;
            if (matcher_.match(ctrlFeatures, corpus_, matchedL, matchedR)) {
                if (!grainReady_) {
                    std::copy(matchedL, matchedL + frameSize_, currentGrainL_.begin());
                    std::copy(matchedR, matchedR + frameSize_, currentGrainR_.begin());
                    grainPos_   = 0;
                    grainReady_ = true;
                } else {
                    std::copy(matchedL, matchedL + frameSize_, nextGrainL_.begin());
                    std::copy(matchedR, matchedR + frameSize_, nextGrainR_.begin());
                    xfadePos_ = 0;
                    xfading_  = true;
                }
            }
```

- [ ] **Step 4: Rewrite the grain output section in `processBlock`**

Replace:
```cpp
    for (int i = 0; i < numSamples; ++i) {
        float grain = 0.f;
        if (grainReady_) {
            const int pos = grainPos_;
            if (xfading_) {
                const float t = static_cast<float>(xfadePos_) / static_cast<float>(kXfadeLen);
                grain = (1.f - t) * currentGrain_[pos] + t * nextGrain_[pos];
                if (++xfadePos_ >= kXfadeLen) {
                    std::copy(nextGrain_.begin(), nextGrain_.end(), currentGrain_.begin());
                    xfading_ = false;
                }
            } else {
                grain = currentGrain_[pos];
            }
            if (++grainPos_ >= frameSize_) grainPos_ = 0;
        }
        grainL[i] = grain;
        grainR[i] = grain;
    }
```

With:
```cpp
    for (int i = 0; i < numSamples; ++i) {
        float gL = 0.f, gR = 0.f;
        if (grainReady_) {
            const int pos = grainPos_;
            if (xfading_) {
                const float t = static_cast<float>(xfadePos_) / static_cast<float>(kXfadeLen);
                gL = (1.f - t) * currentGrainL_[pos] + t * nextGrainL_[pos];
                gR = (1.f - t) * currentGrainR_[pos] + t * nextGrainR_[pos];
                if (++xfadePos_ >= kXfadeLen) {
                    std::copy(nextGrainL_.begin(), nextGrainL_.end(), currentGrainL_.begin());
                    std::copy(nextGrainR_.begin(), nextGrainR_.end(), currentGrainR_.begin());
                    xfading_ = false;
                }
            } else {
                gL = currentGrainL_[pos];
                gR = currentGrainR_[pos];
            }
            if (++grainPos_ >= frameSize_) grainPos_ = 0;
        }
        grainL[i] = gL;
        grainR[i] = gR;
    }
```

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 29 test cases)`

- [ ] **Step 6: Build standalone + pluginval**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target" && cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -4
```

Expected: both succeed.

- [ ] **Step 7: Update `docs/dsp-review.md` — mark Issue #7 resolved**

In the Issue Summary table find:
```
| 7 | ⚙️ By design | PluginProcessor | Wet grain is mono — corpus is mono; increasing Mix collapses stereo image (expected) |
```
Replace:
```
| 7 | ✅ Fixed | PluginProcessor | ~~Wet grain is mono~~ — corpus now stores stereo (audioL/audioR); grain playback preserves stereo image |
```

Also update Section 7 (PluginProcessor) in dsp-review.md to reflect the stereo output.

- [ ] **Step 8: Commit all files**

```bash
git add Source/CorpusStore.h Source/CorpusStore.cpp Source/ConcatenativeMatcher.h Source/ConcatenativeMatcher.cpp Source/PluginProcessor.h Source/PluginProcessor.cpp tests/CorpusStoreTest.cpp tests/ConcatenativeMatcherTest.cpp docs/dsp-review.md docs/superpowers/plans/2026-05-22-stereo-grain.md
git commit -m "feat: stereo grain pipeline — corpus stores L/R audio, grain playback preserves stereo

CorpusFrame now holds audioL and audioR vectors. ConcatenativeMatcher::match()
fills outL/outR by reference. PluginProcessor accumulates stereo src audio,
pushes stereo corpus frames, and maintains separate L/R grain double-buffers.
Feature extraction remains mono (stereo-to-mono mix).
Wet signal now preserves the stereo field of the source input."
```

---

## Verification

1. `./build/tests/StitcherTests` — 29 test cases pass.
2. `cmake --build build --target Stitcher_pluginval_cli` — SUCCESS.
3. Standalone: route a stereo source signal → at mix=1, the output should carry distinct L and R content from the source, not collapsed mono.
