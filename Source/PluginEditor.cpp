#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "FactoryPresets.h"

namespace {

void initRotary(juce::Slider& s, juce::Label& l, const juce::String& name,
                juce::Component& parent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
    parent.addAndMakeVisible(s);
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    parent.addAndMakeVisible(l);
}

} // namespace

StitcherEditor::StitcherEditor(StitcherProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      presetManager_(p.getAPVTS(), PresetManager::getUserPresetsDir(), makeFactoryPresets()),
      presetBar_(presetManager_)
{
    setLookAndFeel(&lnf_);

    // Preset bar
    addAndMakeVisible(presetBar_);
    presetBar_.onCaptureSlot = [this](int slot) {
        abSlots_[slot] = audioProcessor.getAPVTS().copyState();
    };
    presetBar_.onLoadSlot = [this](int slot) {
        if (abSlots_[slot].isValid())
            audioProcessor.getAPVTS().replaceState(abSlots_[slot].createCopy());
    };

    // Settings (gear) button
    settingsButton_.setButtonText(juce::CharPointer_UTF8("\xe2\x9a\x99"));  // ⚙
    addAndMakeVisible(settingsButton_);
    settingsButton_.onClick = [this] {
        auto content = std::make_unique<SettingsPopover>(audioProcessor.getAPVTS());
        juce::CallOutBox::launchAsynchronously(
            std::move(content),
            settingsButton_.getScreenBounds(),
            nullptr);
    };

    setSize(880, 520);
    setResizable(true, true);
    setResizeLimits(660, 390, 1320, 780);

    auto& apvts = audioProcessor.getAPVTS();

    // Center hero
    morphPad_.setParams(
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::zcrWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::rmsWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::scWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::stWeight)));
    addAndMakeVisible(morphPad_);
    addAndMakeVisible(matchViz_);

    // Left column
    initRotary(randSlider_,  randLabel_,  "Rand",  *this);
    initRotary(xfadeSlider_, xfadeLabel_, "Xfade", *this);
    freezeButton_.setButtonText("Freeze");
    addAndMakeVisible(freezeButton_);

    // Right column
    initRotary(tiltSlider_,      tiltLabel_,      "Tilt",   *this);
    initRotary(spaceSlider_,     spaceLabel_,     "Space",  *this);
    initRotary(reverbWetSlider_, reverbWetLabel_, "Wet",    *this);
    initRotary(mixSlider_,       mixLabel_,       "Mix",    *this);
    initRotary(gainOutSlider_,   gainOutLabel_,   "Output", *this);

    addAndMakeVisible(levelMeter_);

    // Attachments
    randAttach_       = std::make_unique<SA>(apvts, ParamIDs::rand_,      randSlider_);
    xfadeAttach_      = std::make_unique<SA>(apvts, ParamIDs::xfade,      xfadeSlider_);
    freezeAttach_     = std::make_unique<BA>(apvts, ParamIDs::freeze,     freezeButton_);
    tiltAttach_       = std::make_unique<SA>(apvts, ParamIDs::eqTilt,     tiltSlider_);
    spaceAttach_      = std::make_unique<SA>(apvts, ParamIDs::reverbSpace, spaceSlider_);
    reverbWetAttach_  = std::make_unique<SA>(apvts, ParamIDs::reverbWet,   reverbWetSlider_);
    mixAttach_        = std::make_unique<SA>(apvts, ParamIDs::mix,         mixSlider_);
    gainOutAttach_    = std::make_unique<SA>(apvts, ParamIDs::gainOut,     gainOutSlider_);

    // MIDI Learn: only main-face sliders
    sliderToParam_ = {
        { &randSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::rand_))       },
        { &xfadeSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::xfade))       },
        { &tiltSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::eqTilt))      },
        { &spaceSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbSpace)) },
        { &reverbWetSlider_, dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbWet))   },
        { &mixSlider_,       dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::mix))         },
        { &gainOutSlider_,   dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainOut))     },
    };
    for (auto& [slider, param] : sliderToParam_)
        slider->addMouseListener(this, false);

    startTimerHz(30);
}

StitcherEditor::~StitcherEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void StitcherEditor::timerCallback()
{
    audioProcessor.getMidiLearn().processCapture();

    auto& proc = audioProcessor;
    morphPad_.setLiveLevels(proc.getLastCtrlZcr(), proc.getLastCtrlRms(),
                            proc.getLastCtrlSc(),  proc.getLastCtrlSt());
    morphPad_.repaint();

    matchViz_.tick(proc.getLastCtrlRms(),
                   proc.getLastMatchedIndex(),
                   proc.getLastCorpusFill());
    matchViz_.repaint();

    levelMeter_.setLevels(proc.getLastOutPeakL(), proc.getLastOutPeakR());
    levelMeter_.repaint();
}

void StitcherEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown()) return;
    auto* slider = dynamic_cast<juce::Slider*>(e.eventComponent);
    if (slider == nullptr) return;
    auto it = sliderToParam_.find(slider);
    if (it == sliderToParam_.end()) return;
    showMidiLearnMenu(slider);
}

void StitcherEditor::showMidiLearnMenu(juce::Slider* slider)
{
    auto* param = sliderToParam_.at(slider);
    auto& ml    = audioProcessor.getMidiLearn();
    const int curCC = ml.getCCForParam(param);

    juce::PopupMenu menu;
    menu.addItem(1, "Learn MIDI CC");
    if (curCC >= 0)
        menu.addItem(2, "Unbind CC " + juce::String(curCC));

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, param](int result) {
            auto& midiLearn = audioProcessor.getMidiLearn();
            if (result == 1)      midiLearn.startLearning(param);
            else if (result == 2) midiLearn.unbind(midiLearn.getCCForParam(param));
        });
}

void StitcherEditor::paint(juce::Graphics& g)
{
    g.fillAll(StitcherLookAndFeel::Background);
}

void StitcherEditor::resized()
{
    const int margin   = 8;
    const int gap      = 6;
    const int presetH  = 36;
    const int labelH   = 16;
    const int knobSz   = 64;   // rotary knob cell width
    const int vizH     = 72;
    const int meterW   = 18;
    const int gearW    = 34;
    const int sideW    = knobSz + 8;

    auto area = getLocalBounds().reduced(margin);

    // Preset bar + gear button
    {
        auto row = area.removeFromTop(presetH);
        settingsButton_.setBounds(row.removeFromRight(gearW).withSizeKeepingCentre(gearW, presetH));
        presetBar_.setBounds(row);
    }
    area.removeFromTop(gap);

    // Level meter on far right
    levelMeter_.setBounds(area.removeFromRight(meterW));
    area.removeFromRight(gap);

    // Side columns
    auto leftCol  = area.removeFromLeft(sideW);  area.removeFromLeft(gap);
    auto rightCol = area.removeFromRight(sideW); area.removeFromRight(gap);

    // Center: MorphPad (square) then MatchVisualizer
    auto& center = area;
    const int padH = center.getHeight() - vizH - gap;
    const int padSz = std::min(center.getWidth(), padH);
    const int centerOffsetX = (center.getWidth() - padSz) / 2;

    const int padTop = center.getY();
    morphPad_.setBounds(center.getX() + centerOffsetX, padTop, padSz, padSz);
    matchViz_.setBounds(center.getX(), padTop + padSz + gap, center.getWidth(), vizH);

    // Left column: Rand, Xfade, Freeze — distributed over pad height
    {
        const int slotH = padSz / 3;
        int y = padTop;
        // Rand
        randLabel_.setBounds(leftCol.getX(), y, sideW, labelH);
        randSlider_.setBounds(leftCol.getX(), y + labelH, sideW, knobSz);
        y += slotH;
        // Xfade
        xfadeLabel_.setBounds(leftCol.getX(), y, sideW, labelH);
        xfadeSlider_.setBounds(leftCol.getX(), y + labelH, sideW, knobSz);
        y += slotH;
        // Freeze — centered in remaining slot
        const int freezeH = 28;
        const int freezeY = y + (slotH - freezeH) / 2;
        freezeButton_.setBounds(leftCol.getX(), freezeY, sideW, freezeH);
    }

    // Right column: Tilt, Space, Wet, Mix, Output — distributed over pad+viz height
    {
        const int totalH = padSz + gap + vizH;
        const int slotH  = totalH / 5;
        int y = padTop;
        for (auto [s, l] : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
                {&tiltSlider_,      &tiltLabel_},
                {&spaceSlider_,     &spaceLabel_},
                {&reverbWetSlider_, &reverbWetLabel_},
                {&mixSlider_,       &mixLabel_},
                {&gainOutSlider_,   &gainOutLabel_} }) {
            l->setBounds(rightCol.getX(), y, sideW, labelH);
            s->setBounds(rightCol.getX(), y + labelH, sideW, knobSz);
            y += slotH;
        }
    }
}
