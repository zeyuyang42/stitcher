#pragma once

#include <JuceHeader.h>

namespace ParamIDs
{
    inline constexpr auto mix       {"mix"};
    inline constexpr auto gain      {"gain"};
    
    inline constexpr auto grainPos  {"grainPos"};
    inline constexpr auto grainSize {"grainSize"};
    inline constexpr auto interval  {"interval"};
    inline constexpr auto pitch     {"pitch"};
    inline constexpr auto width     {"width"};
}


// namespace {
  // output
  // const juce::ParameterID gainPID{"gain", 1};
  // const juce::ParameterID mixPID{"mix", 1};

  // // feature parameter
  // const juce::ParameterID zcrWeightPID{"zcr", 1};  // Zero Crossing Rate
  // const juce::ParameterID lmsWeightPID{"lms", 1};  // log mean square amplitude
  // const juce::ParameterID fltWeightPID{"flt", 1};  // Spectral Flatness

  // // const juce::ParameterID storeSizePID    { "storesize", 1 };
  // const juce::ParameterID seekTimePID{"seektime", 1};
  // const juce::ParameterID seekDurationPID{"seekdur", 1};
  // const juce::ParameterID matchLengthPID{"matchlength", 1};
  // const juce::ParameterID randomizationPID{"rmd", 1};

// }


// const juce::ParameterID freezeStorePID  { "freezestore", 1 };

// const juce::ParameterID bypassPID{"bypass", 1};

class Parameters {
  public:
    Parameters(juce::AudioProcessorValueTreeState& apvts);

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();

    void prepareToPlay(double sampleRate) noexcept;
    void reset() noexcept;
    void update() noexcept;
    void smoothen() noexcept;

    // ConcatState
    float zcrWeight = 50.0f;  // zero crossing rate weight
    float lmsWeight = 50.0f;  // loudness matching weight
    float fltWeight = 50.0f;  // spectral centroid weight

    float seekTime = 1.0f;       // time window to search for matches
    float seekDuration = 1.0f;   // duration to look ahead
    float matchLength = 0.05f;   // grain size for matching
    float randomization = 0.0f;  // randomization factor

    bool freezeStore = false;

    // output control
    float gain = 0.0f;
    float mix = 1.0f;

    bool bypassed = false;

    juce::AudioParameterBool* bypassParam;

  private:
    juce::AudioParameterFloat* gainParam;
    juce::LinearSmoothedValue<float> gainSmoother;

    juce::AudioParameterFloat* mixParam;
    juce::LinearSmoothedValue<float> mixSmoother;

    juce::AudioParameterFloat* zcrWeightParam;
    juce::LinearSmoothedValue<float> zcrWeightSmoother;

    juce::AudioParameterFloat* lmsWeightParam;
    juce::LinearSmoothedValue<float> lmsWeightSmoother;

    juce::AudioParameterFloat* fltWeightParam;
    juce::LinearSmoothedValue<float> fltWeightSmoother;

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
