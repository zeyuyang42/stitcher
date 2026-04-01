#pragma once
#include <juce_dsp/juce_dsp.h>

class ReverbProcessor {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    // roomSize [0,1], damping [0,1], wetLevel [0,1]
    void setParams(float roomSize, float damping, float wetLevel);
    void process(juce::dsp::AudioBlock<float>& block);

private:
    juce::dsp::Reverb reverb_;
};
