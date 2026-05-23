#pragma once
#include <JuceHeader.h>
#include "../LookAndFeel/StitcherLookAndFeel.h"
#include "../Parameters.h"

class SettingsPopover : public juce::Component {
public:
    explicit SettingsPopover(juce::AudioProcessorValueTreeState& apvts);
    ~SettingsPopover() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    StitcherLookAndFeel lnf_;

    juce::Slider     seekSlider_, matchLenSlider_, ctrlGainSlider_, srcGainSlider_;
    juce::ToggleButton syncButton_;
    juce::ComboBox   divBox_;
    juce::Label      seekLabel_, matchLenLabel_, ctrlGainLabel_, srcGainLabel_;
    juce::Label      loadTimeHeader_, trimHeader_;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> seekAttach_, matchLenAttach_, ctrlGainAttach_, srcGainAttach_;
    std::unique_ptr<BA> syncAttach_;
    std::unique_ptr<CA> divAttach_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPopover)
};
