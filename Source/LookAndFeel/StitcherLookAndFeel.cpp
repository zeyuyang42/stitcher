#include "StitcherLookAndFeel.h"
#include <BinaryData.h>

const juce::Colour StitcherLookAndFeel::Background { 0xFF1A1A1F };
const juce::Colour StitcherLookAndFeel::Panel       { 0xFF252530 };
const juce::Colour StitcherLookAndFeel::Accent      { 0xFFFFA84C };
const juce::Colour StitcherLookAndFeel::AccentDim   { 0xFF7A5025 };
const juce::Colour StitcherLookAndFeel::TextBright  { 0xFFE8E4DC };
const juce::Colour StitcherLookAndFeel::TextDim     { 0xFF8A8680 };
const juce::Colour StitcherLookAndFeel::Track       { 0xFF3A3A45 };

StitcherLookAndFeel::StitcherLookAndFeel()
{
    interTypeface_ = juce::Typeface::createSystemTypefaceFor(
        BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    if (interTypeface_ != nullptr)
        interFont_.emplace(juce::FontOptions(interTypeface_).withHeight(12.f));
    else
        interFont_.emplace(juce::FontOptions(12.f));
    interFont10Bold_.emplace(interFont_->withHeight(10.f).boldened());
    interFont11_.emplace(interFont_->withHeight(11.f));
    interFont13_.emplace(interFont_->withHeight(13.f));

    setColour(juce::ResizableWindow::backgroundColourId,        Background);
    setColour(juce::GroupComponent::outlineColourId,            Panel.brighter(0.15f));
    setColour(juce::GroupComponent::textColourId,               TextDim);
    setColour(juce::Slider::rotarySliderFillColourId,           Accent);
    setColour(juce::Slider::rotarySliderOutlineColourId,        Track);
    setColour(juce::Slider::thumbColourId,                      TextBright);
    setColour(juce::Slider::textBoxTextColourId,                TextDim);
    setColour(juce::Slider::textBoxBackgroundColourId,          juce::Colour(0x00000000));
    setColour(juce::Slider::textBoxOutlineColourId,             juce::Colour(0x00000000));
    setColour(juce::Label::textColourId,                        TextDim);
    setColour(juce::ToggleButton::textColourId,                 TextBright);
    setColour(juce::PopupMenu::backgroundColourId,              Panel);
    setColour(juce::PopupMenu::textColourId,                    TextBright);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,   Accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId,         Accent);
    setColour(juce::TextButton::buttonColourId,   Track);
    setColour(juce::TextButton::buttonOnColourId,  Accent);
    setColour(juce::TextButton::textColourOffId,   TextBright);
    setColour(juce::TextButton::textColourOnId,    Background);
}

void StitcherLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& /*slider*/)
{
    const float margin = 6.f;
    auto bounds = juce::Rectangle<float>(
        static_cast<float>(x) + margin,
        static_cast<float>(y) + margin,
        static_cast<float>(width)  - margin * 2.f,
        static_cast<float>(height) - margin * 2.f);

    const float radius   = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX  = bounds.getCentreX();
    const float centreY  = bounds.getCentreY();
    const float angle    = rotaryStartAngle + sliderPosProportional
                           * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 3.f;

    // Background track arc
    juce::Path trackPath;
    trackPath.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                            0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Track);
    g.strokePath(trackPath, juce::PathStrokeType(
        3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Amber value arc
    juce::Path valueArc;
    valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                           0.f, rotaryStartAngle, angle, true);
    g.setColour(Accent);
    g.strokePath(valueArc, juce::PathStrokeType(
        3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body
    const float knobR = radius - 9.f;
    g.setColour(Panel);
    g.fillEllipse(centreX - knobR, centreY - knobR, knobR * 2.f, knobR * 2.f);

    // Subtle rim
    g.setColour(Track.brighter(0.3f));
    g.drawEllipse(centreX - knobR, centreY - knobR, knobR * 2.f, knobR * 2.f, 1.f);

    // Indicator line
    const float pointerLen = knobR * 0.65f;
    const float px = centreX + std::sin(angle) * pointerLen;
    const float py = centreY - std::cos(angle) * pointerLen;
    g.setColour(TextBright);
    g.drawLine(centreX, centreY, px, py, 2.f);

    // Dot at tip
    g.setColour(Accent);
    g.fillEllipse(px - 2.f, py - 2.f, 4.f, 4.f);
}

void StitcherLookAndFeel::drawGroupComponentOutline(juce::Graphics& g,
    int width, int height, const juce::String& text,
    const juce::Justification& /*position*/, juce::GroupComponent& /*group*/)
{
    const float cornerSize = 6.f;

    auto bounds = juce::Rectangle<float>(0.f, 8.f,
        static_cast<float>(width), static_cast<float>(height) - 8.f);

    // Panel fill
    g.setColour(Panel);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Border
    g.setColour(Track.brighter(0.2f));
    g.drawRoundedRectangle(bounds, cornerSize, 1.f);

    // Section label (uppercase, centred at the top border)
    if (text.isNotEmpty()) {
        const float textH = 14.f;
        auto textBounds = bounds.withHeight(textH).translated(0.f, -7.f);

        g.setColour(Panel);
        g.fillRect(textBounds.expanded(4.f, 0.f));

        g.setColour(TextDim);
        g.setFont(interFont10Bold_.value());
        g.drawText(text.toUpperCase(), textBounds.toNearestInt(),
                   juce::Justification::centred, false);
    }
}

void StitcherLookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& button, bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.f);
    const float cornerR = bounds.getHeight() * 0.5f;
    const bool on = button.getToggleState();

    juce::Colour fill = on ? Accent : Track;
    if (isMouseOver || isButtonDown)
        fill = fill.brighter(0.15f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, cornerR);

    g.setColour(on ? Accent.brighter(0.3f) : Track.brighter(0.3f));
    g.drawRoundedRectangle(bounds, cornerR, 1.f);

    g.setColour(on ? Background : TextBright);
    g.setFont(interFont11_.value());
    g.drawText(button.getButtonText(), button.getLocalBounds(),
               juce::Justification::centred, false);
}

juce::Font StitcherLookAndFeel::getLabelFont(juce::Label& /*l*/)
{
    return interFont_.value();
}

void StitcherLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(),
                         label.getLocalBounds(),
                         label.getJustificationType(),
                         2, 1.f);
    }
}

juce::Font StitcherLookAndFeel::getPopupMenuFont()
{
    return interFont13_.value();
}
