#include "LevelMeter.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void LevelMeter::setLevels(float l, float r) noexcept {
    peakL_ = juce::jlimit(0.f, 1.f, l);
    peakR_ = juce::jlimit(0.f, 1.f, r);
}

void LevelMeter::paint(juce::Graphics& g)
{
    if (getWidth() < 4) return;

    const int w2 = getWidth() / 2 - 1;
    const int h  = getHeight();

    auto paintBar = [&](int x, float peak) {
        juce::Rectangle<int> bg{x, 0, w2, h};
        g.setColour(StitcherLookAndFeel::Track);
        g.fillRoundedRectangle(bg.toFloat(), 2.f);

        if (peak > 0.f) {
            const float fillH = static_cast<float>(h) * peak;
            juce::Rectangle<float> fill{static_cast<float>(x), static_cast<float>(h) - fillH,
                                        static_cast<float>(w2), fillH};
            g.setColour(StitcherLookAndFeel::Accent);
            g.fillRoundedRectangle(fill, 2.f);
        }
    };

    paintBar(0,       peakL_);
    paintBar(w2 + 2,  peakR_);
}
