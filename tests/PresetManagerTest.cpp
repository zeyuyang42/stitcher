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
