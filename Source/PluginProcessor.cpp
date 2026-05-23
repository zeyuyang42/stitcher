#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ProtectYourEars.h"

namespace {
// Returns the power of two nearest to x, clamped to [64, 8192].
int nearestPow2(double x)
{
    const int raw = static_cast<int>(x + 0.5);
    if (raw <= 64)   return 64;
    if (raw >= 8192) return 8192;
    int p = 64;
    while (p * 2 <= raw) p <<= 1;
    // p = largest power of 2 <= raw; p*2 = smallest power of 2 > raw
    return ((raw - p) <= (p * 2 - raw)) ? p : p * 2;
}
} // namespace

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
                      matchLenSync, matchLenDiv,
                      gainCtrl, gainSrc,
                      eqLow, eqMid, eqHigh,
                      reverbRoom, reverbDamp, reverbWet,
                      gainOut, mix, xfade })
        apvts_.addParameterListener(id, this);

    apvts_.addParameterListener(freeze, this);

    // Register all APVTS parameters with MidiLearn.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            midiLearn_.registerParam(rp);
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
    // Compute frame size from matchLen parameter (nearest power of 2, load-time only)
    const bool sync = apvts_.getRawParameterValue(ParamIDs::matchLenSync)->load() > 0.5f;
    if (sync) {
        static const double kDivisions[] = { 0.0625, 0.125, 0.25, 0.375, 0.5, 1.0, 2.0 };
        const int divIdx = juce::jlimit(0, 6,
            static_cast<int>(apvts_.getRawParameterValue(ParamIDs::matchLenDiv)->load()));
        const double bpm = lastKnownBpm_.load(std::memory_order_relaxed);
        frameSize_ = nearestPow2(kDivisions[divIdx] * 60.0 / bpm * sampleRate);
    } else {
        float matchLenMs = apvts_.getRawParameterValue(ParamIDs::matchLen)->load();
        frameSize_ = nearestPow2(static_cast<double>(matchLenMs) * sampleRate / 1000.0);
    }
    // Clamp xfade length to [0, frameSize_-1]
    {
        const float xfadeRaw = apvts_.getRawParameterValue(ParamIDs::xfade)->load();
        xfadeLenSamples_.store(
            juce::jlimit(0, frameSize_ - 1, static_cast<int>(xfadeRaw * 256.f)));
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    featureExtractor_.prepare(frameSize_);

    float seekTime = apvts_.getRawParameterValue(ParamIDs::seekTime)->load();
    int maxFrames  = static_cast<int>(seekTime * sampleRate / frameSize_) + 1;
    corpus_.prepare(frameSize_, maxFrames);
    corpusMaxFrames_ = std::max(1, maxFrames);

    matcher_.prepare(frameSize_);
    eq_.prepare(spec);
    reverb_.prepare(spec);
    limiter_.prepare(spec);
    limiter_.setThreshold(-1.0f);  // -1 dBFS ceiling
    limiter_.setRelease(50.0f);    // 50 ms release

    ctrlAccum_.assign(frameSize_, 0.f);
    srcAccum_.assign(frameSize_, 0.f);
    srcAccumL_.assign(frameSize_, 0.f);
    srcAccumR_.assign(frameSize_, 0.f);
    currentGrainL_.assign(frameSize_, 0.f);
    currentGrainR_.assign(frameSize_, 0.f);
    nextGrainL_.assign(frameSize_, 0.f);
    nextGrainR_.assign(frameSize_, 0.f);
    ctrlMono_.assign(samplesPerBlock, 0.f);
    srcMono_.assign(samplesPerBlock, 0.f);
    grainMixBuf_.setSize(2, samplesPerBlock, false, true, false);
    accumPos_   = 0;
    grainPos_   = 0;
    xfadePos_   = 0;
    xfading_    = false;
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

    lastCtrlZcr_.store(0.f);  lastCtrlRms_.store(0.f);
    lastCtrlSc_.store(0.f);   lastCtrlSt_.store(0.f);
    lastCorpusFill_.store(0.f);
    lastOutPeakL_.store(0.f); lastOutPeakR_.store(0.f);
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
    juce::ScopedNoDenormals noDenormals;

    // Dispatch MIDI CC messages to MidiLearn (handles both learn capture and dispatch).
    for (const auto meta : midiMessages) {
        const auto& msg = meta.getMessage();
        if (msg.isController())
            midiLearn_.handleCC(msg.getControllerNumber(), msg.getControllerValue());
    }

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm().hasValue())
                lastKnownBpm_.store(*pos->getBpm(), std::memory_order_relaxed);

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
        srcAccum_[accumPos_]  = srcMono_[i];   // raw mono — gainSrc_ applied post-extraction
        srcAccumL_[accumPos_] = mainInput.getReadPointer(0)[i];
        srcAccumR_[accumPos_] = mainInput.getReadPointer(1)[i];
        ++accumPos_;

        if (accumPos_ == frameSize_) {
            Features srcFeatures  = featureExtractor_.extract(srcAccum_.data(),  frameSize_);
            Features ctrlFeatures = featureExtractor_.extract(ctrlAccum_.data(), frameSize_);

            // Publish ctrl features for UI meters
            lastCtrlZcr_.store(ctrlFeatures.zcr);
            lastCtrlRms_.store(ctrlFeatures.rms);
            lastCtrlSc_.store(ctrlFeatures.sc);
            lastCtrlSt_.store(ctrlFeatures.st);

            // Apply gainSrc_ to corpus audio AFTER feature extraction so features
            // reflect raw source amplitude — independent of the playback gain knob.
            const float gs = gainSrc_.load();
            if (gs != 1.f) {
                for (int j = 0; j < frameSize_; ++j) {
                    srcAccumL_[j] *= gs;
                    srcAccumR_[j] *= gs;
                }
            }

            corpus_.push(srcAccumL_.data(), srcAccumR_.data(), srcFeatures);
            lastCorpusFill_.store(
                static_cast<float>(corpus_.size()) / static_cast<float>(corpusMaxFrames_));

            const float* matchedL = nullptr, *matchedR = nullptr;
            if (matcher_.match(ctrlFeatures, corpus_, matchedL, matchedR)) {
                if (!grainReady_) {
                    // First grain: load directly and start playing from position 0
                    std::copy(matchedL, matchedL + frameSize_, currentGrainL_.begin());
                    std::copy(matchedR, matchedR + frameSize_, currentGrainR_.begin());
                    grainPos_   = 0;
                    grainReady_ = true;
                } else {
                    // Subsequent grains: start position-aligned crossfade
                    // grainPos_ is NOT reset — we continue from the current playback position
                    std::copy(matchedL, matchedL + frameSize_, nextGrainL_.begin());
                    std::copy(matchedR, matchedR + frameSize_, nextGrainR_.begin());
                    xfadePos_ = 0;
                    xfading_  = true;
                }
            }
            accumPos_ = 0;
        }
    }

    // Render grain into a separate buffer so EQ applies to grain only (not the dry path).
    // outL/inL share memory (JUCE in-place); we cannot write to output and then re-read dry.
    float* grainL = grainMixBuf_.getWritePointer(0);
    float* grainR = grainMixBuf_.getWritePointer(1);

    const int xfadeLen = xfadeLenSamples_.load();
    for (int i = 0; i < numSamples; ++i) {
        float gL = 0.f, gR = 0.f;
        if (grainReady_) {
            const int pos = grainPos_;
            if (xfading_) {
                if (xfadeLen == 0) {
                    // Zero crossfade: snap immediately to next grain (intentional click)
                    std::copy(nextGrainL_.begin(), nextGrainL_.end(), currentGrainL_.begin());
                    std::copy(nextGrainR_.begin(), nextGrainR_.end(), currentGrainR_.begin());
                    xfading_ = false;
                    gL = currentGrainL_[pos];
                    gR = currentGrainR_[pos];
                } else {
                    const float t = static_cast<float>(xfadePos_) / static_cast<float>(xfadeLen);
                    gL = (1.f - t) * currentGrainL_[pos] + t * nextGrainL_[pos];
                    gR = (1.f - t) * currentGrainR_[pos] + t * nextGrainR_[pos];
                    if (++xfadePos_ >= xfadeLen) {
                        std::copy(nextGrainL_.begin(), nextGrainL_.end(), currentGrainL_.begin());
                        std::copy(nextGrainR_.begin(), nextGrainR_.end(), currentGrainR_.begin());
                        xfading_ = false;
                    }
                }
            } else {
                gL = currentGrainL_[pos];
                gR = currentGrainR_[pos];
            }
            if (++grainPos_ >= frameSize_) grainPos_ = 0;
        }
        grainL[i] = gL;
        grainR[i] = gR;
    }

    // Apply EQ to grain signal only (grain is mono so L=R; EQ processes both channels independently)
    juce::dsp::AudioBlock<float> grainBlock(grainMixBuf_);
    eq_.process(grainBlock);

    // Blend EQ'd grain with dry main input into output
    const float wet = mix_.load();
    const float dry = 1.f - wet;
    float* outL = output.getWritePointer(0);
    float* outR = output.getWritePointer(1);
    const float* inL = mainInput.getReadPointer(0);
    const float* inR = mainInput.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i) {
        outL[i] = dry * inL[i] + wet * grainL[i];
        outR[i] = dry * inR[i] + wet * grainR[i];
    }

    // Reverb → output gain → limiter (processes the final blended output)
    juce::dsp::AudioBlock<float> outputBlock(output);
    reverb_.process(outputBlock);
    output.applyGain(0, numSamples, gainOut_.load());
    limiter_.process(juce::dsp::ProcessContextReplacing<float>(outputBlock));

    // Update output peak atomics for the level meter
    {
        const float* pl = output.getReadPointer(0);
        const float* pr = output.getReadPointer(1);
        float pkL = 0.f, pkR = 0.f;
        for (int i = 0; i < numSamples; ++i) {
            pkL = std::max(pkL, std::abs(pl[i]));
            pkR = std::max(pkR, std::abs(pr[i]));
        }
        // Decay: ~100ms time constant, independent of block size
        const float blockDecay = std::exp(
            -static_cast<float>(numSamples) / (static_cast<float>(getSampleRate()) * 0.1f));
        lastOutPeakL_.store(std::max(pkL, lastOutPeakL_.load(std::memory_order_relaxed) * blockDecay));
        lastOutPeakR_.store(std::max(pkR, lastOutPeakR_.load(std::memory_order_relaxed) * blockDecay));
    }

#if JUCE_DEBUG
    protectYourEars(buffer);
#endif
}

bool StitcherProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* StitcherProcessor::createEditor()
{
    return new StitcherEditor(*this);
}

void StitcherProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto rootXml = apvts_.copyState().createXml();
    rootXml->addChildElement(midiLearn_.createBindingsXml()); // owned by rootXml
    copyXmlToBinary(*rootXml, destData);
}

void StitcherProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml)
        return;

    // Restore APVTS state (the root element is the APVTS state tree).
    if (xml->hasTagName(apvts_.state.getType())) {
        // Extract and remove MIDI_LEARN child before handing to APVTS
        // (APVTS ignores unknown children, but let's be explicit).
        auto* mlXml = xml->getChildByName("MIDI_LEARN");
        if (mlXml != nullptr)
            midiLearn_.restoreBindingsXml(mlXml, getParameters());

        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }
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
    else if (id == ParamIDs::xfade)
        xfadeLenSamples_.store(
            juce::jlimit(0, frameSize_ - 1, static_cast<int>(newValue * 256.f)));
    else if (id == ParamIDs::seekTime || id == ParamIDs::matchLen)
    {
        // seekTime and matchLen take effect on next prepareToPlay (plugin reload).
        // Runtime resize of circular buffers is not supported.
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
