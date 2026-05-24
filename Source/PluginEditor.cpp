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

    setSize(880, 520);
    setResizable(true, true);
    setResizeLimits(660, 390, 1320, 780);

    auto& apvts = audioProcessor.getAPVTS();

    // Param strip — load-time + trim controls
    initRotary(seekSlider_,     seekLabel_,     "Seek",      *this);
    initRotary(matchLenSlider_, matchLenLabel_, "Match Len", *this);
    initRotary(ctrlGainSlider_, ctrlGainLabel_, "Ctrl Gain", *this);
    initRotary(srcGainSlider_,  srcGainLabel_,  "Src Gain",  *this);

    syncButton_.setButtonText("Sync");
    addAndMakeVisible(syncButton_);

    divBox_.addItemList({"1/16","1/8","1/4","1/4.","1/2","1/1","2/1"}, 1);
    addAndMakeVisible(divBox_);

    seekAttach_     = std::make_unique<SA>(apvts, ParamIDs::seekTime,     seekSlider_);
    matchLenAttach_ = std::make_unique<SA>(apvts, ParamIDs::matchLen,     matchLenSlider_);
    ctrlGainAttach_ = std::make_unique<SA>(apvts, ParamIDs::gainCtrl,     ctrlGainSlider_);
    srcGainAttach_  = std::make_unique<SA>(apvts, ParamIDs::gainSrc,      srcGainSlider_);
    syncAttach_     = std::make_unique<BA>(apvts, ParamIDs::matchLenSync, syncButton_);
    divAttach_      = std::make_unique<CA>(apvts, ParamIDs::matchLenDiv,  divBox_);

    syncButton_.onClick = [this] {
        const bool sync = syncButton_.getToggleState();
        matchLenSlider_.setVisible(!sync);
        divBox_.setVisible(sync);
        matchLenLabel_.setText(sync ? "Div" : "Match Len", juce::dontSendNotification);
        resized();
    };
    {
        const bool sync = syncButton_.getToggleState();
        matchLenSlider_.setVisible(!sync);
        divBox_.setVisible(sync);
        matchLenLabel_.setText(sync ? "Div" : "Match Len", juce::dontSendNotification);
    }

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

    // MIDI Learn: all rotary sliders on the main face
    sliderToParam_ = {
        { &seekSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::seekTime))     },
        { &matchLenSlider_,  dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::matchLen))     },
        { &ctrlGainSlider_,  dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainCtrl))     },
        { &srcGainSlider_,   dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainSrc))      },
        { &randSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::rand_))        },
        { &xfadeSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::xfade))        },
        { &tiltSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::eqTilt))       },
        { &spaceSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbSpace))  },
        { &reverbWetSlider_, dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbWet))    },
        { &mixSlider_,       dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::mix))          },
        { &gainOutSlider_,   dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainOut))      },
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
                   proc.getLastCorpusFill(),
                   proc.getMatchEpoch());
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
    const int presetH  = 32;
    const int paramH   = 72;   // label (14) + rotary (56) + 2px breathing room
    const int labelH   = 14;
    const int knobSz   = 56;
    const int vizH     = 64;
    const int meterW   = 14;
    const int sideW    = knobSz + 6;

    auto area = getLocalBounds().reduced(margin);

    // Preset bar — full width
    presetBar_.setBounds(area.removeFromTop(presetH));
    area.removeFromTop(gap);

    // Param strip — Seek / MatchLen / Sync / CtrlGain / SrcGain
    {
        auto r = area.removeFromTop(paramH);
        area.removeFromTop(gap);

        const int numCells = 5;
        const int cellW    = (r.getWidth() - (numCells - 1) * gap) / numCells;

        auto takeCell = [&]() -> juce::Rectangle<int> {
            auto c = r.removeFromLeft(cellW);
            r.removeFromLeft(gap);
            return c;
        };

        // Seek
        {
            auto c = takeCell();
            seekLabel_.setBounds(c.removeFromTop(labelH));
            seekSlider_.setBounds(c);
        }

        // Match Len / Div
        {
            auto c = takeCell();
            matchLenLabel_.setBounds(c.removeFromTop(labelH));
            if (matchLenSlider_.isVisible())
                matchLenSlider_.setBounds(c);
            else
                divBox_.setBounds(c.withSizeKeepingCentre(c.getWidth(), 24)
                                   .withY(c.getY() + (c.getHeight() - 24) / 2));
        }

        // Sync toggle
        {
            auto c = takeCell();
            syncButton_.setBounds(c.withSizeKeepingCentre(c.getWidth(), 24)
                                   .withY(c.getY() + (c.getHeight() - 24) / 2));
        }

        // Ctrl Gain
        {
            auto c = takeCell();
            ctrlGainLabel_.setBounds(c.removeFromTop(labelH));
            ctrlGainSlider_.setBounds(c);
        }

        // Src Gain (use remaining r)
        srcGainLabel_.setBounds(r.removeFromTop(labelH));
        srcGainSlider_.setBounds(r);
    }

    // Level meter on far right
    levelMeter_.setBounds(area.removeFromRight(meterW));
    area.removeFromRight(gap);

    // Side columns
    auto leftCol  = area.removeFromLeft(sideW);   area.removeFromLeft(gap);
    auto rightCol = area.removeFromRight(sideW);  area.removeFromRight(gap);

    // Center: MorphPad (square) then MatchVisualizer
    auto& center = area;
    const int padH  = center.getHeight() - vizH - gap;
    const int padSz = std::min(center.getWidth(), padH);
    const int centerOffsetX = (center.getWidth() - padSz) / 2;

    const int padTop = center.getY();
    morphPad_.setBounds(center.getX() + centerOffsetX, padTop, padSz, padSz);
    matchViz_.setBounds(center.getX(), padTop + padSz + gap, center.getWidth(), vizH);

    // Left column: Rand, Xfade, Freeze — tighter spacing over pad height
    {
        const int slotH = padSz / 3;
        int y = padTop;

        randLabel_.setBounds(leftCol.getX(), y, sideW, labelH);
        randSlider_.setBounds(leftCol.getX(), y + labelH, sideW, knobSz);
        y += slotH;

        xfadeLabel_.setBounds(leftCol.getX(), y, sideW, labelH);
        xfadeSlider_.setBounds(leftCol.getX(), y + labelH, sideW, knobSz);
        y += slotH;

        const int freezeH = 28;
        freezeButton_.setBounds(leftCol.getX(), y + (slotH - freezeH) / 2, sideW, freezeH);
    }

    // Right column: Tilt, Space, Wet, Mix, Output — over full body height
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
