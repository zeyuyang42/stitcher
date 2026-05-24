// signalsmith-linear uses Accelerate (vecLib/vecLib.h -> MacTypes.h).
// Include it first so Mac types are defined before JUCE's namespace setup,
// letting JUCE's #define Point juce_Point guard handle them properly.
#include <signalsmith-stretch/signalsmith-stretch.h>
#include "PitchShiftProcessor.h"

struct PitchShiftProcessor::Impl {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> scratch;
    int   numChannels = 2;
    float semitones   = 0.f;
};

PitchShiftProcessor::PitchShiftProcessor() : impl_(std::make_unique<Impl>()) {}
PitchShiftProcessor::~PitchShiftProcessor() = default;

void PitchShiftProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    impl_->numChannels = static_cast<int>(spec.numChannels);
    impl_->stretch.presetCheaper(impl_->numChannels, static_cast<float>(spec.sampleRate));
    impl_->stretch.setTransposeSemitones(impl_->semitones);
    impl_->scratch.setSize(impl_->numChannels, static_cast<int>(spec.maximumBlockSize));

    // Prime with silence to fill analysis lookahead, avoiding startup silence.
    const int latency = impl_->stretch.inputLatency() + impl_->stretch.outputLatency();
    if (latency > 0) {
        juce::AudioBuffer<float> silence(impl_->numChannels, latency);
        silence.clear();
        std::vector<float*> inPtrs(static_cast<size_t>(impl_->numChannels));
        std::vector<float*> outPtrs(static_cast<size_t>(impl_->numChannels));
        for (int c = 0; c < impl_->numChannels; ++c) {
            inPtrs[static_cast<size_t>(c)]  = silence.getWritePointer(c);
            outPtrs[static_cast<size_t>(c)] = silence.getWritePointer(c);
        }
        impl_->stretch.process(inPtrs.data(), latency, outPtrs.data(), latency);
    }
}

void PitchShiftProcessor::setSemitones(float semitones)
{
    impl_->semitones = semitones;
    impl_->stretch.setTransposeSemitones(static_cast<double>(semitones));
}

void PitchShiftProcessor::process(juce::dsp::AudioBlock<float>& block)
{
    if (impl_->semitones == 0.f) return;

    const int numSamples = static_cast<int>(block.getNumSamples());
    const int numCh      = impl_->numChannels;

    std::vector<float*> inPtrs(static_cast<size_t>(numCh));
    std::vector<float*> outPtrs(static_cast<size_t>(numCh));
    for (int c = 0; c < numCh; ++c) {
        inPtrs[static_cast<size_t>(c)]  = block.getChannelPointer(static_cast<size_t>(c));
        outPtrs[static_cast<size_t>(c)] = impl_->scratch.getWritePointer(c);
    }

    impl_->stretch.process(inPtrs.data(), numSamples, outPtrs.data(), numSamples);

    for (int c = 0; c < numCh; ++c)
        juce::FloatVectorOperations::copy(
            block.getChannelPointer(static_cast<size_t>(c)),
            impl_->scratch.getReadPointer(c), numSamples);
}
