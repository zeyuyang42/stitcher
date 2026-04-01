#include "EQProcessor.h"

void EQProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate_ = spec.sampleRate;
    lowFilter_.prepare(spec);
    midFilter_.prepare(spec);
    highFilter_.prepare(spec);
    updateCoefficients(0.f, 0.f, 0.f);
}

void EQProcessor::setGains(float lowDb, float midDb, float highDb)
{
    updateCoefficients(lowDb, midDb, highDb);
}

void EQProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    lowFilter_.process(ctx);
    midFilter_.process(ctx);
    highFilter_.process(ctx);
}

void EQProcessor::updateCoefficients(float lowDb, float midDb, float highDb)
{
    *lowFilter_.coefficients = *Coeffs::makeLowShelf(
        sampleRate_, 200.0, 0.707, juce::Decibels::decibelsToGain(lowDb));
    *midFilter_.coefficients = *Coeffs::makePeakFilter(
        sampleRate_, 1000.0, 0.707, juce::Decibels::decibelsToGain(midDb));
    *highFilter_.coefficients = *Coeffs::makeHighShelf(
        sampleRate_, 6000.0, 0.707, juce::Decibels::decibelsToGain(highDb));
}
