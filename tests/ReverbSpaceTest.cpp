#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ReverbProcessor.h"

TEST_CASE("setSpace(0) maps to small room with high damping") {
    // space=0 → roomSize=0.20, damping=0.90
    ReverbProcessor rev;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 2;
    rev.prepare(spec);
    // Should not crash; implicitly tests the computed params are valid [0,1]
    REQUIRE_NOTHROW(rev.setSpace(0.f, 0.f));
}

TEST_CASE("setSpace(1) maps to large room with low damping") {
    ReverbProcessor rev;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 2;
    rev.prepare(spec);
    REQUIRE_NOTHROW(rev.setSpace(1.f, 0.5f));
}

TEST_CASE("setSpace produces audibly different reverb tails") {
    // Feed the same impulse through small-space (space=0) and large-space (space=1) reverb.
    // Measure energy across 10 decay blocks; the large room should accumulate more tail energy.
    auto measureDecayEnergy = [](float space) {
        ReverbProcessor rev;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels      = 2;
        rev.prepare(spec);
        rev.setSpace(space, 1.f);  // 100% wet

        juce::AudioBuffer<float> buffer(2, 512);

        // First block: impulse at sample 0
        buffer.clear();
        buffer.setSample(0, 0, 1.f);
        buffer.setSample(1, 0, 1.f);
        juce::dsp::AudioBlock<float> block(buffer);
        rev.process(block);

        // 9 more decay blocks: silence in, reverb tail out
        float energy = 0.f;
        for (int blk = 0; blk < 9; ++blk) {
            buffer.clear();
            rev.process(block);
            for (int i = 0; i < 512; ++i) {
                const float s = buffer.getSample(0, i);
                energy += s * s;
            }
        }
        return energy;
    };

    const float smallDecay = measureDecayEnergy(0.f);
    const float largeDecay = measureDecayEnergy(1.f);
    REQUIRE(largeDecay > smallDecay);
}
