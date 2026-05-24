#pragma once
#include <JuceHeader.h>
#include <map>
#include "PluginProcessor.h"
#include "LookAndFeel/StitcherLookAndFeel.h"
#include "PresetManager.h"
#include "UI/PresetBar.h"
#include "UI/LevelMeter.h"
#include "UI/MorphPad.h"
#include "UI/MatchVisualizer.h"

class StitcherEditor : public juce::AudioProcessorEditor,
                       private juce::Timer {
public:
    explicit StitcherEditor(StitcherProcessor&);
    ~StitcherEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void showMidiLearnMenu(juce::Slider* slider);

    StitcherProcessor& audioProcessor;

    StitcherLookAndFeel lnf_;

    // PresetManager must be declared before PresetBar (construction order)
    PresetManager presetManager_;
    PresetBar     presetBar_;

    // Param strip (load-time + trim)
    juce::Slider       seekSlider_, matchLenSlider_, ctrlGainSlider_, srcGainSlider_;
    juce::ToggleButton syncButton_;
    juce::ComboBox     divBox_;
    juce::Label        seekLabel_, matchLenLabel_, ctrlGainLabel_, srcGainLabel_;

    // Center hero
    MorphPad        morphPad_;
    MatchVisualizer matchViz_;

    // Left column
    juce::Slider       randSlider_, xfadeSlider_;
    juce::Label        randLabel_,  xfadeLabel_;
    juce::ToggleButton freezeButton_;

    // Right column
    juce::Slider tiltSlider_, spaceSlider_, reverbWetSlider_, mixSlider_, gainOutSlider_;
    juce::Label  tiltLabel_,  spaceLabel_,  reverbWetLabel_,  mixLabel_,  gainOutLabel_;

    // Output meter
    LevelMeter levelMeter_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> seekAttach_, matchLenAttach_, ctrlGainAttach_, srcGainAttach_;
    std::unique_ptr<BA> syncAttach_;
    std::unique_ptr<CA> divAttach_;

    std::unique_ptr<SA> randAttach_, xfadeAttach_;
    std::unique_ptr<BA> freezeAttach_;
    std::unique_ptr<SA> tiltAttach_, spaceAttach_, reverbWetAttach_, mixAttach_, gainOutAttach_;

    // MIDI Learn: maps each main-face slider to its APVTS parameter
    std::map<juce::Slider*, juce::RangedAudioParameter*> sliderToParam_;

    // A/B state slots — session-local, not persisted
    juce::ValueTree abSlots_[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherEditor)
};
