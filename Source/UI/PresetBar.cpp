#include "PresetBar.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

PresetBar::PresetBar(PresetManager& pm) : presetManager_(pm)
{
    presetNameLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetNameLabel_);

    for (auto* b : { &prevButton_, &nextButton_, &saveButton_, &saveAsButton_, &initButton_ })
        addAndMakeVisible(b);

    prevButton_.onClick = [this] {
        presetManager_.selectPrev();
        updateLabel();
    };

    nextButton_.onClick = [this] {
        presetManager_.selectNext();
        updateLabel();
    };

    saveButton_.onClick = [this] {
        auto name = presetManager_.getCurrentPresetName();
        if (name.isNotEmpty()) {
            presetManager_.savePreset(name);
        } else {
            showSaveAsDialog();
        }
    };

    saveAsButton_.onClick = [this] { showSaveAsDialog(); };

    initButton_.onClick = [this] {
        presetManager_.initPreset();
        updateLabel();
    };

    updateLabel();
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced(2);
    const int btnW   = 28;
    const int actionW = 68;
    const int gap    = 4;

    prevButton_  .setBounds(r.removeFromLeft(btnW));    r.removeFromLeft(gap);
    nextButton_  .setBounds(r.removeFromRight(btnW));   r.removeFromRight(gap);
    initButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    saveAsButton_.setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    saveButton_  .setBounds(r.removeFromRight(actionW)); r.removeFromRight(gap);
    presetNameLabel_.setBounds(r);
}

void PresetBar::paint(juce::Graphics& g)
{
    g.setColour(StitcherLookAndFeel::Panel.brighter(0.05f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
}

void PresetBar::updateLabel()
{
    auto name = presetManager_.getCurrentPresetName();
    presetNameLabel_.setText(name.isNotEmpty() ? name : "(unsaved)",
                             juce::dontSendNotification);
}

void PresetBar::showSaveAsDialog()
{
    auto* dlg = new juce::AlertWindow("Save Preset",
                                      "Enter a name for this preset:",
                                      juce::MessageBoxIconType::NoIcon);
    dlg->addTextEditor("name", presetManager_.getCurrentPresetName(), "Name:");
    dlg->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    dlg->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safeThis(this);

    dlg->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, dlg](int result) {
            if (result == 1 && safeThis != nullptr) {
                auto name = dlg->getTextEditorContents("name").trim();
                if (name.isNotEmpty()) {
                    safeThis->presetManager_.savePreset(name);
                    safeThis->updateLabel();
                }
            }
            delete dlg;
        }), false);
}
