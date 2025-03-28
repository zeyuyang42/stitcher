/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

//==============================================================================
/**
 */
class ChaoticSonicStitcherProcessor : public juce::AudioProcessor,
                                      public juce::AudioProcessorValueTreeState::Listener {
 public:
    //==============================================================================
    ChaoticSonicStitcherProcessor();
    ~ChaoticSonicStitcherProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // For parameter changes
    void parameterChanged(const juce::String& parameterID, float newValue) override;

 private:
    // Parameters params;
    juce::AudioProcessorValueTreeState parameters;
    juce::UndoManager undoManager;
    //==============================================================================
    // Buffer management
    std::unique_ptr<juce::AudioBuffer<float>> controlBuffer;
    std::unique_ptr<juce::AudioBuffer<float>> sourceBuffer;

    struct OutputState {
        std::atomic<float> gain;
    } outputState;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaoticSonicStitcherProcessor)
};
