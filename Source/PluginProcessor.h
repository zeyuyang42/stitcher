#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "FeatureExtractor.h"
#include "CorpusStore.h"
#include "ConcatenativeMatcher.h"
#include "EQProcessor.h"
#include "ReverbProcessor.h"
#include "MidiLearn.h"

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
    MidiLearn& getMidiLearn() { return midiLearn_; }

    float getLastCtrlZcr()    const noexcept { return lastCtrlZcr_.load(); }
    float getLastCtrlRms()    const noexcept { return lastCtrlRms_.load(); }
    float getLastCtrlSc()     const noexcept { return lastCtrlSc_.load(); }
    float getLastCtrlSt()     const noexcept { return lastCtrlSt_.load(); }
    float    getLastCorpusFill()   const noexcept { return lastCorpusFill_.load(); }
    int      getLastMatchedIndex() const noexcept { return lastMatchedIndex_.load(); }
    uint32_t getMatchEpoch()       const noexcept { return matchEpoch_.load(); }
    float getLastOutPeakL()      const noexcept { return lastOutPeakL_.load(); }
    float getLastOutPeakR()      const noexcept { return lastOutPeakR_.load(); }

private:
    int frameSize_ = 1024;  // set in prepareToPlay from matchLen parameter
    std::atomic<double> lastKnownBpm_ { 120.0 };

    juce::UndoManager undoManager_;
    juce::AudioProcessorValueTreeState apvts_;

    MidiLearn            midiLearn_;

    FeatureExtractor     featureExtractor_;
    CorpusStore          corpus_;
    ConcatenativeMatcher matcher_;
    EQProcessor          eq_;
    ReverbProcessor      reverb_;
    juce::dsp::Limiter<float> limiter_;

    // Internal frame accumulation buffers
    std::vector<float> ctrlAccum_;
    std::vector<float> srcAccum_;   // mono mix for feature extraction
    std::vector<float> srcAccumL_;  // stereo L for corpus audio (raw, gainSrc_ applied post-extraction)
    std::vector<float> srcAccumR_;  // stereo R for corpus audio
    std::vector<float> ctrlMono_;
    std::vector<float> srcMono_;
    int accumPos_ = 0;

    // Two-voice OLA grain engine — envelope baked into grain buffer at load time
    struct GrainVoice {
        std::vector<float> bufL, bufR;
        int  pos    = 0;
        bool active = false;
    };

    std::atomic<int>         xfadeLenSamples_ { 256 };
    GrainVoice               voices_[2];
    int                      activeSlot_ = 0;
    bool                     grainReady_ = false;
    juce::AudioBuffer<float> grainMixBuf_;  // 2-ch, samplesPerBlock — grain staging for grain-only EQ

    // Cached parameter values (atomic for audio-thread safety)
    std::atomic<float> gainCtrl_   { 1.f };
    std::atomic<float> gainSrc_    { 1.f };
    std::atomic<float> gainOut_    { 1.f };
    std::atomic<float> mix_        { 1.f };
    std::atomic<bool>  freeze_     { false };

    std::atomic<bool> eqDirty_      { false };
    std::atomic<bool> reverbDirty_  { false };
    std::atomic<bool> matcherDirty_ { false };

    // UI observability — written on audio thread, read by message-thread timer ~30 Hz
    std::atomic<float> lastCtrlZcr_    { 0.f };
    std::atomic<float> lastCtrlRms_    { 0.f };
    std::atomic<float> lastCtrlSc_     { 0.f };
    std::atomic<float> lastCtrlSt_     { 0.f };
    std::atomic<float>    lastCorpusFill_   { 0.f };
    std::atomic<int>      lastMatchedIndex_ { -1 };
    std::atomic<uint32_t> matchEpoch_       { 0 };
    std::atomic<float> lastOutPeakL_   { 0.f };
    std::atomic<float> lastOutPeakR_   { 0.f };
    int corpusMaxFrames_ = 1;

    void updateMatcherFromParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherProcessor)
};
