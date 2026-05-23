#include "ReverbProcessor.h"

void ReverbProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    reverb_.prepare(spec);
    juce::dsp::Reverb::Parameters p;
    p.roomSize  = 0.5f;
    p.damping   = 0.5f;
    p.wetLevel  = 0.f;
    p.dryLevel  = 1.f;
    reverb_.setParameters(p);
}

void ReverbProcessor::setParams(float roomSize, float damping, float wetLevel)
{
    juce::dsp::Reverb::Parameters p;
    p.roomSize  = roomSize;
    p.damping   = damping;
    p.wetLevel  = wetLevel;
    p.dryLevel  = 1.f - wetLevel;
    reverb_.setParameters(p);
}

void ReverbProcessor::setSpace(float space, float wet)
{
    const float roomSize = 0.2f + space * 0.75f;  // 0.20 .. 0.95
    const float damping  = 0.9f - space * 0.7f;   // 0.90 .. 0.20
    setParams(roomSize, damping, wet);
}

void ReverbProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb_.process(ctx);
}
