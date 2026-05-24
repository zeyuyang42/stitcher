#pragma once
#include <JuceHeader.h>
#include "../PresetManager.h"

class PresetBar : public juce::Component {
public:
    explicit PresetBar(PresetManager& pm);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override;

    std::function<void(int)> onCaptureSlot;
    std::function<void(int)> onLoadSlot;

private:
    PresetManager& presetManager_;

    juce::Label      presetNameLabel_;
    juce::TextButton saveButton_ { "Save" };
    juce::TextButton slotLoadA_  { "A" };
    juce::TextButton slotLoadB_  { "B" };

    void updateLabel();
    void showSaveAsDialog();
    void showPresetMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
