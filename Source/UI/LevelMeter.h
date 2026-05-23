#pragma once
#include <JuceHeader.h>

class LevelMeter : public juce::Component {
public:
    LevelMeter() = default;
    void setLevels(float peakL, float peakR) noexcept;
    void paint(juce::Graphics&) override;
private:
    float peakL_ = 0.f;
    float peakR_ = 0.f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
