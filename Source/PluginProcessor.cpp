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
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      // .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
      params(apvts) {
    // do nothing
}

ChaoticSonicStitcherProcessor::~ChaoticSonicStitcherProcessor() {}

//==============================================================================
const juce::String ChaoticSonicStitcherProcessor::getName() const {
    return JucePlugin_Name;
}

bool ChaoticSonicStitcherProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ChaoticSonicStitcherProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ChaoticSonicStitcherProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ChaoticSonicStitcherProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int ChaoticSonicStitcherProcessor::getNumPrograms() {
    return 1;  // NB: some hosts don't cope very well if you tell them there are
               // 0 programs, so this should be at least 1, even if you're not
               // really implementing programs.
}

int ChaoticSonicStitcherProcessor::getCurrentProgram() { return 0; }

void ChaoticSonicStitcherProcessor::setCurrentProgram(int index) {}

const juce::String ChaoticSonicStitcherProcessor::getProgramName(int index) {
    return {};
}

void ChaoticSonicStitcherProcessor::changeProgramName(
    int index, const juce::String& newName) {}

//==============================================================================
void ChaoticSonicStitcherProcessor::prepareToPlay(double sampleRate,
                                                  int samplesPerBlock) {
    params.prepareToPlay(sampleRate);
    params.reset();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = 2;

    double numSamples = 5000.0f / 1000.0 * sampleRate;
    int maxDelayInSamples = int(std::ceil(numSamples));

    delayLineSrcL.setMaximumDelayInSamples(maxDelayInSamples);
    delayLineSrcR.setMaximumDelayInSamples(maxDelayInSamples);
    delayLineSrcL.reset();
    delayLineSrcR.reset();
}

void ChaoticSonicStitcherProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool ChaoticSonicStitcherProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn == mono && mainOut == mono) {
        return true;
    }
    if (mainIn == mono && mainOut == stereo) {
        return true;
    }
    if (mainIn == stereo && mainOut == stereo) {
        return true;
    }

    return false;
}

void ChaoticSonicStitcherProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    [[maybe_unused]] juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels that aren't needed
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    params.update();

    float sampleRate = float(getSampleRate());

    // get main input buffers
    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainInputChannels = mainInput.getNumChannels();
    auto isMainInputStereo = mainInputChannels > 1;
    const float* inputDataL = mainInput.getReadPointer(0);
    const float* inputDataR =
        mainInput.getReadPointer(isMainInputStereo ? 1 : 0);

    // get main output buffers
    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto mainOutputChannels = mainOutput.getNumChannels();
    auto isMainOutputStereo = mainOutputChannels > 1;
    float* outputDataL = mainOutput.getWritePointer(0);
    float* outputDataR = mainOutput.getWritePointer(isMainOutputStereo ? 1 : 0);

    // upper clipping 
    float maxL = 0.0f;
    float maxR = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        params.smoothen();

        float dryL = inputDataL[sample];
        float dryR = inputDataR[sample];

        // apply bypass - send dry signal direct to out
        if (params.bypassed) {
            outputDataL[sample] = dryL;
            outputDataR[sample] = dryR;
            continue;
        }


        float mono = (dryL + dryR) * 0.5f;

        delayLineSrcL.write(dryL * 0.8f);
        delayLineSrcR.write(dryR * 0.8f);
 
        float wetL = delayLineSrcL.read(0.2f);
        float wetR = delayLineSrcR.read(0.2f);

        float mixL = dryL + wetL * params.mix;
        float mixR = dryR + wetR * params.mix;

        float outL = mixL * params.gain;
        float outR = mixR * params.gain;



        outputDataL[sample] = outL;
        outputDataR[sample] = outR;

        maxL = std::max(maxL, std::abs(outL));
        maxR = std::max(maxR, std::abs(outR));
    }


    #if JUCE_DEBUG
    protectYourEars(buffer);
    #endif

    levelL.updateIfGreater(maxL);
    levelR.updateIfGreater(maxR);

}

//==============================================================================
bool ChaoticSonicStitcherProcessor::hasEditor() const {
    return true;  // (change this to false if you choose to not supply an
                  // editor)
}

juce::AudioProcessorEditor* ChaoticSonicStitcherProcessor::createEditor() {
    // return new ChaoticSonicStitcherEditor(*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ChaoticSonicStitcherProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
    copyXmlToBinary(*apvts.copyState().createXml(), destData);

    // DBG(apvts.copyState().toXmlString());
}

void ChaoticSonicStitcherProcessor::setStateInformation(const void* data,
                                                        int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ChaoticSonicStitcherProcessor();
}
