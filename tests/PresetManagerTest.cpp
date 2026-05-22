#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PresetManager.h"
#include "Parameters.h"

struct JuceInit {
    JuceInit()  { juce::initialiseJuce_GUI(); }
    ~JuceInit() { juce::shutdownJuce_GUI(); }
};

struct DummyProcessor : public juce::AudioProcessor {
    DummyProcessor() : juce::AudioProcessor(
        BusesProperties()
            .withInput ("In",  juce::AudioChannelSet::stereo())
            .withOutput("Out", juce::AudioChannelSet::stereo())) {}
    const juce::String getName() const override { return "Dummy"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
};

TEST_CASE("PresetManager: getPresetNames returns empty list when dir is empty") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    REQUIRE(pm.getPresetNames().isEmpty());
    REQUIRE(pm.getCurrentPresetName().isEmpty());

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: save/load roundtrip restores parameter value") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(0.42f);
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(0.42f).margin(0.01f));

    pm.savePreset("TestPreset");
    REQUIRE(pm.getPresetNames().contains("TestPreset"));

    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(1.0f);

    bool loaded = pm.loadPreset("TestPreset");
    REQUIRE(loaded);
    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(0.42f).margin(0.01f));
    REQUIRE(pm.getCurrentPresetName() == "TestPreset");

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: init resets all params to defaults") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    apvts.getParameterAsValue(ParamIDs::rmsWeight).setValue(0.42f);
    pm.savePreset("Temp");
    pm.initPreset();

    REQUIRE(apvts.getRawParameterValue(ParamIDs::rmsWeight)->load() == Catch::Approx(1.f).margin(0.01f));
    REQUIRE(pm.getCurrentPresetName().isEmpty());

    tempDir.deleteRecursively();
}

TEST_CASE("PresetManager: selectNext/selectPrev cycles through presets") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};
    PresetManager pm{apvts, tempDir};

    pm.savePreset("Alpha");
    pm.savePreset("Beta");
    pm.savePreset("Gamma");

    pm.setCurrentPresetName("Alpha");
    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Beta");

    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Gamma");

    pm.selectNext();
    REQUIRE(pm.getCurrentPresetName() == "Alpha");

    pm.selectPrev();
    REQUIRE(pm.getCurrentPresetName() == "Gamma");

    tempDir.deleteRecursively();
}

// Helper: builds a minimal valid preset XML string
static juce::String makePresetXml(float rmsWeight)
{
    return juce::String(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<PARAMETERS>\n"
        "  <PARAM id=\"zcr_weight\"  value=\"0.0\"/>\n"
        "  <PARAM id=\"rms_weight\"  value=\"") + juce::String(rmsWeight) + "\"/>\n"
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

    pm.savePreset("Zeta");

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
    REQUIRE(pm.getCurrentPresetName() == "Factory1");

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

TEST_CASE("PresetManager: savePreset refuses to shadow a factory preset name") {
    JuceInit init;
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("StitcherPresetTest_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.deleteRecursively();

    DummyProcessor proc;
    juce::AudioProcessorValueTreeState apvts{proc, nullptr, "P", createParameterLayout()};

    std::vector<PresetManager::FactoryPreset> factory {
        { "Init", "Utility", makePresetXml(1.0f) },
    };
    PresetManager pm{apvts, tempDir, factory};

    // Attempt to save a user preset with a factory name — should be silently rejected
    pm.savePreset("Init");
    // User preset file must NOT exist on disk
    REQUIRE_FALSE(tempDir.getChildFile("Init.xml").existsAsFile());
    // Current preset name must remain empty (save did not succeed)
    REQUIRE(pm.getCurrentPresetName().isEmpty());
    // "Init" appears exactly once in the names list (from factory)
    auto names = pm.getPresetNames();
    int count = 0;
    for (auto& n : names) if (n == "Init") ++count;
    REQUIRE(count == 1);

    tempDir.deleteRecursively();
}
