/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "DelayLine.h"
#include "Parameters.h"

//==============================================================================
/**
 */
class ChaoticSonicStitcherProcessor : public juce::AudioProcessor {
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

    // juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "Parameters", Parameters::createParameterLayout()};

    Parameters params;

 private:
    
    // DelayLine delayLineCtrL, delayLineCtrR;
    DelayLine delayLineSrcL, delayLineSrcR;

    // DelayLine delayLineCtrL, delayLineSrcL;
    // juce::AudioBuffer<float> featureBuffer;
    // bool freezeState = false;

    // Effect chain
    // juce::dsp::ProcessorChain<
    //     juce::dsp::IIR::Filter<float>,
    //     juce::dsp::IIR::Filter<float>,
    //     juce::dsp::IIR::Filter<float>,
    // > eqChain;

    juce::dsp::Reverb reverbProcessor;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChaoticSonicStitcherProcessor)
};
