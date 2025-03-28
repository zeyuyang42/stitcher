#pragma once

#include <JuceHeader.h>

namespace ParamIDs {

inline constexpr auto mix { "mix" };
inline constexpr auto gain { "gain" };

inline constexpr auto grainPos { "grainPos" };
inline constexpr auto grainSize { "grainSize" };
inline constexpr auto interval { "interval" };
inline constexpr auto pitch { "pitch" };
inline constexpr auto width { "width" };

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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::interval, 1 },
        "intv",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.01f, 0.405f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        msFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::pitch, 1 },
        ParamIDs::pitch,
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f, 1.0f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " st"; },
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::grainPos, 1 },
        "pos",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f, 0.405f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        msFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::grainSize, 1 },
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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::mix, 1 },
        ParamIDs::mix,
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::width, 1 },
        ParamIDs::width,
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f),
        50.0,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        percentFormat,
        nullptr));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::gain, 1 },
        "vol",
        juce::NormalisableRange<float>(-36.0f, 12.0f, 0.1f, 2.4f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            if (-10.0f < value && value < 10.0f)
                return juce::String(value, 1) + " dB";
            else
                return juce::String(std::roundf(value), 0) + " dB";
        },
        nullptr));

    return layout;
}