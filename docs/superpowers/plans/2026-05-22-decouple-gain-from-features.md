# Decouple Gain from Feature Extraction Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Issue #8 — `gainSrc_` currently scales the source signal before feature extraction, so boosting it doubles the corpus RMS values and breaks RMS matching unless `gainCtrl_` is also boosted equally. After the fix, `gainSrc_` applies only to corpus audio (playback level), not to features. Feature extraction reads raw signals, making the two gain knobs independent.

**Architecture:** In `processBlock`, remove `* gainSrc_.load()` from the srcAccum accumulation, compute features from raw audio, then apply `gainSrc_` to `srcAccum_` after feature extraction and before `corpus_.push()`. `gainCtrl_` is retained in the ctrl path (still scales ctrl features, allowing ctrl-level normalization for matching). No changes to any other class.

**Tech Stack:** JUCE PluginProcessor, Catch2, C++17

---

## Task 1: Add regression test for gain-independent feature matching

**Files:**
- Modify: `tests/ConcatenativeMatcherTest.cpp`

This test verifies the DESIRED post-fix behavior by directly constructing corpus frames where audio and features are intentionally decoupled.

- [ ] **Step 1: Add test**

Append to `tests/ConcatenativeMatcherTest.cpp`:

```cpp
TEST_CASE("RMS matching uses stored features independent of corpus audio scale") {
    // Verifies that matching is based on the Features struct, not the raw audio amplitude.
    // This mirrors the post-fix PluginProcessor behavior where gainSrc_ applies to
    // corpus audio after feature extraction.
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);  // only RMS weight
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 4);

    // Frame A: raw RMS ≈ 0.1, stored audio scaled 4x (as if gainSrc applied post-extraction)
    float audioA[4] = {0.4f, 0.4f, 0.4f, 0.4f};
    corpus.push(audioA, Features{0.f, 0.1f, 0.f, 0.f});

    // Frame B: raw RMS = 0.5, stored audio scaled 4x
    float audioB[4] = {2.f, 2.f, 2.f, 2.f};
    corpus.push(audioB, Features{0.f, 0.5f, 0.f, 0.f});

    // Ctrl has raw RMS 0.1 — should match frame A by raw RMS feature
    const float* out = matcher.match(Features{0.f, 0.1f, 0.f, 0.f}, corpus);
    REQUIRE(out != nullptr);
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += out[i];
    avg /= 4.f;
    REQUIRE(avg == Catch::Approx(0.4f));  // frame A audio value, not frame B's 2.0
}
```

- [ ] **Step 2: Run tests to verify the new test passes (it should — matcher already behaves this way)**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 29 test cases)`

- [ ] **Step 3: Commit**

```bash
git add tests/ConcatenativeMatcherTest.cpp
git commit -m "test: verify RMS matching uses stored features independent of corpus audio scale"
```

---

## Task 2: Move `gainSrc_` to post-feature-extraction in `processBlock`

**Files:**
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Remove `gainSrc_` from the accumulation loop and apply it after feature extraction**

Find in `processBlock`:
```cpp
    // Accumulate samples into frames; when a frame is complete, analyse + match
    for (int i = 0; i < numSamples; ++i) {
        ctrlAccum_[accumPos_] = ctrlMono_[i] * gainCtrl_.load();
        srcAccum_[accumPos_]  = srcMono_[i]  * gainSrc_.load();
        ++accumPos_;

        if (accumPos_ == frameSize_) {
            Features srcFeatures  = featureExtractor_.extract(srcAccum_.data(),  frameSize_);
            Features ctrlFeatures = featureExtractor_.extract(ctrlAccum_.data(), frameSize_);

            corpus_.push(srcAccum_.data(), srcFeatures);
```

Replace with:
```cpp
    // Accumulate samples into frames; when a frame is complete, analyse + match
    for (int i = 0; i < numSamples; ++i) {
        ctrlAccum_[accumPos_] = ctrlMono_[i] * gainCtrl_.load();
        srcAccum_[accumPos_]  = srcMono_[i];   // raw — gainSrc_ applied post-extraction
        ++accumPos_;

        if (accumPos_ == frameSize_) {
            Features srcFeatures  = featureExtractor_.extract(srcAccum_.data(),  frameSize_);
            Features ctrlFeatures = featureExtractor_.extract(ctrlAccum_.data(), frameSize_);

            // Apply gainSrc_ to corpus audio AFTER feature extraction so features
            // reflect the raw source amplitude — independent of the playback gain knob.
            const float gs = gainSrc_.load();
            if (gs != 1.f)
                for (int j = 0; j < frameSize_; ++j)
                    srcAccum_[j] *= gs;

            corpus_.push(srcAccum_.data(), srcFeatures);
```

- [ ] **Step 2: Build and run tests**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 29 test cases)`

- [ ] **Step 3: Build standalone + pluginval**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target" && cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -4
```

Expected: both succeed.

- [ ] **Step 4: Update `docs/dsp-review.md` — mark Issue #8 resolved**

Find:
```
| 8 | ⚙️ By design | PluginProcessor | `gainCtrl_`/`gainSrc_` applied before feature extraction — consistent with original SC implementation |
```
Replace with:
```
| 8 | ✅ Fixed | PluginProcessor | ~~`gainSrc_` applied before feature extraction~~ — now applied after extract(), features reflect raw audio (commit) |
```

Also update the **Fix Status** section and the Issue #8 description in Section 7 of `dsp-review.md`:

In Section 7 find:
```
🟡 **`gainCtrl_` and `gainSrc_` are applied before feature extraction.**
```
And replace:
```
✅ ~~🟡~~ **`gainSrc_` decoupled from feature extraction.** `gainSrc_` now applies to corpus audio after feature extraction, so features reflect raw source amplitude. Boosting `gainSrc_` affects playback level only, not RMS matching. `gainCtrl_` still applies before ctrl feature extraction (allows ctrl-level normalization for matching).
```

- [ ] **Step 5: Commit**

```bash
git add Source/PluginProcessor.cpp docs/dsp-review.md
git commit -m "fix: decouple gainSrc_ from feature extraction — apply post-extract to corpus audio

gainSrc_ now only affects corpus playback level, not RMS features.
Features are extracted from raw source audio; gainSrc_ is applied to
srcAccum_ after extract() and before corpus_.push().
gainCtrl_ is retained in the ctrl path for matching normalization."
```

---

## Verification

1. `./build/tests/StitcherTests` — 29 test cases pass.
2. `cmake --build build --target Stitcher_pluginval_cli` — SUCCESS.
3. Standalone: with RMS weight = 1.0, boosting gainSrc_ from 0 dB to +12 dB should change playback loudness but NOT change which corpus frame is selected (since corpus RMS features are now from raw audio).
