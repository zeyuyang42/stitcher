#pragma once
#include "FeatureExtractor.h"
#include <vector>

struct CorpusFrame {
    std::vector<float> audio;
    Features features;
};

class CorpusStore {
public:
    // frameSize: samples per frame; maxFrames: circular buffer capacity
    void prepare(int frameSize, int maxFrames);

    // Adds a frame (no-op if frozen)
    void push(const float* audio, const Features& features);

    // Number of valid frames currently stored (0 to maxFrames)
    int size() const;

    // Logical index of the most recently written frame (== size()-1); use with getFrame()
    int newestIndex() const;

    // Access frame by logical index [0 .. size()-1]
    // Index 0 = oldest, index size()-1 = newest
    const CorpusFrame& getFrame(int index) const;

    void setFrozen(bool frozen);

private:
    std::vector<CorpusFrame> frames_;
    int writeIndex_ = 0;  // next slot to write
    int count_      = 0;  // valid frames
    int maxFrames_  = 0;
    int frameSize_  = 0;
    bool frozen_    = false;
};
