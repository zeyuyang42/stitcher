#include "DSP.h"
#include "Parameters.h"

template <typename T>
static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination) {
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination);  // parameter does not exist or wrong type
}

static juce::String stringFromMilliseconds(float value, int) {
    if (value < 10.0f) {
        return juce::String(value, 2) + " ms";
    } else if (value < 100.0f) {
        return juce::String(value, 1) + " ms";
    } else if (value < 1000.0f) {
        return juce::String(int(value)) + " ms";
    } else {
        return juce::String(value * 0.001f, 2) + " s";
    }
}

static float millisecondsFromString(const juce::String& text) {
    float value = text.getFloatValue();
    if (!text.endsWithIgnoreCase("ms")) {
        // if (text.endsWithIgnoreCase("s") || value < Parameters::minDelayTime)
        // {
        if (text.endsWithIgnoreCase("s")) {
            return value * 1000.0f;
        }
    }
    return value;
}

static juce::String stringFromDecibels(float value, int) { return juce::String(value, 1) + " dB"; }

static juce::String stringFromPercent(float value, int) { return juce::String(int(value)) + " %"; }

static juce::String stringFromHz(float value, int) {
    if (value < 1000.0f) {
        return juce::String(int(value)) + " Hz";
    } else if (value < 10000.0f) {
        return juce::String(value / 1000.0f, 2) + " k";
    } else {
        return juce::String(value / 1000.0f, 1) + " k";
    }
}

static float hzFromString(const juce::String& str) {
    float value = str.getFloatValue();
    if (value < 20.0f) {
        return value * 1000.0f;
    }
    return value;
}

Parameters::Parameters(juce::AudioProcessorValueTreeState& apvts) {
    castParameter(apvts, gainPID, gainParam);
    castParameter(apvts, mixPID, mixParam);
    castParameter(apvts, bypassPID, bypassParam);

    castParameter(apvts, zcrWeightPID, zcrWeightParam);
    castParameter(apvts, lmsWeightPID, lmsWeightParam);
    castParameter(apvts, fltWeightPID, fltWeightParam);
}

juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        gainPID, "Output Gain", juce::NormalisableRange<float>{-12.0f, 12.0f}, 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromDecibels)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        mixPID, "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));

    layout.add(std::make_unique<juce::AudioParameterBool>(bypassPID, "Bypass", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        zcrWeightPID, "zcr", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        lmsWeightPID, "lms", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        fltWeightPID, "flt", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));

    // layout.add(std::make_unique<juce::AudioParameterFloat>(
    //     delayTimeParamID,
    //     "Delay Time",
    //     juce::NormalisableRange<float> { minDelayTime, maxDelayTime, 0.001f,
    //     0.25f }, 100.0f, juce::AudioParameterFloatAttributes()
    //         .withStringFromValueFunction(stringFromMilliseconds)
    //         .withValueFromStringFunction(millisecondsFromString)
    // ));

    // layout.add(std::make_unique<juce::AudioParameterFloat>(
    //     feedbackParamID,
    //     "Feedback",
    //     juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
    //     0.0f,
    //     juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)
    // ));

    // layout.add(std::make_unique<juce::AudioParameterFloat>(
    //     stereoParamID,
    //     "Stereo",
    //     juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
    //     0.0f,
    //     juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)
    // ));

    // layout.add(std::make_unique<juce::AudioParameterFloat>(
    //     lowCutParamID,
    //     "Low Cut",
    //     juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f),
    //     20.0f,
    //     juce::AudioParameterFloatAttributes()
    //         .withStringFromValueFunction(stringFromHz)
    //         .withValueFromStringFunction(hzFromString)
    // ));

    // layout.add(std::make_unique<juce::AudioParameterFloat>(
    //     highCutParamID,
    //     "High Cut",
    //     juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f),
    //     20000.0f,
    //     juce::AudioParameterFloatAttributes()
    //         .withStringFromValueFunction(stringFromHz)
    //         .withValueFromStringFunction(hzFromString)
    // ));

    // layout.add(std::make_unique<juce::AudioParameterBool>(
    //     tempoSyncParamID, "Tempo Sync", false));

    return layout;
}

void Parameters::prepareToPlay(double sampleRate) noexcept {
    double duration = 0.02;
    gainSmoother.reset(sampleRate, duration);

    // coeff = 1.0f - std::exp(-1.0f / (0.2f * float(sampleRate)));

    mixSmoother.reset(sampleRate, duration);
    // feedbackSmoother.reset(sampleRate, duration);
    // stereoSmoother.reset(sampleRate, duration);
    // lowCutSmoother.reset(sampleRate, duration);
    // highCutSmoother.reset(sampleRate, duration);
}

void Parameters::reset() noexcept {
    gain = 0.0f;
    gainSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));

    // delayTime = 0.0f;

    mix = 1.0f;
    mixSmoother.setCurrentAndTargetValue(mixParam->get() * 0.01f);

    zcrWeight = 50.0f;
    zcrWeightSmoother.setCurrentAndTargetValue(zcrWeightParam->get());

    lmsWeight = 50.0f;
    lmsWeightSmoother.setCurrentAndTargetValue(lmsWeightParam->get());

    zcrWeight = 50.0f;
    zcrWeightSmoother.setCurrentAndTargetValue(zcrWeightParam->get());

    // panL = 0.0f;
    // panR = 1.0f;
    // stereoSmoother.setCurrentAndTargetValue(stereoParam->get() * 0.01f);

    // lowCut = 20.0f;
    // lowCutSmoother.setCurrentAndTargetValue(lowCutParam->get());

    // highCut = 20000.0f;
    // highCutSmoother.setCurrentAndTargetValue(highCutParam->get());
}

void Parameters::update() noexcept {
    gainSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));

    // targetDelayTime = delayTimeParam->get();
    // if (delayTime == 0.0f) {
    //     delayTime = targetDelayTime;
    // }

    mixSmoother.setTargetValue(mixParam->get() * 0.01f);
    // feedbackSmoother.setTargetValue(feedbackParam->get() * 0.01f);
    // stereoSmoother.setTargetValue(stereoParam->get() * 0.01f);
    // lowCutSmoother.setTargetValue(lowCutParam->get());
    // highCutSmoother.setTargetValue(highCutParam->get());

    // delayNote = delayNoteParam->getIndex();
    // tempoSync = tempoSyncParam->get();
    bypassed = bypassParam->get();
}

void Parameters::smoothen() noexcept {
    gain = gainSmoother.getNextValue();

    // delayTime += (targetDelayTime - delayTime) * coeff;

    // For crossfading and ducking:
    // delayTime = targetDelayTime;

    mix = mixSmoother.getNextValue();
    // feedback = feedbackSmoother.getNextValue();

    // panningEqualPower(stereoSmoother.getNextValue(), panL, panR);

    // lowCut = lowCutSmoother.getNextValue();
    // highCut = highCutSmoother.getNextValue();
}
