#pragma once
#include <JuceHeader.h>
#include <map>
#include "PluginProcessor.h"
#include "LookAndFeel/StitcherLookAndFeel.h"
#include "PresetManager.h"
#include "UI/PresetBar.h"
#include "UI/FeatureMeter.h"
#include "UI/LevelMeter.h"
#include "UI/MorphPad.h"

class StitcherEditor : public juce::AudioProcessorEditor,
                       private juce::Timer {
public:
    explicit StitcherEditor(StitcherProcessor&);
    ~StitcherEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // MouseListener override: intercepts right-clicks on sliders.
    // Component (base of AudioProcessorEditor) already inherits MouseListener.
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void showMidiLearnMenu(juce::Slider* slider);

    StitcherProcessor& audioProcessor;

    StitcherLookAndFeel lnf_;

    // PresetManager must be declared before PresetBar (construction order)
    PresetManager presetManager_;
    PresetBar     presetBar_;

    juce::GroupComponent concatGroup_, eqGroup_, reverbGroup_, outputGroup_;

    MorphPad morphPad_;

    juce::Slider seekTimeSlider_, matchLenSlider_, randSlider_, xfadeSlider_;
    juce::Slider gainCtrlSlider_, gainSrcSlider_;
    juce::ToggleButton freezeButton_;
    juce::ToggleButton matchLenSyncButton_;
    juce::ComboBox     matchLenDivBox_;

    juce::Label seekTimeLabel_, matchLenLabel_, randLabel_, xfadeLabel_;
    juce::Label gainCtrlLabel_, gainSrcLabel_;

    juce::Slider eqLowSlider_, eqMidSlider_, eqHighSlider_;
    juce::Label  eqLowLabel_,  eqMidLabel_,  eqHighLabel_;

    juce::Slider reverbRoomSlider_, reverbDampSlider_, reverbWetSlider_;
    juce::Label  reverbRoomLabel_,  reverbDampLabel_,  reverbWetLabel_;

    juce::Slider gainOutSlider_, mixSlider_;
    juce::Label  gainOutLabel_,  mixLabel_;

    // Live meters
    FeatureMeter corpusFillMeter_;
    LevelMeter   levelMeter_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> seekTimeAttach_, matchLenAttach_, randAttach_, xfadeAttach_;
    std::unique_ptr<SA> gainCtrlAttach_, gainSrcAttach_;
    std::unique_ptr<SA> eqLowAttach_, eqMidAttach_, eqHighAttach_;
    std::unique_ptr<SA> reverbRoomAttach_, reverbDampAttach_, reverbWetAttach_;
    std::unique_ptr<SA> gainOutAttach_, mixAttach_;
    std::unique_ptr<BA> freezeAttach_;
    std::unique_ptr<BA> matchLenSyncAttach_;
    std::unique_ptr<CA> matchLenDivAttach_;

    // MIDI Learn: maps each slider to its bound APVTS parameter
    std::map<juce::Slider*, juce::RangedAudioParameter*> sliderToParam_;

    // A/B state slots — session-local, not persisted
    juce::ValueTree abSlots_[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherEditor)
};
