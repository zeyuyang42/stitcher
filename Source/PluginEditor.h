#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class StitcherEditor : public juce::AudioProcessorEditor {
public:
    explicit StitcherEditor(StitcherProcessor&);
    ~StitcherEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    StitcherProcessor& audioProcessor;

    // Section groups
    juce::GroupComponent concatGroup_, eqGroup_, reverbGroup_, outputGroup_;

    // ── Concatenator sliders ──────────────────────────────────────────────
    juce::Slider zcrSlider_, rmsSlider_, scSlider_, stSlider_;
    juce::Slider seekTimeSlider_, matchLenSlider_, randSlider_;
    juce::Slider gainCtrlSlider_, gainSrcSlider_;
    juce::ToggleButton freezeButton_;

    juce::Label zcrLabel_, rmsLabel_, scLabel_, stLabel_;
    juce::Label seekTimeLabel_, matchLenLabel_, randLabel_;
    juce::Label gainCtrlLabel_, gainSrcLabel_;

    // ── EQ sliders ────────────────────────────────────────────────────────
    juce::Slider eqLowSlider_, eqMidSlider_, eqHighSlider_;
    juce::Label  eqLowLabel_,  eqMidLabel_,  eqHighLabel_;

    // ── Reverb sliders ────────────────────────────────────────────────────
    juce::Slider reverbRoomSlider_, reverbDampSlider_, reverbWetSlider_;
    juce::Label  reverbRoomLabel_,  reverbDampLabel_,  reverbWetLabel_;

    // ── Output sliders ────────────────────────────────────────────────────
    juce::Slider gainOutSlider_, mixSlider_;
    juce::Label  gainOutLabel_,  mixLabel_;

    // ── APVTS attachments (must be declared after their sliders) ──────────
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
