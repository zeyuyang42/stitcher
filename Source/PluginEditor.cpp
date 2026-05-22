#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace {

void initRotary(juce::Slider& s, juce::Label& l, const juce::String& name,
                juce::Component& parent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    parent.addAndMakeVisible(s);

    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(11.f)));
    parent.addAndMakeVisible(l);
}

void layoutRotary(juce::Slider& s, juce::Label& l, juce::Rectangle<int> area)
{
    l.setBounds(area.removeFromTop(18));
    s.setBounds(area);
}

} // namespace

StitcherEditor::StitcherEditor(StitcherProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(820, 480);
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
    initRotary(gainCtrlSlider_, gainCtrlLabel_, "Ctrl Gain", *this);
    initRotary(gainSrcSlider_,  gainSrcLabel_,  "Src Gain",  *this);

    freezeButton_.setButtonText("Freeze");
    addAndMakeVisible(freezeButton_);

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
}

void StitcherEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void StitcherEditor::resized()
{
    auto area = getLocalBounds().reduced(6);
    const int kn = 76;  // knob block height (label 18 + slider 58)
    const int kw = 76;  // knob block width

    // ── Section widths ──────────────────────────────────────────────────
    auto concatArea = area.removeFromLeft(380);
    area.removeFromLeft(4);
    auto eqArea     = area.removeFromLeft(130);
    area.removeFromLeft(4);
    auto reverbArea = area.removeFromLeft(130);
    area.removeFromLeft(4);
    auto outputArea = area;

    concatGroup_.setBounds(concatArea);
    eqGroup_    .setBounds(eqArea);
    reverbGroup_.setBounds(reverbArea);
    outputGroup_.setBounds(outputArea);

    const int headerH = 20;

    // ── Concatenator ───────────────────────────────────────────────────
    {
        auto r = concatArea.reduced(6).withTrimmedTop(headerH);

        // Row 1: zcr, rms, sc, st
        auto row1 = r.removeFromTop(kn);
        layoutRotary(zcrSlider_, zcrLabel_, row1.removeFromLeft(kw));
        row1.removeFromLeft(3);
        layoutRotary(rmsSlider_, rmsLabel_, row1.removeFromLeft(kw));
        row1.removeFromLeft(3);
        layoutRotary(scSlider_,  scLabel_,  row1.removeFromLeft(kw));
        row1.removeFromLeft(3);
        layoutRotary(stSlider_,  stLabel_,  row1.removeFromLeft(kw));

        r.removeFromTop(4);

        // Row 2: seekTime, matchLen, rand
        auto row2 = r.removeFromTop(kn);
        layoutRotary(seekTimeSlider_, seekTimeLabel_, row2.removeFromLeft(kw));
        row2.removeFromLeft(3);
        layoutRotary(matchLenSlider_, matchLenLabel_, row2.removeFromLeft(kw));
        row2.removeFromLeft(3);
        layoutRotary(randSlider_,     randLabel_,     row2.removeFromLeft(kw));

        r.removeFromTop(4);

        // Row 3: gainCtrl, gainSrc + Freeze toggle
        auto row3 = r.removeFromTop(kn);
        layoutRotary(gainCtrlSlider_, gainCtrlLabel_, row3.removeFromLeft(kw));
        row3.removeFromLeft(3);
        layoutRotary(gainSrcSlider_,  gainSrcLabel_,  row3.removeFromLeft(kw));
        row3.removeFromLeft(8);
        freezeButton_.setBounds(row3.removeFromLeft(80).withSizeKeepingCentre(80, 28));
    }

    // ── EQ ─────────────────────────────────────────────────────────────
    {
        auto r = eqArea.reduced(6).withTrimmedTop(headerH);
        layoutRotary(eqLowSlider_,  eqLowLabel_,  r.removeFromTop(kn));
        r.removeFromTop(4);
        layoutRotary(eqMidSlider_,  eqMidLabel_,  r.removeFromTop(kn));
        r.removeFromTop(4);
        layoutRotary(eqHighSlider_, eqHighLabel_, r.removeFromTop(kn));
    }

    // ── Reverb ─────────────────────────────────────────────────────────
    {
        auto r = reverbArea.reduced(6).withTrimmedTop(headerH);
        layoutRotary(reverbRoomSlider_, reverbRoomLabel_, r.removeFromTop(kn));
        r.removeFromTop(4);
        layoutRotary(reverbDampSlider_, reverbDampLabel_, r.removeFromTop(kn));
        r.removeFromTop(4);
        layoutRotary(reverbWetSlider_,  reverbWetLabel_,  r.removeFromTop(kn));
    }

    // ── Output ─────────────────────────────────────────────────────────
    {
        auto r = outputArea.reduced(6).withTrimmedTop(headerH);
        layoutRotary(gainOutSlider_, gainOutLabel_, r.removeFromTop(kn));
        r.removeFromTop(4);
        layoutRotary(mixSlider_,     mixLabel_,     r.removeFromTop(kn));
    }
}
