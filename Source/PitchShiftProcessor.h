#pragma once
#include <JuceHeader.h>
#include <memory>

// signalsmith-stretch is included only in the .cpp to avoid the MacTypes.h
// Point ambiguity that occurs when its STFT headers mix with JUCE's global namespace.
class PitchShiftProcessor {
public:
    PitchShiftProcessor();
    ~PitchShiftProcessor();
    PitchShiftProcessor(const PitchShiftProcessor&)            = delete;
    PitchShiftProcessor& operator=(const PitchShiftProcessor&) = delete;

    void prepare(const juce::dsp::ProcessSpec& spec);
    // semitones in [-24, 24]; 0 = bypass
    void setSemitones(float semitones);
    void process(juce::dsp::AudioBlock<float>& block);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
