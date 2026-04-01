#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "FeatureExtractor.h"
#include <vector>
#include <cmath>

TEST_CASE("ZCR is 0 for silence") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024, 0.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.zcr == Catch::Approx(0.f));
}

TEST_CASE("ZCR is approximately 1 for fully alternating signal") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024);
    for (int i = 0; i < 1024; ++i)
        buf[i] = (i % 2 == 0) ? 1.f : -1.f;
    auto f = fe.extract(buf.data(), 1024);
    // 1023 zero crossings in 1024 samples → ZCR = 1023/1023 = 1.0
    REQUIRE(f.zcr == Catch::Approx(1.f).margin(0.01f));
}

TEST_CASE("RMS is 0 for silence") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024, 0.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.rms == Catch::Approx(0.f));
}

TEST_CASE("RMS is 1 for DC signal of amplitude 1") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024, 1.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.rms == Catch::Approx(1.f));
}

TEST_CASE("RMS of sine wave is amplitude / sqrt(2)") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024);
    const float A = 0.8f;
    for (int i = 0; i < 1024; ++i)
        buf[i] = A * std::sin(2.f * 3.14159f * 10.f * i / 1024.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.rms == Catch::Approx(A / std::sqrt(2.f)).margin(0.01f));
}

TEST_CASE("SC is 0 for silence") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024, 0.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.sc == Catch::Approx(0.f));
}

TEST_CASE("SC is in [0,1] for any signal") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024);
    for (int i = 0; i < 1024; ++i)
        buf[i] = std::sin(2.f * 3.14159f * 100.f * i / 1024.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.sc >= 0.f);
    REQUIRE(f.sc <= 1.f);
}

TEST_CASE("ST is in [0,1] for any signal") {
    FeatureExtractor fe;
    fe.prepare(1024);
    std::vector<float> buf(1024);
    for (int i = 0; i < 1024; ++i)
        buf[i] = std::sin(2.f * 3.14159f * 400.f * i / 1024.f);
    auto f = fe.extract(buf.data(), 1024);
    REQUIRE(f.st >= 0.f);
    REQUIRE(f.st <= 1.f);
}
