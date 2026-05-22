#include "PresetManager.h"

juce::File PresetManager::getUserPresetsDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Stitcher")
               .getChildFile("Presets")
               .getChildFile("User");
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             juce::File presetsDir)
    : apvts_(apvts), presetsDir_(presetsDir)
{
    presetsDir_.createDirectory();
}

void PresetManager::savePreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    auto xml = apvts_.copyState().createXml();
    if (!xml) return;
    presetsDir_.createDirectory();
    if (presetsDir_.getChildFile(name + ".xml").replaceWithText(xml->toString()))
        currentPresetName_ = name;
}

bool PresetManager::loadPreset(const juce::String& name)
{
    auto file = presetsDir_.getChildFile(name + ".xml");
    if (!file.existsAsFile()) return false;
    auto xml = juce::parseXML(file);
    if (!xml) return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;
    apvts_.replaceState(tree);
    currentPresetName_ = name;
    return true;
}

void PresetManager::deletePreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    presetsDir_.getChildFile(name + ".xml").deleteFile();
    if (currentPresetName_ == name)
        currentPresetName_ = {};
}

void PresetManager::initPreset()
{
    for (auto* param : apvts_.processor.getParameters())
        param->setValueNotifyingHost(param->getDefaultValue());
    currentPresetName_ = {};
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& f : presetsDir_.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(false);
    return names;
}

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName_;
}

void PresetManager::setCurrentPresetName(const juce::String& name)
{
    currentPresetName_ = name;
}

void PresetManager::selectNext()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = names.indexOf(currentPresetName_);
    loadPreset(names[(idx < 0 ? 0 : (idx + 1) % names.size())]);
}

void PresetManager::selectPrev()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = names.indexOf(currentPresetName_);
    loadPreset(names[idx < 0 ? names.size() - 1 : (idx - 1 + names.size()) % names.size()]);
}
