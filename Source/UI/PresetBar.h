#pragma once
#include <JuceHeader.h>
#include "../PresetManager.h"

class PresetBar : public juce::Component {
public:
    explicit PresetBar(PresetManager& pm);
    void resized() override;
    void paint(juce::Graphics&) override;

    std::function<void(int)> onCaptureSlot;
    std::function<void(int)> onLoadSlot;

private:
    PresetManager& presetManager_;

    juce::Label      presetNameLabel_;
    juce::TextButton prevButton_      { "<" };
    juce::TextButton nextButton_      { ">" };
    juce::TextButton saveButton_      { "Save" };
    juce::TextButton saveAsButton_    { "Save As" };
    juce::TextButton initButton_      { "Init" };
    juce::TextButton slotCaptureA_    { "A+" };
    juce::TextButton slotLoadA_       { "A"  };
    juce::TextButton slotCaptureB_    { "B+" };
    juce::TextButton slotLoadB_       { "B"  };

    void updateLabel();
    void showSaveAsDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
