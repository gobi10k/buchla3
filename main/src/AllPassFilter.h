#pragma once

#include <vector>
#include <Arduino.h> // For constrain etc.
#include <cmath>     // For floorf, ceilf

// ============================================================================
// AllPassFilter Utility Class
// A building block for effects like reverb.
// Processes audio samples as floats.
// Difference equation: y[n] = -g * x[n] + x[n-D] + g * y[n-D]
// ============================================================================
class AllPassFilter {
private:
    std::vector<float> delayBuffer;
    int writePos;
    int maxDelaySamples;       // Maximum capacity of the buffer
    float currentDelaySamples; // Current delay in samples (can be fractional)
    float gain;                // Coefficient 'g'

    // Helper for linear interpolation
    float getInterpolatedSample(float readPosFractional) const;

public:
    // Constructor
    AllPassFilter(int maxDelay = 512, float initialDelay = 0.0f, float g = 0.5f);

    // Setters
    void setMaxDelay(int maxSamples); // Resizes the buffer
    void setDelay(float delaySamples); // Sets current delay, must be <= maxDelaySamples
    void setGain(float g);             // Sets the all-pass coefficient 'g'

    // Processing
    float process(float inputSample);

    // Utility
    void clear(); // Clears the delay buffer contents
    int getMaxDelay() const { return maxDelaySamples; }
    float getCurrentDelay() const { return currentDelaySamples; }
};
