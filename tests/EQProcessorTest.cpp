#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "EQProcessor.h"

TEST_CASE("EQ with 0dB gains passes DC signal at unity (post-transient)") {
    EQProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq.prepare(spec);
    eq.setGains(0.f, 0.f, 0.f);

    // Process two blocks so IIR transients settle
    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i)
            buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq.process(block);
    }

    // Last quarter of second block should be near unity
    for (int i = 384; i < 512; ++i)
        REQUIRE(buffer.getSample(0, i) == Catch::Approx(1.f).margin(0.05f));
}

TEST_CASE("Positive low-shelf gain boosts low-frequency DC above unity") {
    EQProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq.prepare(spec);
    eq.setGains(6.f, 0.f, 0.f); // +6 dB low shelf

    juce::AudioBuffer<float> buffer(1, 512);
    // Process twice to settle transients
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i)
            buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq.process(block);
    }

    // Output should be above 1.0 (DC boosted by low shelf)
    float avg = 0.f;
    for (int i = 384; i < 512; ++i)
        avg += buffer.getSample(0, i);
    avg /= 128.f;
    REQUIRE(avg > 1.1f);
}
