#pragma once
#include <juce_dsp/juce_dsp.h>

class EQProcessor {
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    // gains in dB: lowDb = low shelf (200 Hz), midDb = mid peak (1 kHz), highDb = high shelf (6 kHz)
    void setGains(float lowDb, float midDb, float highDb);
    void process(juce::dsp::AudioBlock<float>& block);

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    Filter lowFilter_, midFilter_, highFilter_;
    double sampleRate_ = 44100.0;

    void updateCoefficients(float lowDb, float midDb, float highDb);
};
