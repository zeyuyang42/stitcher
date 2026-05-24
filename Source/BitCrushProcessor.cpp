#include "BitCrushProcessor.h"
#include <cmath>

void BitCrushProcessor::setBits(float bits)
{
    bits_ = juce::jlimit(2.f, 16.f, bits);
}

void BitCrushProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    if (bits_ >= 15.99f) return;  // bypass at/near 16 bits

    const int N     = static_cast<int>(block.getNumSamples());
    const int numCh = static_cast<int>(block.getNumChannels());
    // Number of quantisation levels = 2^bits - 1 (for bipolar signal in [-1,1])
    const float levels = std::pow(2.f, std::floor(bits_)) - 1.f;
    const float inv    = 1.f / levels;

    for (int c = 0; c < numCh; ++c) {
        float* data = block.getChannelPointer(c);
        for (int i = 0; i < N; ++i)
            data[i] = std::round(data[i] * levels) * inv;
    }
}
