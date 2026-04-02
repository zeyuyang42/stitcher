# Grain Crossfade Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the click-at-grain-boundary bug by replacing the position-misaligned crossfade in `ConcatenativeMatcher` with a position-aligned double-buffer crossfade in `PluginProcessor`.

**Architecture:** The core problem is that `ConcatenativeMatcher::match()` crossfades from position 0 of the previous grain, but `PluginProcessor` was playing from an arbitrary mid-grain position. The fix moves all crossfade logic to `PluginProcessor`, which owns both the playback position (`grainPos_`) and the audio buffers. `ConcatenativeMatcher::match()` becomes a pure function: find best frame, return its raw audio. `PluginProcessor` manages two grain buffers (`currentGrain_` / `nextGrain_`) and blends them at the current playback position using a 256-sample linear crossfade — ~5.8ms at 44.1kHz.

**Tech Stack:** JUCE DSP, Catch2 unit tests, C++17

---

## File Map

| File | Change |
|---|---|
| `Source/ConcatenativeMatcher.h` | Remove `prevBuffer_`, `kCrossfadeLen`; update `match()` comment |
| `Source/ConcatenativeMatcher.cpp` | Remove crossfade logic from `match()`; remove `prevBuffer_` init in `prepare()` |
| `Source/PluginProcessor.h` | Replace `grainBuf_` with `currentGrain_`/`nextGrain_`; add `xfadePos_`, `xfading_`, `kXfadeLen` |
| `Source/PluginProcessor.cpp` | Update `prepareToPlay` init; rewrite grain-update and output loops in `processBlock` |
| `tests/ConcatenativeMatcherTest.cpp` | Update stale comment in one test; add test that `match()` returns raw frame audio |

---

## Task 1: Simplify `ConcatenativeMatcher` — remove crossfade, return raw audio

**Files:**
- Modify: `Source/ConcatenativeMatcher.h`
- Modify: `Source/ConcatenativeMatcher.cpp`
- Test: `tests/ConcatenativeMatcherTest.cpp`

The crossfade was in the wrong place (it had no knowledge of playback position). `match()` should now just find the best frame and return its audio verbatim.

- [ ] **Step 1: Write a failing test — `match()` returns raw frame audio without modification**

Add to `tests/ConcatenativeMatcherTest.cpp` after the existing tests:

```cpp
TEST_CASE("match returns raw frame audio without crossfade modification") {
    // With the old code, outputBuffer_[0] was prevBuffer_[0]*1.0 (t=0), not frame.audio[0].
    // With the new code, outputBuffer_[0] == frame.audio[0] exactly.
    ConcatenativeMatcher matcher;
    matcher.prepare(128);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(128, 4);
    std::vector<float> audio(128, 0.75f);
    corpus.push(audio.data(), Features{0.5f, 0.5f, 0.5f, 0.5f});

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};

    // Call match twice — old code crossfades from prev (silence) on first call,
    // so first call returns 0.0 at index 0 (t=0). New code returns 0.75f immediately.
    const float* out = matcher.match(ctrl, corpus);
    REQUIRE(out != nullptr);
    REQUIRE(out[0] == Catch::Approx(0.75f));   // raw frame value, no crossfade attenuation
    REQUIRE(out[127] == Catch::Approx(0.75f)); // same across the whole buffer
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /Users/zeyuyang42/Workspace/stitcher-010426/stitcher
cmake --build build --target StitcherTests 2>&1 | tail -5
./build/tests/StitcherTests "[match returns raw frame audio]"
```

Expected: FAIL — `out[0]` is currently `0.f` (prevBuffer_ starts as silence, t=0 at i=0).

- [ ] **Step 3: Remove `prevBuffer_` and `kCrossfadeLen` from the header**

Replace the private section of `Source/ConcatenativeMatcher.h`:

```cpp
private:
    int frameSize_ = 1024;
    float wZcr_ = 0.f, wRms_ = 1.f, wSc_ = 0.f, wSt_ = 0.f;
    float rand_ = 0.f;

    std::vector<float> outputBuffer_;
    std::vector<int>   candidates_;   // pre-allocated, reused each block to avoid audio-thread heap alloc

    juce::Random random_;
```

Also update the `match()` comment:

```cpp
// Returns pointer to output audio buffer (frameSize samples) or nullptr if corpus empty.
// Caller is responsible for crossfading — this returns the raw matched frame audio.
const float* match(const Features& controlFeatures, const CorpusStore& corpus);
```

- [ ] **Step 4: Remove crossfade logic from `match()` in `ConcatenativeMatcher.cpp`**

Replace the section after `const auto& frame = corpus.getFrame(matchIdx);` through the end of `match()`:

```cpp
    const auto& frame = corpus.getFrame(matchIdx);
    std::copy(frame.audio.begin(), frame.audio.end(), outputBuffer_.begin());
    return outputBuffer_.data();
}
```

Also update `prepare()` — remove `prevBuffer_` init:

```cpp
void ConcatenativeMatcher::prepare(int frameSize)
{
    frameSize_ = frameSize;
    outputBuffer_.assign(frameSize, 0.f);
    candidates_.reserve(512);
}
```

- [ ] **Step 5: Run tests to verify the new test passes and no regressions**

```bash
./build/tests/StitcherTests 2>&1
```

Expected output:
```
All tests passed (N assertions in 26 test cases)
```

If the rand test or "single frame" test fails, check: the "single frame" test asserts `out[3] != 0.f`. With raw audio `{0.1f, 0.2f, 0.3f, 0.4f}`, `out[3] = 0.4f` — still passes.

- [ ] **Step 6: Commit**

```bash
git add Source/ConcatenativeMatcher.h Source/ConcatenativeMatcher.cpp tests/ConcatenativeMatcherTest.cpp
git commit -m "refactor: remove crossfade from ConcatenativeMatcher — match() returns raw audio

Crossfade belongs in PluginProcessor where playback position is known.
match() now simply finds the best frame and returns its audio verbatim.
Removes prevBuffer_, kCrossfadeLen."
```

---

## Task 2: Implement position-aligned double-buffer crossfade in `PluginProcessor`

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

The crossfade now lives here. Two buffers: `currentGrain_` (playing) and `nextGrain_` (incoming). When a new match arrives, `nextGrain_` is loaded and a 256-sample crossfade starts at the current `grainPos_`. Both buffers are read at the same index — no position jump.

```
Before fix:  ...old[510], old[511] | new[0]=blend(old[0], new[0]) — CLICK (old[511]→old[0])
After fix:   ...old[510], old[511] | blend(old[512], new[512])    — SMOOTH (value continuity)
```

- [ ] **Step 1: Update `PluginProcessor.h` — replace single grain buffer with double-buffer**

Replace the "Output grain buffer and playback position" block:

```cpp
    // Grain double-buffer for position-aligned crossfade
    static constexpr int kXfadeLen = 256;   // ~5.8ms at 44.1kHz
    std::vector<float> currentGrain_;
    std::vector<float> nextGrain_;
    int  grainPos_   = 0;
    int  xfadePos_   = 0;
    bool xfading_    = false;
    bool grainReady_ = false;
```

- [ ] **Step 2: Update `prepareToPlay` in `PluginProcessor.cpp`**

Replace:

```cpp
    grainBuf_.assign(kFrameSize, 0.f);
    ctrlMono_.assign(samplesPerBlock, 0.f);
    srcMono_.assign(samplesPerBlock, 0.f);
    accumPos_   = 0;
    grainPos_   = 0;
    grainReady_ = false;
```

With:

```cpp
    currentGrain_.assign(kFrameSize, 0.f);
    nextGrain_.assign(kFrameSize, 0.f);
    ctrlMono_.assign(samplesPerBlock, 0.f);
    srcMono_.assign(samplesPerBlock, 0.f);
    accumPos_   = 0;
    grainPos_   = 0;
    xfadePos_   = 0;
    xfading_    = false;
    grainReady_ = false;
```

- [ ] **Step 3: Update the grain-update logic in the accumulation loop**

Replace:

```cpp
            const float* matched = matcher_.match(ctrlFeatures, corpus_);
            if (matched != nullptr)
                std::copy(matched, matched + kFrameSize, grainBuf_.begin());

            grainPos_   = 0;
            grainReady_ = true;
            accumPos_   = 0;
```

With:

```cpp
            const float* matched = matcher_.match(ctrlFeatures, corpus_);
            if (matched != nullptr) {
                if (!grainReady_) {
                    // First grain: load directly and start playing from position 0
                    std::copy(matched, matched + kFrameSize, currentGrain_.begin());
                    grainPos_   = 0;
                    grainReady_ = true;
                } else {
                    // Subsequent grains: start position-aligned crossfade
                    // grainPos_ is NOT reset — we continue from the current playback position
                    std::copy(matched, matched + kFrameSize, nextGrain_.begin());
                    xfadePos_ = 0;
                    xfading_  = true;
                }
            }
            accumPos_ = 0;
```

- [ ] **Step 4: Rewrite the output loop with double-buffer crossfade**

Replace:

```cpp
    for (int i = 0; i < numSamples; ++i) {
        float grain = grainReady_ ? grainBuf_[grainPos_] : 0.f;
        if (++grainPos_ >= kFrameSize) grainPos_ = 0;
        outL[i] = dry * inL[i] + wet * grain;
        outR[i] = dry * inR[i] + wet * grain;
    }
```

With:

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
            if (++grainPos_ >= kFrameSize) grainPos_ = 0;
        }
        outL[i] = dry * inL[i] + wet * grain;
        outR[i] = dry * inR[i] + wet * grain;
    }
```

- [ ] **Step 5: Build and verify no compile errors**

```bash
cmake --build build --target Stitcher_VST3 2>&1 | grep -E "error:|Built target" | tail -5
```

Expected:
```
[100%] Built target Stitcher_VST3
```

If you see `error: use of undeclared identifier 'grainBuf_'`, you missed replacing a reference. Search the file: `grep -n "grainBuf_" Source/PluginProcessor.cpp`.

- [ ] **Step 6: Run tests**

```bash
./build/tests/StitcherTests 2>&1
```

Expected: All tests pass. The existing tests don't directly exercise processBlock, so this is primarily a compile + pluginval check.

- [ ] **Step 7: Run pluginval**

```bash
cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -5
```

Expected:
```
SUCCESS
[100%] Built target Stitcher_pluginval_cli
```

- [ ] **Step 8: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "fix: position-aligned double-buffer crossfade eliminates grain-boundary click

Replace single grainBuf_ with currentGrain_/nextGrain_ pair. When a new
match arrives, nextGrain_ is loaded and a 256-sample (~5.8ms) crossfade
starts from the CURRENT grainPos_ — both buffers read at the same index.
Eliminates the jump from arbitrary playback position to position 0 of
the old grain's crossfade that caused a click on every grain boundary."
```

---

## Task 3: Update the stale test comment and verify rand test still works

**Files:**
- Modify: `tests/ConcatenativeMatcherTest.cpp`

One test comment refers to the old crossfade behaviour. Update it so it reflects reality.

- [ ] **Step 1: Update the stale comment in "match returns audio from the single frame"**

Find this line in `tests/ConcatenativeMatcherTest.cpp`:

```cpp
    REQUIRE(out[3] != 0.f); // non-silence (crossfade from silence to 0.4)
```

Change to:

```cpp
    REQUIRE(out[3] == Catch::Approx(0.4f)); // raw frame value — no crossfade in matcher
```

- [ ] **Step 2: Run full test suite one final time**

```bash
./build/tests/StitcherTests 2>&1
```

Expected:
```
All tests passed (N assertions in 26 test cases)
```

- [ ] **Step 3: Commit**

```bash
git add tests/ConcatenativeMatcherTest.cpp
git commit -m "test: strengthen single-frame test to assert exact raw audio value"
```

---

## What the Fix Does and Does Not Change

**Fixes:**
- ✅ Click at every grain boundary (positional misalignment)
- ✅ Crossfade length: 64 samples (1.45ms) → 256 samples (5.8ms)

**Unchanged by design:**
- No overlap-add. Grains are still substituted, not overlapped. OLA would require storing longer corpus frames (1024 + 256 tail samples) and a separate playback cursor for the old grain. That is a larger refactor.
- Grain size still fixed at 1024 samples (~23ms). `matchLen` parameter still reserved.
- Wet path still mono.
- Phase discontinuity between unrelated corpus frames still exists — the longer crossfade reduces its audibility but cannot eliminate it without pitch-synchronous analysis.
