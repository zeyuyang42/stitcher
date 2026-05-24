#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>

class MatchVisualizer : public juce::Component {
public:
    static constexpr int kSlots = 32;

    // Called from 30Hz timer — push latest ctrl RMS, update matched index and fill.
    // matchEpoch increments every grain hand-off in the processor (regardless of index).
    void tick(float ctrlRms, int matchedIndex, float corpusFill, uint32_t matchEpoch);

    void paint(juce::Graphics&) override;

private:
    // Ring buffer for top-track ctrl RMS history
    std::array<float, kSlots> ctrlHistory_ {};
    int historyHead_ = 0;   // index of oldest slot (newest = (historyHead_ - 1 + kSlots) % kSlots)
    int historyCount_ = 0;  // number of valid entries (0..kSlots)

    // Most recent values from tick()
    int   matchedIndex_  = -1;
    float corpusFill_    = 0.f;

    // Pulse animation: fires on every grain hand-off (epoch-driven, not index-driven)
    uint32_t lastMatchEpoch_ = 0;
    float    matchPulse_     = 0.f;   // 0..1
};
