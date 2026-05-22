#pragma once
#include <JuceHeader.h>
#include "../PresetManager.h"

class PresetBar : public juce::Component {
public:
    explicit PresetBar(PresetManager& pm);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    PresetManager& presetManager_;

    juce::Label      presetNameLabel_;
    juce::TextButton prevButton_   { "<" };
    juce::TextButton nextButton_   { ">" };
    juce::TextButton saveButton_   { "Save" };
    juce::TextButton saveAsButton_ { "Save As" };
    juce::TextButton initButton_   { "Init" };

    void updateLabel();
    void showSaveAsDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
