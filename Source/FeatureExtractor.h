#pragma once
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

struct Features {
    float zcr = 0.f;
    float rms = 0.f;
    float sc  = 0.f;  // spectral centroid, normalized [0,1]
    float st  = 0.f;  // spectral tilt: high-half energy / total energy
};

class FeatureExtractor {
public:
    // frameSize must be a power of 2; sets FFT order internally
    void prepare(int frameSize);
    Features extract(const float* samples, int numSamples);

private:
    int frameSize_ = 1024;
    int fftOrder_  = 10;
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> fftBuffer_;
    std::vector<float> window_;     // pre-computed Hann window, length frameSize_

    float computeZcr(const float* samples, int N) const;
    float computeRms(const float* samples, int N) const;
    float computeSc(const float* mag, int halfN) const;
    float computeSt(const float* mag, int halfN) const;
};
