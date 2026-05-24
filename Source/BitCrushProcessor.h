#pragma once
#include <juce_dsp/juce_dsp.h>

class BitCrushProcessor {
public:
    // bits: 2.0 .. 16.0; at 16 the processor bypasses completely
    void setBits(float bits);
    void process(juce::dsp::AudioBlock<float>& block);

private:
    float bits_ = 16.f;
};
