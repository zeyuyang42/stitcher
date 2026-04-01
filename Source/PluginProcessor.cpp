#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ProtectYourEars.h"

StitcherProcessor::StitcherProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output",    juce::AudioChannelSet::stereo(), true))
    , apvts_(*this, &undoManager_, "PARAMETERS", createParameterLayout())
{
    using namespace ParamIDs;
    for (auto* id : { zcrWeight, rmsWeight, scWeight, stWeight,
                      matchLen, seekTime, rand_,
                      gainCtrl, gainSrc,
                      eqLow, eqMid, eqHigh,
                      reverbRoom, reverbDamp, reverbWet,
                      gainOut, mix })
        apvts_.addParameterListener(id, this);

    apvts_.addParameterListener(freeze, this);
}

StitcherProcessor::~StitcherProcessor() {}

const juce::String StitcherProcessor::getName() const { return JucePlugin_Name; }
bool StitcherProcessor::acceptsMidi() const { return false; }
bool StitcherProcessor::producesMidi() const { return false; }
bool StitcherProcessor::isMidiEffect() const { return false; }
double StitcherProcessor::getTailLengthSeconds() const { return 5.0; }
int StitcherProcessor::getNumPrograms() { return 1; }
int StitcherProcessor::getCurrentProgram() { return 0; }
void StitcherProcessor::setCurrentProgram(int) {}
const juce::String StitcherProcessor::getProgramName(int) { return {}; }
void StitcherProcessor::changeProgramName(int, const juce::String&) {}

void StitcherProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    featureExtractor_.prepare(kFrameSize);

    float seekTime = apvts_.getRawParameterValue(ParamIDs::seekTime)->load();
    int maxFrames  = static_cast<int>(seekTime * sampleRate / kFrameSize) + 1;
    corpus_.prepare(kFrameSize, maxFrames);

    matcher_.prepare(kFrameSize);
    eq_.prepare(spec);
    reverb_.prepare(spec);
    limiter_.prepare(spec);
    limiter_.setThreshold(-1.0f);  // -1 dBFS ceiling
    limiter_.setRelease(50.0f);    // 50 ms release

    ctrlAccum_.assign(kFrameSize, 0.f);
    srcAccum_.assign(kFrameSize, 0.f);
    grainBuf_.assign(kFrameSize, 0.f);
    ctrlMono_.assign(samplesPerBlock, 0.f);
    srcMono_.assign(samplesPerBlock, 0.f);
    accumPos_   = 0;
    grainPos_   = 0;
    grainReady_ = false;

    updateMatcherFromParams();

    gainCtrl_ = juce::Decibels::decibelsToGain(
        apvts_.getRawParameterValue(ParamIDs::gainCtrl)->load());
    gainSrc_ = juce::Decibels::decibelsToGain(
        apvts_.getRawParameterValue(ParamIDs::gainSrc)->load());
    gainOut_ = juce::Decibels::decibelsToGain(
        apvts_.getRawParameterValue(ParamIDs::gainOut)->load());
    mix_    = apvts_.getRawParameterValue(ParamIDs::mix)->load() / 100.f;
    freeze_ = apvts_.getRawParameterValue(ParamIDs::freeze)->load() > 0.5f;
}

void StitcherProcessor::releaseResources() {}

bool StitcherProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    // Sidechain bus must be stereo when present
    const auto& inputBuses = layouts.inputBuses;
    if (inputBuses.size() > 1 && inputBuses[1] != juce::AudioChannelSet::stereo()
                               && !inputBuses[1].isDisabled())
        return false;
    return true;
}

void StitcherProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    if (matcherDirty_.exchange(false))
        updateMatcherFromParams();

    if (eqDirty_.exchange(false))
        eq_.setGains(
            apvts_.getRawParameterValue(ParamIDs::eqLow)->load(),
            apvts_.getRawParameterValue(ParamIDs::eqMid)->load(),
            apvts_.getRawParameterValue(ParamIDs::eqHigh)->load());

    if (reverbDirty_.exchange(false))
        reverb_.setParams(
            apvts_.getRawParameterValue(ParamIDs::reverbRoom)->load(),
            apvts_.getRawParameterValue(ParamIDs::reverbDamp)->load(),
            apvts_.getRawParameterValue(ParamIDs::reverbWet)->load());

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto sidechain = getBusBuffer(buffer, true, 1);
    auto output    = getBusBuffer(buffer, false, 0);

    const int numSamples = buffer.getNumSamples();

    corpus_.setFrozen(freeze_.load());

    // Mix stereo buses to mono for DSP processing
    // Guard against unconnected sidechain (VST3 DAWs may not connect it by default)
    const bool hasSidechain = (sidechain.getNumChannels() >= 2);
    for (int i = 0; i < numSamples; ++i) {
        ctrlMono_[i] = hasSidechain
            ? (sidechain.getReadPointer(0)[i] + sidechain.getReadPointer(1)[i]) * 0.5f
            : 0.f;
        srcMono_[i]  = (mainInput.getReadPointer(0)[i] + mainInput.getReadPointer(1)[i]) * 0.5f;
    }

    // Accumulate samples into frames; when a frame is complete, analyse + match
    for (int i = 0; i < numSamples; ++i) {
        ctrlAccum_[accumPos_] = ctrlMono_[i] * gainCtrl_.load();
        srcAccum_[accumPos_]  = srcMono_[i]  * gainSrc_.load();
        ++accumPos_;

        if (accumPos_ == kFrameSize) {
            Features srcFeatures  = featureExtractor_.extract(srcAccum_.data(),  kFrameSize);
            Features ctrlFeatures = featureExtractor_.extract(ctrlAccum_.data(), kFrameSize);

            corpus_.push(srcAccum_.data(), srcFeatures);

            const float* matched = matcher_.match(ctrlFeatures, corpus_);
            if (matched != nullptr)
                std::copy(matched, matched + kFrameSize, grainBuf_.begin());

            grainPos_   = 0;
            grainReady_ = true;
            accumPos_   = 0;
        }
    }

    // Write grain to stereo output with dry/wet mix
    const float wet = mix_.load();
    const float dry = 1.f - wet;
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);
    const float* inL = mainInput.getReadPointer(0);
    const float* inR = mainInput.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i) {
        float grain = grainReady_ ? grainBuf_[grainPos_ % kFrameSize] : 0.f;
        ++grainPos_;
        outL[i] = dry * inL[i] + wet * grain;
        outR[i] = dry * inR[i] + wet * grain;
    }

    // EQ → Reverb → output gain → limiter (scoped to output bus only — VST3 buffer includes sidechain channels)
    juce::dsp::AudioBlock<float> outputBlock(output);
    eq_.process(outputBlock);
    reverb_.process(outputBlock);
    output.applyGain(0, numSamples, gainOut_.load());
    limiter_.process(juce::dsp::ProcessContextReplacing<float>(outputBlock));

#if JUCE_DEBUG
    protectYourEars(buffer);
#endif
}

bool StitcherProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* StitcherProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void StitcherProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts_.copyState().createXml(), destData);
}

void StitcherProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

void StitcherProcessor::parameterChanged(const juce::String& id, float newValue)
{
    using namespace ParamIDs;
    if (id == zcrWeight || id == rmsWeight || id == scWeight || id == stWeight || id == rand_)
        matcherDirty_ = true;
    else if (id == gainCtrl)
        gainCtrl_ = juce::Decibels::decibelsToGain(newValue);
    else if (id == gainSrc)
        gainSrc_ = juce::Decibels::decibelsToGain(newValue);
    else if (id == gainOut)
        gainOut_ = juce::Decibels::decibelsToGain(newValue);
    else if (id == mix)
        mix_ = newValue / 100.f;
    else if (id == freeze)
        freeze_ = newValue > 0.5f;
    else if (id == eqLow || id == eqMid || id == eqHigh)
        eqDirty_ = true;
    else if (id == reverbRoom || id == reverbDamp || id == reverbWet)
        reverbDirty_ = true;
    else if (id == ParamIDs::seekTime || id == ParamIDs::matchLen)
    {
        // seekTime: corpus capacity is fixed at prepareToPlay; resize not supported at runtime.
        // matchLen: frame size is fixed at kFrameSize (1024 samples); param is reserved for future use.
    }
}

void StitcherProcessor::updateMatcherFromParams()
{
    matcher_.setWeights(
        apvts_.getRawParameterValue(ParamIDs::zcrWeight)->load(),
        apvts_.getRawParameterValue(ParamIDs::rmsWeight)->load(),
        apvts_.getRawParameterValue(ParamIDs::scWeight)->load(),
        apvts_.getRawParameterValue(ParamIDs::stWeight)->load());
    matcher_.setRand(apvts_.getRawParameterValue(ParamIDs::rand_)->load());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StitcherProcessor();
}
