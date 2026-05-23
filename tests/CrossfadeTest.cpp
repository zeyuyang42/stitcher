#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

// Minimal standalone simulation of the two-voice OLA grain engine,
// mirroring the logic in StitcherProcessor::processBlock exactly.
namespace {

struct SimVoice {
    std::vector<float> buf;
    int  pos    = 0;
    bool active = false;
};

void applyGrainWindow(SimVoice& v, int frameSize, int xfadeLen)
{
    const int ramp = std::min(xfadeLen, frameSize / 2);
    if (ramp < 2) return;
    const float rampF = static_cast<float>(ramp - 1);
    for (int j = 0; j < ramp; ++j) {
        const float t = static_cast<float>(j) / rampF;
        v.buf[j]                  *= t;
        v.buf[frameSize - 1 - j] *= t;
    }
}

struct SimEngine {
    int frameSize;
    int xfadeLen;

    SimVoice voices[2];
    int  activeSlot = 0;
    bool grainReady = false;

    SimEngine(int fs, int xfl) : frameSize(fs), xfadeLen(xfl) {
        for (auto& v : voices) v.buf.resize(static_cast<size_t>(fs), 0.f);
    }

    void handoff(float grainValue) {
        const int newSlot = grainReady ? 1 - activeSlot : 0;
        auto& nv = voices[newSlot];
        std::fill(nv.buf.begin(), nv.buf.end(), grainValue);

        if (xfadeLen > 0) {
            applyGrainWindow(nv, frameSize, xfadeLen);
        } else {
            if (grainReady) voices[activeSlot].active = false;
        }

        nv.pos    = 0;
        nv.active = true;
        activeSlot  = newSlot;
        grainReady  = true;
    }

    float renderSample() {
        float out = 0.f;
        for (auto& v : voices) {
            if (!v.active) continue;
            out += v.buf[static_cast<size_t>(v.pos)];
            if (++v.pos >= frameSize) v.active = false;
        }
        return out;
    }
};

} // namespace

TEST_CASE("Two-voice OLA — windowed grain: no large per-sample jump") {
    const int frameSize = 256;
    const int xfadeLen  = 64;
    SimEngine eng(frameSize, xfadeLen);

    // Frame 1: constant value 1.0 (old grain)
    eng.handoff(1.0f);
    std::vector<float> out;
    for (int i = 0; i < frameSize; ++i)
        out.push_back(eng.renderSample());

    // Frame 2: constant value 0.5 (new grain — very different, stress test for clicks)
    eng.handoff(0.5f);
    for (int i = 0; i < frameSize; ++i)
        out.push_back(eng.renderSample());

    // Maximum per-sample step must be bounded by the ramp slope
    // (linear ramp of 1.0 over ramp-1 steps = 1/(ramp-1) per sample)
    float maxStep = 0.f;
    for (int i = 1; i < static_cast<int>(out.size()); ++i)
        maxStep = std::max(maxStep, std::abs(out[i] - out[i - 1]));

    const int ramp = std::min(xfadeLen, frameSize / 2);
    const float allowedStep = 1.0f / static_cast<float>(ramp - 1) + 1e-4f;
    CHECK(maxStep <= allowedStep);
}

TEST_CASE("Two-voice OLA — xfade=0 preserves click at grain boundary") {
    const int frameSize = 256;
    SimEngine eng(frameSize, 0);  // no crossfade

    eng.handoff(1.0f);
    std::vector<float> out;
    for (int i = 0; i < frameSize; ++i)
        out.push_back(eng.renderSample());

    eng.handoff(0.0f);  // new grain is silence — maximises jump
    for (int i = 0; i < frameSize; ++i)
        out.push_back(eng.renderSample());

    // Jump at boundary: last sample of old grain (1.0) → first of new (0.0)
    const float jumpAtBoundary = std::abs(out[frameSize] - out[frameSize - 1]);
    CHECK(jumpAtBoundary >= 0.99f);
}

TEST_CASE("Two-voice OLA — voice deactivates after exactly frameSize samples") {
    const int frameSize = 128;
    SimEngine eng(frameSize, 32);

    eng.handoff(1.0f);

    // Render frameSize-1 samples; voice should still be active
    for (int i = 0; i < frameSize - 1; ++i)
        eng.renderSample();

    bool stillActiveBeforeLast = eng.voices[eng.activeSlot].active;
    eng.renderSample();  // frameSize-th render; pos hits frameSize → inactive
    bool inactiveAfterLast = !eng.voices[0].active && !eng.voices[1].active;

    CHECK(stillActiveBeforeLast);
    CHECK(inactiveAfterLast);
}

TEST_CASE("Two-voice OLA — windowed grain endpoints are zero") {
    const int frameSize = 256;
    const int xfadeLen  = 128;
    SimEngine eng(frameSize, xfadeLen);

    eng.handoff(1.0f);
    const auto& buf = eng.voices[eng.activeSlot].buf;

    // First and last samples must be 0 after windowing
    CHECK(std::abs(buf[0]) < 1e-6f);
    CHECK(std::abs(buf[frameSize - 1]) < 1e-6f);

    // Middle samples should retain full amplitude
    CHECK(buf[frameSize / 2] == Catch::Approx(1.0f).epsilon(0.01));
}
