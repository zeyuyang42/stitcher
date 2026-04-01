#include "CorpusStore.h"
#include <cassert>
#include <algorithm>

void CorpusStore::prepare(int frameSize, int maxFrames)
{
    frameSize_ = frameSize;
    maxFrames_ = maxFrames;
    frames_.resize(maxFrames);
    for (auto& f : frames_)
        f.audio.resize(frameSize, 0.f);
    writeIndex_ = 0;
    count_      = 0;
    frozen_     = false;
}

void CorpusStore::push(const float* audio, const Features& features)
{
    if (frozen_) return;

    auto& slot = frames_[writeIndex_];
    std::copy(audio, audio + frameSize_, slot.audio.begin());
    slot.features = features;

    writeIndex_ = (writeIndex_ + 1) % maxFrames_;
    if (count_ < maxFrames_)
        ++count_;
}

int CorpusStore::size() const { return count_; }

int CorpusStore::newestIndex() const
{
    if (count_ == 0) return 0;
    // Returns logical index of newest frame (size()-1), for use with getFrame()
    return count_ - 1;
}

const CorpusFrame& CorpusStore::getFrame(int index) const
{
    assert(index >= 0 && index < count_);
    // Oldest frame is at (writeIndex_ - count_ + maxFrames_) % maxFrames_
    int oldestSlot = (writeIndex_ - count_ + maxFrames_) % maxFrames_;
    int slot = (oldestSlot + index) % maxFrames_;
    return frames_[slot];
}

void CorpusStore::setFrozen(bool frozen) { frozen_ = frozen; }
