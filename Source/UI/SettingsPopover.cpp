#include "SettingsPopover.h"

namespace {

void initRotaryCompact(juce::Slider& s, juce::Label& l, const juce::String& name,
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

SettingsPopover::SettingsPopover(juce::AudioProcessorValueTreeState& apvts)
{
    setLookAndFeel(&lnf_);
    setSize(340, 230);

    // Section headers
    loadTimeHeader_.setText("LOAD-TIME  (applies on plugin reload)", juce::dontSendNotification);
    loadTimeHeader_.setFont(juce::Font(10.f).boldened());
    loadTimeHeader_.setColour(juce::Label::textColourId, StitcherLookAndFeel::TextDim);
    addAndMakeVisible(loadTimeHeader_);

    trimHeader_.setText("TRIM GAINS", juce::dontSendNotification);
    trimHeader_.setFont(juce::Font(10.f).boldened());
    trimHeader_.setColour(juce::Label::textColourId, StitcherLookAndFeel::TextDim);
    addAndMakeVisible(trimHeader_);

    // Controls
    initRotaryCompact(seekSlider_,    seekLabel_,    "Seek Time", *this);
    initRotaryCompact(matchLenSlider_, matchLenLabel_, "Match Len", *this);
    initRotaryCompact(ctrlGainSlider_, ctrlGainLabel_, "Ctrl Gain", *this);
    initRotaryCompact(srcGainSlider_,  srcGainLabel_,  "Src Gain",  *this);

    syncButton_.setButtonText("Sync");
    addAndMakeVisible(syncButton_);

    divBox_.addItemList({"1/16","1/8","1/4","1/4.","1/2","1/1","2/1"}, 1);
    addAndMakeVisible(divBox_);

    seekAttach_     = std::make_unique<SA>(apvts, ParamIDs::seekTime,    seekSlider_);
    matchLenAttach_ = std::make_unique<SA>(apvts, ParamIDs::matchLen,    matchLenSlider_);
    ctrlGainAttach_ = std::make_unique<SA>(apvts, ParamIDs::gainCtrl,    ctrlGainSlider_);
    srcGainAttach_  = std::make_unique<SA>(apvts, ParamIDs::gainSrc,     srcGainSlider_);
    syncAttach_     = std::make_unique<BA>(apvts, ParamIDs::matchLenSync, syncButton_);
    divAttach_      = std::make_unique<CA>(apvts, ParamIDs::matchLenDiv,  divBox_);

    syncButton_.onClick = [this] {
        const bool sync = syncButton_.getToggleState();
        matchLenSlider_.setVisible(!sync);
        divBox_.setVisible(sync);
        resized();
    };
    const bool sync = syncButton_.getToggleState();
    matchLenSlider_.setVisible(!sync);
    divBox_.setVisible(sync);
}

SettingsPopover::~SettingsPopover()
{
    setLookAndFeel(nullptr);
}

void SettingsPopover::paint(juce::Graphics& g)
{
    g.fillAll(StitcherLookAndFeel::Panel);
    g.setColour(StitcherLookAndFeel::Track.brighter(0.2f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.f, 1.f);
}

void SettingsPopover::resized()
{
    const int margin   = 12;
    const int gap      = 8;
    const int headerH  = 14;
    const int labelH   = 14;
    const int knobSize = 70;
    const int knobCell = knobSize + labelH;
    const int divH     = 24;

    auto r = getLocalBounds().reduced(margin);

    // Load-time section
    loadTimeHeader_.setBounds(r.removeFromTop(headerH));
    r.removeFromTop(4);

    auto loadRow = r.removeFromTop(knobCell);
    {
        auto cell = loadRow.removeFromLeft(knobSize + 16);
        seekLabel_  .setBounds(cell.removeFromTop(labelH));
        seekSlider_ .setBounds(cell);
        loadRow.removeFromLeft(gap);
    }
    {
        auto cell = loadRow.removeFromLeft(knobSize + 16);
        matchLenLabel_.setBounds(cell.removeFromTop(labelH));
        if (matchLenSlider_.isVisible())
            matchLenSlider_.setBounds(cell);
        else
            divBox_.setBounds(cell.withSizeKeepingCentre(cell.getWidth(), divH));
        loadRow.removeFromLeft(gap);
    }
    {
        auto btnBounds = loadRow.removeFromLeft(60)
                                .withSizeKeepingCentre(60, 24)
                                .translated(0, (knobCell - 24) / 2);
        syncButton_.setBounds(btnBounds);
    }

    r.removeFromTop(gap * 2);

    // Trim gains section
    trimHeader_.setBounds(r.removeFromTop(headerH));
    r.removeFromTop(4);

    auto trimRow = r.removeFromTop(knobCell);
    for (auto pair : std::initializer_list<std::pair<juce::Slider*, juce::Label*>>{
            {&ctrlGainSlider_, &ctrlGainLabel_},
            {&srcGainSlider_,  &srcGainLabel_} }) {
        auto cell = trimRow.removeFromLeft(knobSize + 16);
        pair.second->setBounds(cell.removeFromTop(labelH));
        pair.first->setBounds(cell);
        trimRow.removeFromLeft(gap);
    }
}
