#include "FeatureMeter.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void FeatureMeter::setLevel(float v) noexcept  { level_  = juce::jlimit(0.f, 1.f, v); }
void FeatureMeter::setActive(bool a) noexcept  { active_ = a; }

void FeatureMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(StitcherLookAndFeel::Track);
    g.fillRoundedRectangle(b, 2.f);

    if (level_ > 0.f) {
        auto fill = b.withWidth(b.getWidth() * level_);
        g.setColour(active_ ? StitcherLookAndFeel::Accent
                             : StitcherLookAndFeel::AccentDim);
        g.fillRoundedRectangle(fill, 2.f);
    }
}
