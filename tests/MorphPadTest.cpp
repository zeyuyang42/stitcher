#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include "../Source/UI/MorphPad.h"

using Catch::Approx;

// ── Bilinear forward (thumb → weights) ───────────────────────────────────

TEST_CASE("MorphPad bilinear forward: TL corner (0,0) → ZCR=1", "[MorphPad]")
{
    float zcr, rms, sc, st;
    MorphPad::computeWeights({ 0.f, 0.f }, zcr, rms, sc, st);
    REQUIRE(zcr == Approx(1.f));
    REQUIRE(rms == Approx(0.f));
    REQUIRE(sc  == Approx(0.f));
    REQUIRE(st  == Approx(0.f));
}

TEST_CASE("MorphPad bilinear forward: TR corner (1,0) → RMS=1", "[MorphPad]")
{
    float zcr, rms, sc, st;
    MorphPad::computeWeights({ 1.f, 0.f }, zcr, rms, sc, st);
    REQUIRE(zcr == Approx(0.f));
    REQUIRE(rms == Approx(1.f));
    REQUIRE(sc  == Approx(0.f));
    REQUIRE(st  == Approx(0.f));
}

TEST_CASE("MorphPad bilinear forward: BL corner (0,1) → SC=1", "[MorphPad]")
{
    float zcr, rms, sc, st;
    MorphPad::computeWeights({ 0.f, 1.f }, zcr, rms, sc, st);
    REQUIRE(zcr == Approx(0.f));
    REQUIRE(rms == Approx(0.f));
    REQUIRE(sc  == Approx(1.f));
    REQUIRE(st  == Approx(0.f));
}

TEST_CASE("MorphPad bilinear forward: BR corner (1,1) → ST=1", "[MorphPad]")
{
    float zcr, rms, sc, st;
    MorphPad::computeWeights({ 1.f, 1.f }, zcr, rms, sc, st);
    REQUIRE(zcr == Approx(0.f));
    REQUIRE(rms == Approx(0.f));
    REQUIRE(sc  == Approx(0.f));
    REQUIRE(st  == Approx(1.f));
}

TEST_CASE("MorphPad bilinear forward: centre (0.5,0.5) → all equal 0.25", "[MorphPad]")
{
    float zcr, rms, sc, st;
    MorphPad::computeWeights({ 0.5f, 0.5f }, zcr, rms, sc, st);
    REQUIRE(zcr == Approx(0.25f));
    REQUIRE(rms == Approx(0.25f));
    REQUIRE(sc  == Approx(0.25f));
    REQUIRE(st  == Approx(0.25f));
}

// ── Inverse mapping (weights → thumb) ────────────────────────────────────

TEST_CASE("MorphPad inverse: ZCR-only weights (1,0,0,0) → thumb (0,0)", "[MorphPad]")
{
    auto thumb = MorphPad::computeThumb(1.f, 0.f, 0.f, 0.f);
    REQUIRE(thumb.x == Approx(0.f));
    REQUIRE(thumb.y == Approx(0.f));
}

TEST_CASE("MorphPad inverse: RMS-only weights (0,1,0,0) → thumb (1,0)", "[MorphPad]")
{
    auto thumb = MorphPad::computeThumb(0.f, 1.f, 0.f, 0.f);
    REQUIRE(thumb.x == Approx(1.f));
    REQUIRE(thumb.y == Approx(0.f));
}

TEST_CASE("MorphPad inverse: SC-only weights (0,0,1,0) → thumb (0,1)", "[MorphPad]")
{
    auto thumb = MorphPad::computeThumb(0.f, 0.f, 1.f, 0.f);
    REQUIRE(thumb.x == Approx(0.f));
    REQUIRE(thumb.y == Approx(1.f));
}

TEST_CASE("MorphPad inverse: ST-only weights (0,0,0,1) → thumb (1,1)", "[MorphPad]")
{
    auto thumb = MorphPad::computeThumb(0.f, 0.f, 0.f, 1.f);
    REQUIRE(thumb.x == Approx(1.f));
    REQUIRE(thumb.y == Approx(1.f));
}

TEST_CASE("MorphPad inverse: equal ZCR+RMS (0.5,0.5,0,0) → thumb (0.5, 0)", "[MorphPad]")
{
    auto thumb = MorphPad::computeThumb(0.5f, 0.5f, 0.f, 0.f);
    REQUIRE(thumb.x == Approx(0.5f));
    REQUIRE(thumb.y == Approx(0.f));
}
