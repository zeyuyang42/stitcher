#include "ConcatenativeMatcher.h"
#include <cmath>
#include <algorithm>
#include <limits>

void ConcatenativeMatcher::prepare(int frameSize)
{
    frameSize_ = frameSize;
    outputBuffer_.assign(frameSize, 0.f);
    prevBuffer_.assign(frameSize, 0.f);
}

void ConcatenativeMatcher::setWeights(float zcr, float rms, float sc, float st)
{
    wZcr_ = zcr;
    wRms_ = rms;
    wSc_  = sc;
    wSt_  = st;
}

void ConcatenativeMatcher::setRand(float rand) { rand_ = rand; }

float ConcatenativeMatcher::distance(const Features& a, const Features& b) const
{
    float dZcr = (a.zcr - b.zcr) * wZcr_;
    float dRms = (a.rms - b.rms) * wRms_;
    float dSc  = (a.sc  - b.sc)  * wSc_;
    float dSt  = (a.st  - b.st)  * wSt_;
    return std::sqrt(dZcr*dZcr + dRms*dRms + dSc*dSc + dSt*dSt);
}

const float* ConcatenativeMatcher::match(const Features& controlFeatures,
                                          const CorpusStore& corpus)
{
    if (corpus.size() == 0) return nullptr;

    // Find best match (minimum distance)
    float minDist = std::numeric_limits<float>::max();
    int   bestIdx = 0;
    for (int i = 0; i < corpus.size(); ++i) {
        float d = distance(controlFeatures, corpus.getFrame(i).features);
        if (d < minDist) {
            minDist = d;
            bestIdx = i;
        }
    }

    // If rand > 0, collect candidates within (1 + rand) * minDist and pick one randomly
    int matchIdx = bestIdx;
    if (rand_ > 0.f && corpus.size() > 1) {
        float threshold = (1.f + rand_) * minDist + 1e-6f;
        std::vector<int> candidates;
        for (int i = 0; i < corpus.size(); ++i)
            if (distance(controlFeatures, corpus.getFrame(i).features) <= threshold)
                candidates.push_back(i);
        if (!candidates.empty())
            matchIdx = candidates[random_.nextInt(static_cast<int>(candidates.size()))];
    }

    const auto& frame = corpus.getFrame(matchIdx);

    // Crossfade from previous output into the new frame
    std::copy(outputBuffer_.begin(), outputBuffer_.end(), prevBuffer_.begin());
    std::copy(frame.audio.begin(), frame.audio.end(), outputBuffer_.begin());

    const int xfadeLen = std::min(kCrossfadeLen, frameSize_);
    for (int i = 0; i < xfadeLen; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(xfadeLen);
        outputBuffer_[i] = prevBuffer_[i] * (1.f - t) + outputBuffer_[i] * t;
    }

    return outputBuffer_.data();
}
