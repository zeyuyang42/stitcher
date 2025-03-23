#pragma once

#include <JuceHeader.h>

const juce::ParameterID gainParamID{"gain", 1};
const juce::ParameterID thresholdParamID{"threshold", 2};
const juce::ParameterID alphaParamID{"alpha", 3};

class Parameters {
  public:
    Parameters(juce::AudioProcessorValueTreeState& apvts);

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();

    void prepareToPlay(double sampleRate) noexcept;
    void reset() noexcept;
    void update() noexcept;
    void smoothen() noexcept;

    float gain = 0.0f;
    float threshold = 0.5f;
    float alpha = 0.8f;

  private:
    // gain: 1
    juce::AudioParameterFloat* gainParam;
    juce::LinearSmoothedValue<float> gainSmoother;
    // threshold: 2
    juce::AudioParameterFloat* thresholdParam;
    juce::LinearSmoothedValue<float> thresholdSmoother;
    // alpha: 3
    juce::AudioParameterFloat* alphaParam;
    juce::LinearSmoothedValue<float> alphaSmoother;
};
