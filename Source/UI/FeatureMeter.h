#pragma once
#include <JuceHeader.h>

class FeatureMeter : public juce::Component {
public:
    void setLevel(float value01) noexcept;
    void setActive(bool active) noexcept;
    void paint(juce::Graphics&) override;
private:
    float level_  = 0.f;
    bool  active_ = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatureMeter)
};
