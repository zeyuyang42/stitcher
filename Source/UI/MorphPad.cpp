#include "MorphPad.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void MorphPad::setParams(juce::RangedAudioParameter* zcr,
                          juce::RangedAudioParameter* rms,
                          juce::RangedAudioParameter* sc,
                          juce::RangedAudioParameter* st)
{
    paramZcr_ = zcr;
    paramRms_ = rms;
    paramSc_  = sc;
    paramSt_  = st;
}

void MorphPad::setLiveLevels(float zcr, float rms, float sc, float st)
{
    liveZcr_.store(juce::jlimit(0.f, 1.f, zcr));
    liveRms_.store(juce::jlimit(0.f, 1.f, rms));
    liveSc_ .store(juce::jlimit(0.f, 1.f, sc));
    liveSt_ .store(juce::jlimit(0.f, 1.f, st));
}

// ── Static math helpers ────────────────────────────────────────────────────

void MorphPad::computeWeights(juce::Point<float> norm,
                               float& zcr, float& rms, float& sc, float& st)
{
    const float x = norm.x;
    const float y = norm.y;
    zcr = (1.f - x) * (1.f - y);
    rms =        x  * (1.f - y);
    sc  = (1.f - x) *        y;
    st  =        x  *        y;
}

juce::Point<float> MorphPad::computeThumb(float zcr, float rms, float sc, float st)
{
    const float total = zcr + rms + sc + st;
    if (total < 0.001f)
        return { 0.5f, 0.5f };
    const float x = (rms + st) / total;
    const float y = (sc  + st) / total;
    return { x, y };
}

// ── Private helpers ────────────────────────────────────────────────────────

juce::Point<float> MorphPad::thumbFromWeights() const
{
    const float zcr = paramZcr_ ? paramZcr_->getValue() : 0.f;
    const float rms = paramRms_ ? paramRms_->getValue() : 0.f;
    const float sc  = paramSc_  ? paramSc_ ->getValue() : 0.f;
    const float st  = paramSt_  ? paramSt_ ->getValue() : 0.f;
    return computeThumb(zcr, rms, sc, st);
}

void MorphPad::writeWeightsFromThumb(juce::Point<float> normPos)
{
    float zcr, rms, sc, st;
    computeWeights(normPos, zcr, rms, sc, st);

    if (paramZcr_) paramZcr_->setValueNotifyingHost(paramZcr_->convertTo0to1(zcr));
    if (paramRms_) paramRms_->setValueNotifyingHost(paramRms_->convertTo0to1(rms));
    if (paramSc_)  paramSc_ ->setValueNotifyingHost(paramSc_ ->convertTo0to1(sc));
    if (paramSt_)  paramSt_ ->setValueNotifyingHost(paramSt_ ->convertTo0to1(st));
}

juce::Point<float> MorphPad::normFromMouseEvent(const juce::MouseEvent& e) const
{
    const auto b = getLocalBounds().toFloat();
    const float x = juce::jlimit(0.f, 1.f, e.position.x / b.getWidth());
    const float y = juce::jlimit(0.f, 1.f, e.position.y / b.getHeight());
    return { x, y };
}

// ── Mouse handling ─────────────────────────────────────────────────────────

void MorphPad::mouseDown(const juce::MouseEvent& e)
{
    writeWeightsFromThumb(normFromMouseEvent(e));
    repaint();
}

void MorphPad::mouseDrag(const juce::MouseEvent& e)
{
    writeWeightsFromThumb(normFromMouseEvent(e));
    repaint();
}

// ── Paint ──────────────────────────────────────────────────────────────────

void MorphPad::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Background fill
    g.setColour(StitcherLookAndFeel::Panel);
    g.fillRoundedRectangle(bounds, 4.f);

    // Border
    g.setColour(StitcherLookAndFeel::Track.brighter(0.2f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.f, 1.f);

    // Grid lines (3×3 grid → 2 interior lines in each axis)
    g.setColour(StitcherLookAndFeel::Track.brighter(0.1f));
    for (int i = 1; i <= 2; ++i) {
        const float xLine = bounds.getX() + bounds.getWidth()  * (static_cast<float>(i) / 3.f);
        const float yLine = bounds.getY() + bounds.getHeight() * (static_cast<float>(i) / 3.f);
        g.drawLine(xLine, bounds.getY(), xLine, bounds.getBottom(), 1.f);
        g.drawLine(bounds.getX(), yLine, bounds.getRight(), yLine, 1.f);
    }

    // Corner glow dots
    const float dotRadius = 8.f;
    const float dotOffset = 12.f;  // centre distance from each corner edge

    struct CornerDot {
        float cx, cy;
        float liveLevel;
    };

    const CornerDot dots[4] = {
        // TL — ZCR
        { bounds.getX() + dotOffset,                bounds.getY() + dotOffset,                liveZcr_.load() },
        // TR — RMS
        { bounds.getRight() - dotOffset,            bounds.getY() + dotOffset,                liveRms_.load() },
        // BL — SC
        { bounds.getX() + dotOffset,                bounds.getBottom() - dotOffset,           liveSc_.load()  },
        // BR — ST
        { bounds.getRight() - dotOffset,            bounds.getBottom() - dotOffset,           liveSt_.load()  },
    };

    for (const auto& dot : dots) {
        const float alpha = juce::jmax(0.15f, dot.liveLevel);
        g.setColour(StitcherLookAndFeel::Accent.withAlpha(alpha));
        g.fillEllipse(dot.cx - dotRadius, dot.cy - dotRadius,
                      dotRadius * 2.f, dotRadius * 2.f);
    }

    // Thumb crosshair + circle
    const juce::Point<float> thumb = thumbFromWeights();
    const float thumbX = bounds.getX() + thumb.x * bounds.getWidth();
    const float thumbY = bounds.getY() + thumb.y * bounds.getHeight();

    const auto crosshairColour = StitcherLookAndFeel::Accent.withAlpha(0.5f);
    g.setColour(crosshairColour);
    g.drawLine(thumbX, bounds.getY(), thumbX, bounds.getBottom(), 1.f);
    g.drawLine(bounds.getX(), thumbY, bounds.getRight(), thumbY, 1.f);

    const float thumbRadius = 5.f;
    g.setColour(StitcherLookAndFeel::Accent);
    g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius,
                  thumbRadius * 2.f, thumbRadius * 2.f);
}
