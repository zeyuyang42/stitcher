#pragma once
#include "FeatureExtractor.h"
#include "CorpusStore.h"
#include <juce_core/juce_core.h>
#include <vector>

class ConcatenativeMatcher {
public:
    void prepare(int frameSize);
    void setWeights(float zcr, float rms, float sc, float st);
    // rand in [0,1]: 0 = best match, higher = more random selection among near-matches
    void setRand(float rand);

    // Returns pointer to output audio buffer (frameSize samples) or nullptr if corpus empty.
    // Caller is responsible for crossfading — this returns the raw matched frame audio.
    const float* match(const Features& controlFeatures, const CorpusStore& corpus);

    // Exposed for testing
    float distance(const Features& a, const Features& b) const;

private:
    int frameSize_ = 1024;
    float wZcr_ = 0.f, wRms_ = 1.f, wSc_ = 0.f, wSt_ = 0.f;
    float rand_ = 0.f;

    std::vector<float> outputBuffer_;
    std::vector<int>   candidates_;   // pre-allocated, reused each block to avoid audio-thread heap alloc

    juce::Random random_;
};
