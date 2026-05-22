#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

class PresetManager {
public:
    struct FactoryPreset {
        juce::String name;
        juce::String category;
        juce::String xml;
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts,
                           juce::File presetsDir = getUserPresetsDir(),
                           std::vector<FactoryPreset> factoryPresets = {});

    void savePreset(const juce::String& name);
    void deletePreset(const juce::String& name);
    void initPreset();

    bool loadPreset(const juce::String& name);

    juce::StringArray getPresetNames() const;
    juce::StringArray getCategoryNames() const;
    juce::StringArray getPresetNamesInCategory(const juce::String& category) const;

    juce::String getCurrentPresetName() const;
    void setCurrentPresetName(const juce::String& name);

    void selectNext();
    void selectPrev();

    static juce::File getUserPresetsDir();

private:
    bool loadFromXmlString(const juce::String& xml, const juce::String& name);
    juce::StringArray getUserPresetNames() const;

    juce::AudioProcessorValueTreeState& apvts_;
    juce::File presetsDir_;
    juce::String currentPresetName_;
    std::vector<FactoryPreset> factoryPresets_;
};
