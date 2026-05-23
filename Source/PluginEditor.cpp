#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "UI/FeatureMeter.h"
#include "UI/LevelMeter.h"
#include "FactoryPresets.h"

namespace {

void initRotary(juce::Slider& s, juce::Label& l, const juce::String& name,
                juce::Component& parent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    parent.addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    parent.addAndMakeVisible(l);
}

void layoutRotary(juce::Slider& s, juce::Label& l, juce::Rectangle<int> area)
{
    l.setBounds(area.removeFromTop(18));
    s.setBounds(area);
}

} // namespace

StitcherEditor::StitcherEditor(StitcherProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      presetManager_(p.getAPVTS(), PresetManager::getUserPresetsDir(), makeFactoryPresets()),
      presetBar_(presetManager_)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(presetBar_);

    presetBar_.onCaptureSlot = [this](int slot) {
        abSlots_[slot] = audioProcessor.getAPVTS().copyState();
    };
    presetBar_.onLoadSlot = [this](int slot) {
        if (abSlots_[slot].isValid())
            audioProcessor.getAPVTS().replaceState(abSlots_[slot].createCopy());
    };

    setSize(960, 560);
    setResizable(true, true);
    setResizeLimits(720, 420, 1440, 840);
    auto& apvts = audioProcessor.getAPVTS();

    // ── Section group labels ────────────────────────────────────────────
    concatGroup_.setText("Concatenator");
    eqGroup_    .setText("EQ");
    reverbGroup_.setText("Reverb");
    outputGroup_.setText("Output");
    for (auto* g : { &concatGroup_, &eqGroup_, &reverbGroup_, &outputGroup_ })
        addAndMakeVisible(g);

    // ── Concatenator ───────────────────────────────────────────────────
    addAndMakeVisible(morphPad_);
    initRotary(seekTimeSlider_, seekTimeLabel_, "Seek",  *this);
    initRotary(matchLenSlider_, matchLenLabel_, "Len",   *this);
    initRotary(randSlider_,     randLabel_,     "Rand",  *this);
    initRotary(xfadeSlider_,    xfadeLabel_,    "Xfade", *this);
    initRotary(gainCtrlSlider_, gainCtrlLabel_, "Ctrl", *this);
    initRotary(gainSrcSlider_,  gainSrcLabel_,  "Src",  *this);

    freezeButton_.setButtonText("Freeze");
    addAndMakeVisible(freezeButton_);

    matchLenSyncButton_.setButtonText("Sync");
    addAndMakeVisible(matchLenSyncButton_);

    matchLenDivBox_.addItemList({"1/16","1/8","1/4","1/4.","1/2","1/1","2/1"}, 1);
    addAndMakeVisible(matchLenDivBox_);

    addAndMakeVisible(matchViz_);
    addAndMakeVisible(levelMeter_);

    // ── EQ ─────────────────────────────────────────────────────────────
    initRotary(eqLowSlider_,  eqLowLabel_,  "Low",  *this);
    initRotary(eqMidSlider_,  eqMidLabel_,  "Mid",  *this);
    initRotary(eqHighSlider_, eqHighLabel_, "High", *this);

    // ── Reverb ─────────────────────────────────────────────────────────
    initRotary(reverbRoomSlider_, reverbRoomLabel_, "Room", *this);
    initRotary(reverbDampSlider_, reverbDampLabel_, "Damp", *this);
    initRotary(reverbWetSlider_,  reverbWetLabel_,  "Wet",  *this);

    // ── Output ─────────────────────────────────────────────────────────
    initRotary(gainOutSlider_, gainOutLabel_, "Gain", *this);
    initRotary(mixSlider_,     mixLabel_,     "Mix",  *this);

    // ── Attachments (after sliders are visible) ─────────────────────────
    morphPad_.setParams(
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::zcrWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::rmsWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::scWeight)),
        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::stWeight)));
    seekTimeAttach_   = std::make_unique<SA>(apvts, ParamIDs::seekTime,   seekTimeSlider_);
    matchLenAttach_   = std::make_unique<SA>(apvts, ParamIDs::matchLen,   matchLenSlider_);
    randAttach_       = std::make_unique<SA>(apvts, ParamIDs::rand_,      randSlider_);
    xfadeAttach_      = std::make_unique<SA>(apvts, ParamIDs::xfade,      xfadeSlider_);
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
    freezeAttach_        = std::make_unique<BA>(apvts, ParamIDs::freeze,        freezeButton_);
    matchLenSyncAttach_  = std::make_unique<BA>(apvts, ParamIDs::matchLenSync, matchLenSyncButton_);
    matchLenDivAttach_   = std::make_unique<CA>(apvts, ParamIDs::matchLenDiv,  matchLenDivBox_);

    matchLenSyncButton_.onClick = [this] {
        const bool sync = matchLenSyncButton_.getToggleState();
        matchLenSlider_.setVisible(!sync);
        matchLenDivBox_.setVisible(sync);
        resized();
    };
    // Apply initial visibility from APVTS default/restored state
    {
        const bool sync = matchLenSyncButton_.getToggleState();
        matchLenSlider_.setVisible(!sync);
        matchLenDivBox_.setVisible(sync);
    }

    // ── MIDI Learn: map each slider to its APVTS parameter ─────────────────
    sliderToParam_ = {
        { &seekTimeSlider_,    dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::seekTime))   },
        { &matchLenSlider_,    dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::matchLen))   },
        { &randSlider_,        dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::rand_))      },
        { &xfadeSlider_,       dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::xfade))      },
        { &gainCtrlSlider_,    dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainCtrl))   },
        { &gainSrcSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainSrc))    },
        { &eqLowSlider_,       dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::eqLow))      },
        { &eqMidSlider_,       dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::eqMid))      },
        { &eqHighSlider_,      dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::eqHigh))     },
        { &reverbRoomSlider_,  dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbRoom)) },
        { &reverbDampSlider_,  dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbDamp)) },
        { &reverbWetSlider_,   dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::reverbWet))  },
        { &gainOutSlider_,     dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::gainOut))    },
        { &mixSlider_,         dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(ParamIDs::mix))        },
    };

    // Install this editor as a mouse listener on every slider for right-click menus.
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
    // Drain any pending MIDI learn capture from the audio thread.
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
    if (!e.mods.isRightButtonDown())
        return;
    auto* slider = dynamic_cast<juce::Slider*>(e.eventComponent);
    if (slider == nullptr)
        return;
    auto it = sliderToParam_.find(slider);
    if (it == sliderToParam_.end())
        return;
    showMidiLearnMenu(slider);
}

void StitcherEditor::showMidiLearnMenu(juce::Slider* slider)
{
    auto* param    = sliderToParam_.at(slider);
    auto& ml       = audioProcessor.getMidiLearn();
    const int curCC = ml.getCCForParam(param);

    juce::PopupMenu menu;
    menu.addItem(1, "Learn MIDI CC");
    if (curCC >= 0)
        menu.addItem(2, "Unbind CC " + juce::String(curCC));

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, param](int result) {
            auto& ml = audioProcessor.getMidiLearn();
            if (result == 1)
                ml.startLearning(param);
            else if (result == 2)
                ml.unbind(ml.getCCForParam(param));
        });
}

void StitcherEditor::paint(juce::Graphics& g)
{
    g.fillAll(StitcherLookAndFeel::Background);
}

void StitcherEditor::resized()
{
    const int margin     = 8;
    const int gap        = 6;
    const int headerH    = 18;
    const int labelH     = 14;
    const int meterH     = 6;
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

        // Row 1: MorphPad (replaces the 4 weight knobs + meters)
        const int kw1 = std::min(90, (r.getWidth() - 9) / 4);
        const int kh1 = kw1 + labelH + meterH;
        {
            const int padSize = std::min(r.getWidth(), kh1);
            auto row1 = r.removeFromTop(kh1);
            morphPad_.setBounds(row1.withSizeKeepingCentre(padSize, kh1));
        }
        r.removeFromTop(gap);

        // Row 2: seek, matchLen (with sync toggle), rand, xfade
        const int syncBtnH = 18;
        const int kw2 = std::min(90, (r.getWidth() - 9) / 4);
        const int kh2 = kw2 + labelH + syncBtnH;
        auto row2 = r.removeFromTop(kh2);
        // Seek
        {
            auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
            seekTimeLabel_.setBounds(cell.removeFromTop(labelH));
            cell.removeFromTop(syncBtnH);
            seekTimeSlider_.setBounds(cell);
        }
        // MatchLen
        {
            auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
            matchLenLabel_.setBounds(cell.removeFromTop(labelH));
            matchLenSyncButton_.setBounds(cell.removeFromTop(syncBtnH).reduced(2, 2));
            if (matchLenSlider_.isVisible())
                matchLenSlider_.setBounds(cell);
            else
                matchLenDivBox_.setBounds(
                    cell.withSizeKeepingCentre(cell.getWidth(), std::min(cell.getHeight(), 24)));
        }
        // Rand
        {
            auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
            randLabel_.setBounds(cell.removeFromTop(labelH));
            cell.removeFromTop(syncBtnH);
            randSlider_.setBounds(cell);
        }
        // Xfade
        {
            auto cell = row2.removeFromLeft(kw2); row2.removeFromLeft(3);
            xfadeLabel_.setBounds(cell.removeFromTop(labelH));
            cell.removeFromTop(syncBtnH);
            xfadeSlider_.setBounds(cell);
        }
        r.removeFromTop(gap);

        // Match visualizer (replaces corpus fill bar)
        matchViz_.setBounds(r.removeFromTop(50));
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
