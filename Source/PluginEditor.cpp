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
    initRotary(zcrSlider_,      zcrLabel_,      "ZCR",       *this);
    initRotary(rmsSlider_,      rmsLabel_,      "RMS",       *this);
    initRotary(scSlider_,       scLabel_,       "S.Cent",    *this);
    initRotary(stSlider_,       stLabel_,       "S.Tilt",    *this);
    initRotary(seekTimeSlider_, seekTimeLabel_, "Seek",      *this);
    initRotary(matchLenSlider_, matchLenLabel_, "Len",       *this);
    initRotary(randSlider_,     randLabel_,     "Rand",      *this);
    initRotary(gainCtrlSlider_, gainCtrlLabel_, "Ctrl", *this);
    initRotary(gainSrcSlider_,  gainSrcLabel_,  "Src",  *this);

    freezeButton_.setButtonText("Freeze");
    addAndMakeVisible(freezeButton_);

    for (auto* m : { &zcrMeter_, &rmsMeter_, &scMeter_, &stMeter_,
                     &corpusFillMeter_ })
        addAndMakeVisible(m);
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

    startTimerHz(30);
}

StitcherEditor::~StitcherEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void StitcherEditor::timerCallback()
{
    auto& proc  = audioProcessor;
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

    levelMeter_.setLevels(proc.getLastOutPeakL(), proc.getLastOutPeakR());

    zcrMeter_.repaint();
    rmsMeter_.repaint();
    scMeter_.repaint();
    stMeter_.repaint();
    corpusFillMeter_.repaint();
    levelMeter_.repaint();
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
