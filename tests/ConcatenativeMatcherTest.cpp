#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ConcatenativeMatcher.h"
#include "CorpusStore.h"
#include <vector>

TEST_CASE("match returns nullptr when corpus is empty") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    REQUIRE(matcher.match(ctrl, corpus) == nullptr);
}

TEST_CASE("match returns audio from the single frame when corpus has one frame") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);
    float audio[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    Features f{0.5f, 0.5f, 0.5f, 0.5f};
    corpus.push(audio, f);

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    const float* out = matcher.match(ctrl, corpus);
    REQUIRE(out != nullptr);
    REQUIRE(out[3] == Catch::Approx(0.4f)); // raw frame value — no crossfade in matcher
}

TEST_CASE("Matcher picks frame with closest RMS when only rms_weight is nonzero") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f); // only RMS
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);
    float audioLow[4]  = {0.1f, 0.1f, 0.1f, 0.1f};
    float audioHigh[4] = {0.9f, 0.9f, 0.9f, 0.9f};
    Features fLow  {0.f, 0.1f, 0.f, 0.f}; // low RMS
    Features fHigh {0.f, 0.9f, 0.f, 0.f}; // high RMS
    corpus.push(audioLow, fLow);
    corpus.push(audioHigh, fHigh);

    // Control has high RMS → should match audioHigh
    Features ctrl{0.f, 0.9f, 0.f, 0.f};
    const float* out = matcher.match(ctrl, corpus);
    REQUIRE(out != nullptr);
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += out[i];
    avg /= 4.f;
    REQUIRE(avg > 0.05f); // clearly from the high frame, not silence
}

TEST_CASE("distance is 0 between identical features") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    Features a{0.3f, 0.5f, 0.7f, 0.2f};
    REQUIRE(matcher.distance(a, a) == Catch::Approx(0.f));
}

TEST_CASE("weight of 0 means that feature does not affect distance") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f); // only RMS
    Features a{0.f, 0.5f, 0.f, 0.f};
    Features b{0.9f, 0.5f, 0.9f, 0.9f}; // different ZCR/SC/ST, same RMS
    REQUIRE(matcher.distance(a, b) == Catch::Approx(0.f));
}

TEST_CASE("rand path executes without crash and can return different frames") {
    // Use frameSize=128 so the audio values at any index directly reflect the selected frame.
    ConcatenativeMatcher matcher;
    matcher.prepare(128);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);
    matcher.setRand(1.f);  // maximum randomness — all equidistant frames are candidates

    CorpusStore corpus;
    corpus.prepare(128, 20);

    // Push 10 frames with identical RMS features but distinct audio values
    for (int j = 0; j < 10; ++j) {
        std::vector<float> audio(128, float(j + 1));
        corpus.push(audio.data(), Features{0.f, 0.5f, 0.f, 0.f});
    }

    Features ctrl{0.f, 0.5f, 0.f, 0.f};

    // Collect values from repeated matches (index 100)
    bool seenDifferentOutputs = false;
    float prevVal = matcher.match(ctrl, corpus)[100];
    for (int i = 0; i < 50; ++i) {
        const float* out = matcher.match(ctrl, corpus);
        REQUIRE(out != nullptr);
        if (out[100] != prevVal)
            seenDifferentOutputs = true;
    }
    REQUIRE(seenDifferentOutputs);
}

TEST_CASE("match returns raw frame audio without crossfade modification") {
    // With the old code, outputBuffer_[0] was prevBuffer_[0]*1.0 (t=0), not frame.audio[0].
    // With the new code, outputBuffer_[0] == frame.audio[0] exactly.
    ConcatenativeMatcher matcher;
    matcher.prepare(128);
    matcher.setWeights(1.f, 1.f, 1.f, 1.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(128, 4);
    std::vector<float> audio(128, 0.75f);
    corpus.push(audio.data(), Features{0.5f, 0.5f, 0.5f, 0.5f});

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};

    // Call match twice — old code crossfades from prev (silence) on first call,
    // so first call returns 0.0 at index 0 (t=0). New code returns 0.75f immediately.
    const float* out = matcher.match(ctrl, corpus);
    REQUIRE(out != nullptr);
    REQUIRE(out[0] == Catch::Approx(0.75f));   // raw frame value, no crossfade attenuation
    REQUIRE(out[127] == Catch::Approx(0.75f)); // same across the whole buffer
}
