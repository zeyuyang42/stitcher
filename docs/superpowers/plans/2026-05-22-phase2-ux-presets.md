# Phase 2 — UX Enhancements (Preset Bar + Live Visualization) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a preset management bar (save/load/prev/next/init), 4 live ctrl-feature meters, a corpus fill indicator, and a stereo output level meter, all driven by audio-thread atomics polled at 30 Hz.

**Architecture:** PluginProcessor exposes 7 `std::atomic<float>` fields written on the audio thread. StitcherEditor inherits from `juce::Timer` and polls those atomics at 30 Hz to repaint meters. PresetManager owns disk-based preset save/load and operates on APVTS state XML. PresetBar is a self-contained UI component that delegates to PresetManager. All new UI components live under `Source/UI/`.

**Tech Stack:** JUCE 8.x, C++17, `juce::Timer`, `juce::AudioProcessorValueTreeState`, `juce::File`, `std::atomic<float>`, Catch2.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `Source/PluginProcessor.h` | Modify | Add 7 atomic fields + public getters + `corpusMaxFrames_` |
| `Source/PluginProcessor.cpp` | Modify | Write atomics in processBlock |
| `Source/UI/FeatureMeter.h` | Create | Horizontal bargraph component |
| `Source/UI/FeatureMeter.cpp` | Create | paint(): amber fill on Track bg |
| `Source/UI/LevelMeter.h` | Create | Stereo vertical peak meter component |
| `Source/UI/LevelMeter.cpp` | Create | paint(): two vertical amber bars |
| `Source/PresetManager.h` | Create | Preset save/load/navigate; injectable dir for testing |
| `Source/PresetManager.cpp` | Create | Disk XML save/load via APVTS |
| `Source/UI/PresetBar.h` | Create | Preset bar UI component |
| `Source/UI/PresetBar.cpp` | Create | Buttons + label; calls PresetManager |
| `Source/LookAndFeel/StitcherLookAndFeel.h/.cpp` | Modify | Add TextButton colour IDs |
| `Source/PluginEditor.h` | Modify | Add Timer inheritance + new members |
| `Source/PluginEditor.cpp` | Modify | timerCallback, resized layout, PresetManager wiring |
| `Tests/PresetManagerTest.cpp` | Create | Catch2 save/load roundtrip test |
| `Tests/CMakeLists.txt` | Modify | Add PresetManagerTest.cpp + PresetManager.cpp to test target |
| `CMakeLists.txt` | Modify | Add new source files to SourceFiles |

---

## Task 0: Observability atomics in PluginProcessor

**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

- [ ] **Step 1: Add atomic fields and public getters to PluginProcessor.h**

In `Source/PluginProcessor.h`, after the existing `std::atomic<bool> matcherDirty_` block, add:

```cpp
    // UI observability — written on audio thread, read by message-thread timer ~30 Hz
    std::atomic<float> lastCtrlZcr_    { 0.f };
    std::atomic<float> lastCtrlRms_    { 0.f };
    std::atomic<float> lastCtrlSc_     { 0.f };
    std::atomic<float> lastCtrlSt_     { 0.f };
    std::atomic<float> lastCorpusFill_ { 0.f };
    std::atomic<float> lastOutPeakL_   { 0.f };
    std::atomic<float> lastOutPeakR_   { 0.f };
    int corpusMaxFrames_ = 1;
```

In the public section (after `getAPVTS()`), add:

```cpp
    float getLastCtrlZcr()    const noexcept { return lastCtrlZcr_.load(); }
    float getLastCtrlRms()    const noexcept { return lastCtrlRms_.load(); }
    float getLastCtrlSc()     const noexcept { return lastCtrlSc_.load(); }
    float getLastCtrlSt()     const noexcept { return lastCtrlSt_.load(); }
    float getLastCorpusFill() const noexcept { return lastCorpusFill_.load(); }
    float getLastOutPeakL()   const noexcept { return lastOutPeakL_.load(); }
    float getLastOutPeakR()   const noexcept { return lastOutPeakR_.load(); }
```

- [ ] **Step 2: Store corpusMaxFrames_ in prepareToPlay**

In `Source/PluginProcessor.cpp`, in `prepareToPlay`, after the line `corpus_.prepare(frameSize_, maxFrames);`, add:

```cpp
    corpusMaxFrames_ = std::max(1, maxFrames);
```

- [ ] **Step 3: Write ctrl features and corpus fill in processBlock**

In `Source/PluginProcessor.cpp`, inside the `if (accumPos_ == frameSize_)` block, immediately after the two `featureExtractor_.extract(...)` calls (before the `if (gs != 1.f)` block), add:

```cpp
            // Publish ctrl features for UI meters
            lastCtrlZcr_.store(ctrlFeatures.zcr);
            lastCtrlRms_.store(ctrlFeatures.rms);
            lastCtrlSc_.store(ctrlFeatures.sc);
            lastCtrlSt_.store(ctrlFeatures.st);
```

After `corpus_.push(...)`, add:

```cpp
            lastCorpusFill_.store(
                static_cast<float>(corpus_.size()) / static_cast<float>(corpusMaxFrames_));
```

- [ ] **Step 4: Write output peaks after limiter**

In `Source/PluginProcessor.cpp`, at the end of `processBlock`, immediately after `limiter_.process(...)` and before the `#if JUCE_DEBUG` block, add:

```cpp
    // Update output peak atomics for the level meter
    {
        const float* pl = output.getReadPointer(0);
        const float* pr = output.getReadPointer(1);
        float pkL = 0.f, pkR = 0.f;
        for (int i = 0; i < numSamples; ++i) {
            pkL = std::max(pkL, std::abs(pl[i]));
            pkR = std::max(pkR, std::abs(pr[i]));
        }
        constexpr float kDecay = 0.9f;
        lastOutPeakL_.store(std::max(pkL, lastOutPeakL_.load() * kDecay));
        lastOutPeakR_.store(std::max(pkR, lastOutPeakR_.load() * kDecay));
    }
```

- [ ] **Step 5: Build and verify no regressions**

```bash
cd /Users/zeyuyang42/Workspace/stitcher-010426/stitcher
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
./build/tests/StitcherTests
```

Expected: `Built target Stitcher_Standalone`. `All tests passed (226 assertions in 31 test cases)`.

- [ ] **Step 6: Commit**

```bash
git add Source/PluginProcessor.h Source/PluginProcessor.cpp
git commit -m "feat: add audio-thread observability atomics for UI meters"
```

---

## Task 1: FeatureMeter and LevelMeter components

**Files:**
- Create: `Source/UI/FeatureMeter.h`
- Create: `Source/UI/FeatureMeter.cpp`
- Create: `Source/UI/LevelMeter.h`
- Create: `Source/UI/LevelMeter.cpp`
- Modify: `Source/LookAndFeel/StitcherLookAndFeel.cpp` (add TextButton colours)
- Modify: `CMakeLists.txt` (add new sources)

- [ ] **Step 1: Create Source/UI/ directory**

```bash
mkdir -p /Users/zeyuyang42/Workspace/stitcher-010426/stitcher/Source/UI
```

- [ ] **Step 2: Write FeatureMeter.h**

```cpp
// Source/UI/FeatureMeter.h
#pragma once
#include <JuceHeader.h>

class FeatureMeter : public juce::Component {
public:
    void setLevel(float value01) noexcept;
    void setActive(bool active) noexcept;
    void paint(juce::Graphics&) override;
private:
    float level_  = 0.f;
    bool  active_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatureMeter)
};
```

- [ ] **Step 3: Write FeatureMeter.cpp**

```cpp
// Source/UI/FeatureMeter.cpp
#include "FeatureMeter.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void FeatureMeter::setLevel(float v) noexcept  { level_  = juce::jlimit(0.f, 1.f, v); }
void FeatureMeter::setActive(bool a) noexcept  { active_ = a; }

void FeatureMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(StitcherLookAndFeel::Track);
    g.fillRoundedRectangle(b, 2.f);

    if (level_ > 0.f) {
        auto fill = b.withWidth(b.getWidth() * level_);
        g.setColour(active_ ? StitcherLookAndFeel::Accent
                             : StitcherLookAndFeel::AccentDim);
        g.fillRoundedRectangle(fill, 2.f);
    }
}
```

- [ ] **Step 4: Write LevelMeter.h**

```cpp
// Source/UI/LevelMeter.h
#pragma once
#include <JuceHeader.h>

class LevelMeter : public juce::Component {
public:
    void setLevels(float peakL, float peakR) noexcept;
    void paint(juce::Graphics&) override;
private:
    float peakL_ = 0.f;
    float peakR_ = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
```

- [ ] **Step 5: Write LevelMeter.cpp**

```cpp
// Source/UI/LevelMeter.cpp
#include "LevelMeter.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void LevelMeter::setLevels(float l, float r) noexcept {
    peakL_ = juce::jlimit(0.f, 1.f, l);
    peakR_ = juce::jlimit(0.f, 1.f, r);
}

void LevelMeter::paint(juce::Graphics& g)
{
    const int w2 = getWidth() / 2 - 1;
    const int h  = getHeight();

    auto paintBar = [&](int x, float peak) {
        juce::Rectangle<int> bg{x, 0, w2, h};
        g.setColour(StitcherLookAndFeel::Track);
        g.fillRoundedRectangle(bg.toFloat(), 2.f);

        if (peak > 0.f) {
            int fillH = static_cast<int>(h * peak);
            juce::Rectangle<int> fill{x, h - fillH, w2, fillH};
            g.setColour(StitcherLookAndFeel::Accent);
            g.fillRoundedRectangle(fill.toFloat(), 2.f);
        }
    };

    paintBar(0,       peakL_);
    paintBar(w2 + 2,  peakR_);
}
```

- [ ] **Step 6: Add TextButton colour IDs to StitcherLookAndFeel constructor**

In `Source/LookAndFeel/StitcherLookAndFeel.cpp`, inside the constructor, after the last `setColour(...)` call, add:

```cpp
    setColour(juce::TextButton::buttonColourId,   Track);
    setColour(juce::TextButton::buttonOnColourId,  Accent);
    setColour(juce::TextButton::textColourOffId,   TextBright);
    setColour(juce::TextButton::textColourOnId,    Background);
```

- [ ] **Step 7: Add new sources to CMakeLists.txt**

In `CMakeLists.txt`, in the `set(SourceFiles ...)` block, add after the LookAndFeel entries:

```cmake
    Source/UI/FeatureMeter.h
    Source/UI/FeatureMeter.cpp
    Source/UI/LevelMeter.h
    Source/UI/LevelMeter.cpp
    Source/UI/PresetBar.h
    Source/UI/PresetBar.cpp
    Source/PresetManager.h
    Source/PresetManager.cpp
```

(Add all Phase 2 source files at once here so CMakeLists is only touched once.)

- [ ] **Step 8: Build to verify new files compile**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
```

Expected: `Built target Stitcher_Standalone` with no errors.

- [ ] **Step 9: Commit**

```bash
git add Source/UI/FeatureMeter.h Source/UI/FeatureMeter.cpp \
        Source/UI/LevelMeter.h Source/UI/LevelMeter.cpp \
        Source/LookAndFeel/StitcherLookAndFeel.cpp CMakeLists.txt
git commit -m "feat: add FeatureMeter and LevelMeter UI components"
```

---

## Task 2: PresetManager (with unit test)

**Files:**
- Create: `Source/PresetManager.h`
- Create: `Source/PresetManager.cpp`
- Create: `Tests/PresetManagerTest.cpp`
- Modify: `Tests/CMakeLists.txt`

- [ ] **Step 1: Write PresetManager.h**

```cpp
// Source/PresetManager.h
#pragma once
#include <JuceHeader.h>

class PresetManager {
public:
    // presetsDir defaults to getUserPresetsDir(); pass a temp dir for testing
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts,
                           juce::File presetsDir = getUserPresetsDir());

    void savePreset(const juce::String& name);
    bool loadPreset(const juce::String& name);   // false if file not found
    void deletePreset(const juce::String& name);
    void initPreset();

    juce::StringArray getPresetNames() const;
    juce::String getCurrentPresetName() const;
    void setCurrentPresetName(const juce::String& name);

    void selectNext();
    void selectPrev();

    static juce::File getUserPresetsDir();

private:
    juce::AudioProcessorValueTreeState& apvts_;
    juce::File presetsDir_;
    juce::String currentPresetName_;
};
```

- [ ] **Step 2: Write PresetManager.cpp**

```cpp
// Source/PresetManager.cpp
#include "PresetManager.h"

juce::File PresetManager::getUserPresetsDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Stitcher")
               .getChildFile("Presets")
               .getChildFile("User");
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             juce::File presetsDir)
    : apvts_(apvts), presetsDir_(presetsDir)
{
    presetsDir_.createDirectory();
}

void PresetManager::savePreset(const juce::String& name)
{
    auto xml = apvts_.copyState().createXml();
    if (!xml) return;
    presetsDir_.createDirectory();
    presetsDir_.getChildFile(name + ".xml").replaceWithText(xml->toString());
    currentPresetName_ = name;
}

bool PresetManager::loadPreset(const juce::String& name)
{
    auto file = presetsDir_.getChildFile(name + ".xml");
    if (!file.existsAsFile()) return false;
    if (auto xml = juce::XmlDocument::parse(file)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
            apvts_.replaceState(tree);
    }
    currentPresetName_ = name;
    return true;
}

void PresetManager::deletePreset(const juce::String& name)
{
    presetsDir_.getChildFile(name + ".xml").deleteFile();
    if (currentPresetName_ == name)
        currentPresetName_ = {};
}

void PresetManager::initPreset()
{
    for (auto* param : apvts_.processor.getParameters())
        param->setValueNotifyingHost(param->getDefaultValue());
    currentPresetName_ = {};
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& f : presetsDir_.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(false);
    return names;
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName_;
}

void PresetManager::setCurrentPresetName(const juce::String& name)
{
    currentPresetName_ = name;
}

void PresetManager::selectNext()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = names.indexOf(currentPresetName_);
    loadPreset(names[(idx + 1) % names.size()]);
}

void PresetManager::selectPrev()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = names.indexOf(currentPresetName_);
    loadPreset(names[(idx - 1 + names.size()) % names.size()]);
}
```

- [ ] **Step 3: Write the failing test**

```cpp
// Tests/PresetManagerTest.cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>
#include "PresetManager.h"
#include "Parameters.h"

// Minimal AudioProcessor stub needed to construct APVTS
struct DummyProcessor : public juce::AudioProcessor {
    DummyProcessor() : juce::AudioProcessor(
        BusesProperties()
            .withInput ("In",  juce::AudioChannelSet::stereo())
            .withOutput("Out", juce::AudioChannelSet::stereo())) {}
    const juce::String getName() const override { return "Dummy"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
};

TEST_CASE("PresetManager: getPresetNames returns empty list when dir is empty") {
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    REQUIRE(pm.getPresetNames().isEmpty());
    REQUIRE(pm.getCurrentPresetName().isEmpty());

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: save/load roundtrip restores parameter value") {
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    // Set RMS weight to a non-default value
    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(0.42f);
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(0.42f).margin(0.01f));

    pm.savePreset("TestPreset");
    REQUIRE(pm.getPresetNames().contains("TestPreset"));

    // Reset to default (1.0)
    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(1.0f);

    bool loaded = pm.loadPreset("TestPreset");
    REQUIRE(loaded);
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(0.42f).margin(0.01f));
    REQUIRE(pm.getCurrentPresetName() == "TestPreset");

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: init resets all params to defaults") {
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(0.42f);
    pm.savePreset("Temp");
    pm.initPreset();

    // rmsWeight default is 1.0 (see Parameters.h)
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(1.f).margin(0.01f));
    REQUIRE(pm.getCurrentPresetName().isEmpty());

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: selectNext/selectPrev cycles through presets") {
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    pm.savePreset("Alpha");
    pm.savePreset("Beta");
    pm.savePreset("Gamma");

    // sorted: Alpha, Beta, Gamma
    pm.setCurrentPresetName("Alpha");
    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Beta");

    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Gamma");

    pm.selectNext();  // wraps
    REQUIRE(pm.getCurrentPresetName() == "Alpha");

    pm.selectPrev();  // wraps back to Gamma
    REQUIRE(pm.getCurrentPresetName() == "Gamma");

    tempDir.deleteRecursively();
}
```

- [ ] **Step 4: Run test to verify it fails**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target"
./build/tests/StitcherTests "[PresetManager]" 2>&1 | tail -5
```

Expected: compile error `'PresetManager' file not found` (or similar) — test not yet compiled in.

- [ ] **Step 5: Add PresetManagerTest to Tests/CMakeLists.txt**

In `Tests/CMakeLists.txt`, in `add_executable(StitcherTests ...)`, add:

```cmake
    PresetManagerTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/PresetManager.cpp
```

In `target_link_libraries(StitcherTests PRIVATE ...)`, add `juce_audio_processors` to ensure APVTS is available:

```cmake
    juce_audio_processors
```

- [ ] **Step 6: Run test to verify it passes**

```bash
cmake --build build --target StitcherTests 2>&1 | grep -E "error:|Built target"
./build/tests/StitcherTests "[PresetManager]" -v
```

Expected: all 4 PresetManager test cases pass.

- [ ] **Step 7: Run full test suite**

```bash
./build/tests/StitcherTests
```

Expected: All tests passed (all 31 original + 4 new = 35 test cases).

- [ ] **Step 8: Commit**

```bash
git add Source/PresetManager.h Source/PresetManager.cpp \
        Tests/PresetManagerTest.cpp Tests/CMakeLists.txt
git commit -m "feat: add PresetManager with disk-based save/load/navigate and unit tests"
```

---

## Task 3: PresetBar component

**Files:**
- Create: `Source/UI/PresetBar.h`
- Create: `Source/UI/PresetBar.cpp`

Note: `CMakeLists.txt` was already updated in Task 1 Step 7 with `Source/UI/PresetBar.h/.cpp`.

- [ ] **Step 1: Write PresetBar.h**

```cpp
// Source/UI/PresetBar.h
#pragma once
#include <JuceHeader.h>
#include "../PresetManager.h"

class PresetBar : public juce::Component {
public:
    explicit PresetBar(PresetManager& pm);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    PresetManager& presetManager_;

    juce::Label      presetNameLabel_;
    juce::TextButton prevButton_   { "<" };
    juce::TextButton nextButton_   { ">" };
    juce::TextButton saveButton_   { "Save" };
    juce::TextButton saveAsButton_ { "Save As" };
    juce::TextButton initButton_   { "Init" };

    void updateLabel();
    void showSaveAsDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
```

- [ ] **Step 2: Write PresetBar.cpp**

```cpp
// Source/UI/PresetBar.cpp
#include "PresetBar.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

PresetBar::PresetBar(PresetManager& pm) : presetManager_(pm)
{
    presetNameLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetNameLabel_);

    for (auto* b : { &prevButton_, &nextButton_, &saveButton_, &saveAsButton_, &initButton_ })
        addAndMakeVisible(b);

    prevButton_.onClick = [this] {
        presetManager_.selectPrev();
        updateLabel();
    };

    nextButton_.onClick = [this] {
        presetManager_.selectNext();
        updateLabel();
    };

    saveButton_.onClick = [this] {
        auto name = presetManager_.getCurrentPresetName();
        if (name.isNotEmpty()) {
            presetManager_.savePreset(name);
        } else {
            showSaveAsDialog();
        }
    };

    saveAsButton_.onClick = [this] { showSaveAsDialog(); };

    initButton_.onClick = [this] {
        presetManager_.initPreset();
        updateLabel();
    };

    updateLabel();
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced(2);
    const int btnW = 28;
    const int actionW = 68;
    const int gap = 4;

    prevButton_  .setBounds(r.removeFromLeft(btnW));    r.removeFromLeft(gap);
    nextButton_  .setBounds(r.removeFromRight(btnW));
    initButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    saveAsButton_.setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    saveButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    presetNameLabel_.setBounds(r);
}

void PresetBar::paint(juce::Graphics& g)
{
    g.setColour(StitcherLookAndFeel::Panel.brighter(0.05f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
}

void PresetBar::updateLabel()
{
    auto name = presetManager_.getCurrentPresetName();
    presetNameLabel_.setText(name.isNotEmpty() ? name : "(unsaved)",
                             juce::dontSendNotification);
}

void PresetBar::showSaveAsDialog()
{
    juce::AlertWindow::showInputBoxAsync(
        "Save Preset",
        "Enter a name for this preset:",
        presetManager_.getCurrentPresetName(),
        nullptr,
        [this](const juce::String& text) {
            if (text.trim().isNotEmpty()) {
                presetManager_.savePreset(text.trim());
                updateLabel();
            }
        });
}
```

- [ ] **Step 3: Build to verify PresetBar compiles**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
```

Expected: `Built target Stitcher_Standalone` with no errors.

- [ ] **Step 4: Commit**

```bash
git add Source/UI/PresetBar.h Source/UI/PresetBar.cpp
git commit -m "feat: add PresetBar component with save/load/prev/next/init buttons"
```

---

## Task 4: Wire PluginEditor (Timer + meters + PresetBar + layout)

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Update PluginEditor.h**

Replace the entire content of `Source/PluginEditor.h` with:

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel/StitcherLookAndFeel.h"
#include "PresetManager.h"
#include "UI/PresetBar.h"
#include "UI/FeatureMeter.h"
#include "UI/LevelMeter.h"

class StitcherEditor : public juce::AudioProcessorEditor,
                       private juce::Timer {
public:
    explicit StitcherEditor(StitcherProcessor&);
    ~StitcherEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    StitcherProcessor& audioProcessor;

    StitcherLookAndFeel lnf_;

    // Preset management — PresetManager must be declared before PresetBar
    PresetManager presetManager_;
    PresetBar     presetBar_;

    juce::GroupComponent concatGroup_, eqGroup_, reverbGroup_, outputGroup_;

    juce::Slider zcrSlider_, rmsSlider_, scSlider_, stSlider_;
    juce::Slider seekTimeSlider_, matchLenSlider_, randSlider_;
    juce::Slider gainCtrlSlider_, gainSrcSlider_;
    juce::ToggleButton freezeButton_;

    juce::Label zcrLabel_, rmsLabel_, scLabel_, stLabel_;
    juce::Label seekTimeLabel_, matchLenLabel_, randLabel_;
    juce::Label gainCtrlLabel_, gainSrcLabel_;

    juce::Slider eqLowSlider_, eqMidSlider_, eqHighSlider_;
    juce::Label  eqLowLabel_,  eqMidLabel_,  eqHighLabel_;

    juce::Slider reverbRoomSlider_, reverbDampSlider_, reverbWetSlider_;
    juce::Label  reverbRoomLabel_,  reverbDampLabel_,  reverbWetLabel_;

    juce::Slider gainOutSlider_, mixSlider_;
    juce::Label  gainOutLabel_,  mixLabel_;

    // Live meters
    FeatureMeter zcrMeter_, rmsMeter_, scMeter_, stMeter_;
    FeatureMeter corpusFillMeter_;
    LevelMeter   levelMeter_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> zcrAttach_, rmsAttach_, scAttach_, stAttach_;
    std::unique_ptr<SA> seekTimeAttach_, matchLenAttach_, randAttach_;
    std::unique_ptr<SA> gainCtrlAttach_, gainSrcAttach_;
    std::unique_ptr<SA> eqLowAttach_, eqMidAttach_, eqHighAttach_;
    std::unique_ptr<SA> reverbRoomAttach_, reverbDampAttach_, reverbWetAttach_;
    std::unique_ptr<SA> gainOutAttach_, mixAttach_;
    std::unique_ptr<BA> freezeAttach_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherEditor)
};
```

- [ ] **Step 2: Update PluginEditor.cpp constructor initializer list**

The constructor now needs to initialize `presetManager_` before `presetBar_`. Replace the constructor opening with:

```cpp
StitcherEditor::StitcherEditor(StitcherProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      presetManager_(p.getAPVTS()),
      presetBar_(presetManager_)
{
    setLookAndFeel(&lnf_);
    setSize(960, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1440, 840);
    auto& apvts = audioProcessor.getAPVTS();

    addAndMakeVisible(presetBar_);
```

The rest of the constructor body (group setup, initRotary calls, attachments) remains unchanged.

- [ ] **Step 3: Add meter visibility calls in constructor**

After the existing `addAndMakeVisible(freezeButton_)` line, add:

```cpp
    for (auto* m : { &zcrMeter_, &rmsMeter_, &scMeter_, &stMeter_,
                     &corpusFillMeter_ })
        addAndMakeVisible(m);
    addAndMakeVisible(levelMeter_);
```

At the end of the constructor body (after all attachments), add:

```cpp
    startTimerHz(30);
```

- [ ] **Step 4: Update destructor**

```cpp
StitcherEditor::~StitcherEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
```

- [ ] **Step 5: Add timerCallback implementation**

```cpp
void StitcherEditor::timerCallback()
{
    auto& proc = audioProcessor;

    auto& apvts = proc.getAPVTS();
    zcrMeter_.setLevel(proc.getLastCtrlZcr());
    zcrMeter_.setActive(apvts.getRawParameterValue(ParamIDs::zcrWeight)->load() > 0.f);

    rmsMeter_.setLevel(proc.getLastCtrlRms());
    rmsMeter_.setActive(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() > 0.f);

    scMeter_.setLevel(proc.getLastCtrlSc());
    scMeter_.setActive(apvts.getRawParameterValue(ParamIDs::scWeight)->load() > 0.f);

    stMeter_.setLevel(proc.getLastCtrlSt());
    stMeter_.setActive(apvts.getRawParameterValue(ParamIDs::stWeight)->load() > 0.f);

    corpusFillMeter_.setLevel(proc.getLastCorpusFill());
    corpusFillMeter_.setActive(true);

    levelMeter_.setLevels(proc.getLastOutPeakL(), proc.getLastOutPeakR());

    zcrMeter_.repaint();
    rmsMeter_.repaint();
    scMeter_.repaint();
    stMeter_.repaint();
    corpusFillMeter_.repaint();
    levelMeter_.repaint();
}
```

- [ ] **Step 6: Update resized() for presetBar + meters**

Replace the entire `StitcherEditor::resized()` with:

```cpp
void StitcherEditor::resized()
{
    const int margin  = 8;
    const int gap     = 6;
    const int headerH = 18;
    const int labelH  = 14;
    const int meterH  = 6;
    const int presetBarH = 36;

    auto area = getLocalBounds().reduced(margin);

    // Preset bar at top
    presetBar_.setBounds(area.removeFromTop(presetBarH));
    area.removeFromTop(gap);

    // Section proportions: Concat 42%, EQ 15%, Reverb 15%, Output remainder
    const int concatW = static_cast<int>(area.getWidth() * 0.42f);
    const int eqW     = static_cast<int>(area.getWidth() * 0.15f);
    const int revW    = static_cast<int>(area.getWidth() * 0.15f);

    auto concatArea = area.removeFromLeft(concatW);  area.removeFromLeft(gap);
    auto eqArea     = area.removeFromLeft(eqW);      area.removeFromLeft(gap);
    auto reverbArea = area.removeFromLeft(revW);     area.removeFromLeft(gap);
    auto outputArea = area;

    concatGroup_.setBounds(concatArea);
    eqGroup_    .setBounds(eqArea);
    reverbGroup_.setBounds(reverbArea);
    outputGroup_.setBounds(outputArea);

    // ── Concatenator ──────────────────────────────────────────────────────
    {
        auto r = concatArea.reduced(6).withTrimmedTop(headerH);

        // Row 1: zcr, rms, sc, st — each has label + meter + slider
        const int kw1 = std::min(90, (r.getWidth() - 9) / 4);
        const int kh1 = kw1 + labelH + meterH;
        auto row1 = r.removeFromTop(kh1);
        struct WK { juce::Slider* s; juce::Label* l; FeatureMeter* m; };
        for (auto wk : std::initializer_list<WK>{
                {&zcrSlider_, &zcrLabel_, &zcrMeter_},
                {&rmsSlider_, &rmsLabel_, &rmsMeter_},
                {&scSlider_,  &scLabel_,  &scMeter_},
                {&stSlider_,  &stLabel_,  &stMeter_} }) {
            auto cell = row1.removeFromLeft(kw1); row1.removeFromLeft(3);
            wk.l->setBounds(cell.removeFromTop(labelH));
            wk.m->setBounds(cell.removeFromTop(meterH));
            wk.s->setBounds(cell);
        }
        r.removeFromTop(gap);

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
        r.removeFromTop(gap);

        // Corpus fill indicator
        corpusFillMeter_.setBounds(r.removeFromTop(meterH));
        r.removeFromTop(gap);

        // Row 3: gainCtrl, gainSrc + Freeze
        const int kw3 = std::min(90, (r.getWidth() - 6) / 3);
        const int kh3 = kw3 + labelH;
        auto row3 = r.removeFromTop(kh3);
        for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&gainCtrlSlider_, &gainCtrlLabel_},
                {&gainSrcSlider_,  &gainSrcLabel_} }) {
            auto cell = row3.removeFromLeft(kw3); row3.removeFromLeft(3);
            pair.second->setBounds(cell.removeFromTop(labelH));
            pair.first->setBounds(cell);
        }
        row3.removeFromLeft(8);
        freezeButton_.setBounds(row3.removeFromLeft(kw3 + 12)
                                    .withSizeKeepingCentre(kw3 + 12, 28));
    }

    // ── EQ ────────────────────────────────────────────────────────────────
    {
        auto r = eqArea.reduced(6).withTrimmedTop(headerH);
        const int kw = std::min(90, r.getWidth());
        const int kh = kw + labelH;
        for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&eqLowSlider_, &eqLowLabel_},
                {&eqMidSlider_, &eqMidLabel_},
                {&eqHighSlider_,&eqHighLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            pair.second->setBounds(cell.removeFromTop(labelH));
            pair.first->setBounds(cell);
        }
    }

    // ── Reverb ────────────────────────────────────────────────────────────
    {
        auto r = reverbArea.reduced(6).withTrimmedTop(headerH);
        const int kw = std::min(90, r.getWidth());
        const int kh = kw + labelH;
        for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&reverbRoomSlider_, &reverbRoomLabel_},
                {&reverbDampSlider_, &reverbDampLabel_},
                {&reverbWetSlider_,  &reverbWetLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            pair.second->setBounds(cell.removeFromTop(labelH));
            pair.first->setBounds(cell);
        }
    }

    // ── Output ────────────────────────────────────────────────────────────
    {
        auto r = outputArea.reduced(6).withTrimmedTop(headerH);
        // Level meter: narrow column on right
        auto meterCol = r.removeFromRight(16);
        r.removeFromRight(4);
        levelMeter_.setBounds(meterCol);

        const int kw = std::min(90, r.getWidth());
        const int kh = kw + labelH;
        for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&gainOutSlider_, &gainOutLabel_},
                {&mixSlider_,     &mixLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            pair.second->setBounds(cell.removeFromTop(labelH));
            pair.first->setBounds(cell);
        }
    }
}
```

- [ ] **Step 7: Build and verify**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
./build/tests/StitcherTests
```

Expected: `Built target Stitcher_Standalone`. All tests pass.

- [ ] **Step 8: Launch standalone for visual verification**

```bash
open build/Stitcher_artefacts/Release/Standalone/Stitcher.app
```

Manually verify:
- Preset bar at top with `(unsaved)` label, `<`, `>`, Save, Save As, Init buttons
- Feature meters (thin amber bars) below each of ZCR, RMS, SC, ST labels
- Corpus fill bar between row2 and row3 in Concatenator section
- Stereo level meter on right side of Output section
- All existing knobs still functional and properly sized

- [ ] **Step 9: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "feat: wire PresetBar, feature meters, corpus fill, and level meter into editor"
```

---

## Task 5: Build, verify, install, commit

**Files:** (no new code — verification + install only)

- [ ] **Step 1: Full build all targets**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
```

Expected: `Built target Stitcher_Standalone` with no errors.

- [ ] **Step 2: Run full test suite**

```bash
./build/tests/StitcherTests
```

Expected: All tests passed (35 test cases: 31 original + 4 PresetManager).

- [ ] **Step 3: Install plugins**

```bash
cp -r build/Stitcher_artefacts/Release/VST3/Stitcher.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -r build/Stitcher_artefacts/Release/AU/Stitcher.component ~/Library/Audio/Plug-Ins/Components/
```

- [ ] **Step 4: Open Ableton test project**

```bash
open ~/Downloads/Ableton-Projects/stitch\ Project/stitch.als
```

Verify: preset bar visible, meters animate when audio plays, presets save/load to `~/Library/Application Support/Stitcher/Presets/User/`.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "feat: Phase 2 complete — preset bar, live feature meters, corpus fill, level meter"
```

---

## Self-Review

**Spec coverage:**
- ✅ Preset Bar: name label, prev/next arrows, Save, Save As, Init — all present in PresetBar
- ✅ Preset serialization: APVTS XML to `~/Library/Application Support/Stitcher/Presets/User/`
- ✅ 4 live ctrl feature meters (ZCR/RMS/SC/ST), dim when weight=0 — FeatureMeter + timerCallback
- ✅ Corpus fill indicator — FeatureMeter reused, driven by `lastCorpusFill_` atomic
- ✅ Output level meter — LevelMeter in output section
- ✅ Audio-thread atomics (7 fields) + message-thread 30 Hz timer — Task 0 + Task 4
- ✅ PresetManager unit test: 4 tests (empty list, save/load roundtrip, init, prev/next cycling)
- ✅ All existing 31 tests continue to pass

**Placeholder scan:** No TBD/TODO/similar. All code blocks complete.

**Type consistency:**
- `PresetManager` constructor: `(juce::AudioProcessorValueTreeState&, juce::File)` — consistent between .h and .cpp and test
- `FeatureMeter::setLevel(float)` / `setActive(bool)` — consistent in .h, .cpp, and timerCallback
- `LevelMeter::setLevels(float, float)` — consistent throughout
- `PresetBar(PresetManager&)` — consistent between .h, .cpp, and editor initialization
- `corpusMaxFrames_` — declared in PluginProcessor.h, set in prepareToPlay, used in processBlock — consistent
