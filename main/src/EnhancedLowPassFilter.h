#pragma once

#include "AudioEffect.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>

// ============================================================================
// Enhanced Low Pass Filter - Moog Ladder Style with Resonance
// ============================================================================
class EnhancedLowPassFilter : public AudioEffect {
private:
    float currentCutoff;    // Renamed from cutoff
    float currentResonance; // Renamed from resonance
    float sampleRate;

    // State variables for the 4 poles
    float y1, y2, y3, y4;
    // float oldx; // Not used in current ZDF-like implementation
    // float oldy1, oldy2, oldy3; // Not used

    // Coefficients
    float g; // Controls cutoff frequency (higher g = higher cutoff)
    float k; // Controls resonance feedback amount (0-4 typical for Moog)

    void calculateCoefficients();

public:
    EnhancedLowPassFilter(float cutoffFreq = 5000.0f, float res = 0.1f, float sr = SAMPLE_RATE);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;

    // Public getters for current parameters
    float getCutoff() const { return currentCutoff; }
    float getResonance() const { return currentResonance; }
};
