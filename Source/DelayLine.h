#pragma once

#include <memory>
// #include <juce_AudioSampleBuffer.h>

class DelayLine
{
public:
    void setMaximumDelayInSamples(int maxLengthInSamples);
    void reset() noexcept;

    void write(float input) noexcept;
    float read(float delayInSamples) const noexcept;

    int getBufferLength() const noexcept
    {
        return bufferLength;
    }

private:
    // TODO: switch to JUCE buffer
    // std::unique_ptr<juce::AudioBuffer<float>> buffer;
    std::unique_ptr<float[]> buffer;
    int bufferLength = 0;
    int writeIndex = 0;   // where the most recent value was written
};
