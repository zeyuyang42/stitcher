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

void ReverbProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb_.process(ctx);
}
