#pragma once
#include <JuceHeader.h>

class StitcherLookAndFeel : public juce::LookAndFeel_V4 {
public:
    StitcherLookAndFeel();
    ~StitcherLookAndFeel() override = default;

    // Palette
    static const juce::Colour Background;
    static const juce::Colour Panel;
    static const juce::Colour Accent;
    static const juce::Colour AccentDim;
    static const juce::Colour TextBright;
    static const juce::Colour TextDim;
    static const juce::Colour Track;

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawGroupComponentOutline(juce::Graphics&, int w, int h,
                                   const juce::String& text,
                                   const juce::Justification&,
                                   juce::GroupComponent&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getLabelFont(juce::Label&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;

private:
    juce::Typeface::Ptr interTypeface_;
    std::optional<juce::Font> interFont_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StitcherLookAndFeel)
};
