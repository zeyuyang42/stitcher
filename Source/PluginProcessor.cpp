/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
ChaoticSonicStitcherProcessor::ChaoticSonicStitcherProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              // .withInput("SourceIn", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
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
}

void ChaoticSonicStitcherProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool ChaoticSonicStitcherProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChaoticSonicStitcherProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    [[maybe_unused]] juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    params.update();

    float* channelDataL = buffer.getWritePointer(0);
    float* channelDataR = buffer.getWritePointer(1);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        params.smoothen();

        channelDataL[sample] *= params.gain;
        channelDataR[sample] *= params.gain;
    }

    /* DeepSeek codes:
        // Feature extraction
    auto controlData = controlBuffer.getReadPointer(0);
    auto sourceData = sourceBuffer.getReadPointer(0);

    // Implement your Concat2 algorithm here
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        float controlFrame = extractFeatures(controlData, sample);
        float sourceFrame = findBestMatch(sourceData, controlFrame);

        buffer.setSample(0, sample, sourceFrame);
    }

    // Process through EQ and reverb
    applyEQ(buffer);
    applyReverb(buffer);
    */
}

//==============================================================================
bool ChaoticSonicStitcherProcessor::hasEditor() const {
    return true;  // (change this to false if you choose to not supply an
                  // editor)
}

juce::AudioProcessorEditor* ChaoticSonicStitcherProcessor::createEditor() {
    // return new ChaoticSonicStitcherEditor(*this);
    return new juce::GenericAudioProcessorEditor (*this);
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
