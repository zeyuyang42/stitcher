#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "MidiLearn.h"

// ─── Stub parameter ───────────────────────────────────────────────────────────

struct StubParam : juce::RangedAudioParameter {
    explicit StubParam(const juce::String& id)
        : juce::RangedAudioParameter({id, 1}, id) {}

    float getValue() const override                           { return value_; }
    void  setValue(float v) override                          { value_ = v; }
    float getDefaultValue() const override                    { return 0.f; }
    juce::String getText(float, int) const override           { return {}; }
    float getValueForText(const juce::String&) const override { return 0.f; }

    // RangedAudioParameter pure virtual
    const juce::NormalisableRange<float>& getNormalisableRange() const override
    {
        static const juce::NormalisableRange<float> r { 0.f, 1.f };
        return r;
    }

    // setValueNotifyingHost is NOT virtual in JUCE — we intercept dispatch via
    // setValue() which is called internally. For the tests we just read value_.
    float value_ = 0.f;
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("MidiLearn: bind and dispatch CC") {
    MidiLearn ml;
    StubParam param("test_param");

    // Simulate what processCapture does: bind CC 7 directly via startLearning + handleCC + processCapture
    ml.startLearning(&param);
    ml.handleCC(7, 0);          // triggers capture
    REQUIRE(ml.processCapture()); // commits binding

    // CC 7 with value 64 → param value should be 64/127
    ml.handleCC(7, 64);
    REQUIRE(param.value_ == Catch::Approx(64.f / 127.f).margin(1e-5f));
}

TEST_CASE("MidiLearn: learning capture binds next CC") {
    MidiLearn ml;
    StubParam param("p1");

    ml.startLearning(&param);
    REQUIRE(ml.isLearning());

    // Audio thread receives CC 10 during learning
    ml.handleCC(10, 0);
    REQUIRE(!ml.isLearning() == false); // still flagged until processCapture

    // Message thread drains
    REQUIRE(ml.processCapture());
    REQUIRE(!ml.isLearning());
    REQUIRE(ml.getCCForParam(&param) == 10);

    // Now dispatch CC 10 → param should be updated
    ml.handleCC(10, 127);
    REQUIRE(param.value_ == Catch::Approx(1.f).margin(1e-5f));
}

TEST_CASE("MidiLearn: rebind overwrites previous binding") {
    MidiLearn ml;
    StubParam paramA("paramA");
    StubParam paramB("paramB");

    // Bind CC 7 to paramA
    ml.startLearning(&paramA);
    ml.handleCC(7, 0);
    ml.processCapture();
    REQUIRE(ml.getCCForParam(&paramA) == 7);

    // Bind CC 7 to paramB (should evict paramA)
    ml.startLearning(&paramB);
    ml.handleCC(7, 0);
    ml.processCapture();
    REQUIRE(ml.getCCForParam(&paramB) == 7);
    REQUIRE(ml.getCCForParam(&paramA) == -1); // paramA is no longer bound

    // Dispatch CC 7 → only paramB updated
    ml.handleCC(7, 64);
    REQUIRE(paramB.value_ == Catch::Approx(64.f / 127.f).margin(1e-5f));
    REQUIRE(paramA.value_ == 0.f); // unchanged
}

TEST_CASE("MidiLearn: serialization round-trip") {
    StubParam param("my_param");

    // Build source MidiLearn with CC 5 bound to param
    MidiLearn src;
    src.startLearning(&param);
    src.handleCC(5, 0);
    src.processCapture();
    REQUIRE(src.getCCForParam(&param) == 5);

    // Serialize
    std::unique_ptr<juce::XmlElement> xml(src.createBindingsXml());
    REQUIRE(xml != nullptr);
    REQUIRE(xml->getTagName() == "MIDI_LEARN");

    // Build a fresh MidiLearn and restore from XML using an Array of params.
    StubParam param2("my_param");
    juce::Array<juce::AudioProcessorParameter*> paramArray;
    paramArray.add(&param2);

    MidiLearn dst;
    dst.restoreBindingsXml(xml.get(), paramArray);
    REQUIRE(dst.getCCForParam(&param2) == 5);

    // Dispatch to verify the binding is live
    dst.handleCC(5, 127);
    REQUIRE(param2.value_ == Catch::Approx(1.f).margin(1e-5f));
}
