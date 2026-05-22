#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

/**
 * MidiLearn — maps incoming MIDI CC numbers to APVTS parameters.
 *
 * Thread model:
 *   handleCC()       — called from audio thread (processBlock)
 *   processCapture() — called from message thread (~30 Hz timer)
 *   All other public methods — message thread only
 */
class MidiLearn {
public:
    MidiLearn() = default;

    // Register a parameter so it is available for serialization lookup.
    // Call once per param after construction (message thread).
    void registerParam(juce::RangedAudioParameter* param);

    // Audio thread: handle one CC event.
    //   If learning: capture this CC (first one wins; CAS).
    //   If not learning and a param is bound: dispatch value to param.
    void handleCC(int cc, int value);

    // Message thread: drain any pending capture from the audio thread.
    // Returns true if a new binding was just committed.
    bool processCapture();

    // Message thread: begin learning — next CC received will be bound to param.
    // Pass nullptr to cancel.
    void startLearning(juce::RangedAudioParameter* param);

    // Message thread: cancel learning without binding anything.
    void stopLearning();

    // Returns true if currently waiting for a CC (non-null learningParam_).
    bool isLearning() const { return learningParam_.load(std::memory_order_relaxed) != nullptr; }

    // Returns the param currently being learned (may be null).
    juce::RangedAudioParameter* getLearningParam() const {
        return learningParam_.load(std::memory_order_relaxed);
    }

    // Returns the CC number bound to param, or -1 if none.
    int getCCForParam(juce::RangedAudioParameter* param) const;

    // Unbind a specific CC number.
    void unbind(int cc);

    // Serialization — returns a heap-allocated XmlElement (caller owns it).
    juce::XmlElement* createBindingsXml() const;

    // Deserialization — restores bindings; uses params to look up by paramID.
    void restoreBindingsXml(const juce::XmlElement* xml,
                            const juce::Array<juce::AudioProcessorParameter*>& params);

private:
    // Audio-thread-safe: indexed by CC 0-127; null = unbound.
    std::array<std::atomic<juce::RangedAudioParameter*>, 128> ccToParam_ {};

    // Message-thread-only reverse map (for getCCForParam, serialization).
    std::vector<std::pair<int, juce::RangedAudioParameter*>> bindings_;

    // Learning state.
    std::atomic<juce::RangedAudioParameter*> learningParam_ { nullptr };

    // Staging area written by audio thread, drained by processCapture().
    std::atomic<int> capturedCC_ { -1 };

    // Helper: remove all binding entries for a given param (message thread).
    void removeBindingsForParam(juce::RangedAudioParameter* param);

    // Helper: remove all binding entries for a given CC (message thread).
    void removeBindingsForCC(int cc);
};
