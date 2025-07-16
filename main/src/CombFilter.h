#pragma once

#include <vector>
#include <Arduino.h> // For fabs, constrain, etc.
#include <cmath>     // For floorf, ceilf

// ============================================================================
// CombFilter Utility Class
// Not an AudioEffect itself, but a building block for other effects.
// Processes audio samples as floats.
// ============================================================================
class CombFilter {
private:
    std::vector<float> delayBuffer;
    int writePos;
    int maxDelaySamples; // Maximum capacity of the buffer
    float currentDelaySamples; // Current delay in samples (can be fractional)
    float feedbackGain;
    float feedforwardGain; // Typically 0 for IIR, or used for FIR

    // Helper for linear interpolation
    float getInterpolatedSample(float readPosFractional) const;

public:
    // Constructor
    CombFilter(int maxDelay = 1024, float initialDelay = 0.0f, float fbGain = 0.0f, float ffGain = 0.0f);

    // Setters
    void setMaxDelay(int maxSamples); // Resizes the buffer
    void setDelay(float delaySamples); // Sets current delay, must be <= maxDelaySamples
    void setFeedback(float gain);
    void setFeedforward(float gain);

    // Processing
    float process(float inputSample);

    // Utility
    void clear(); // Clears the delay buffer contents
    int getMaxDelay() const { return maxDelaySamples; }
    float getCurrentDelay() const { return currentDelaySamples; }
};
