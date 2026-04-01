#pragma once

#include <JuceHeader.h>

namespace ParamIDs {
    // Concatenative
    inline constexpr auto zcrWeight  { "zcr_weight" };
    inline constexpr auto rmsWeight  { "rms_weight" };
    inline constexpr auto scWeight   { "sc_weight" };
    inline constexpr auto stWeight   { "st_weight" };
    inline constexpr auto matchLen   { "match_len" };
    inline constexpr auto seekTime   { "seek_time" };
    inline constexpr auto rand_      { "rand" };
    inline constexpr auto freeze     { "freeze" };
    inline constexpr auto gainCtrl   { "gain_ctrl" };
    inline constexpr auto gainSrc    { "gain_src" };
    // EQ
    inline constexpr auto eqLow      { "eq_low" };
    inline constexpr auto eqMid      { "eq_mid" };
    inline constexpr auto eqHigh     { "eq_high" };
    // Reverb
    inline constexpr auto reverbRoom { "reverb_room" };
    inline constexpr auto reverbDamp { "reverb_damp" };
    inline constexpr auto reverbWet  { "reverb_wet" };
    // Output
    inline constexpr auto gainOut    { "gain_out" };
    inline constexpr auto mix        { "mix" };
}

static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbFormat = [](float v, int) {
        return String(v, 1) + " dB";
    };
    auto pctFormat = [](float v, int) {
        return String(v, 0) + " %";
    };
    auto secFormat = [](float v, int) {
        return String(v, 2) + " s";
    };
    auto msFormat = [](float v, int) {
        return String(v, 0) + " ms";
    };

    // Concatenative
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::zcrWeight, 1}, "ZCR",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::rmsWeight, 1}, "RMS",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::scWeight, 1}, "S.Centroid",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::stWeight, 1}, "S.Tilt",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::matchLen, 1}, "Match Len (fixed ~23ms)",
        NormalisableRange<float>(10.f, 100.f, 1.f), 50.f,
        String(), AudioProcessorParameter::genericParameter,
        msFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::seekTime, 1}, "Seek Time (set before load)",
        NormalisableRange<float>(1.f, 5.f, 0.01f), 1.f,
        String(), AudioProcessorParameter::genericParameter,
        secFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::rand_, 1}, "Rand",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterBool>(
        ParameterID{ParamIDs::freeze, 1}, "Freeze", false));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::gainCtrl, 1}, "Ctrl Gain",
        NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::gainSrc, 1}, "Src Gain",
        NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    // EQ
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::eqLow, 1}, "Low",
        NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::eqMid, 1}, "Mid",
        NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::eqHigh, 1}, "High",
        NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    // Reverb
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::reverbRoom, 1}, "Room",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::reverbDamp, 1}, "Damp",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.5f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::reverbWet, 1}, "Wet",
        NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        [](float v, int) { return String(v, 2); }, nullptr));

    // Output
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::gainOut, 1}, "Output",
        NormalisableRange<float>(-36.f, 12.f, 0.1f, 2.4f), 0.f,
        String(), AudioProcessorParameter::genericParameter,
        dbFormat, nullptr));

    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{ParamIDs::mix, 1}, "Mix",
        NormalisableRange<float>(0.f, 100.f, 0.1f), 100.f,
        String(), AudioProcessorParameter::genericParameter,
        pctFormat, nullptr));

    return layout;
}
