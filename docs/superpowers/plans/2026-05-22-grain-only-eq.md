# Grain-Only EQ Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Issue #9 — EQ currently processes `dry*main + wet*grain`. Move EQ to apply only to the grain signal before the dry/wet blend, so the dry (main input) path is unaffected by EQ. Reverb stays after the blend (processes the full mixed output).

**Architecture:** The output and main-input share the same buffer (JUCE in-place processing), so writing grain to `output` first would destroy the dry signal needed for mixing. Solution: pre-allocate a 2-channel `grainMixBuf_` (size = `samplesPerBlock`). In the output loop, write the grain signal (mono, so L=R) to `grainMixBuf_`. Apply EQ to `grainMixBuf_`. Then write the blended output: `outL[i] = dry*inL[i] + wet*grainMixBuf_.getReadPointer(0)[i]`. Reverb and limiter process the final output unchanged.

New signal chain:
```
grain → grainMixBuf_ → EQ → blend with dry main → Reverb → gainOut → Limiter
```

**Tech Stack:** JUCE PluginProcessor, juce::AudioBuffer, juce::dsp::AudioBlock, C++17

---

## Task 1: Add `grainMixBuf_` to processor and wire the grain-only EQ path

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

There is no unit test framework for PluginProcessor; verification is by build + pluginval.

- [ ] **Step 1: Add `grainMixBuf_` member to `PluginProcessor.h`**

In the "Grain double-buffer" section, after the existing grain members, add:

```cpp
    juce::AudioBuffer<float> grainMixBuf_;  // 2-ch, samplesPerBlock — grain staging for grain-only EQ
```

- [ ] **Step 2: Allocate `grainMixBuf_` in `prepareToPlay`**

After the line `ctrlMono_.assign(samplesPerBlock, 0.f);`, add:

```cpp
    grainMixBuf_.setSize(2, samplesPerBlock, false, true, false);
```

- [ ] **Step 3: Rewrite the output loop and EQ/Reverb chain in `processBlock`**

Replace:
```cpp
    // Write grain to stereo output with dry/wet mix
    const float wet = mix_.load();
    const float dry = 1.f - wet;
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);
    const float* inL = mainInput.getReadPointer(0);
    const float* inR = mainInput.getReadPointer(1);

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
        outL[i] = dry * inL[i] + wet * grain;
        outR[i] = dry * inR[i] + wet * grain;
    }

    // EQ → Reverb → output gain → limiter (scoped to output bus only — VST3 buffer includes sidechain channels)
    juce::dsp::AudioBlock<float> outputBlock(output);
    eq_.process(outputBlock);
    reverb_.process(outputBlock);
```

With:
```cpp
    // Render grain into a separate buffer so EQ applies to grain only (not the dry path).
    // outL/inL share memory (JUCE in-place); we cannot write to output and then re-read dry.
    float* grainL = grainMixBuf_.getWritePointer(0);
    float* grainR = grainMixBuf_.getWritePointer(1);

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

    // Apply EQ to grain signal only (grain is mono so L=R; EQ processes both channels independently)
    juce::dsp::AudioBlock<float> grainBlock(grainMixBuf_);
    eq_.process(grainBlock);

    // Blend EQ'd grain with dry main input into output
    const float wet = mix_.load();
    const float dry = 1.f - wet;
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);
    const float* inL = mainInput.getReadPointer(0);
    const float* inR = mainInput.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i) {
        outL[i] = dry * inL[i] + wet * grainL[i];
        outR[i] = dry * inR[i] + wet * grainR[i];
    }

    // Reverb → output gain → limiter (processes the final blended output)
    juce::dsp::AudioBlock<float> outputBlock(output);
    reverb_.process(outputBlock);
```

- [ ] **Step 4: Build and run tests**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target" && ./build/tests/StitcherTests 2>&1
```

Expected: `All tests passed (N assertions in 29 test cases)`

- [ ] **Step 5: Build standalone + pluginval**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target" && cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -4
```

Expected: both succeed.

- [ ] **Step 6: Update `docs/dsp-review.md` — mark Issue #9 resolved**

In the Issue Summary table, find:
```
| 9 | ⚙️ By design | EQProcessor | EQ processes dry+wet blend — architectural; EQ cannot target grain alone without signal path restructure |
```
Replace with:
```
| 9 | ✅ Fixed | PluginProcessor | ~~EQ processes dry+wet blend~~ — EQ now applied to grain signal only via grainMixBuf_ staging; dry path unaffected |
```

Also update Section 5 of dsp-review.md (EQProcessor Issues) to reflect this fix.

- [ ] **Step 7: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp docs/dsp-review.md docs/superpowers/plans/2026-05-22-grain-only-eq.md
git commit -m "fix: apply EQ to grain signal only via grainMixBuf_ staging

EQ no longer processes the dry main-input path. A 2-channel grainMixBuf_
(pre-allocated to samplesPerBlock) holds the grain signal before EQ.
After EQ, grain is blended with dry main input into output.
Reverb still processes the final blended output."
```

---

## Verification

1. `./build/tests/StitcherTests` — 29 test cases pass.
2. `cmake --build build --target Stitcher_pluginval_cli` — SUCCESS.
3. Standalone: set mix=0 (all dry) → EQ knobs should have no effect on the output.
4. Standalone: set mix=1 (all wet) → EQ shapes the grain normally.
5. Standalone: set mix=0.5 → EQ shapes only the grain portion; dry path is unaffected.
