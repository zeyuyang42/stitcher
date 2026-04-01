#include "FeatureExtractor.h"
#include <cmath>

void FeatureExtractor::prepare(int frameSize)
{
    frameSize_ = frameSize;
    fftOrder_ = static_cast<int>(std::log2(frameSize));
    fft_ = std::make_unique<juce::dsp::FFT>(fftOrder_);
    fftBuffer_.assign(frameSize * 2, 0.f);
}

Features FeatureExtractor::extract(const float* samples, int numSamples)
{
    Features f;
    const int N = std::min(numSamples, frameSize_);

    f.zcr = computeZcr(samples, N);
    f.rms = computeRms(samples, N);

    // Copy into FFT buffer (zero-padded to 2*frameSize for complex output)
    std::fill(fftBuffer_.begin(), fftBuffer_.end(), 0.f);
    for (int i = 0; i < N; ++i)
        fftBuffer_[i] = samples[i];

    fft_->performFrequencyOnlyForwardTransform(fftBuffer_.data());

    // fftBuffer_ now holds magnitudes for bins [0 .. frameSize_/2]
    const int halfN = frameSize_ / 2;
    f.sc = computeSc(fftBuffer_.data(), halfN);
    f.st = computeSt(fftBuffer_.data(), halfN);

    return f;
}

float FeatureExtractor::computeZcr(const float* samples, int N) const
{
    if (N <= 1) return 0.f;
    int crossings = 0;
    for (int i = 1; i < N; ++i)
        if ((samples[i] >= 0.f) != (samples[i - 1] >= 0.f))
            ++crossings;
    return static_cast<float>(crossings) / static_cast<float>(N - 1);
}

float FeatureExtractor::computeRms(const float* samples, int N) const
{
    if (N == 0) return 0.f;
    float sum = 0.f;
    for (int i = 0; i < N; ++i)
        sum += samples[i] * samples[i];
    return std::sqrt(sum / static_cast<float>(N));
}

float FeatureExtractor::computeSc(const float* mag, int halfN) const
{
    float weightedSum = 0.f;
    float totalMag    = 0.f;
    for (int k = 1; k < halfN; ++k) {
        weightedSum += static_cast<float>(k) * mag[k];
        totalMag    += mag[k];
    }
    if (totalMag < 1e-10f) return 0.f;
    // normalize by halfN so result is in [0,1]
    return (weightedSum / totalMag) / static_cast<float>(halfN);
}

float FeatureExtractor::computeSt(const float* mag, int halfN) const
{
    float total    = 0.f;
    float highHalf = 0.f;
    const int mid  = halfN / 2;
    for (int k = 1; k < halfN; ++k) {
        total += mag[k];
        if (k >= mid)
            highHalf += mag[k];
    }
    if (total < 1e-10f) return 0.f;
    return highHalf / total;
}
