# Phase 1 — Visual Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the default JUCE grey UI with a custom `StitcherLookAndFeel` (dark background, amber accent, value-arc rotaries, embedded Inter font) and make the editor window resizable with proportional knob layout.

**Architecture:** A new `StitcherLookAndFeel` class (subclassing `juce::LookAndFeel_V4`) owns all custom drawing. It is instantiated as a member of `StitcherEditor`, set via `setLookAndFeel()`, and cleared in the destructor. Inter-Regular.ttf is embedded via `juce_add_binary_data` and loaded once inside the LookAndFeel constructor. The editor's `resized()` is rewritten to compute knob sizes proportionally from the available window area, enabling `setResizable(true, true)` with a minimum and maximum constraint.

**Tech Stack:** JUCE 8.x, C++17, `juce::LookAndFeel_V4`, `juce_add_binary_data`, Inter font (SIL OFL 1.1).

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `Source/LookAndFeel/StitcherLookAndFeel.h` | Create | Class declaration: palette constants, overrides |
| `Source/LookAndFeel/StitcherLookAndFeel.cpp` | Create | Custom rotary, group, toggle, label drawing; font loading |
| `Source/PluginEditor.h` | Modify | Add `StitcherLookAndFeel lnf_` member; add destructor `= default` → explicit clear |
| `Source/PluginEditor.cpp` | Modify | Use LookAndFeel, `setResizable`, proportional `resized()` |
| `Source/Assets/Fonts/Inter-Regular.ttf` | Add | Embedded font binary (OFL 1.1) |
| `Source/Assets/Fonts/LICENSE.txt` | Add | OFL 1.1 license for Inter |
| `CMakeLists.txt` | Modify | `juce_add_binary_data` target + link; add LookAndFeel sources to `SourceFiles` |

---

## Task 0: Create branch and directory structure

**Files:**
- (git)
- Create: `Source/LookAndFeel/` (directory)
- Create: `Source/Assets/Fonts/` (directory)

- [ ] **Step 1: Create the feature branch off main**

```bash
git checkout main
git checkout -b ui-presets
```

Expected: `Switched to a new branch 'ui-presets'`

- [ ] **Step 2: Create the new directories**

```bash
mkdir -p /Users/zeyuyang42/Workspace/stitcher-010426/stitcher/Source/LookAndFeel
mkdir -p /Users/zeyuyang42/Workspace/stitcher-010426/stitcher/Source/Assets/Fonts
```

---

## Task 1: Embed Inter font via BinaryData

**Files:**
- Create: `Source/Assets/Fonts/Inter-Regular.ttf`
- Create: `Source/Assets/Fonts/LICENSE.txt`
- Modify: `CMakeLists.txt`

The Inter font is SIL OFL 1.1 licensed. We download it and embed it so the plugin doesn't depend on the system font.

- [ ] **Step 1: Download Inter-Regular.ttf**

```bash
cd /Users/zeyuyang42/Workspace/stitcher-010426/stitcher/Source/Assets/Fonts
curl -L "https://github.com/rsms/inter/raw/master/docs/font-files/Inter-Regular.otf" -o Inter-Regular.otf
# Convert otf to ttf isn't needed — JUCE can load .otf files. Rename for clarity:
cp Inter-Regular.otf Inter-Regular.ttf
```

If curl fails (network not available), obtain `Inter-Regular.ttf` (or `.otf`) from https://github.com/rsms/inter/releases and place it at `Source/Assets/Fonts/Inter-Regular.ttf`.

- [ ] **Step 2: Add the OFL license**

Create `Source/Assets/Fonts/LICENSE.txt` with the following content (OFL 1.1 short form):

```
Copyright 2020 The Inter Project Authors (https://github.com/rsms/inter)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
http://scripts.sil.org/OFL
```

- [ ] **Step 3: Add juce_add_binary_data to CMakeLists.txt**

In `CMakeLists.txt`, after the `juce_add_plugin(...)` block and before the pluginval line, add:

```cmake
juce_add_binary_data(StitcherBinaryData
    SOURCES
        Source/Assets/Fonts/Inter-Regular.ttf
)
```

Then, in the `target_link_libraries(${PROJECT_NAME} ...)` block, add `StitcherBinaryData` to the `PRIVATE` section:

```cmake
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_gui_extra
        StitcherBinaryData
    PUBLIC
        ...rest unchanged...
```

Also add the two new source files to `SourceFiles`:

```cmake
set(SourceFiles
    Source/LookAndFeel/StitcherLookAndFeel.h
    Source/LookAndFeel/StitcherLookAndFeel.cpp
    Source/PluginEditor.cpp
    ...rest unchanged...
)
```

- [ ] **Step 4: Verify CMake configures cleanly**

```bash
cd /Users/zeyuyang42/Workspace/stitcher-010426/stitcher
cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "error:|warning:|-- "
```

Expected: No errors. `StitcherBinaryData` target should appear in the configure output.

---

## Task 2: StitcherLookAndFeel — palette and rotary knob

**Files:**
- Create: `Source/LookAndFeel/StitcherLookAndFeel.h`
- Create: `Source/LookAndFeel/StitcherLookAndFeel.cpp`

- [ ] **Step 1: Write `StitcherLookAndFeel.h`**

```cpp
#pragma once
#include <JuceHeader.h>

class StitcherLookAndFeel : public juce::LookAndFeel_V4 {
public:
    StitcherLookAndFeel();
    ~StitcherLookAndFeel() override = default;

    // Palette (ARGB hex constants)
    static const juce::Colour Background;
    static const juce::Colour Panel;
    static const juce::Colour Accent;
    static const juce::Colour AccentDim;
    static const juce::Colour TextBright;
    static const juce::Colour TextDim;
    static const juce::Colour Track;

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawGroupComponentOutline(juce::Graphics&, int w, int h,
                                   const juce::String& text,
                                   const juce::Justification&,
                                   juce::GroupComponent&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;

private:
    juce::Typeface::Ptr interTypeface_;
    juce::Font interFont_;
};
```

- [ ] **Step 2: Write `StitcherLookAndFeel.cpp` — palette constants and constructor**

```cpp
#include "StitcherLookAndFeel.h"
#include <BinaryData.h>

const juce::Colour StitcherLookAndFeel::Background { 0xFF1A1A1F };
const juce::Colour StitcherLookAndFeel::Panel       { 0xFF252530 };
const juce::Colour StitcherLookAndFeel::Accent      { 0xFFFFA84C };
const juce::Colour StitcherLookAndFeel::AccentDim   { 0xFF7A5025 };
const juce::Colour StitcherLookAndFeel::TextBright  { 0xFFE8E4DC };
const juce::Colour StitcherLookAndFeel::TextDim     { 0xFF8A8680 };
const juce::Colour StitcherLookAndFeel::Track       { 0xFF3A3A45 };

StitcherLookAndFeel::StitcherLookAndFeel()
{
    interTypeface_ = juce::Typeface::createSystemTypefaceFor(
        BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    interFont_ = juce::Font(juce::FontOptions(interTypeface_).withHeight(12.f));

    // Map JUCE colour IDs to our palette
    setColour(juce::ResizableWindow::backgroundColourId,        Background);
    setColour(juce::GroupComponent::outlineColourId,            Panel.brighter(0.15f));
    setColour(juce::GroupComponent::textColourId,               TextDim);
    setColour(juce::Slider::rotarySliderFillColourId,           Accent);
    setColour(juce::Slider::rotarySliderOutlineColourId,        Track);
    setColour(juce::Slider::thumbColourId,                      TextBright);
    setColour(juce::Slider::textBoxTextColourId,                TextDim);
    setColour(juce::Slider::textBoxBackgroundColourId,          juce::Colour(0x00000000));
    setColour(juce::Slider::textBoxOutlineColourId,             juce::Colour(0x00000000));
    setColour(juce::Label::textColourId,                        TextDim);
    setColour(juce::ToggleButton::textColourId,                 TextBright);
    setColour(juce::PopupMenu::backgroundColourId,              Panel);
    setColour(juce::PopupMenu::textColourId,                    TextBright);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,   Accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId,         Accent);
}
```

- [ ] **Step 3: Write `drawRotarySlider`**

Append to `StitcherLookAndFeel.cpp`:

```cpp
void StitcherLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& /*slider*/)
{
    const float margin = 6.f;
    auto bounds = juce::Rectangle<float>(
        static_cast<float>(x) + margin,
        static_cast<float>(y) + margin,
        static_cast<float>(width)  - margin * 2.f,
        static_cast<float>(height) - margin * 2.f);

    const float radius   = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX  = bounds.getCentreX();
    const float centreY  = bounds.getCentreY();
    const float angle    = rotaryStartAngle + sliderPosProportional
                           * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 3.f;

    // Background track arc
    juce::Path track;
    track.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                        0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Track);
    g.strokePath(track, juce::PathStrokeType(
        3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Accent value arc (from start to current angle)
    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                           0.f, rotaryStartAngle, angle, true);
    g.setColour(Accent);
    g.strokePath(valueArc, juce::PathStrokeType(
        3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body
    const float knobR = radius - 9.f;
    g.setColour(Panel);
    g.fillEllipse(centreX - knobR, centreY - knobR, knobR * 2.f, knobR * 2.f);

    // Subtle rim
    g.setColour(Track.brighter(0.3f));
    g.drawEllipse(centreX - knobR, centreY - knobR, knobR * 2.f, knobR * 2.f, 1.f);

    // Indicator line from centre to edge
    const float pointerLen = knobR * 0.65f;
    const float px = centreX + std::sin(angle) * pointerLen;
    const float py = centreY - std::cos(angle) * pointerLen;
    g.setColour(TextBright);
    g.drawLine(centreX, centreY, px, py, 2.f);

    // Dot at pointer tip
    g.setColour(Accent);
    g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);
}
```

- [ ] **Step 4: Build to verify rotary compiles**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Linking|Built target"
```

Expected: `Built target Stitcher_Standalone` with no errors.

---

## Task 3: StitcherLookAndFeel — group, toggle, label overrides

**Files:**
- Modify: `Source/LookAndFeel/StitcherLookAndFeel.cpp` (append)

- [ ] **Step 1: Write `drawGroupComponentOutline`**

Append to `StitcherLookAndFeel.cpp`:

```cpp
void StitcherLookAndFeel::drawGroupComponentOutline(juce::Graphics& g,
    int width, int height, const juce::String& text,
    const juce::Justification& position, juce::GroupComponent& /*group*/)
{
    const float cornerSize = 6.f;
    const float lineThickness = 1.f;

    auto bounds = juce::Rectangle<float>(0.f, 8.f,
        static_cast<float>(width), static_cast<float>(height) - 8.f);

    // Panel fill
    g.setColour(Panel);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Border
    g.setColour(Track.brighter(0.2f));
    g.drawRoundedRectangle(bounds, cornerSize, lineThickness);

    // Section label
    if (text.isNotEmpty()) {
        const float textH = 14.f;
        auto textBounds = bounds.withHeight(textH).translated(0.f, -7.f);

        g.setColour(Panel);
        g.fillRect(textBounds.expanded(4.f, 0.f));

        g.setColour(TextDim);
        g.setFont(interFont_.withHeight(10.f).boldened());
        g.drawText(text.toUpperCase(), textBounds.toNearestInt(),
                   juce::Justification::centred, false);
    }
}
```

- [ ] **Step 2: Write `drawToggleButton`**

Append to `StitcherLookAndFeel.cpp`:

```cpp
void StitcherLookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& button, bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.f);
    const float cornerR = bounds.getHeight() * 0.5f;
    const bool on = button.getToggleState();

    // Background pill
    g.setColour(on ? Accent : Track);
    if (isMouseOver || isButtonDown)
        g.setColour(on ? Accent.brighter(0.15f) : Track.brighter(0.15f));
    g.fillRoundedRectangle(bounds, cornerR);

    // Border
    g.setColour(on ? Accent.brighter(0.3f) : Track.brighter(0.3f));
    g.drawRoundedRectangle(bounds, cornerR, 1.f);

    // Label text
    g.setColour(on ? Background : TextBright);
    g.setFont(interFont_.withHeight(11.f));
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, false);
}
```

- [ ] **Step 3: Write font overrides**

Append to `StitcherLookAndFeel.cpp`:

```cpp
juce::Font StitcherLookAndFeel::getLabelFont(juce::Label& /*label*/)
{
    return interFont_;
}

void StitcherLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(),
                         label.getLocalBounds(),
                         label.getJustificationType(),
                         2, 1.f);
    }
}

juce::Font StitcherLookAndFeel::getPopupMenuFont()
{
    return interFont_.withHeight(13.f);
}
```

- [ ] **Step 4: Build to check compilation**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|Built target"
```

Expected: `Built target Stitcher_Standalone` with no errors.

---

## Task 4: Update PluginEditor — wire LookAndFeel, resizable window, proportional layout

**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

- [ ] **Step 1: Update `PluginEditor.h` to add LookAndFeel member and explicit destructor**

Replace the full content of `Source/PluginEditor.h` with:

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel/StitcherLookAndFeel.h"

class StitcherEditor : public juce::AudioProcessorEditor {
public:
    explicit StitcherEditor(StitcherProcessor&);
    ~StitcherEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    StitcherProcessor& audioProcessor;

    StitcherLookAndFeel lnf_;

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

- [ ] **Step 2: Update the constructor in `PluginEditor.cpp` to install LookAndFeel, resize + make resizable**

Replace lines 28–89 of `Source/PluginEditor.cpp` (the constructor) with:

```cpp
StitcherEditor::StitcherEditor(StitcherProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lnf_);

    setSize(960, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1440, 840);

    auto& apvts = audioProcessor.getAPVTS();

    // ── Section group labels ─────────────────────────────────────────────
    concatGroup_.setText("Concatenator");
    eqGroup_    .setText("EQ");
    reverbGroup_.setText("Reverb");
    outputGroup_.setText("Output");
    for (auto* g : { &concatGroup_, &eqGroup_, &reverbGroup_, &outputGroup_ })
        addAndMakeVisible(g);

    // ── Concatenator ─────────────────────────────────────────────────────
    initRotary(zcrSlider_,      zcrLabel_,      "ZCR",     *this);
    initRotary(rmsSlider_,      rmsLabel_,      "RMS",     *this);
    initRotary(scSlider_,       scLabel_,       "S.Cent",  *this);
    initRotary(stSlider_,       stLabel_,       "S.Tilt",  *this);
    initRotary(seekTimeSlider_, seekTimeLabel_, "Seek",    *this);
    initRotary(matchLenSlider_, matchLenLabel_, "Len",     *this);
    initRotary(randSlider_,     randLabel_,     "Rand",    *this);
    initRotary(gainCtrlSlider_, gainCtrlLabel_, "Ctrl",    *this);
    initRotary(gainSrcSlider_,  gainSrcLabel_,  "Src",     *this);

    freezeButton_.setButtonText("Freeze");
    addAndMakeVisible(freezeButton_);

    // ── EQ ───────────────────────────────────────────────────────────────
    initRotary(eqLowSlider_,  eqLowLabel_,  "Low",  *this);
    initRotary(eqMidSlider_,  eqMidLabel_,  "Mid",  *this);
    initRotary(eqHighSlider_, eqHighLabel_, "High", *this);

    // ── Reverb ───────────────────────────────────────────────────────────
    initRotary(reverbRoomSlider_, reverbRoomLabel_, "Room", *this);
    initRotary(reverbDampSlider_, reverbDampLabel_, "Damp", *this);
    initRotary(reverbWetSlider_,  reverbWetLabel_,  "Wet",  *this);

    // ── Output ───────────────────────────────────────────────────────────
    initRotary(gainOutSlider_, gainOutLabel_, "Gain", *this);
    initRotary(mixSlider_,     mixLabel_,     "Mix",  *this);

    // ── Attachments ──────────────────────────────────────────────────────
    zcrAttach_        = std::make_unique<SA>(apvts, ParamIDs::zcrWeight,  zcrSlider_);
    rmsAttach_        = std::make_unique<SA>(apvts, ParamIDs::rmsWeight,  rmsSlider_);
    scAttach_         = std::make_unique<SA>(apvts, ParamIDs::scWeight,   scSlider_);
    stAttach_         = std::make_unique<SA>(apvts, ParamIDs::stWeight,   stSlider_);
    seekTimeAttach_   = std::make_unique<SA>(apvts, ParamIDs::seekTime,   seekTimeSlider_);
    matchLenAttach_   = std::make_unique<SA>(apvts, ParamIDs::matchLen,   matchLenSlider_);
    randAttach_       = std::make_unique<SA>(apvts, ParamIDs::rand_,      randSlider_);
    gainCtrlAttach_   = std::make_unique<SA>(apvts, ParamIDs::gainCtrl,   gainCtrlSlider_);
    gainSrcAttach_    = std::make_unique<SA>(apvts, ParamIDs::gainSrc,    gainSrcSlider_);
    eqLowAttach_      = std::make_unique<SA>(apvts, ParamIDs::eqLow,      eqLowSlider_);
    eqMidAttach_      = std::make_unique<SA>(apvts, ParamIDs::eqMid,      eqMidSlider_);
    eqHighAttach_     = std::make_unique<SA>(apvts, ParamIDs::eqHigh,     eqHighSlider_);
    reverbRoomAttach_ = std::make_unique<SA>(apvts, ParamIDs::reverbRoom, reverbRoomSlider_);
    reverbDampAttach_ = std::make_unique<SA>(apvts, ParamIDs::reverbDamp, reverbDampSlider_);
    reverbWetAttach_  = std::make_unique<SA>(apvts, ParamIDs::reverbWet,  reverbWetSlider_);
    gainOutAttach_    = std::make_unique<SA>(apvts, ParamIDs::gainOut,    gainOutSlider_);
    mixAttach_        = std::make_unique<SA>(apvts, ParamIDs::mix,        mixSlider_);
    freezeAttach_     = std::make_unique<BA>(apvts, ParamIDs::freeze,     freezeButton_);
}
```

- [ ] **Step 3: Add the destructor that clears the LookAndFeel**

After the constructor closing brace in `PluginEditor.cpp`, add:

```cpp
StitcherEditor::~StitcherEditor()
{
    setLookAndFeel(nullptr);
}
```

- [ ] **Step 4: Update `paint()` to fill with our background colour**

Replace the existing `paint()` in `PluginEditor.cpp` with:

```cpp
void StitcherEditor::paint(juce::Graphics& g)
{
    g.fillAll(StitcherLookAndFeel::Background);
}
```

- [ ] **Step 5: Rewrite `resized()` with proportional layout**

Replace the existing `resized()` in `PluginEditor.cpp` with:

```cpp
void StitcherEditor::resized()
{
    const int W = getWidth();
    const int H = getHeight();
    const int margin  = 8;
    const int gap     = 6;
    const int headerH = 18;
    const int labelH  = 14;

    auto area = getLocalBounds().reduced(margin);

    // Section proportions: Concat 42%, EQ 15%, Reverb 15%, Output remainder
    const int concatW = static_cast<int>(area.getWidth() * 0.42f);
    const int eqW     = static_cast<int>(area.getWidth() * 0.15f);
    const int revW    = static_cast<int>(area.getWidth() * 0.15f);

    auto concatArea = area.removeFromLeft(concatW);   area.removeFromLeft(gap);
    auto eqArea     = area.removeFromLeft(eqW);       area.removeFromLeft(gap);
    auto reverbArea = area.removeFromLeft(revW);      area.removeFromLeft(gap);
    auto outputArea = area;

    concatGroup_.setBounds(concatArea);
    eqGroup_    .setBounds(eqArea);
    reverbGroup_.setBounds(reverbArea);
    outputGroup_.setBounds(outputArea);

    // Helper: lay out a row of N knobs across a rectangle
    // knobH = labelH + square knob (= knobW)
    // returns the kw (knob width) actually used
    auto layoutRow = [&](std::vector<std::pair<juce::Slider*, juce::Label*>> knobs,
                         juce::Rectangle<int> rowBounds, int numKnobs) -> int
    {
        const int kw = std::min(90, (rowBounds.getWidth() - (numKnobs - 1) * 3) / numKnobs);
        for (auto& [s, l] : knobs) {
            auto cell = rowBounds.removeFromLeft(kw);
            rowBounds.removeFromLeft(3);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
        return kw;
    };

    // ── Concatenator ──────────────────────────────────────────────────────
    {
        auto r = concatArea.reduced(6).withTrimmedTop(headerH);

        // Row 1: zcr, rms, sc, st
        const int kw1 = std::min(90, (r.getWidth() - 9) / 4);
        const int kh1 = kw1 + labelH;
        auto row1 = r.removeFromTop(kh1);
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&zcrSlider_, &zcrLabel_}, {&rmsSlider_, &rmsLabel_},
                {&scSlider_,  &scLabel_},  {&stSlider_,  &stLabel_} }) {
            auto cell = row1.removeFromLeft(kw1); row1.removeFromLeft(3);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
        r.removeFromTop(gap);

        // Row 2: seek, matchLen, rand
        const int kw2 = std::min(90, (r.getWidth() - 6) / 3);
        const int kh2 = kw2 + labelH;
        auto row2 = r.removeFromTop(kh2);
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&seekTimeSlider_, &seekTimeLabel_},
                {&matchLenSlider_, &matchLenLabel_},
                {&randSlider_,     &randLabel_} }) {
            auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
        r.removeFromTop(gap);

        // Row 3: gainCtrl, gainSrc, Freeze button
        const int kw3 = std::min(90, (r.getWidth() - 6) / 3);
        const int kh3 = kw3 + labelH;
        auto row3 = r.removeFromTop(kh3);
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&gainCtrlSlider_, &gainCtrlLabel_},
                {&gainSrcSlider_,  &gainSrcLabel_} }) {
            auto cell = row3.removeFromLeft(kw3); row3.removeFromLeft(3);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
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
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&eqLowSlider_, &eqLowLabel_},
                {&eqMidSlider_, &eqMidLabel_},
                {&eqHighSlider_,&eqHighLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
    }

    // ── Reverb ────────────────────────────────────────────────────────────
    {
        auto r = reverbArea.reduced(6).withTrimmedTop(headerH);
        const int kw = std::min(90, r.getWidth());
        const int kh = kw + labelH;
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&reverbRoomSlider_, &reverbRoomLabel_},
                {&reverbDampSlider_, &reverbDampLabel_},
                {&reverbWetSlider_,  &reverbWetLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
    }

    // ── Output ────────────────────────────────────────────────────────────
    {
        auto r = outputArea.reduced(6).withTrimmedTop(headerH);
        const int kw = std::min(90, r.getWidth());
        const int kh = kw + labelH;
        for (auto& [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&gainOutSlider_, &gainOutLabel_},
                {&mixSlider_,     &mixLabel_} }) {
            auto cell = r.removeFromTop(kh); r.removeFromTop(gap);
            l->setBounds(cell.removeFromTop(labelH));
            s->setBounds(cell);
        }
    }
}
```

---

## Task 5: Build, launch, verify, commit

**Files:** (no new code — verification only)

- [ ] **Step 1: Full build**

```bash
cmake --build build --target Stitcher_Standalone 2>&1 | grep -E "error:|warning:|Built target"
```

Expected: `Built target Stitcher_Standalone`. No errors.

- [ ] **Step 2: Launch standalone and visual inspect**

```bash
open build/Stitcher_artefacts/Release/Standalone/Stitcher.app
```

Verify manually:
- Background is dark (#1A1A1F)
- Knobs have amber value arcs
- Section labels visible but subtle (textDim colour)
- Freeze button is pill-shaped, filled amber when toggled on
- All 17 sliders + Freeze button are clickable and move
- Window resizes — knobs reflow proportionally, no overlap at 720×420

- [ ] **Step 3: Run tests (no DSP changes — all should pass)**

```bash
./build/tests/StitcherTests
```

Expected: `All tests passed (226 assertions in 31 test cases)`

- [ ] **Step 4: pluginval**

```bash
cmake --build build --target Stitcher_pluginval_cli 2>&1 | tail -5
```

Expected: `SUCCESS`

- [ ] **Step 5: Install and open Ableton test project**

```bash
cp -r build/Stitcher_artefacts/Release/VST3/Stitcher.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -r build/Stitcher_artefacts/Release/AU/Stitcher.component ~/Library/Audio/Plug-Ins/Components/
open ~/Downloads/Ableton-Projects/stitch\ Project/stitch.als
```

Verify the new visual identity in Ableton at default zoom.

- [ ] **Step 6: Commit**

```bash
git add Source/LookAndFeel/ Source/Assets/Fonts/ Source/PluginEditor.h Source/PluginEditor.cpp CMakeLists.txt
git commit -m "feat: custom LookAndFeel — dark palette, amber value-arc rotaries, Inter font, resizable window"
```

---

## Self-Review

**Spec coverage:**
- ✅ Custom `LookAndFeel_V4` subclass with palette constants
- ✅ `drawRotarySlider`: dark body, amber value arc, indicator dot
- ✅ `drawGroupComponentOutline`: panel fill, subtle border, dimmed uppercase header
- ✅ `drawToggleButton`: pill-shaped, amber fill when on
- ✅ Font overrides: embedded Inter loaded from BinaryData
- ✅ Color IDs set in constructor for Slider text box, Labels, PopupMenu
- ✅ Window resizable (720×420 min, 1440×840 max)
- ✅ Proportional `resized()` — knob size derived from available area, capped at 90 px
- ✅ `~StitcherEditor()` clears LookAndFeel to avoid dangling pointer on shutdown
- ✅ `juce_add_binary_data` CMake target for Inter + wired to plugin library
- ✅ OFL license file bundled

**Placeholder scan:** No TBD/TODO/similar. All code blocks are complete.

**Type consistency:** `StitcherLookAndFeel`, `lnf_`, colour constants (`Background`, `Panel`, `Accent`, `Track`, `TextBright`, `TextDim`, `AccentDim`) used consistently across tasks 2, 3, 4.
