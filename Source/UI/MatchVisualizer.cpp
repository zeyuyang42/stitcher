#include "MatchVisualizer.h"
#include "../LookAndFeel/StitcherLookAndFeel.h"

void MatchVisualizer::tick(float ctrlRms, int matchedIndex, float corpusFill)
{
    // Push to ring buffer (overwrite oldest)
    ctrlHistory_[historyHead_] = ctrlRms;
    historyHead_ = (historyHead_ + 1) % kSlots;
    if (historyCount_ < kSlots) ++historyCount_;

    if (matchedIndex != matchedIndex_ && matchedIndex >= 0)
        matchPulse_ = 1.f;
    matchPulse_ *= 0.78f;  // ~150 ms 1/e decay at 30 Hz

    matchedIndex_ = matchedIndex;
    corpusFill_   = corpusFill;
}

void MatchVisualizer::paint(juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();
    if (W < 4 || H < 4) return;

    // Background
    g.setColour(StitcherLookAndFeel::Panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);
    g.setColour(StitcherLookAndFeel::Track.brighter(0.2f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.f, 1.f);

    const int trackH    = (H - 6) / 2;  // height for each track (2px gap between)
    const int gap       = H - 2 * trackH - 2;  // remaining gap (at least 2px)
    const int topY      = 1;
    const int bottomY   = topY + trackH + gap + 2;

    const float blockW  = static_cast<float>(W - 2) / static_cast<float>(kSlots);
    const float blockPad = std::max(1.f, blockW * 0.1f);

    // ── Top track: ctrl RMS history ────────────────────────────────────────
    // Draw slots left=oldest, right=newest
    // historyHead_ points to the oldest valid entry (or the next-to-write if full)
    for (int slot = 0; slot < kSlots; ++slot) {
        // Map slot to ring buffer index: slot 0 = oldest
        int ringIdx;
        if (historyCount_ < kSlots) {
            // Buffer not yet full: slot 0 is index 0, slot historyCount_-1 is newest
            if (slot >= historyCount_) {
                // Not yet filled — skip
                continue;
            }
            ringIdx = slot;
        } else {
            ringIdx = (historyHead_ + slot) % kSlots;
        }
        const float rms = ctrlHistory_[ringIdx];
        const float alpha = juce::jlimit(0.15f, 1.0f, rms * 4.f);  // scale up small RMS values

        const float bx = 1.f + slot * blockW;
        const juce::Rectangle<float> block(bx + blockPad, static_cast<float>(topY),
                                           blockW - 2.f * blockPad, static_cast<float>(trackH));

        g.setColour(StitcherLookAndFeel::Accent.withAlpha(alpha));
        g.fillRoundedRectangle(block, 1.f);
    }

    // ── Bottom track: corpus slots ─────────────────────────────────────────
    const int filledSlots = juce::jlimit(0, kSlots,
                                static_cast<int>(std::round(corpusFill_ * kSlots)));

    // Which display slot corresponds to the matched corpus index?
    // corpusFill_ * kSlots gives filled count. matchedIndex_ is the logical index in CorpusStore.
    // Map matchedIndex_ to display slot: scale to [0, kSlots-1]
    // If corpusFill_ == 0, no match slot.
    int matchDisplaySlot = -1;
    if (matchedIndex_ >= 0 && filledSlots > 0) {
        // Corpus logical indices [0..corpusFill_*maxFrames-1] map to display [0..kSlots-1]
        // We don't know maxFrames here, so use a simpler heuristic:
        // Assume matchedIndex_ is in [0, filledSlots*scale-1].
        // Actually corpusFill = size/maxFrames; matchedIndex = [0, size-1].
        // So slot = round(matchedIndex_ / size * (kSlots-1)) ... but we don't know size here.
        // Simplest: use corpusFill to estimate size = round(corpusFill * maxSlots).
        // We'll just map matchedIndex_ mod filledSlots to the display slot.
        // Actually, the simplest correct approach:
        //   matched_slot = matchedIndex_ * kSlots / max(1, round(corpusFill * kSlots))
        // but capped at kSlots-1.
        matchDisplaySlot = juce::jlimit(0, kSlots - 1,
            static_cast<int>(static_cast<float>(matchedIndex_) /
                             static_cast<float>(juce::jmax(1, filledSlots)) * kSlots));
        matchDisplaySlot = juce::jlimit(0, filledSlots - 1, matchDisplaySlot);
    }

    for (int slot = 0; slot < kSlots; ++slot) {
        const float bx = 1.f + slot * blockW;
        const juce::Rectangle<float> block(bx + blockPad, static_cast<float>(bottomY),
                                           blockW - 2.f * blockPad, static_cast<float>(trackH));

        if (slot < filledSlots) {
            const bool isMatch = (slot == matchDisplaySlot);
            if (isMatch && matchPulse_ > 0.01f) {
                // Outer glow
                g.setColour(StitcherLookAndFeel::Accent.withAlpha(0.4f * matchPulse_));
                g.fillRoundedRectangle(block.expanded(2.f), 2.f);
                // Blended fill: AccentDim → Accent by pulse
                auto colour = StitcherLookAndFeel::AccentDim.interpolatedWith(
                    StitcherLookAndFeel::Accent, matchPulse_);
                g.setColour(colour);
            } else {
                g.setColour(isMatch ? StitcherLookAndFeel::Accent
                                    : StitcherLookAndFeel::AccentDim);
            }
            g.fillRoundedRectangle(block, 1.f);
        } else {
            g.setColour(StitcherLookAndFeel::Track.brighter(0.1f));
            g.fillRoundedRectangle(block, 1.f);
        }
    }
}
