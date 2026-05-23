#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "EQProcessor.h"

static float measureDcLevel(EQProcessor& eq, float gainDb)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq.prepare(spec);
    eq.setGains(gainDb, 0.f, 0.f);

    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i) buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq.process(block);
    }
    float avg = 0.f;
    for (int i = 384; i < 512; ++i) avg += buffer.getSample(0, i);
    return avg / 128.f;
}

TEST_CASE("setTilt(0) is flat at DC") {
    EQProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq.prepare(spec);
    eq.setTilt(0.f);

    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i) buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq.process(block);
    }
    float avg = 0.f;
    for (int i = 384; i < 512; ++i) avg += buffer.getSample(0, i);
    avg /= 128.f;
    REQUIRE(avg == Catch::Approx(1.f).margin(0.05f));
}

TEST_CASE("setTilt(+1) cuts DC (low shelf at -12 dB)") {
    // tilt +1 → setGains(-12, 0, +12) → low shelf cut
    EQProcessor eq;
    float cut = measureDcLevel(eq, -12.f);  // reference: direct -12 dB
    EQProcessor eq2;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq2.prepare(spec);
    eq2.setTilt(1.f);
    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i) buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq2.process(block);
    }
    float avg = 0.f;
    for (int i = 384; i < 512; ++i) avg += buffer.getSample(0, i);
    avg /= 128.f;
    REQUIRE(avg < 1.f);          // DC attenuated
    REQUIRE(avg == Catch::Approx(cut).margin(0.05f));  // matches -12 dB directly
}

TEST_CASE("setTilt(-1) boosts DC (low shelf at +12 dB)") {
    // tilt -1 → setGains(+12, 0, -12) → low shelf boost
    EQProcessor eq;
    float boosted = measureDcLevel(eq, 12.f);  // reference: direct +12 dB
    EQProcessor eq2;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq2.prepare(spec);
    eq2.setTilt(-1.f);
    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i) buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq2.process(block);
    }
    float avg = 0.f;
    for (int i = 384; i < 512; ++i) avg += buffer.getSample(0, i);
    avg /= 128.f;
    REQUIRE(avg > 1.f);          // DC boosted
    REQUIRE(avg == Catch::Approx(boosted).margin(0.05f));  // matches +12 dB directly
}
