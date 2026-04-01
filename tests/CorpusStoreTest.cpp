#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "CorpusStore.h"
#include <vector>

static Features zeroFeatures() { return {0.f, 0.f, 0.f, 0.f}; }

TEST_CASE("CorpusStore starts empty") {
    CorpusStore store;
    store.prepare(4, 5);
    REQUIRE(store.size() == 0);
}

TEST_CASE("Push increases size up to maxFrames") {
    CorpusStore store;
    store.prepare(4, 3);
    float audio[4] = {1.f, 2.f, 3.f, 4.f};
    store.push(audio, zeroFeatures());
    REQUIRE(store.size() == 1);
    store.push(audio, zeroFeatures());
    store.push(audio, zeroFeatures());
    REQUIRE(store.size() == 3);
}

TEST_CASE("Push wraps circularly when full") {
    CorpusStore store;
    store.prepare(4, 3);
    float a[4] = {1.f, 1.f, 1.f, 1.f};
    float b[4] = {2.f, 2.f, 2.f, 2.f};
    store.push(a, zeroFeatures());
    store.push(a, zeroFeatures());
    store.push(a, zeroFeatures()); // full
    store.push(b, zeroFeatures()); // overwrites oldest
    REQUIRE(store.size() == 3);
    // newest frame should contain b values
    REQUIRE(store.getFrame(store.newestIndex()).audio[0] == Catch::Approx(2.f));
}

TEST_CASE("Freeze prevents new frames from being written") {
    CorpusStore store;
    store.prepare(4, 5);
    float audio[4] = {1.f, 1.f, 1.f, 1.f};
    store.push(audio, zeroFeatures());
    store.setFrozen(true);
    store.push(audio, zeroFeatures());
    REQUIRE(store.size() == 1);
}

TEST_CASE("getFrame returns stored audio and features") {
    CorpusStore store;
    store.prepare(4, 5);
    float audio[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    Features f{0.5f, 0.6f, 0.7f, 0.8f};
    store.push(audio, f);
    const auto& frame = store.getFrame(0);
    REQUIRE(frame.audio[0] == Catch::Approx(0.1f));
    REQUIRE(frame.features.zcr == Catch::Approx(0.5f));
    REQUIRE(frame.features.rms == Catch::Approx(0.6f));
}
