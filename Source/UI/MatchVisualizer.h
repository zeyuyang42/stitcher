#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class MatchVisualizer : public juce::Component {
public:
    static constexpr int kSlots = 32;

    // Called from 30Hz timer — push latest ctrl RMS, update matched index and fill
    void tick(float ctrlRms, int matchedIndex, float corpusFill);

    void paint(juce::Graphics&) override;

private:
    // Ring buffer for top-track ctrl RMS history
    std::array<float, kSlots> ctrlHistory_ {};
    int historyHead_ = 0;   // index of oldest slot (newest = (historyHead_ - 1 + kSlots) % kSlots)
    int historyCount_ = 0;  // number of valid entries (0..kSlots)

    // Most recent values from tick()
    int   matchedIndex_  = -1;
    float corpusFill_    = 0.f;

    // Pulse animation: fires on each new match, decays at ~150 ms
    float matchPulse_    = 0.f;   // 0..1
};
