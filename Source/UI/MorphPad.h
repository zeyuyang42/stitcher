#pragma once
#include <JuceHeader.h>
#include <atomic>

class MorphPad : public juce::Component {
public:
    MorphPad() = default;

    // Call after construction, before adding to parent
    void setParams(juce::RangedAudioParameter* zcr,
                   juce::RangedAudioParameter* rms,
                   juce::RangedAudioParameter* sc,
                   juce::RangedAudioParameter* st);

    // Called from 30Hz timer in PluginEditor
    void setLiveLevels(float zcr, float rms, float sc, float st);

    // ── Pure math helpers (also called by tests) ──────────────────────────
    // Bilinear forward: thumb (x,y) → weights
    static void computeWeights(juce::Point<float> norm,
                                float& zcr, float& rms, float& sc, float& st);

    // Inverse: weights → thumb position
    static juce::Point<float> computeThumb(float zcr, float rms, float sc, float st);

    // ── juce::Component overrides ─────────────────────────────────────────
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    // Derive thumb from current param values
    juce::Point<float> thumbFromWeights() const;

    // Compute weights from thumb position and write to APVTS
    void writeWeightsFromThumb(juce::Point<float> normPos);

    // Clamp mouse position to bounds and normalise to [0,1]^2
    juce::Point<float> normFromMouseEvent(const juce::MouseEvent& e) const;

    juce::RangedAudioParameter* paramZcr_ = nullptr;
    juce::RangedAudioParameter* paramRms_ = nullptr;
    juce::RangedAudioParameter* paramSc_  = nullptr;
    juce::RangedAudioParameter* paramSt_  = nullptr;

    // Live feature levels for corner glow dots (0..1)
    std::atomic<float> liveZcr_ { 0.f };
    std::atomic<float> liveRms_ { 0.f };
    std::atomic<float> liveSc_  { 0.f };
    std::atomic<float> liveSt_  { 0.f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphPad)
};
