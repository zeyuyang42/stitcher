# Functional matchLen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the `matchLen` parameter (10–100 ms) actually drive the FFT analysis frame size at `prepareToPlay` time, replacing the hardcoded `kFrameSize = 1024` constant. Semantics match `seekTime`: load-time only (no runtime resize).

**Architecture:** A `nearestPow2` helper in `PluginProcessor.cpp` rounds `matchLen_ms * sampleRate / 1000` to the nearest power of two (clamped to [64, 8192]). The result is stored in `int frameSize_` on `StitcherProcessor` and replaces every `kFrameSize` use in `prepareToPlay` and `processBlock`. `FeatureExtractor`, `CorpusStore`, and `ConcatenativeMatcher` already accept a runtime `frameSize` — only `PluginProcessor` needs to change. The `static_assert(kXfadeLen < kFrameSize)` is replaced by a `jassert` in `prepareToPlay`.

**Tech Stack:** JUCE, Catch2, C++17

---

## Task 1: Add a test for FeatureExtractor at non-default frame size

**Files:**
- Modify: `tests/FeatureExtractorTest.cpp`

- [ ] **Step 1: Add test for prepare(512)**

Append to `tests/FeatureExtractorTest.cpp`:

```cpp
TEST_CASE("FeatureExtractor works correctly at frameSize 512") {
    FeatureExtractor fe;
    fe.prepare(512);
    std::vector<float> buf(512, 0.f);
    auto f = fe.extract(buf.data(), 512);
    REQUIRE(f.zcr == Catch::Approx(0.f));
    REQUIRE(f.rms == Catch::Approx(0.f));
    REQUIRE(f.sc  == Catch::Approx(0.f));
    REQUIRE(f.st  == Catch::Approx(0.f));
}

TEST_CASE("FeatureExtractor works correctly at frameSize 2048") {
    FeatureExtractor fe;
    fe.prepare(2048);
    std::vector<float> buf(2048, 1.f);
    auto f = fe.extract(buf.data(), 2048);
    REQUIRE(f.rms == Catch::Approx(1.f));
}
```

- [ ] **Step 2: Build and run tests — verify they pass**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 28 test cases)` (26 existing + 2 new)

- [ ] **Step 3: Commit**

```bash
git add tests/FeatureExtractorTest.cpp
git commit -m "test: verify FeatureExtractor works at 512 and 2048 frame sizes"
```

---

## Task 2: Replace `kFrameSize` with runtime `frameSize_` in `PluginProcessor`

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `Source/Parameters.h`

- [ ] **Step 1: Update `PluginProcessor.h`**

Replace:
```cpp
    static constexpr int kFrameSize = 1024;
```
With:
```cpp
    int frameSize_ = 1024;  // set in prepareToPlay from matchLen parameter
```

Also remove the `static_assert` block (lines 65-67 in the original):
```cpp
    static_assert(kXfadeLen < kFrameSize,
        "kXfadeLen must be less than kFrameSize: a new grain arrives every kFrameSize samples, "
        "so a crossfade must complete before the next grain update.");
```

- [ ] **Step 2: Add `nearestPow2` helper and compute `frameSize_` in `PluginProcessor.cpp`**

Add this free function in the anonymous namespace at the top of `PluginProcessor.cpp` (after the includes, before `StitcherProcessor::StitcherProcessor()`):

```cpp
namespace {
// Round x to the nearest power of 2, clamped to [64, 8192].
int nearestPow2(double x)
{
    const int raw = static_cast<int>(x + 0.5);
    if (raw <= 64)   return 64;
    if (raw >= 8192) return 8192;
    int p = 64;
    while (p * 2 <= raw) p <<= 1;
    // p = largest power of 2 <= raw; p*2 = smallest power of 2 > raw
    return ((raw - p) <= (p * 2 - raw)) ? p : p * 2;
}
} // namespace
```

Then, at the **start** of `prepareToPlay`, before any `prepare()` calls, add:

```cpp
    float matchLenMs = apvts_.getRawParameterValue(ParamIDs::matchLen)->load();
    frameSize_ = nearestPow2(static_cast<double>(matchLenMs) * sampleRate / 1000.0);
    jassert(frameSize_ > kXfadeLen);
```

- [ ] **Step 3: Replace all `kFrameSize` with `frameSize_` in `PluginProcessor.cpp`**

In `prepareToPlay` (8 occurrences):
- `featureExtractor_.prepare(kFrameSize)` → `featureExtractor_.prepare(frameSize_)`
- `... / kFrameSize) + 1` → `... / frameSize_) + 1`
- `corpus_.prepare(kFrameSize, maxFrames)` → `corpus_.prepare(frameSize_, maxFrames)`
- `matcher_.prepare(kFrameSize)` → `matcher_.prepare(frameSize_)`
- `ctrlAccum_.assign(kFrameSize, 0.f)` → `ctrlAccum_.assign(frameSize_, 0.f)`
- `srcAccum_.assign(kFrameSize, 0.f)` → `srcAccum_.assign(frameSize_, 0.f)`
- `currentGrain_.assign(kFrameSize, 0.f)` → `currentGrain_.assign(frameSize_, 0.f)`
- `nextGrain_.assign(kFrameSize, 0.f)` → `nextGrain_.assign(frameSize_, 0.f)`

In `processBlock` (7 occurrences):
- `if (accumPos_ == kFrameSize)` → `if (accumPos_ == frameSize_)`
- `featureExtractor_.extract(srcAccum_.data(), kFrameSize)` → `..., frameSize_)`
- `featureExtractor_.extract(ctrlAccum_.data(), kFrameSize)` → `..., frameSize_)`
- `std::copy(matched, matched + kFrameSize, currentGrain_.begin())` → `..., frameSize_, ...)`
- `std::copy(matched, matched + kFrameSize, nextGrain_.begin())` → `..., frameSize_, ...)`
- `if (++grainPos_ >= kFrameSize) grainPos_ = 0;` → `..., frameSize_, ...)`

In `parameterChanged` (1 occurrence — the comment):
- Update the stale comment about matchLen being non-functional:
```cpp
    else if (id == ParamIDs::seekTime || id == ParamIDs::matchLen)
    {
        // seekTime and matchLen take effect on next prepareToPlay (plugin reload).
        // Runtime resize of circular buffers is not supported.
    }
```

- [ ] **Step 4: Strip the "(fixed ~23ms)" suffix from the matchLen label in `Parameters.h`**

Find:
```cpp
        ParameterID{ParamIDs::matchLen, 1}, "Match Len (fixed ~23ms)",
```
Replace with:
```cpp
        ParameterID{ParamIDs::matchLen, 1}, "Match Len",
```

Also strip the "seekTime" tooltip if it has one — check `Source/Parameters.h:80`:
```cpp
        ParameterID{ParamIDs::seekTime, 1}, "Seek Time (set before load)",
```
Replace with:
```cpp
        ParameterID{ParamIDs::seekTime, 1}, "Seek Time",
```
(Both parameters have the same load-time-only semantics; the UI labels no longer need to say so.)

- [ ] **Step 5: Build and run tests**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 28 test cases)`

- [ ] **Step 6: Build standalone and run pluginval**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target" && cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -4
```

Expected: both succeed.

- [ ] **Step 7: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp Source/Parameters.h
git commit -m "feat: make matchLen drive frame size at prepareToPlay time

Replace hardcoded kFrameSize=1024 with runtime frameSize_ computed from
matchLen parameter (rounded to nearest power of two, clamped [64,8192]).
Semantics match seekTime: takes effect on plugin reload, not at runtime.
Removes the static_assert; replaces it with jassert in prepareToPlay."
```

---

## Verification

1. `./build/tests/StitcherTests` — 28 test cases all pass.
2. `cmake --build build --target Stitcher_pluginval_cli` — SUCCESS.
3. Standalone: set matchLen to 50 ms → reload → corpus grains are audibly longer (~46 ms, 2048 frames).
4. Standalone: set matchLen to 10 ms → reload → corpus grains are shorter (~11.6 ms, 512 frames at 44.1 kHz).
