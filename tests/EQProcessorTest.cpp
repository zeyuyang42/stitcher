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

TEST_CASE("Negative low-shelf gain attenuates low-frequency DC below unity") {
    EQProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;
    eq.prepare(spec);
    eq.setGains(-6.f, 0.f, 0.f); // -6 dB low shelf

    juce::AudioBuffer<float> buffer(1, 512);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < 512; ++i)
            buffer.setSample(0, i, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        eq.process(block);
    }

    float avg = 0.f;
    for (int i = 384; i < 512; ++i)
        avg += buffer.getSample(0, i);
    avg /= 128.f;
    REQUIRE(avg < 0.9f);  // -6 dB ≈ 0.5× gain; clearly below unity
}

TEST_CASE("High-shelf boost raises output of a high-frequency sine") {
    EQProcessor eq;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 1024;
    spec.numChannels      = 1;
    eq.prepare(spec);

    auto measure = [&](float highDb) {
        eq.setGains(0.f, 0.f, highDb);
        juce::AudioBuffer<float> buffer(1, 1024);
        // ~10 kHz sine (well above 6 kHz shelf)
        for (int pass = 0; pass < 4; ++pass) {
            for (int i = 0; i < 1024; ++i)
                buffer.setSample(0, i, std::sin(2.f * 3.14159f * 10000.f * float(i) / 44100.f));
            juce::dsp::AudioBlock<float> block(buffer);
            eq.process(block);
        }
        float rms = 0.f;
        for (int i = 512; i < 1024; ++i)
            rms += buffer.getSample(0, i) * buffer.getSample(0, i);
        return std::sqrt(rms / 512.f);
    };

    float flat    = measure(0.f);
    float boosted = measure(6.f);
    REQUIRE(boosted > flat * 1.1f);  // +6 dB high shelf clearly raises 10 kHz content
}
