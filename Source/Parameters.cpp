#include "DSP.h"
#include "Parameters.h"

template <typename T>
static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination); // parameter does not exist or wrong type
}

static juce::String stringFromMilliseconds(float value, int)
{
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

static float millisecondsFromString(const juce::String& text)
{
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

static juce::String stringFromHz(float value, int)
{
    if (value < 1000.0f) {
        return juce::String(int(value)) + " Hz";
    } else if (value < 10000.0f) {
        return juce::String(value / 1000.0f, 2) + " k";
    } else {
        return juce::String(value / 1000.0f, 1) + " k";
    }
}

static float hzFromString(const juce::String& str)
{
    float value = str.getFloatValue();
    if (value < 20.0f) {
        return value * 1000.0f;
    }
    return value;
}

Parameters::Parameters(juce::AudioProcessorValueTreeState& apvts)
{
    castParameter(apvts, gainPID, gainParam);
    castParameter(apvts, mixPID, mixParam);
    castParameter(apvts, bypassPID, bypassParam);

    castParameter(apvts, zcrWeightPID, zcrWeightParam);
    castParameter(apvts, lmsWeightPID, lmsWeightParam);
    castParameter(apvts, fltWeightPID, fltWeightParam);
}

static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto msFormat = [](float value, int) {
        if (value < 100.0f)
            return juce::String(value, 1) + " ms";
        else
            return juce::String(std::roundf(value)) + " ms";
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::interval, 1 },
        "intv",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.01f, 0.405f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        msFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::pitch, 1 }, ParamIDs::pitch, juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f, 1.0f), 0.0f, juce::String(), juce::AudioProcessorParameter::genericParameter, [](float value, int) { return juce::String(value, 1) + " st"; }, nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::grainPos, 1 },
        "pos",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f, 0.405f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        msFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::grainSize, 1 },
        "size",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.01f, 0.405f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        msFormat,
        nullptr));

    auto percentFormat = [](float value, int) {
        if (value < 10.0f)
            return juce::String(value, 2) + " %";
        else if (value < 100.0f)
            return juce::String(value, 1) + " %";
        else
            return juce::String(value, 0) + " %";
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::mix, 1 },
        ParamIDs::mix,
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::width, 1 },
        ParamIDs::width,
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f),
        50.0,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ParamIDs::gain, 1 }, "vol", juce::NormalisableRange<float>(-36.0f, 12.0f, 0.1f, 2.4f), 0.0f, juce::String(), juce::AudioProcessorParameter::genericParameter, [](float value, int) {
                                                             if (-10.0f < value && value < 10.0f)
                                                                 return juce::String (value, 1) + " dB";
                                                             else
                                                                 return juce::String (std::roundf (value), 0) + " dB"; }, nullptr));

    return layout;
}

void Parameters::prepareToPlay(double sampleRate) noexcept
{
    double duration = 0.02;
    gainSmoother.reset(sampleRate, duration);

    mixSmoother.reset(sampleRate, duration);
}

void Parameters::reset() noexcept
{
    gain = 0.0f;
    gainSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));

    mix = 1.0f;
    mixSmoother.setCurrentAndTargetValue(mixParam->get() * 0.01f);

    zcrWeight = 50.0f;
    zcrWeightSmoother.setCurrentAndTargetValue(zcrWeightParam->get());

    lmsWeight = 50.0f;
    lmsWeightSmoother.setCurrentAndTargetValue(lmsWeightParam->get());

    zcrWeight = 50.0f;
    zcrWeightSmoother.setCurrentAndTargetValue(zcrWeightParam->get());
}

void Parameters::update() noexcept
{
    gainSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));

    mixSmoother.setTargetValue(mixParam->get() * 0.01f);
    bypassed = bypassParam->get();
}

void Parameters::smoothen() noexcept
{
    gain = gainSmoother.getNextValue();
    mix = mixSmoother.getNextValue();
}
