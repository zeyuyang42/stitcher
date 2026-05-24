#include "PresetBar.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"
#include <map>

PresetBar::PresetBar(PresetManager& pm) : presetManager_(pm)
{
    presetNameLabel_.setJustificationType(juce::Justification::centredLeft);
    presetNameLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    presetNameLabel_.addMouseListener(this, false);
    addAndMakeVisible(presetNameLabel_);

    addAndMakeVisible(saveButton_);
    addAndMakeVisible(slotLoadA_);
    addAndMakeVisible(slotLoadB_);

    slotLoadA_.setTooltip("Click to recall A  ·  Shift-click to capture");
    slotLoadB_.setTooltip("Click to recall B  ·  Shift-click to capture");

    slotLoadA_.onClick = [this] {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
            if (onCaptureSlot) onCaptureSlot(0);
        } else {
            if (onLoadSlot) onLoadSlot(0);
        }
    };

    slotLoadB_.onClick = [this] {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) {
            if (onCaptureSlot) onCaptureSlot(1);
        } else {
            if (onLoadSlot) onLoadSlot(1);
        }
    };

    saveButton_.onClick = [this] {
        if (presetManager_.isCurrentPresetUser()) {
            presetManager_.savePreset(presetManager_.getCurrentPresetName());
        } else {
            showSaveAsDialog();
        }
    };

    updateLabel();
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced(2);
    const int btnW = 32;
    const int gap  = 4;

    saveButton_.setBounds(r.removeFromRight(btnW)); r.removeFromRight(gap);
    slotLoadB_ .setBounds(r.removeFromRight(btnW)); r.removeFromRight(gap);
    slotLoadA_ .setBounds(r.removeFromRight(btnW)); r.removeFromRight(gap);
    presetNameLabel_.setBounds(r);
}

void PresetBar::paint(juce::Graphics& g)
{
    g.setColour(StitcherLookAndFeel::Panel.brighter(0.05f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);
}

void PresetBar::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == &presetNameLabel_)
        showPresetMenu();
}

void PresetBar::updateLabel()
{
    auto name = presetManager_.getCurrentPresetName();
    presetNameLabel_.setText(name.isNotEmpty() ? name : "(unsaved)",
                             juce::dontSendNotification);
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Init");
    menu.addSeparator();

    const auto cats = presetManager_.getCategoryNames();
    int itemId = 100;
    std::map<int, juce::String> idToPreset;

    for (const auto& cat : cats) {
        juce::PopupMenu sub;
        for (const auto& name : presetManager_.getPresetNamesInCategory(cat)) {
            sub.addItem(itemId, name);
            idToPreset[itemId++] = name;
        }
        menu.addSubMenu(cat, sub);
    }

    const auto userPresets = presetManager_.getUserPresetNames();
    if (!userPresets.isEmpty()) {
        menu.addSeparator();
        juce::PopupMenu userSub;
        for (const auto& name : userPresets) {
            userSub.addItem(itemId, name);
            idToPreset[itemId++] = name;
        }
        menu.addSubMenu("User", userSub);
    }

    juce::Component::SafePointer<PresetBar> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options{},
        [safeThis, idToPreset](int result) {
            if (safeThis == nullptr || result == 0) return;
            if (result == 1) {
                safeThis->presetManager_.initPreset();
            } else {
                auto it = idToPreset.find(result);
                if (it != idToPreset.end())
                    safeThis->presetManager_.loadPreset(it->second);
            }
            safeThis->updateLabel();
        });
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
