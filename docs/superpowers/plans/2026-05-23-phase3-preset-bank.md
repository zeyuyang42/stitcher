# Phase 3 — Factory Preset Bank

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Embed ~51 factory presets across 8 categories into the plugin binary and wire them through `PresetManager` so they appear before user presets in the preset list.

**Architecture:** Factory presets are XML blobs embedded via `juce_add_binary_data`. `PresetManager` gains a `FactoryPreset` struct and a constructor param that accepts a `std::vector<FactoryPreset>`. A thin header `Source/FactoryPresets.h` (compiled only into the plugin, not tests) constructs that vector from `BinaryData::*`. Tests inject mock XML directly.

**Tech Stack:** JUCE 8, C++17, Catch2 3.7.1, `juce_add_binary_data` CMake target.

---

## Key Facts

### APVTS XML Format

`apvts_.copyState().createXml()` produces (state ID = "PARAMETERS"):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<PARAMETERS>
  <PARAM id="zcr_weight"   value="0.0"/>
  <PARAM id="rms_weight"   value="1.0"/>
  <PARAM id="sc_weight"    value="0.0"/>
  <PARAM id="st_weight"    value="0.0"/>
  <PARAM id="match_len"    value="50.0"/>
  <PARAM id="seek_time"    value="1.0"/>
  <PARAM id="rand"         value="0.0"/>
  <PARAM id="freeze"       value="0"/>
  <PARAM id="gain_ctrl"    value="0.0"/>
  <PARAM id="gain_src"     value="0.0"/>
  <PARAM id="eq_low"       value="0.0"/>
  <PARAM id="eq_mid"       value="0.0"/>
  <PARAM id="eq_high"      value="0.0"/>
  <PARAM id="reverb_room"  value="0.5"/>
  <PARAM id="reverb_damp"  value="0.5"/>
  <PARAM id="reverb_wet"   value="0.0"/>
  <PARAM id="gain_out"     value="0.0"/>
  <PARAM id="mix"          value="100.0"/>
</PARAMETERS>
```

Values are **denormalized actual values** (not 0–1 normalized).

### BinaryData Naming Convention

`juce_add_binary_data` uses filename only (not path). Rule:
1. Replace ` ` and `.` with `_`
2. Strip all remaining non-alphanumeric-or-underscore characters

Examples:
- `Init.xml` → `Init_xml`
- `Drums_KickFollower.xml` → `Drums_KickFollower_xml`
- `LoFi_TapeWarp.xml` → `LoFi_TapeWarp_xml`

### Preset File Naming Convention

All preset XMLs live flat in `Source/Assets/Presets/`.  
Filename: `<CategoryKey>_<PascalCaseName>.xml`  
CategoryKey is exactly one of: `Utility`, `Drums`, `Texture`, `Glitch`, `Vocal`, `LoFi`, `Tonal`, `FX`

### Parameter Ranges (for authoring)

| Param | Range | Default |
|---|---|---|
| `zcr_weight` | 0–1 | 0 |
| `rms_weight` | 0–1 | 1 |
| `sc_weight` | 0–1 | 0 |
| `st_weight` | 0–1 | 0 |
| `match_len` | 10–100 ms | 50 |
| `seek_time` | 1–5 s | 1 |
| `rand` | 0–1 | 0 |
| `freeze` | 0 or 1 | 0 |
| `gain_ctrl` | -24–12 dB | 0 |
| `gain_src` | -24–12 dB | 0 |
| `eq_low/mid/high` | -12–12 dB | 0 |
| `reverb_room/damp` | 0–1 | 0.5 |
| `reverb_wet` | 0–1 | 0 |
| `gain_out` | -36–12 dB | 0 |
| `mix` | 0–100 % | 100 |

---

## Files Overview

| File | Action | Responsibility |
|---|---|---|
| `Source/PresetManager.h` | Modify | Add `FactoryPreset` struct; extend constructor |
| `Source/PresetManager.cpp` | Modify | Factory preset storage, lookup, selectNext/Prev |
| `Source/FactoryPresets.h` | Create | `makeFactoryPresets()` using BinaryData |
| `Source/Assets/Presets/*.xml` | Create (51 files) | One file per preset |
| `CMakeLists.txt` | Modify | Add all preset XMLs to `StitcherBinaryData` SOURCES |
| `Source/PluginProcessor.cpp` | Modify | Pass `makeFactoryPresets()` to `PresetManager` ctor |
| `tests/PresetManagerTest.cpp` | Modify | Add factory preset tests |

---

## Task 1: Extend PresetManager API

**Files:**
- Modify: `Source/PresetManager.h`
- Modify: `Source/PresetManager.cpp`
- Modify: `tests/PresetManagerTest.cpp`

### Step 1.1 — Write failing tests for factory preset behaviour

Add to `tests/PresetManagerTest.cpp`:

```cpp
// Helper: builds a minimal valid preset XML string for a given rms_weight value
static juce::String makePresetXml(float rmsWeight)
{
    return juce::String(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<PARAMETERS>\n"
        "  <PARAM id=\"zcr_weight\"  value=\"0.0\"/>\n"
        "  <PARAM id=\"rms_weight\"  value=\"") + rmsWeight + "\"/>\n"
        "  <PARAM id=\"sc_weight\"   value=\"0.0\"/>\n"
        "  <PARAM id=\"st_weight\"   value=\"0.0\"/>\n"
        "  <PARAM id=\"match_len\"   value=\"50.0\"/>\n"
        "  <PARAM id=\"seek_time\"   value=\"1.0\"/>\n"
        "  <PARAM id=\"rand\"        value=\"0.0\"/>\n"
        "  <PARAM id=\"freeze\"      value=\"0\"/>\n"
        "  <PARAM id=\"gain_ctrl\"   value=\"0.0\"/>\n"
        "  <PARAM id=\"gain_src\"    value=\"0.0\"/>\n"
        "  <PARAM id=\"eq_low\"      value=\"0.0\"/>\n"
        "  <PARAM id=\"eq_mid\"      value=\"0.0\"/>\n"
        "  <PARAM id=\"eq_high\"     value=\"0.0\"/>\n"
        "  <PARAM id=\"reverb_room\" value=\"0.5\"/>\n"
        "  <PARAM id=\"reverb_damp\" value=\"0.5\"/>\n"
        "  <PARAM id=\"reverb_wet\"  value=\"0.0\"/>\n"
        "  <PARAM id=\"gain_out\"    value=\"0.0\"/>\n"
        "  <PARAM id=\"mix\"         value=\"100.0\"/>\n"
        "</PARAMETERS>\n";
}

TEST_CASE("PresetManager: factory presets appear before user presets in getPresetNames") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};

    std::vector<PresetManager::FactoryPreset> factory {
        { "Alpha", "Drums",   makePresetXml(0.7f) },
        { "Beta",  "Texture", makePresetXml(0.3f) },
    };
    PresetManager pm{apvts, tempDir, factory};

    pm.savePreset("Zeta");  // user preset — should come after factory presets

    auto names = pm.getPresetNames();
    REQUIRE(names.size() == 3);
    REQUIRE(names[0] == "Alpha");
    REQUIRE(names[1] == "Beta");
    REQUIRE(names[2] == "Zeta");

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: loadPreset loads factory preset by name") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};

    std::vector<PresetManager::FactoryPreset> factory {
        { "TestFactory", "Drums", makePresetXml(0.42f) },
    };
    PresetManager pm{apvts, tempDir, factory};

    bool loaded = pm.loadPreset("TestFactory");
    REQUIRE(loaded);
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(0.42f).margin(0.01f));
    REQUIRE(pm.getCurrentPresetName() == "TestFactory");

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: selectNext cycles factory then user") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};

    std::vector<PresetManager::FactoryPreset> factory {
        { "Factory1", "Drums",   makePresetXml(0.1f) },
        { "Factory2", "Texture", makePresetXml(0.2f) },
    };
    PresetManager pm{apvts, tempDir, factory};
    pm.savePreset("UserA");

    pm.setCurrentPresetName("Factory1");
    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Factory2");
    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "UserA");
    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Factory1");  // wraps

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: getCategoryNames returns factory categories in insertion order") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};

    std::vector<PresetManager::FactoryPreset> factory {
        { "P1", "Drums",   makePresetXml(0.1f) },
        { "P2", "Texture", makePresetXml(0.2f) },
        { "P3", "Drums",   makePresetXml(0.3f) },
    };
    PresetManager pm{apvts, tempDir, factory};

    auto cats = pm.getCategoryNames();
    REQUIRE(cats.size() == 2);
    REQUIRE(cats[0] == "Drums");
    REQUIRE(cats[1] == "Texture");

    auto drumNames = pm.getPresetNamesInCategory("Drums");
    REQUIRE(drumNames.size() == 2);
    REQUIRE(drumNames.contains("P1"));
    REQUIRE(drumNames.contains("P3"));

    tempDir.deleteRecursively();
}
```

- [ ] Add these four test cases to `tests/PresetManagerTest.cpp` (after existing tests).
- [ ] Run tests: `./build/tests/StitcherTests` — expect 4 new FAILs, 35 existing PASS.

### Step 1.2 — Extend `Source/PresetManager.h`

Replace the entire `PresetManager.h` with:

```cpp
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

    // User presets (disk-based)
    void savePreset(const juce::String& name);
    void deletePreset(const juce::String& name);
    void initPreset();

    // Loads by name: factory first, then user disk.
    bool loadPreset(const juce::String& name);

    // All presets: factory (in insertion order) then user (alpha sorted).
    juce::StringArray getPresetNames() const;

    // Factory-only queries
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
```

- [ ] Replace `Source/PresetManager.h` with the above.

### Step 1.3 — Implement new methods in `Source/PresetManager.cpp`

Full replacement of `Source/PresetManager.cpp`:

```cpp
#include "PresetManager.h"

juce::File PresetManager::getUserPresetsDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
               .getChildFile("Stitcher")
               .getChildFile("Presets")
               .getChildFile("User");
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts,
                             juce::File presetsDir,
                             std::vector<FactoryPreset> factoryPresets)
    : apvts_(apvts), presetsDir_(presetsDir), factoryPresets_(std::move(factoryPresets))
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

bool PresetManager::loadFromXmlString(const juce::String& xmlString, const juce::String& name)
{
    auto xml = juce::parseXML(xmlString);
    if (!xml) return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return false;
    apvts_.replaceState(tree);
    currentPresetName_ = name;
    return true;
}

bool PresetManager::loadPreset(const juce::String& name)
{
    // Try factory first
    for (const auto& fp : factoryPresets_)
    {
        if (fp.name == name)
            return loadFromXmlString(fp.xml, name);
    }
    // Fall back to user disk
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
    for (const auto& fp : factoryPresets_)
        names.add(fp.name);
    for (const auto& n : getUserPresetNames())
        names.addIfNotAlreadyThere(n);
    return names;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : presetsDir_.findChildFiles(juce::File::findFiles, false, "*.xml"))
        names.add(f.getFileNameWithoutExtension());
    names.sort(false);
    return names;
}

juce::StringArray PresetManager::getCategoryNames() const
{
    juce::StringArray cats;
    for (const auto& fp : factoryPresets_)
        cats.addIfNotAlreadyThere(fp.category);
    return cats;
}

juce::StringArray PresetManager::getPresetNamesInCategory(const juce::String& category) const
{
    juce::StringArray names;
    for (const auto& fp : factoryPresets_)
        if (fp.category == category)
            names.add(fp.name);
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
```

- [ ] Replace `Source/PresetManager.cpp` with the above.

### Step 1.4 — Run tests

```
cmake --build build --target StitcherTests && ./build/tests/StitcherTests
```

Expected: all 39 tests pass (35 old + 4 new).

- [ ] Run tests, confirm 39/39 pass.

### Step 1.5 — Commit

```bash
git add Source/PresetManager.h Source/PresetManager.cpp tests/PresetManagerTest.cpp
git commit -m "feat: extend PresetManager to support factory presets"
```

- [ ] Commit.

---

## Task 2: Create Preset XML Files (51 files)

**Files:**
- Create: `Source/Assets/Presets/<Category>_<Name>.xml` × 51

All files use the same template — only the values differ. All 18 PARAMs must appear.

### Template

```xml
<?xml version="1.0" encoding="UTF-8"?>
<PARAMETERS>
  <PARAM id="zcr_weight"   value="ZCR"/>
  <PARAM id="rms_weight"   value="RMS"/>
  <PARAM id="sc_weight"    value="SC"/>
  <PARAM id="st_weight"    value="ST"/>
  <PARAM id="match_len"    value="LEN"/>
  <PARAM id="seek_time"    value="SEEK"/>
  <PARAM id="rand"         value="RAND"/>
  <PARAM id="freeze"       value="FREEZE"/>
  <PARAM id="gain_ctrl"    value="GCTRL"/>
  <PARAM id="gain_src"     value="GSRC"/>
  <PARAM id="eq_low"       value="EQLO"/>
  <PARAM id="eq_mid"       value="EQMI"/>
  <PARAM id="eq_high"      value="EQHI"/>
  <PARAM id="reverb_room"  value="RROO"/>
  <PARAM id="reverb_damp"  value="RDMP"/>
  <PARAM id="reverb_wet"   value="RWET"/>
  <PARAM id="gain_out"     value="GOUT"/>
  <PARAM id="mix"          value="MIX"/>
</PARAMETERS>
```

### Preset Values Table

Columns: File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix

**Utility (3)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Utility_Init.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 50 | 1.0 | 0.0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.5 | 0.5 | 0.0 | 0 | 100 |
| `Utility_Passthrough.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 50 | 1.0 | 0.0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.5 | 0.5 | 0.0 | 0 | 0 |
| `Utility_FullCorpus.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 50 | 5.0 | 0.0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.5 | 0.5 | 0.0 | 0 | 100 |

**Drums (8)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Drums_KickFollower.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 15 | 1.0 | 0.0 | 0 | 0 | 0 | 2.0 | 0 | -2.0 | 0.3 | 0.6 | 0.0 | 0 | 100 |
| `Drums_SnareSnatcher.xml` | 0.3 | 0.8 | 0.0 | 0.0 | 20 | 1.0 | 0.05 | 0 | 0 | 0 | 0 | 1.0 | 0 | 0.4 | 0.5 | 0.1 | 0 | 100 |
| `Drums_HatScatter.xml` | 1.0 | 0.0 | 0.0 | 0.0 | 10 | 1.0 | 0.3 | 0 | 0 | 0 | -3.0 | 0 | 3.0 | 0.3 | 0.5 | 0.0 | 0 | 100 |
| `Drums_DrumCrush.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 10 | 1.0 | 0.0 | 0 | 6.0 | 0 | 3.0 | 0 | -1.0 | 0.2 | 0.7 | 0.0 | 0 | 100 |
| `Drums_PercGhost.xml` | 0.2 | 0.7 | 0.0 | 0.0 | 25 | 1.5 | 0.15 | 0 | -3.0 | 0 | 0 | 0 | 2.0 | 0.4 | 0.5 | 0.1 | 0 | 100 |
| `Drums_RoomDrums.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 20 | 1.0 | 0.0 | 0 | 0 | 0 | 1.0 | 0 | 0 | 0.4 | 0.4 | 0.3 | 0 | 100 |
| `Drums_GlueLayer.xml` | 0.0 | 0.6 | 0.2 | 0.0 | 30 | 2.0 | 0.05 | 0 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.15 | 0 | 60 |
| `Drums_BeatGranule.xml` | 0.3 | 0.5 | 0.0 | 0.0 | 10 | 1.5 | 0.4 | 0 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.1 | 0 | 100 |

**Texture (10)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Texture_AetherPad.xml` | 0.0 | 0.0 | 1.0 | 0.0 | 80 | 4.0 | 0.3 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.7 | 0.3 | 0.5 | 0 | 100 |
| `Texture_CrystalChoir.xml` | 0.0 | 0.0 | 0.7 | 0.5 | 70 | 3.0 | 0.2 | 0 | 0 | 0 | -1.0 | 0 | 3.0 | 0.6 | 0.2 | 0.6 | 0 | 100 |
| `Texture_SilkTexture.xml` | 0.0 | 0.2 | 0.5 | 0.0 | 70 | 3.0 | 0.15 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.5 | 0.3 | 0.4 | 0 | 100 |
| `Texture_FrozenAir.xml` | 0.0 | 0.0 | 1.0 | 0.2 | 90 | 3.0 | 0.1 | 1 | -3.0 | 0 | 0 | 0 | 1.0 | 0.8 | 0.2 | 0.7 | 0 | 100 |
| `Texture_WarmBlanket.xml` | 0.0 | 0.3 | 0.4 | 0.0 | 60 | 2.5 | 0.1 | 0 | 0 | 0 | 3.0 | 1.0 | -2.0 | 0.5 | 0.4 | 0.3 | 0 | 100 |
| `Texture_SpectralDrift.xml` | 0.0 | 0.0 | 0.8 | 0.2 | 90 | 4.0 | 0.6 | 0 | 0 | 0 | 0 | 0 | 1.0 | 0.6 | 0.3 | 0.5 | 0 | 100 |
| `Texture_ShimmerHalo.xml` | 0.0 | 0.0 | 0.6 | 0.6 | 80 | 4.0 | 0.2 | 0 | 0 | 0 | -1.0 | 0 | 3.0 | 0.7 | 0.2 | 0.7 | 0 | 100 |
| `Texture_DeepSpace.xml` | 0.0 | 0.0 | 1.0 | 0.0 | 100 | 5.0 | 0.3 | 0 | 0 | 0 | 2.0 | 0 | 0 | 0.9 | 0.1 | 0.8 | 0 | 100 |
| `Texture_VelvetNoise.xml` | 0.2 | 0.1 | 0.4 | 0.1 | 60 | 3.0 | 0.5 | 0 | 0 | 0 | 0 | 0 | -1.0 | 0.5 | 0.4 | 0.3 | 0 | 100 |
| `Texture_Nebula.xml` | 0.0 | 0.1 | 0.7 | 0.3 | 80 | 4.0 | 0.4 | 0 | 0 | 0 | 0 | 0 | 1.0 | 0.6 | 0.3 | 0.5 | 0 | 100 |

**Glitch (8)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Glitch_MicroStutter.xml` | 0.3 | 0.5 | 0.0 | 0.0 | 10 | 1.0 | 0.8 | 0 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.0 | 0 | 100 |
| `Glitch_Shatter.xml` | 0.4 | 0.5 | 0.0 | 0.0 | 10 | 1.0 | 1.0 | 0 | 0 | 0 | 0 | 0 | 4.0 | 0.3 | 0.5 | 0.0 | 0 | 100 |
| `Glitch_DataRot.xml` | 0.3 | 0.3 | 0.2 | 0.0 | 10 | 1.0 | 0.9 | 0 | 0 | 0 | -3.0 | 0 | 0 | 0.3 | 0.5 | 0.1 | 0 | 100 |
| `Glitch_GranularMelt.xml` | 0.1 | 0.5 | 0.2 | 0.0 | 25 | 2.0 | 0.7 | 0 | 0 | 0 | 0 | 0 | 0 | 0.4 | 0.4 | 0.2 | 0 | 100 |
| `Glitch_TimeSplice.xml` | 0.0 | 0.7 | 0.0 | 0.0 | 15 | 1.5 | 0.5 | 1 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.2 | 0 | 100 |
| `Glitch_BitCrumble.xml` | 0.6 | 0.3 | 0.0 | 0.0 | 10 | 1.0 | 0.7 | 0 | 0 | 0 | -3.0 | 0 | 0 | 0.3 | 0.5 | 0.0 | 0 | 100 |
| `Glitch_PulseScatter.xml` | 0.1 | 0.8 | 0.0 | 0.0 | 10 | 1.0 | 0.6 | 0 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.1 | 0 | 100 |
| `Glitch_ChaosEngine.xml` | 0.5 | 0.3 | 0.0 | 0.0 | 10 | 1.0 | 0.95 | 0 | 0 | 0 | 0 | 0 | 0 | 0.3 | 0.5 | 0.2 | 0 | 100 |

**Vocal (6)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Vocal_VowelLock.xml` | 0.0 | 0.2 | 0.7 | 0.5 | 50 | 2.0 | 0.0 | 0 | 0 | 0 | -1.0 | 2.0 | -1.0 | 0.4 | 0.5 | 0.2 | 0 | 100 |
| `Vocal_ChoirSmear.xml` | 0.0 | 0.3 | 0.6 | 0.2 | 60 | 2.5 | 0.2 | 0 | 0 | 0 | 0 | 1.0 | 0 | 0.5 | 0.3 | 0.4 | 0 | 100 |
| `Vocal_FormantFreeze.xml` | 0.0 | 0.1 | 1.0 | 0.7 | 80 | 2.0 | 0.05 | 1 | -3.0 | 0 | 0 | 2.0 | 0 | 0.5 | 0.3 | 0.4 | 0 | 100 |
| `Vocal_BreathExtract.xml` | 0.8 | 0.3 | 0.0 | 0.0 | 30 | 1.5 | 0.1 | 0 | 0 | 0 | -2.0 | 0 | 1.0 | 0.3 | 0.6 | 0.1 | 0 | 100 |
| `Vocal_SyllableEcho.xml` | 0.2 | 0.5 | 0.4 | 0.1 | 40 | 2.0 | 0.1 | 0 | 0 | 0 | -1.0 | 1.0 | 0 | 0.4 | 0.4 | 0.3 | 0 | 100 |
| `Vocal_Haunt.xml` | 0.1 | 0.3 | 0.5 | 0.2 | 60 | 2.5 | 0.3 | 0 | -3.0 | 0 | 0 | 2.0 | -1.0 | 0.5 | 0.3 | 0.6 | 0 | 100 |

**LoFi (6)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `LoFi_TapeWarp.xml` | 0.0 | 0.7 | 0.1 | 0.0 | 40 | 2.0 | 0.05 | 0 | 0 | 0 | 1.0 | 0 | -4.0 | 0.4 | 0.5 | 0.2 | 0 | 100 |
| `LoFi_VinylDust.xml` | 0.1 | 0.6 | 0.0 | 0.0 | 40 | 1.5 | 0.08 | 0 | 0 | 0 | 2.0 | 0 | -6.0 | 0.4 | 0.8 | 0.2 | 0 | 100 |
| `LoFi_OldRadio.xml` | 0.3 | 0.8 | 0.0 | 0.0 | 30 | 1.0 | 0.05 | 0 | 0 | 0 | -6.0 | 2.0 | -8.0 | 0.3 | 0.8 | 0.1 | 0 | 100 |
| `LoFi_LoPad.xml` | 0.0 | 0.3 | 0.5 | 0.1 | 70 | 3.0 | 0.15 | 0 | 0 | 0 | 2.0 | 0 | -4.0 | 0.5 | 0.8 | 0.4 | 0 | 100 |
| `LoFi_WashedOut.xml` | 0.1 | 0.5 | 0.2 | 0.0 | 50 | 2.5 | 0.2 | 0 | 0 | 0 | 1.0 | 0 | -2.0 | 0.5 | 0.9 | 0.5 | 0 | 100 |
| `LoFi_Nostalgic.xml` | 0.0 | 0.6 | 0.3 | 0.0 | 50 | 2.0 | 0.1 | 0 | 0 | 0 | 1.0 | 0 | -3.0 | 0.4 | 0.6 | 0.3 | 0 | 100 |

**Tonal (5)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Tonal_PitchTrace.xml` | 1.0 | 0.0 | 0.0 | 0.0 | 60 | 2.0 | 0.0 | 0 | 0 | 0 | 0 | 0 | 1.0 | 0.4 | 0.5 | 0.2 | 0 | 100 |
| `Tonal_HarmonicCloud.xml` | 0.3 | 0.0 | 0.8 | 0.0 | 70 | 3.0 | 0.2 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.6 | 0.3 | 0.4 | 0 | 100 |
| `Tonal_SingleWeight.xml` | 0.0 | 1.0 | 0.0 | 0.0 | 40 | 1.5 | 0.0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.4 | 0.5 | 0.1 | 0 | 100 |
| `Tonal_BrightChord.xml` | 0.2 | 0.3 | 0.5 | 0.1 | 50 | 2.0 | 0.1 | 0 | 0 | 0 | -1.0 | 0 | 4.0 | 0.5 | 0.3 | 0.3 | 0 | 100 |
| `Tonal_BassFoundation.xml` | 0.0 | 0.7 | 0.1 | 0.0 | 40 | 2.0 | 0.05 | 0 | 0 | 0 | 4.0 | -2.0 | -1.0 | 0.3 | 0.6 | 0.1 | 0 | 100 |

**FX (5)**

| File | ZCR | RMS | SC | ST | Len | Seek | Rand | Freeze | GCtrl | GSrc | EqLo | EqMi | EqHi | RRoom | RDmp | RWet | GOut | Mix |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `FX_RiserBuild.xml` | 0.0 | 0.3 | 0.8 | 0.2 | 30 | 5.0 | 0.4 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.7 | 0.2 | 0.5 | 0 | 100 |
| `FX_FreezeSwell.xml` | 0.0 | 0.6 | 0.3 | 0.1 | 60 | 2.0 | 0.1 | 1 | 0 | 0 | 0 | 0 | 1.0 | 0.7 | 0.2 | 0.7 | 0 | 100 |
| `FX_ChaosRiser.xml` | 0.2 | 0.3 | 0.5 | 0.0 | 20 | 4.0 | 0.9 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.6 | 0.2 | 0.5 | 0 | 100 |
| `FX_ReverseGrain.xml` | 0.0 | 0.3 | 0.3 | 0.7 | 20 | 2.0 | 0.6 | 0 | 0 | 0 | 0 | 0 | 0 | 0.5 | 0.4 | 0.4 | 0 | 100 |
| `FX_NoiseBloom.xml` | 0.8 | 0.2 | 0.0 | 0.0 | 15 | 2.0 | 0.8 | 0 | 0 | 0 | 0 | 0 | 2.0 | 0.5 | 0.3 | 0.6 | 0 | 100 |

### Steps

- [ ] `mkdir -p Source/Assets/Presets`
- [ ] Create all 51 XML files per the table above (expand each row into the XML template — substitute the exact values).
- [ ] Spot-check 3 files to verify XML is well-formed: `xmllint --noout Source/Assets/Presets/Utility_Init.xml Source/Assets/Presets/Texture_AetherPad.xml Source/Assets/Presets/Glitch_ChaosEngine.xml`

---

## Task 3: BinaryData + FactoryPresets.h + CMakeLists

**Files:**
- Modify: `CMakeLists.txt`
- Create: `Source/FactoryPresets.h`
- Modify: `Source/PluginProcessor.cpp`

### Step 3.1 — Add preset XMLs to CMakeLists.txt

In `CMakeLists.txt`, the `juce_add_binary_data` block currently reads:
```cmake
juce_add_binary_data(StitcherBinaryData
    SOURCES
        Source/Assets/Fonts/Inter-Regular.ttf
)
```

Expand it to include all 51 preset files (in category order). Full replacement of that block:

```cmake
juce_add_binary_data(StitcherBinaryData
    SOURCES
        Source/Assets/Fonts/Inter-Regular.ttf
        # Utility
        Source/Assets/Presets/Utility_Init.xml
        Source/Assets/Presets/Utility_Passthrough.xml
        Source/Assets/Presets/Utility_FullCorpus.xml
        # Drums
        Source/Assets/Presets/Drums_KickFollower.xml
        Source/Assets/Presets/Drums_SnareSnatcher.xml
        Source/Assets/Presets/Drums_HatScatter.xml
        Source/Assets/Presets/Drums_DrumCrush.xml
        Source/Assets/Presets/Drums_PercGhost.xml
        Source/Assets/Presets/Drums_RoomDrums.xml
        Source/Assets/Presets/Drums_GlueLayer.xml
        Source/Assets/Presets/Drums_BeatGranule.xml
        # Texture
        Source/Assets/Presets/Texture_AetherPad.xml
        Source/Assets/Presets/Texture_CrystalChoir.xml
        Source/Assets/Presets/Texture_SilkTexture.xml
        Source/Assets/Presets/Texture_FrozenAir.xml
        Source/Assets/Presets/Texture_WarmBlanket.xml
        Source/Assets/Presets/Texture_SpectralDrift.xml
        Source/Assets/Presets/Texture_ShimmerHalo.xml
        Source/Assets/Presets/Texture_DeepSpace.xml
        Source/Assets/Presets/Texture_VelvetNoise.xml
        Source/Assets/Presets/Texture_Nebula.xml
        # Glitch
        Source/Assets/Presets/Glitch_MicroStutter.xml
        Source/Assets/Presets/Glitch_Shatter.xml
        Source/Assets/Presets/Glitch_DataRot.xml
        Source/Assets/Presets/Glitch_GranularMelt.xml
        Source/Assets/Presets/Glitch_TimeSplice.xml
        Source/Assets/Presets/Glitch_BitCrumble.xml
        Source/Assets/Presets/Glitch_PulseScatter.xml
        Source/Assets/Presets/Glitch_ChaosEngine.xml
        # Vocal
        Source/Assets/Presets/Vocal_VowelLock.xml
        Source/Assets/Presets/Vocal_ChoirSmear.xml
        Source/Assets/Presets/Vocal_FormantFreeze.xml
        Source/Assets/Presets/Vocal_BreathExtract.xml
        Source/Assets/Presets/Vocal_SyllableEcho.xml
        Source/Assets/Presets/Vocal_Haunt.xml
        # LoFi
        Source/Assets/Presets/LoFi_TapeWarp.xml
        Source/Assets/Presets/LoFi_VinylDust.xml
        Source/Assets/Presets/LoFi_OldRadio.xml
        Source/Assets/Presets/LoFi_LoPad.xml
        Source/Assets/Presets/LoFi_WashedOut.xml
        Source/Assets/Presets/LoFi_Nostalgic.xml
        # Tonal
        Source/Assets/Presets/Tonal_PitchTrace.xml
        Source/Assets/Presets/Tonal_HarmonicCloud.xml
        Source/Assets/Presets/Tonal_SingleWeight.xml
        Source/Assets/Presets/Tonal_BrightChord.xml
        Source/Assets/Presets/Tonal_BassFoundation.xml
        # FX
        Source/Assets/Presets/FX_RiserBuild.xml
        Source/Assets/Presets/FX_FreezeSwell.xml
        Source/Assets/Presets/FX_ChaosRiser.xml
        Source/Assets/Presets/FX_ReverseGrain.xml
        Source/Assets/Presets/FX_NoiseBloom.xml
)
```

- [ ] Replace the `juce_add_binary_data` block in `CMakeLists.txt` with the above.

### Step 3.2 — Create `Source/FactoryPresets.h`

```cpp
#pragma once
#include <BinaryData.h>
#include "PresetManager.h"
#include <vector>

inline std::vector<PresetManager::FactoryPreset> makeFactoryPresets()
{
    return {
        // Utility
        { "Init",           "Utility", juce::String::fromUTF8(BinaryData::Utility_Init_xml,         BinaryData::Utility_Init_xmlSize) },
        { "Passthrough",    "Utility", juce::String::fromUTF8(BinaryData::Utility_Passthrough_xml,  BinaryData::Utility_Passthrough_xmlSize) },
        { "Full Corpus",    "Utility", juce::String::fromUTF8(BinaryData::Utility_FullCorpus_xml,   BinaryData::Utility_FullCorpus_xmlSize) },
        // Drums
        { "Kick Follower",  "Drums",   juce::String::fromUTF8(BinaryData::Drums_KickFollower_xml,   BinaryData::Drums_KickFollower_xmlSize) },
        { "Snare Snatcher", "Drums",   juce::String::fromUTF8(BinaryData::Drums_SnareSnatcher_xml,  BinaryData::Drums_SnareSnatcher_xmlSize) },
        { "Hat Scatter",    "Drums",   juce::String::fromUTF8(BinaryData::Drums_HatScatter_xml,     BinaryData::Drums_HatScatter_xmlSize) },
        { "Drum Crush",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_DrumCrush_xml,      BinaryData::Drums_DrumCrush_xmlSize) },
        { "Perc Ghost",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_PercGhost_xml,      BinaryData::Drums_PercGhost_xmlSize) },
        { "Room Drums",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_RoomDrums_xml,      BinaryData::Drums_RoomDrums_xmlSize) },
        { "Glue Layer",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_GlueLayer_xml,      BinaryData::Drums_GlueLayer_xmlSize) },
        { "Beat Granule",   "Drums",   juce::String::fromUTF8(BinaryData::Drums_BeatGranule_xml,    BinaryData::Drums_BeatGranule_xmlSize) },
        // Texture
        { "Aether Pad",     "Texture", juce::String::fromUTF8(BinaryData::Texture_AetherPad_xml,    BinaryData::Texture_AetherPad_xmlSize) },
        { "Crystal Choir",  "Texture", juce::String::fromUTF8(BinaryData::Texture_CrystalChoir_xml, BinaryData::Texture_CrystalChoir_xmlSize) },
        { "Silk Texture",   "Texture", juce::String::fromUTF8(BinaryData::Texture_SilkTexture_xml,  BinaryData::Texture_SilkTexture_xmlSize) },
        { "Frozen Air",     "Texture", juce::String::fromUTF8(BinaryData::Texture_FrozenAir_xml,    BinaryData::Texture_FrozenAir_xmlSize) },
        { "Warm Blanket",   "Texture", juce::String::fromUTF8(BinaryData::Texture_WarmBlanket_xml,  BinaryData::Texture_WarmBlanket_xmlSize) },
        { "Spectral Drift", "Texture", juce::String::fromUTF8(BinaryData::Texture_SpectralDrift_xml,BinaryData::Texture_SpectralDrift_xmlSize) },
        { "Shimmer Halo",   "Texture", juce::String::fromUTF8(BinaryData::Texture_ShimmerHalo_xml,  BinaryData::Texture_ShimmerHalo_xmlSize) },
        { "Deep Space",     "Texture", juce::String::fromUTF8(BinaryData::Texture_DeepSpace_xml,    BinaryData::Texture_DeepSpace_xmlSize) },
        { "Velvet Noise",   "Texture", juce::String::fromUTF8(BinaryData::Texture_VelvetNoise_xml,  BinaryData::Texture_VelvetNoise_xmlSize) },
        { "Nebula",         "Texture", juce::String::fromUTF8(BinaryData::Texture_Nebula_xml,       BinaryData::Texture_Nebula_xmlSize) },
        // Glitch
        { "Micro Stutter",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_MicroStutter_xml,  BinaryData::Glitch_MicroStutter_xmlSize) },
        { "Shatter",        "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_Shatter_xml,        BinaryData::Glitch_Shatter_xmlSize) },
        { "Data Rot",       "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_DataRot_xml,        BinaryData::Glitch_DataRot_xmlSize) },
        { "Granular Melt",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_GranularMelt_xml,   BinaryData::Glitch_GranularMelt_xmlSize) },
        { "Time Splice",    "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_TimeSplice_xml,     BinaryData::Glitch_TimeSplice_xmlSize) },
        { "Bit Crumble",    "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_BitCrumble_xml,     BinaryData::Glitch_BitCrumble_xmlSize) },
        { "Pulse Scatter",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_PulseScatter_xml,   BinaryData::Glitch_PulseScatter_xmlSize) },
        { "Chaos Engine",   "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_ChaosEngine_xml,    BinaryData::Glitch_ChaosEngine_xmlSize) },
        // Vocal
        { "Vowel Lock",     "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_VowelLock_xml,       BinaryData::Vocal_VowelLock_xmlSize) },
        { "Choir Smear",    "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_ChoirSmear_xml,       BinaryData::Vocal_ChoirSmear_xmlSize) },
        { "Formant Freeze", "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_FormantFreeze_xml,    BinaryData::Vocal_FormantFreeze_xmlSize) },
        { "Breath Extract", "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_BreathExtract_xml,    BinaryData::Vocal_BreathExtract_xmlSize) },
        { "Syllable Echo",  "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_SyllableEcho_xml,     BinaryData::Vocal_SyllableEcho_xmlSize) },
        { "Haunt",          "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_Haunt_xml,            BinaryData::Vocal_Haunt_xmlSize) },
        // LoFi
        { "Tape Warp",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_TapeWarp_xml,         BinaryData::LoFi_TapeWarp_xmlSize) },
        { "Vinyl Dust",     "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_VinylDust_xml,        BinaryData::LoFi_VinylDust_xmlSize) },
        { "Old Radio",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_OldRadio_xml,         BinaryData::LoFi_OldRadio_xmlSize) },
        { "Lo Pad",         "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_LoPad_xml,            BinaryData::LoFi_LoPad_xmlSize) },
        { "Washed Out",     "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_WashedOut_xml,        BinaryData::LoFi_WashedOut_xmlSize) },
        { "Nostalgic",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_Nostalgic_xml,        BinaryData::LoFi_Nostalgic_xmlSize) },
        // Tonal
        { "Pitch Trace",    "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_PitchTrace_xml,      BinaryData::Tonal_PitchTrace_xmlSize) },
        { "Harmonic Cloud", "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_HarmonicCloud_xml,   BinaryData::Tonal_HarmonicCloud_xmlSize) },
        { "Single Weight",  "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_SingleWeight_xml,    BinaryData::Tonal_SingleWeight_xmlSize) },
        { "Bright Chord",   "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_BrightChord_xml,     BinaryData::Tonal_BrightChord_xmlSize) },
        { "Bass Foundation","Tonal",   juce::String::fromUTF8(BinaryData::Tonal_BassFoundation_xml,  BinaryData::Tonal_BassFoundation_xmlSize) },
        // FX
        { "Riser Build",    "FX",      juce::String::fromUTF8(BinaryData::FX_RiserBuild_xml,         BinaryData::FX_RiserBuild_xmlSize) },
        { "Freeze Swell",   "FX",      juce::String::fromUTF8(BinaryData::FX_FreezeSwell_xml,        BinaryData::FX_FreezeSwell_xmlSize) },
        { "Chaos Riser",    "FX",      juce::String::fromUTF8(BinaryData::FX_ChaosRiser_xml,         BinaryData::FX_ChaosRiser_xmlSize) },
        { "Reverse Grain",  "FX",      juce::String::fromUTF8(BinaryData::FX_ReverseGrain_xml,       BinaryData::FX_ReverseGrain_xmlSize) },
        { "Noise Bloom",    "FX",      juce::String::fromUTF8(BinaryData::FX_NoiseBloom_xml,         BinaryData::FX_NoiseBloom_xmlSize) },
    };
}
```

Note: `LoFi_TapeWarp_xml` — the `F` in `LoFi` stays lowercase in the filename `LoFi_TapeWarp.xml`, and `LoFi_TapeWarp.xml` → `LoFi_TapeWarp_xml` (no special chars to strip). ✓

- [ ] Create `Source/FactoryPresets.h` with the above.

### Step 3.3 — Add `FactoryPresets.h` to CMakeLists.txt SourceFiles

In `CMakeLists.txt`, the `set(SourceFiles ...)` block must include the new header so it appears in the IDE. Find this line:

```cmake
    Source/PresetManager.h
    Source/PresetManager.cpp
```

Add `Source/FactoryPresets.h` after it:

```cmake
    Source/PresetManager.h
    Source/PresetManager.cpp
    Source/FactoryPresets.h
```

- [ ] Add `Source/FactoryPresets.h` to the `SourceFiles` list.

### Step 3.4 — Wire into `Source/PluginProcessor.cpp`

In `PluginProcessor.cpp`, find the `#include` block at the top. Add:

```cpp
#include "FactoryPresets.h"
```

Then find the `presetManager_` constructor call (it's a member initializer in `StitcherProcessor`'s constructor). Currently it looks like:

```cpp
presetManager_(apvts_)
```

or may already pass a directory. Change it to:

```cpp
presetManager_(apvts_, PresetManager::getUserPresetsDir(), makeFactoryPresets())
```

- [ ] Add `#include "FactoryPresets.h"` in `PluginProcessor.cpp`.
- [ ] Update `presetManager_` construction to pass `makeFactoryPresets()`.

### Step 3.5 — Build

```
cmake --build build --target Stitcher_Standalone
```

Expected: clean build, no warnings. BinaryData will now contain 52 resources.

- [ ] Build and confirm success.

### Step 3.6 — Run tests

```
./build/tests/StitcherTests
```

Expected: all 39 tests still pass (FactoryPresets.h not included in tests — BinaryData not linked to test target).

- [ ] Confirm 39/39 pass.

### Step 3.7 — Commit

```bash
git add CMakeLists.txt Source/FactoryPresets.h Source/Assets/Presets/ Source/PluginProcessor.cpp
git commit -m "feat: embed 51 factory presets across 8 categories via BinaryData"
```

- [ ] Commit.

---

## Task 4: Verification and Install

- [ ] Launch Standalone; open preset bar → cycle through presets with Next button — verify factory presets appear before any user preset, all categories cycle.
- [ ] Save a user preset "MyTest"; verify it appears after factory presets in the list.
- [ ] Init → all knobs return to defaults.
- [ ] Build AU and VST3: `cmake --build build --target Stitcher_AU Stitcher_VST3`
- [ ] Verify plugins installed under `~/Library/Audio/Plug-Ins/`.
- [ ] Run tests one final time: `./build/tests/StitcherTests` — all 39 pass.
- [ ] Final commit:

```bash
git add -u
git commit -m "phase3: factory preset bank — 51 presets, 8 categories, embedded"
```

---

## Self-Review

**Spec coverage check:**
- ✅ 51 presets across 8 categories
- ✅ Embedded via `juce_add_binary_data`
- ✅ Factory presets appear before user presets
- ✅ Prev/Next cycles through factory + user
- ✅ `getCategoryNames()` for future category dropdown
- ✅ Unit tests for factory preset loading (mock XML injection, no BinaryData dep in tests)
- ✅ pluginval-compatible (no new APVTS params, no audio-thread changes)
- ✅ Binary size: 51 × ~600 bytes XML ≈ 31 KB — well under 200 KB budget

**Type consistency check:**
- `FactoryPreset::xml` is `juce::String` — matches `loadFromXmlString(const juce::String&, ...)` ✓
- `makeFactoryPresets()` returns `std::vector<PresetManager::FactoryPreset>` — matches constructor param ✓
- `BinaryData::*_xml` and `BinaryData::*_xmlSize` naming — verified against JUCE `makeBinaryDataIdentifierName` rules ✓
