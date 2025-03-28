/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ProtectYourEars.h"

//==============================================================================
ChaoticSonicStitcherProcessor::ChaoticSonicStitcherProcessor()
    : AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    parameters.addParameterListener(ParamIDs::gain, this);
}

ChaoticSonicStitcherProcessor::~ChaoticSonicStitcherProcessor() { }

//==============================================================================
const juce::String ChaoticSonicStitcherProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChaoticSonicStitcherProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ChaoticSonicStitcherProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ChaoticSonicStitcherProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ChaoticSonicStitcherProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChaoticSonicStitcherProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are
              // 0 programs, so this should be at least 1, even if you're not
              // really implementing programs.
}

int ChaoticSonicStitcherProcessor::getCurrentProgram()
{
    return 0;
}

void ChaoticSonicStitcherProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ChaoticSonicStitcherProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ChaoticSonicStitcherProcessor::changeProgramName(int index,
    const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void ChaoticSonicStitcherProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = 2;

    outputState.gain = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(ParamIDs::gain)->load());
}

void ChaoticSonicStitcherProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool ChaoticSonicStitcherProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    // const auto mono = juce::AudioChannelSet::mono();
    // const auto stereo = juce::AudioChannelSet::stereo();
    // const auto mainIn = layouts.getMainInputChannelSet();
    // const auto mainOut = layouts.getMainOutputChannelSet();

    // if (mainIn == mono && mainOut == mono) {
    //     return true;
    // }
    // if (mainIn == mono && mainOut == stereo) {
    //     return true;
    // }
    // if (mainIn == stereo && mainOut == stereo) {
    //     return true;
    // }

    // return false;

    // test
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && !layouts.getMainInputChannelSet().isDisabled();
}

void ChaoticSonicStitcherProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    // We don't need midi
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels that aren't needed
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());



    auto mainInput = getBusBuffer(buffer, true, 0);
    const float* inputData = mainInput.getReadPointer(0);

    auto sideChain = getBusBuffer(buffer, true, 1);
    const float* sideData = sideChain.getReadPointer(0);

    auto mainOutput = getBusBuffer(buffer, false, 0);
    float* outputData = mainOutput.getWritePointer(0);


    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) 
    {
        // test adding sidechain signal into output signal
        outputData[sample] = (inputData[sample] + sideData[sample]) * 0.5f;
    }

    // apply gain to the final output
    buffer.applyGain(outputState.gain);

#if JUCE_DEBUG
    protectYourEars(buffer);
#endif
}

//==============================================================================
bool ChaoticSonicStitcherProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ChaoticSonicStitcherProcessor::createEditor()
{
    // return new ChaoticSonicStitcherEditor(*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ChaoticSonicStitcherProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*parameters.copyState().createXml(), destData);

    // DBG(apvts.copyState().toXmlString());
}

void ChaoticSonicStitcherProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(parameters.state.getType())) {
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

void ChaoticSonicStitcherProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamIDs::gain) {
        outputState.gain = juce::Decibels::decibelsToGain(newValue);
    }

    // Handle parameter changes here
    // The parameterID identifies which parameter changed, and newValue is its new value
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChaoticSonicStitcherProcessor();
}
