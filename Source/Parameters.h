#pragma once

#include <JuceHeader.h>

// control  audio rate input, acts as control
// source   audio rate input, source for cross-synthesis
// storesize   size of source store sample buffer in seconds
// seektime    Time in seconds into the past to start searching for matches
// seekdur Time in seconds from seektime towards the present to test matches
// matchlength Match length in seconds (this will be rounded to the nearest FFT frame)
// freezestore Stop collecting novel source input, keep store (database) fixed
// zcr Weight for zero crossing rate feature
// lms Weight for log mean square amplitude feature
// sc  Weight for spectral centroid feature
// st  Weight for spectral tilt feature
// randscore   threshold


const juce::ParameterID gainPID         { "gain", 1 };
const juce::ParameterID mixPID          { "mix", 1 };

// const juce::ParameterID storeSizePID    { "storesize", 1 };
// const juce::ParameterID seekTimePID     { "seektime", 1 };
// const juce::ParameterID seekDurPID      { "seekdur", 1 };
// const juce::ParameterID matchLenPID     { "matchlength", 1 };
// const juce::ParameterID freezeStorePID  { "freezestore", 1 };

// feature parameter
// const juce::ParameterID zcrWeightPID    { "zcr", 1 }; // Zero Crossing Rate
// const juce::ParameterID lmsWeightPID    { "lms", 1 }; // log mean square amplitude
// const juce::ParameterID fltWeightPID    { "flt", 1 };  // Spectral Flatness

// const juce::ParameterID randomPID       { "rmd", 1};

const juce::ParameterID bypassPID   { "bypass", 1 };

class Parameters
{
public:
    Parameters(juce::AudioProcessorValueTreeState& apvts);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void prepareToPlay(double sampleRate) noexcept;
    void reset() noexcept;
    void update() noexcept;
    void smoothen() noexcept;

    float gain = 0.0f;
    float mix = 1.0f;

    // float storesize = 10.0f;
    // float seektime = 2.0f;
    // float seekdur = 0.5f;
    // float matchlength = 0.2f;

    // bool freezestore = false;

    bool bypassed = false; 

    // static constexpr float maxStoreSize = 10.0f;
    // static constexpr float minStareSize = 1.0f;

    juce::AudioParameterBool* bypassParam;

private:
    juce::AudioParameterFloat* gainParam;
    juce::LinearSmoothedValue<float> gainSmoother;

    juce::AudioParameterFloat* mixParam;
    juce::LinearSmoothedValue<float> mixSmoother;

    // juce::AudioParameterFloat* delayTimeParam;

    // float targetDelayTime = 0.0f;
    // float coeff = 0.0f;  // one-pole smoothing

    // juce::AudioParameterFloat* feedbackParam;
    // juce::LinearSmoothedValue<float> feedbackSmoother;

    // juce::AudioParameterFloat* stereoParam;
    // juce::LinearSmoothedValue<float> stereoSmoother;

    // juce::AudioParameterFloat* lowCutParam;
    // juce::LinearSmoothedValue<float> lowCutSmoother;

    // juce::AudioParameterFloat* highCutParam;
    // juce::LinearSmoothedValue<float> highCutSmoother;

    // juce::AudioParameterChoice* delayNoteParam;
};
