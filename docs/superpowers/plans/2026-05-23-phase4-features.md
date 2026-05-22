# Phase 4 — Features Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship three features — tempo-synced matchLen (4a), A/B state slots (4c), MIDI learn (4b) — in sub-order 4a → 4c → 4b.

**Architecture:** 4a extends existing params and reads BPM from play head at prepare time. 4c adds two `juce::ValueTree` snapshot slots in `StitcherEditor`. 4b adds `MidiLearn` (lock-free atomic array) owned by `StitcherProcessor`, with a `MouseListener` in the editor for right-click context menus.

**Tech Stack:** JUCE 8, C++17, Catch2 3.7.1.

---

## Key Facts

### Current State

- 18 APVTS parameters in `Source/Parameters.h`, state ID `"PARAMETERS"`
- `frameSize_` computed once in `prepareToPlay` from `match_len` ms
- `processBlock` has access to `midiMessages` (currently unused: `juce::ignoreUnused(midiMessages)`)
- `getPlayHead()` available in `processBlock`; JUCE 8 API: `getPlayHead()->getPosition()` returns `juce::Optional<juce::AudioPlayHead::PositionInfo>`; `.getBpm()` returns `juce::Optional<double>`
- `parameterChanged` for `matchLen` does nothing at runtime ("takes effect on next prepareToPlay")
- `PresetBar` owns 5 buttons in one row; controlled by `StitcherEditor`'s `presetManager_`
- `StitcherEditor` owns all sliders with APVTS attachments via `SA = SliderAttachment`

### Files Overview

| File | 4a | 4c | 4b |
|---|---|---|---|
| `Source/Parameters.h` | Add 2 params | — | — |
| `Source/PluginProcessor.h` | Add BPM cache | — | Add `MidiLearn` member |
| `Source/PluginProcessor.cpp` | Update `prepareToPlay`; cache BPM in `processBlock`; listener | — | MIDI CC dispatch |
| `Source/MidiLearn.h` | — | — | New file |
| `Source/MidiLearn.cpp` | — | — | New file |
| `Source/PluginEditor.h` | Add sync toggle + div ComboBox | Add `abSlot_[2]` | Register mouse listeners |
| `Source/PluginEditor.cpp` | Wire sync UI | Add A/B buttons to PresetBar area | Show MIDI learn menu |
| `Source/UI/PresetBar.h/.cpp` | — | Add A/B buttons | — |
| `CMakeLists.txt` | — | — | Add MidiLearn.cpp |
| `tests/FeatureExtractorTest.cpp` | — | — | — |
| (new) `tests/MidiLearnTest.cpp` | — | — | New |
| `tests/CMakeLists.txt` | — | — | Add MidiLearnTest.cpp |

---

## Feature 4a — Tempo-Synced matchLen

### Task 4a-1: Add new APVTS parameters

**Files:**
- Modify: `Source/Parameters.h`

#### Step 4a-1.1 — Add two parameters to `createParameterLayout()`

Add these two declarations **after** the existing `matchLen` parameter block (after line ~77 in Parameters.h):

```cpp
layout.add(std::make_unique<AudioParameterBool>(
    ParameterID{ParamIDs::matchLenSync, 1}, "Match Len Sync", false));

layout.add(std::make_unique<AudioParameterChoice>(
    ParameterID{ParamIDs::matchLenDiv, 1}, "Match Len Div",
    juce::StringArray{"1/16","1/8","1/4","1/4.","1/2","1/1","2/1"}, 2));
```

And add the ParamIDs constants in the `namespace ParamIDs` block:

```cpp
inline constexpr auto matchLenSync { "match_len_sync" };
inline constexpr auto matchLenDiv  { "match_len_div" };
```

- [ ] Add `matchLenSync` and `matchLenDiv` to `namespace ParamIDs` in `Source/Parameters.h`.
- [ ] Add the two `layout.add(...)` calls to `createParameterLayout()` after the `matchLen` parameter.
- [ ] Build tests to confirm nothing breaks: `cmake --build build --target StitcherTests && ./build/tests/StitcherTests` — expect 40 pass.
- [ ] Commit: `git add Source/Parameters.h && git commit -m "feat(4a): add match_len_sync and match_len_div APVTS parameters"`

### Task 4a-2: Update PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

#### Step 4a-2.1 — Add BPM cache and subdivision table to PluginProcessor.h

In the `private:` section of `StitcherProcessor`, after `int frameSize_`:

```cpp
std::atomic<double> lastKnownBpm_ { 120.0 };
```

- [ ] Add `std::atomic<double> lastKnownBpm_ { 120.0 };` to `PluginProcessor.h`.

#### Step 4a-2.2 — Cache BPM in processBlock

In `PluginProcessor.cpp::processBlock`, after `juce::ScopedNoDenormals noDenormals;` and before the `matcherDirty_` check, add:

```cpp
// Cache BPM for tempo-sync frame size computation
if (auto* ph = getPlayHead())
    if (auto pos = ph->getPosition())
        if (pos->getBpm().hasValue())
            lastKnownBpm_.store(*pos->getBpm(), std::memory_order_relaxed);
```

- [ ] Add BPM caching block to top of `processBlock`.

#### Step 4a-2.3 — Update prepareToPlay to use sync when enabled

In `prepareToPlay`, find:

```cpp
float matchLenMs = apvts_.getRawParameterValue(ParamIDs::matchLen)->load();
frameSize_ = nearestPow2(static_cast<double>(matchLenMs) * sampleRate / 1000.0);
```

Replace with:

```cpp
const bool sync = apvts_.getRawParameterValue(ParamIDs::matchLenSync)->load() > 0.5f;
if (sync) {
    static const double kDivisions[] = { 0.0625, 0.125, 0.25, 0.375, 0.5, 1.0, 2.0 };
    const int divIdx = juce::jlimit(0, 6,
        static_cast<int>(apvts_.getRawParameterValue(ParamIDs::matchLenDiv)->load()));
    const double bpm = lastKnownBpm_.load(std::memory_order_relaxed);
    frameSize_ = nearestPow2(kDivisions[divIdx] * 60.0 / bpm * sampleRate);
} else {
    float matchLenMs = apvts_.getRawParameterValue(ParamIDs::matchLen)->load();
    frameSize_ = nearestPow2(static_cast<double>(matchLenMs) * sampleRate / 1000.0);
}
```

- [ ] Replace the `frameSize_` computation in `prepareToPlay`.

#### Step 4a-2.4 — Register listeners for new params

In `StitcherProcessor()` constructor, extend the listener registration loop:

```cpp
for (auto* id : { zcrWeight, rmsWeight, scWeight, stWeight,
                  matchLen, seekTime, rand_,
                  matchLenSync, matchLenDiv,      // ← add these two
                  gainCtrl, gainSrc,
                  ...
```

- [ ] Add `matchLenSync, matchLenDiv` to the listener loop in the constructor.

#### Step 4a-2.5 — Build

```
cmake --build build --target Stitcher_Standalone 2>&1 | tail -5
```

Expected: clean build.

- [ ] Build clean.

#### Step 4a-2.6 — Commit

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(4a): use BPM+subdivision to compute frameSize when sync is enabled"
```

- [ ] Commit.

### Task 4a-3: UI — Sync toggle and division ComboBox

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

#### Step 4a-3.1 — Add members to PluginEditor.h

In the private section, after the existing matchLen declarations:

```cpp
juce::ToggleButton  matchLenSyncButton_;
juce::ComboBox      matchLenDivBox_;
std::unique_ptr<BA> matchLenSyncAttach_;
std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> matchLenDivAttach_;
```

- [ ] Add these four members to `PluginEditor.h`.

#### Step 4a-3.2 — Wire up in PluginEditor.cpp constructor

After the `initRotary(matchLenSlider_, matchLenLabel_, "Len", *this);` line, add:

```cpp
matchLenSyncButton_.setButtonText("Sync");
addAndMakeVisible(matchLenSyncButton_);

matchLenDivBox_.addItemList({"1/16","1/8","1/4","1/4.","1/2","1/1","2/1"}, 1);
addAndMakeVisible(matchLenDivBox_);

// Show/hide based on sync state
auto updateSyncUI = [this] {
    const bool s = matchLenSyncButton_.getToggleState();
    matchLenSlider_.setVisible(!s);
    matchLenLabel_.setVisible(!s);
    matchLenDivBox_.setVisible(s);
};
matchLenSyncButton_.onClick = [updateSyncUI] { updateSyncUI(); };
updateSyncUI();
```

After the attachments block, add:

```cpp
using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
matchLenSyncAttach_  = std::make_unique<BA>(apvts, ParamIDs::matchLenSync, matchLenSyncButton_);
matchLenDivAttach_   = std::make_unique<CA>(apvts, ParamIDs::matchLenDiv,  matchLenDivBox_);
```

- [ ] Add the construction code above to the editor constructor.

#### Step 4a-3.3 — Layout in resized()

In the Concatenator Row 2 section of `resized()`, the current layout is:

```cpp
// Row 2: seek, matchLen, rand
const int kw2 = std::min(90, (r.getWidth() - 6) / 3);
const int kh2 = kw2 + labelH;
auto row2 = r.removeFromTop(kh2);
for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
        {&seekTimeSlider_, &seekTimeLabel_},
        {&matchLenSlider_, &matchLenLabel_},
        {&randSlider_,     &randLabel_} }) {
    auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
    pair.second->setBounds(cell.removeFromTop(labelH));
    pair.first->setBounds(cell);
}
```

Replace with:

```cpp
// Row 2: seek, matchLen (or sync div), rand
const int kw2 = std::min(90, (r.getWidth() - 6) / 3);
const int kh2 = kw2 + labelH;
auto row2 = r.removeFromTop(kh2);

// seek
{
    auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
    seekTimeLabel_.setBounds(cell.removeFromTop(labelH));
    seekTimeSlider_.setBounds(cell);
}
// matchLen — show either rotary or combobox+sync button depending on sync state
{
    auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
    if (matchLenSyncButton_.getToggleState()) {
        // Sync on: Sync label at top, ComboBox fills rest
        matchLenSyncButton_.setBounds(cell.removeFromTop(labelH));
        matchLenDivBox_.setBounds(cell);
        matchLenSlider_.setBounds({});
        matchLenLabel_.setBounds({});
    } else {
        // Sync off: normal label+knob, hide div box
        matchLenLabel_.setBounds(cell.removeFromTop(labelH));
        matchLenSlider_.setBounds(cell);
        matchLenDivBox_.setBounds({});
        matchLenSyncButton_.setBounds({});
    }
}
// rand
{
    auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
    randLabel_.setBounds(cell.removeFromTop(labelH));
    randSlider_.setBounds(cell);
}
```

- [ ] Replace the Row 2 layout block in `resized()`.
- [ ] Also update the `onClick` lambda for `matchLenSyncButton_` to call `resized()` in addition to the show/hide logic:

```cpp
matchLenSyncButton_.onClick = [this] {
    const bool s = matchLenSyncButton_.getToggleState();
    matchLenSlider_.setVisible(!s);
    matchLenLabel_.setVisible(!s);
    matchLenDivBox_.setVisible(s);
    resized();
};
```

- [ ] Build: `cmake --build build --target Stitcher_Standalone 2>&1 | tail -5`
- [ ] Confirm 40 tests still pass.
- [ ] Commit:

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(4a): UI — sync toggle and division combobox for matchLen"
```

---

## Feature 4c — A/B State Slots

### Task 4c-1: Extend PresetBar with A/B buttons

**Files:**
- Modify: `Source/UI/PresetBar.h`
- Modify: `Source/UI/PresetBar.cpp`

#### Step 4c-1.1 — Add callbacks and buttons to PresetBar.h

Add to the public section of `PresetBar`:

```cpp
std::function<void(int slot)> onCaptureSlot;   // called with 0 or 1 to capture A or B
std::function<void(int slot)> onLoadSlot;      // called with 0 or 1 to load A or B
std::function<void()>         onSwapSlots;     // copy A→B or B→A (swaps both)
```

Add to private:

```cpp
juce::TextButton slotCaptureA_ { "•A" };
juce::TextButton slotCaptureB_ { "•B" };
juce::TextButton slotLoadA_    { "A"  };
juce::TextButton slotLoadB_    { "B"  };
```

- [ ] Add callbacks and buttons to `PresetBar.h`.

#### Step 4c-1.2 — Wire buttons in PresetBar constructor

In `PresetBar::PresetBar`, after existing button setup:

```cpp
for (auto* b : { &slotCaptureA_, &slotCaptureB_, &slotLoadA_, &slotLoadB_ })
    addAndMakeVisible(b);

slotCaptureA_.onClick = [this] { if (onCaptureSlot) onCaptureSlot(0); };
slotCaptureB_.onClick = [this] { if (onCaptureSlot) onCaptureSlot(1); };
slotLoadA_.onClick    = [this] { if (onLoadSlot)    onLoadSlot(0); };
slotLoadB_.onClick    = [this] { if (onLoadSlot)    onLoadSlot(1); };
```

- [ ] Add button wiring to `PresetBar.cpp` constructor.

#### Step 4c-1.3 — Layout in PresetBar::resized()

The current layout:
```cpp
prevButton_  .setBounds(r.removeFromLeft(btnW));    r.removeFromLeft(gap);
nextButton_  .setBounds(r.removeFromRight(btnW));   r.removeFromRight(gap);
initButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
saveAsButton_.setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
saveButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
presetNameLabel_.setBounds(r);
```

Replace with:

```cpp
const int abW = 22;
prevButton_    .setBounds(r.removeFromLeft(btnW));      r.removeFromLeft(gap);
nextButton_    .setBounds(r.removeFromRight(btnW));     r.removeFromRight(gap);
initButton_    .setBounds(r.removeFromRight(actionW));  r.removeFromRight(gap);
saveAsButton_  .setBounds(r.removeFromRight(actionW));  r.removeFromRight(gap);
saveButton_    .setBounds(r.removeFromRight(actionW));  r.removeFromRight(gap);
r.removeFromRight(gap);
slotLoadB_     .setBounds(r.removeFromRight(abW));      r.removeFromRight(2);
slotCaptureB_  .setBounds(r.removeFromRight(abW));      r.removeFromRight(gap);
slotLoadA_     .setBounds(r.removeFromRight(abW));      r.removeFromRight(2);
slotCaptureA_  .setBounds(r.removeFromRight(abW));      r.removeFromRight(gap);
presetNameLabel_.setBounds(r);
```

- [ ] Replace `resized()` layout in `PresetBar.cpp`.

#### Step 4c-1.4 — Commit PresetBar changes

```bash
git add Source/UI/PresetBar.h Source/UI/PresetBar.cpp
git commit -m "feat(4c): add A/B slot buttons to PresetBar"
```

- [ ] Commit.

### Task 4c-2: A/B state logic in StitcherEditor

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

#### Step 4c-2.1 — Add slot members to PluginEditor.h

In the private section, before `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`:

```cpp
juce::ValueTree abSlots_[2];
```

- [ ] Add `juce::ValueTree abSlots_[2];` to `PluginEditor.h`.

#### Step 4c-2.2 — Wire callbacks in PluginEditor.cpp constructor

After `addAndMakeVisible(presetBar_);`, add:

```cpp
presetBar_.onCaptureSlot = [this](int slot) {
    abSlots_[slot] = audioProcessor.getAPVTS().copyState().createCopy();
};

presetBar_.onLoadSlot = [this](int slot) {
    if (abSlots_[slot].isValid())
        audioProcessor.getAPVTS().replaceState(abSlots_[slot].createCopy());
};
```

- [ ] Add A/B callback wiring to the editor constructor.
- [ ] Build: `cmake --build build --target Stitcher_Standalone 2>&1 | tail -5`
- [ ] Confirm 40 tests pass.
- [ ] Commit:

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(4c): A/B state snapshot slots wired in StitcherEditor"
```

---

## Feature 4b — MIDI Learn

### Task 4b-1: MidiLearn class

**Files:**
- Create: `Source/MidiLearn.h`
- Create: `Source/MidiLearn.cpp`
- Create: `tests/MidiLearnTest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

#### Step 4b-1.1 — Write failing tests first

Create `tests/MidiLearnTest.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "MidiLearn.h"
#include "Parameters.h"

// Minimal stub for RangedAudioParameter that returns clamped normalised values
struct StubParam : public juce::RangedAudioParameter {
    StubParam(const juce::String& pid)
        : juce::RangedAudioParameter(juce::ParameterID{pid, 1}, pid) {}
    float getValue() const override { return value; }
    void  setValue(float v) override { value = v; }
    float getDefaultValue() const override { return 0.f; }
    juce::String getText(float v, int) const override { return juce::String(v); }
    float getValueForText(const juce::String& t) const override { return t.getFloatValue(); }
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }
    juce::NormalisableRange<float> range { 0.f, 1.f };
    float value { 0.f };
};

TEST_CASE("MidiLearn: bind and apply CC") {
    StubParam param("rms_weight");
    MidiLearn ml;
    ml.setBinding(74, &param);

    // CC 74 at value 64 → normalised = 64/127
    ml.handleCC(74, 64);
    REQUIRE(param.value == Catch::Approx(64.f / 127.f).margin(0.01f));
}

TEST_CASE("MidiLearn: clear binding removes it") {
    StubParam param("rms_weight");
    MidiLearn ml;
    ml.setBinding(74, &param);
    ml.clearBinding(&param);

    param.value = 0.5f;
    ml.handleCC(74, 0);  // no binding — should not touch param
    REQUIRE(param.value == Catch::Approx(0.5f));
}

TEST_CASE("MidiLearn: learning state is captured on next CC") {
    StubParam param("rms_weight");
    MidiLearn ml;
    ml.startLearning(&param);
    REQUIRE(ml.isLearning());

    ml.handleCC(20, 100);  // learning: should bind CC 20 to param
    REQUIRE_FALSE(ml.isLearning());
    REQUIRE(ml.getCC(&param).hasValue());
    REQUIRE(*ml.getCC(&param) == 20);
    REQUIRE(param.value == Catch::Approx(100.f / 127.f).margin(0.01f));
}

TEST_CASE("MidiLearn: save and load round-trip") {
    StubParam p1("rms_weight");
    StubParam p2("zcr_weight");
    MidiLearn ml;
    ml.setBinding(74, &p1);
    ml.setBinding(71, &p2);

    juce::ValueTree state("ROOT");
    ml.saveToValueTree(state);

    MidiLearn ml2;
    ml2.addParamForLoad("rms_weight", &p1);
    ml2.addParamForLoad("zcr_weight", &p2);
    ml2.loadFromValueTree(state);

    REQUIRE(ml2.getCC(&p1).hasValue());
    REQUIRE(*ml2.getCC(&p1) == 74);
    REQUIRE(ml2.getCC(&p2).hasValue());
    REQUIRE(*ml2.getCC(&p2) == 71);
}
```

- [ ] Create `tests/MidiLearnTest.cpp` with the above content.

#### Step 4b-1.2 — Add MidiLearn to test CMakeLists

In `tests/CMakeLists.txt`, add `MidiLearnTest.cpp` to the executable sources and add `${CMAKE_SOURCE_DIR}/Source/MidiLearn.cpp` to the source list:

```cmake
add_executable(StitcherTests
    FeatureExtractorTest.cpp
    CorpusStoreTest.cpp
    ConcatenativeMatcherTest.cpp
    EQProcessorTest.cpp
    PresetManagerTest.cpp
    MidiLearnTest.cpp                              # ← add
    ${CMAKE_SOURCE_DIR}/Source/FeatureExtractor.cpp
    ${CMAKE_SOURCE_DIR}/Source/CorpusStore.cpp
    ${CMAKE_SOURCE_DIR}/Source/ConcatenativeMatcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/EQProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/MidiLearn.cpp       # ← add
)
```

- [ ] Modify `tests/CMakeLists.txt` to include `MidiLearnTest.cpp` and `MidiLearn.cpp`.

#### Step 4b-1.3 — Create `Source/MidiLearn.h`

```cpp
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <vector>
#include <string>

class MidiLearn {
public:
    static constexpr int kNoCc = -1;
    static constexpr int kMaxCc = 128;

    // Register a param for save/load lookups (call before loadFromValueTree)
    void addParamForLoad(const juce::String& paramId, juce::RangedAudioParameter* param);

    // Explicit binding management (message thread)
    void setBinding(int cc, juce::RangedAudioParameter* param);
    void clearBinding(juce::RangedAudioParameter* param);
    juce::Optional<int> getCC(juce::RangedAudioParameter* param) const;

    // Learning state (message thread sets; audio thread reads)
    void startLearning(juce::RangedAudioParameter* param);
    void cancelLearning();
    bool isLearning() const;

    // Audio thread: call from processBlock for each incoming CC message
    void handleCC(int cc, int value);

    // Persistence (message thread only)
    void saveToValueTree(juce::ValueTree& state) const;
    void loadFromValueTree(const juce::ValueTree& state);

private:
    // CC → param: index = CC number (0–127), value = param ptr or nullptr
    std::array<std::atomic<juce::RangedAudioParameter*>, kMaxCc> ccToParam_ {};

    // param → CC: iterate to find; small linear scan, only on message thread
    struct Binding { juce::RangedAudioParameter* param; int cc; };
    std::vector<Binding> bindings_;  // message-thread only

    std::atomic<juce::RangedAudioParameter*> learningParam_ { nullptr };

    // Registry for save/load: paramId → param
    std::vector<std::pair<juce::String, juce::RangedAudioParameter*>> paramRegistry_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearn)
};
```

- [ ] Create `Source/MidiLearn.h`.

#### Step 4b-1.4 — Create `Source/MidiLearn.cpp`

```cpp
#include "MidiLearn.h"

void MidiLearn::addParamForLoad(const juce::String& paramId, juce::RangedAudioParameter* param)
{
    paramRegistry_.push_back({ paramId, param });
}

void MidiLearn::setBinding(int cc, juce::RangedAudioParameter* param)
{
    if (cc < 0 || cc >= kMaxCc || param == nullptr) return;

    // Remove any existing binding for this param
    clearBinding(param);

    // Remove any param already bound to this CC
    if (auto* old = ccToParam_[cc].load(std::memory_order_acquire))
    {
        bindings_.erase(std::remove_if(bindings_.begin(), bindings_.end(),
            [old](const Binding& b){ return b.param == old; }), bindings_.end());
    }

    ccToParam_[cc].store(param, std::memory_order_release);
    bindings_.push_back({ param, cc });
}

void MidiLearn::clearBinding(juce::RangedAudioParameter* param)
{
    for (auto& b : bindings_)
    {
        if (b.param == param)
        {
            ccToParam_[b.cc].store(nullptr, std::memory_order_release);
            b = bindings_.back();
            bindings_.pop_back();
            return;
        }
    }
}

juce::Optional<int> MidiLearn::getCC(juce::RangedAudioParameter* param) const
{
    for (const auto& b : bindings_)
        if (b.param == param)
            return b.cc;
    return {};
}

void MidiLearn::startLearning(juce::RangedAudioParameter* param)
{
    learningParam_.store(param, std::memory_order_release);
}

void MidiLearn::cancelLearning()
{
    learningParam_.store(nullptr, std::memory_order_release);
}

bool MidiLearn::isLearning() const
{
    return learningParam_.load(std::memory_order_acquire) != nullptr;
}

void MidiLearn::handleCC(int cc, int value)
{
    if (cc < 0 || cc >= kMaxCc) return;

    // If in learning mode, bind this CC to the learning param
    if (auto* lp = learningParam_.exchange(nullptr, std::memory_order_acq_rel))
    {
        setBinding(cc, lp);
    }

    // Apply binding if exists
    if (auto* p = ccToParam_[cc].load(std::memory_order_acquire))
    {
        const float normalised = static_cast<float>(value) / 127.f;
        p->setValueNotifyingHost(normalised);
    }
}

void MidiLearn::saveToValueTree(juce::ValueTree& state) const
{
    for (const auto& b : bindings_)
    {
        const juce::String key = "midiCC_" + b.param->paramID;
        state.setProperty(key, b.cc, nullptr);
    }
}

void MidiLearn::loadFromValueTree(const juce::ValueTree& state)
{
    for (const auto& [id, param] : paramRegistry_)
    {
        const juce::String key = "midiCC_" + id;
        if (state.hasProperty(key))
        {
            const int cc = static_cast<int>(state.getProperty(key));
            if (cc >= 0 && cc < kMaxCc)
                setBinding(cc, param);
        }
    }
}
```

- [ ] Create `Source/MidiLearn.cpp`.

#### Step 4b-1.5 — Run tests

```bash
cmake --build build --target StitcherTests 2>&1 | tail -10
./build/tests/StitcherTests
```

Expected: 44 tests pass (40 old + 4 new).

- [ ] Tests pass 44/44.

#### Step 4b-1.6 — Commit

```bash
git add Source/MidiLearn.h Source/MidiLearn.cpp tests/MidiLearnTest.cpp tests/CMakeLists.txt
git commit -m "feat(4b): add MidiLearn class with lock-free CC dispatch and save/load"
```

- [ ] Commit.

### Task 4b-2: Wire MidiLearn into PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`
- Modify: `CMakeLists.txt`

#### Step 4b-2.1 — Add MidiLearn.cpp to CMakeLists SourceFiles

In `CMakeLists.txt`, add to the `set(SourceFiles ...)` block:

```cmake
    Source/MidiLearn.h
    Source/MidiLearn.cpp
```

- [ ] Add `MidiLearn.h` and `MidiLearn.cpp` to `SourceFiles` in `CMakeLists.txt`.

#### Step 4b-2.2 — Add MidiLearn member to PluginProcessor.h

Add `#include "MidiLearn.h"` to the includes, then add to private section:

```cpp
MidiLearn midiLearn_;
```

Add a public accessor:

```cpp
MidiLearn& getMidiLearn() { return midiLearn_; }
```

- [ ] Add include, member, and accessor to `PluginProcessor.h`.

#### Step 4b-2.3 — Register all params with MidiLearn in constructor

At the end of `StitcherProcessor()` constructor, add:

```cpp
// Register all ranged params for MIDI learn save/load
for (auto* param : apvts_.processor.getParameters())
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
        midiLearn_.addParamForLoad(rp->paramID, rp);
```

- [ ] Add registration loop to constructor.

#### Step 4b-2.4 — Update processBlock to dispatch MIDI CC

Replace `juce::ignoreUnused(midiMessages);` with:

```cpp
// Dispatch MIDI CC to MIDI learn
for (const auto meta : midiMessages)
{
    const auto msg = meta.getMessage();
    if (msg.isController())
        midiLearn_.handleCC(msg.getControllerNumber(), msg.getControllerValue());
}
```

- [ ] Replace `ignoreUnused` with CC dispatch loop.

#### Step 4b-2.5 — Persist MIDI bindings in state info

In `getStateInformation`:

```cpp
void StitcherProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    midiLearn_.saveToValueTree(state);
    copyXmlToBinary(*state.createXml(), destData);
}
```

In `setStateInformation`:

```cpp
void StitcherProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        apvts_.replaceState(tree);
        midiLearn_.loadFromValueTree(tree);
    }
}
```

- [ ] Update `getStateInformation` and `setStateInformation`.

#### Step 4b-2.6 — Build and test

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | tail -5
./build/tests/StitcherTests
```

Expected: clean build, 44 tests pass.

- [ ] Clean build and 44 tests pass.

#### Step 4b-2.7 — Commit

```bash
git add CMakeLists.txt Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat(4b): wire MidiLearn into PluginProcessor — CC dispatch + state persistence"
```

- [ ] Commit.

### Task 4b-3: MIDI Learn UI — right-click context menu on sliders

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

This task adds right-click "Learn MIDI CC" / "Clear MIDI CC" context menus to all APVTS-bound sliders. We do this via a shared `MouseListener` held by the editor, registered on every slider.

#### Step 4b-3.1 — Add MidiLearnMouseListener inner class to PluginEditor.h

Add inside `StitcherEditor` private section:

```cpp
struct MidiLearnListener : public juce::MouseListener {
    MidiLearnListener(StitcherEditor& owner) : owner_(owner) {}
    void mouseDown(const juce::MouseEvent& e) override;
    StitcherEditor& owner_;
};
MidiLearnListener midiLearnListener_ { *this };
```

- [ ] Add `MidiLearnListener` struct and `midiLearnListener_` member to `PluginEditor.h`.

#### Step 4b-3.2 — Register listener on all sliders

At the end of the `StitcherEditor` constructor (after attachments), add:

```cpp
// Register MIDI learn right-click listener on all APVTS sliders
for (auto* s : { &zcrSlider_, &rmsSlider_, &scSlider_, &stSlider_,
                 &seekTimeSlider_, &matchLenSlider_, &randSlider_,
                 &gainCtrlSlider_, &gainSrcSlider_,
                 &eqLowSlider_, &eqMidSlider_, &eqHighSlider_,
                 &reverbRoomSlider_, &reverbDampSlider_, &reverbWetSlider_,
                 &gainOutSlider_, &mixSlider_ })
    s->addMouseListener(&midiLearnListener_, false);
```

- [ ] Add mouse listener registration loop to constructor.

#### Step 4b-3.3 — Implement mouseDown in PluginEditor.cpp

Add this function definition in `PluginEditor.cpp`:

```cpp
void StitcherEditor::MidiLearnListener::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown()) return;

    auto* slider = dynamic_cast<juce::Slider*>(e.originalComponent);
    if (!slider) return;

    // Find the APVTS parameter bound to this slider
    auto& apvts = owner_.audioProcessor.getAPVTS();
    juce::RangedAudioParameter* param = nullptr;
    for (auto* p : apvts.processor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            if (apvts.getParameter(rp->paramID) == rp)
            {
                // Check if this slider is attached to this param by checking slider value
                // We check attachment table indirectly: match by slider component
                // Use paramID-to-slider map built once at construction
            }
        }
    }

    // Look up param from slider via the owner's attachment-to-slider map
    param = owner_.getParamForSlider(slider);
    if (!param) return;

    auto& ml = owner_.audioProcessor.getMidiLearn();
    juce::PopupMenu menu;
    menu.addItem(1, "Learn MIDI CC");
    if (ml.getCC(param).hasValue())
        menu.addItem(2, "Clear MIDI CC (CC " + juce::String(*ml.getCC(param)) + ")");

    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(slider),
        [this, param, &ml](int result) {
            if (result == 1) ml.startLearning(param);
            else if (result == 2) ml.clearBinding(param);
        });
}
```

#### Step 4b-3.4 — Add getParamForSlider helper

This requires a slider→param mapping. The cleanest approach: build it at construction using the APVTS parameter IDs.

Add to `PluginEditor.h` private section:

```cpp
std::unordered_map<juce::Slider*, juce::RangedAudioParameter*> sliderToParam_;
juce::RangedAudioParameter* getParamForSlider(juce::Slider* s) const;
```

In `PluginEditor.cpp`, populate `sliderToParam_` after all attachments are created. Add this block at the end of the constructor:

```cpp
// Build slider→param map for MIDI learn context menu
auto& apvts2 = audioProcessor.getAPVTS();
const std::pair<juce::Slider*, const char*> sliderParamPairs[] = {
    { &zcrSlider_,        ParamIDs::zcrWeight  },
    { &rmsSlider_,        ParamIDs::rmsWeight  },
    { &scSlider_,         ParamIDs::scWeight   },
    { &stSlider_,         ParamIDs::stWeight   },
    { &seekTimeSlider_,   ParamIDs::seekTime   },
    { &matchLenSlider_,   ParamIDs::matchLen   },
    { &randSlider_,       ParamIDs::rand_      },
    { &gainCtrlSlider_,   ParamIDs::gainCtrl   },
    { &gainSrcSlider_,    ParamIDs::gainSrc    },
    { &eqLowSlider_,      ParamIDs::eqLow      },
    { &eqMidSlider_,      ParamIDs::eqMid      },
    { &eqHighSlider_,     ParamIDs::eqHigh     },
    { &reverbRoomSlider_, ParamIDs::reverbRoom },
    { &reverbDampSlider_, ParamIDs::reverbDamp },
    { &reverbWetSlider_,  ParamIDs::reverbWet  },
    { &gainOutSlider_,    ParamIDs::gainOut    },
    { &mixSlider_,        ParamIDs::mix        },
};
for (const auto& [slider, id] : sliderParamPairs)
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(apvts2.getParameter(id)))
        sliderToParam_[slider] = rp;
```

Add `getParamForSlider` implementation:

```cpp
juce::RangedAudioParameter* StitcherEditor::getParamForSlider(juce::Slider* s) const
{
    auto it = sliderToParam_.find(s);
    return it != sliderToParam_.end() ? it->second : nullptr;
}
```

Now simplify the `mouseDown` implementation to use `getParamForSlider`:

```cpp
void StitcherEditor::MidiLearnListener::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown()) return;

    auto* slider = dynamic_cast<juce::Slider*>(e.originalComponent);
    if (!slider) return;

    auto* param = owner_.getParamForSlider(slider);
    if (!param) return;

    auto& ml = owner_.audioProcessor.getMidiLearn();
    juce::PopupMenu menu;
    menu.addItem(1, "Learn MIDI CC");
    if (ml.getCC(param).hasValue())
        menu.addItem(2, "Clear MIDI CC (CC " + juce::String(*ml.getCC(param)) + ")");

    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(slider),
        [param, &ml](int result) {
            if (result == 1) ml.startLearning(param);
            else if (result == 2) ml.clearBinding(param);
        });
}
```

Note: the lambda captures `&ml` by reference — `ml` is `owner_.audioProcessor.getMidiLearn()` which is a long-lived reference. This is safe as long as the popup callback fires before the editor is destroyed (JUCE ensures this via `SafePointer` internally for `showMenuAsync`). For extra safety, capture `&owner_` and dereference:

```cpp
juce::Component::SafePointer<StitcherEditor> safeOwner(&owner_);
menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(slider),
    [safeOwner, param](int result) {
        if (!safeOwner) return;
        auto& ml = safeOwner->audioProcessor.getMidiLearn();
        if (result == 1) ml.startLearning(param);
        else if (result == 2) ml.clearBinding(param);
    });
```

- [ ] Implement `mouseDown`, `getParamForSlider`, build the `sliderToParam_` map.

#### Step 4b-3.5 — Also need `#include <unordered_map>` in PluginEditor.h

- [ ] Add `#include <unordered_map>` to `PluginEditor.h`.

#### Step 4b-3.6 — Build

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | tail -10
./build/tests/StitcherTests
```

Expected: clean build, 44 tests pass.

- [ ] Clean build, 44 tests pass.

#### Step 4b-3.7 — Commit

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat(4b): right-click MIDI learn context menu on all sliders"
```

- [ ] Commit.

---

## Task 5: Final Verification and Install

- [ ] Build all targets: `cmake --build build --target Stitcher_Standalone Stitcher_AU Stitcher_VST3`
- [ ] Run tests: `./build/tests/StitcherTests` — expect 44 pass
- [ ] Manual check 4a: Enable Sync on matchLen → ComboBox appears; disable → knob reappears
- [ ] Manual check 4c: Capture to A, change a knob, load A → knob restores; copy B works
- [ ] Manual check 4b: Right-click a knob → "Learn MIDI CC" appears in menu; send MIDI CC → knob moves
- [ ] Verify plugins installed at `~/Library/Audio/Plug-Ins/`
- [ ] Final commit if any stray changes: `git add -u && git commit -m "phase4: tempo sync, A/B slots, MIDI learn"`

---

## Self-Review

**Spec coverage:**
- ✅ 4a: `match_len_sync` bool + `match_len_div` choice params; BPM from play head at prepare time; fallback to ms
- ✅ 4a: UI shows ComboBox when sync on, knob when off
- ✅ 4c: Two `juce::ValueTree` slots in editor; capture/load via PresetBar callbacks; session-local
- ✅ 4b: `MidiLearn` with lock-free atomic array; right-click context menu; CC dispatch in processBlock; MIDI state persists with project (not with presets)
- ✅ All three features in `state roundtrip`: 4a new params auto-saved in APVTS; 4c is session-local by design; 4b persisted alongside APVTS state

**No new audio-thread allocations**: `MidiLearn::handleCC` uses only atomic loads/stores. ✅

**Preset compatibility**: Loading a factory or user preset does NOT clear MIDI bindings (MIDI state is on the root state tree, not in PARAM children). ✅

**Tests**: 4 new tests for `MidiLearn` using a `StubParam` that doesn't require APVTS, `DummyProcessor`, or JUCE GUI init. ✅
