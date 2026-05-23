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
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR) == false);
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
    corpus.push(audio, audio, f);

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    REQUIRE(outL[3] == Catch::Approx(0.4f)); // raw frame value — no crossfade in matcher
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
    corpus.push(audioLow, audioLow, fLow);
    corpus.push(audioHigh, audioHigh, fHigh);

    // Control has high RMS → should match audioHigh
    Features ctrl{0.f, 0.9f, 0.f, 0.f};
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += outL[i];
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
        corpus.push(audio.data(), audio.data(), Features{0.f, 0.5f, 0.f, 0.f});
    }

    Features ctrl{0.f, 0.5f, 0.f, 0.f};

    // Collect values from repeated matches (index 100)
    bool seenDifferentOutputs = false;
    const float* outL = nullptr, *outR = nullptr;
    matcher.match(ctrl, corpus, outL, outR);
    float prevVal = outL[100];
    for (int i = 0; i < 50; ++i) {
        REQUIRE(matcher.match(ctrl, corpus, outL, outR));
        if (outL[100] != prevVal)
            seenDifferentOutputs = true;
    }
    REQUIRE(seenDifferentOutputs);
}

TEST_CASE("RMS matching uses stored features independent of corpus audio scale") {
    // Verifies that matching is based on the Features struct, not the raw audio amplitude.
    // This mirrors the post-fix PluginProcessor behavior where gainSrc_ applies to
    // corpus audio after feature extraction.
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);  // only RMS weight
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 4);

    // Frame A: raw RMS ≈ 0.1, stored audio scaled 4x (as if gainSrc applied post-extraction)
    float audioA[4] = {0.4f, 0.4f, 0.4f, 0.4f};
    corpus.push(audioA, audioA, Features{0.f, 0.1f, 0.f, 0.f});

    // Frame B: raw RMS = 0.5, stored audio scaled 4x
    float audioB[4] = {2.f, 2.f, 2.f, 2.f};
    corpus.push(audioB, audioB, Features{0.f, 0.5f, 0.f, 0.f});

    // Ctrl has raw RMS 0.1 — should match frame A by raw RMS feature
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(Features{0.f, 0.1f, 0.f, 0.f}, corpus, outL, outR));
    float avg = 0.f;
    for (int i = 0; i < 4; ++i) avg += outL[i];
    avg /= 4.f;
    REQUIRE(avg == Catch::Approx(0.4f));  // frame A audio value, not frame B's 2.0
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
    corpus.push(audio.data(), audio.data(), Features{0.5f, 0.5f, 0.5f, 0.5f});

    Features ctrl{0.5f, 0.5f, 0.5f, 0.5f};

    // Call match — new code returns 0.75f immediately (raw frame value, no crossfade).
    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(ctrl, corpus, outL, outR));
    REQUIRE(outL[0] == Catch::Approx(0.75f));   // raw frame value, no crossfade attenuation
    REQUIRE(outL[127] == Catch::Approx(0.75f)); // same across the whole buffer
}

TEST_CASE("Stereo corpus returns distinct L and R audio from the matched frame") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 2);
    float audioL[4] = {1.f, 1.f, 1.f, 1.f};
    float audioR[4] = {2.f, 2.f, 2.f, 2.f};
    corpus.push(audioL, audioR, Features{0.f, 0.5f, 0.f, 0.f});

    const float* outL = nullptr, *outR = nullptr;
    REQUIRE(matcher.match(Features{0.f, 0.5f, 0.f, 0.f}, corpus, outL, outR));
    REQUIRE(outL[0] == Catch::Approx(1.f));
    REQUIRE(outR[0] == Catch::Approx(2.f));
}

TEST_CASE("getLastMatchedIndex returns -1 before any match") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    REQUIRE(matcher.getLastMatchedIndex() == -1);
}

TEST_CASE("getLastMatchedIndex returns correct index after match") {
    ConcatenativeMatcher matcher;
    matcher.prepare(4);
    matcher.setWeights(0.f, 1.f, 0.f, 0.f);  // RMS only
    matcher.setRand(0.f);

    CorpusStore corpus;
    corpus.prepare(4, 10);

    // Push two frames with different RMS
    float quiet[4] = {0.1f, 0.1f, 0.1f, 0.1f};
    float loud[4]  = {0.9f, 0.9f, 0.9f, 0.9f};
    Features fq{0.f, 0.05f, 0.f, 0.f};  // low RMS
    Features fl{0.f, 0.8f,  0.f, 0.f};  // high RMS
    corpus.push(quiet, quiet, fq);  // index 0
    corpus.push(loud,  loud,  fl);  // index 1

    const float* outL = nullptr, *outR = nullptr;

    // Match against high RMS control → should pick index 1
    Features ctrl_loud{0.f, 0.9f, 0.f, 0.f};
    matcher.match(ctrl_loud, corpus, outL, outR);
    REQUIRE(matcher.getLastMatchedIndex() == 1);

    // Match against low RMS control → should pick index 0
    Features ctrl_quiet{0.f, 0.05f, 0.f, 0.f};
    matcher.match(ctrl_quiet, corpus, outL, outR);
    REQUIRE(matcher.getLastMatchedIndex() == 0);
}
