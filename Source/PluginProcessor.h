#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "FeatureExtractor.h"
#include "CorpusStore.h"
#include "ConcatenativeMatcher.h"
#include "EQProcessor.h"
#include "ReverbProcessor.h"

class StitcherProcessor : public juce::AudioProcessor,
                          public juce::AudioProcessorValueTreeState::Listener {
public:
    StitcherProcessor();
    ~StitcherProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

private:
    static constexpr int kFrameSize = 1024;

    juce::UndoManager undoManager_;
    juce::AudioProcessorValueTreeState apvts_;

    FeatureExtractor     featureExtractor_;
    CorpusStore          corpus_;
    ConcatenativeMatcher matcher_;
    EQProcessor          eq_;
    ReverbProcessor      reverb_;

    // Internal frame accumulation buffers
    std::vector<float> ctrlAccum_;
    std::vector<float> srcAccum_;
    std::vector<float> ctrlMono_;
    std::vector<float> srcMono_;
    int accumPos_ = 0;

    // Output grain buffer and playback position
    std::vector<float> grainBuf_;
    int grainPos_ = 0;
    bool grainReady_ = false;

    // Cached parameter values (atomic for audio-thread safety)
    std::atomic<float> gainCtrl_   { 1.f };
    std::atomic<float> gainSrc_    { 1.f };
    std::atomic<float> gainOut_    { 1.f };
    std::atomic<float> mix_        { 1.f };
    std::atomic<bool>  freeze_     { false };

    std::atomic<bool> eqDirty_     { false };
    std::atomic<bool> reverbDirty_ { false };

    void updateMatcherFromParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherProcessor)
};
