#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class PresetManager {
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts,
                           juce::File presetsDir = getUserPresetsDir());

    void savePreset(const juce::String& name);
    bool loadPreset(const juce::String& name);
    void deletePreset(const juce::String& name);
    void initPreset();

    juce::StringArray getPresetNames() const;
    juce::String getCurrentPresetName() const;
    void setCurrentPresetName(const juce::String& name);

    void selectNext();
    void selectPrev();

    static juce::File getUserPresetsDir();

private:
    juce::AudioProcessorValueTreeState& apvts_;
    juce::File presetsDir_;
    juce::String currentPresetName_;
};
