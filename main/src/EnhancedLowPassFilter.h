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
    float currentCutoff;
    float currentResonance;
    float sampleRate;

    float y1, y2, y3, y4;

    float g;
    float k;

    void calculateCoefficients();

public:
    EnhancedLowPassFilter(float cutoffFreq = 5000.0f, float res = 0.1f, float sr = SAMPLE_RATE);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;

    void setCutoff(float cutoff);
    void setResonance(float resonance);

    float getCutoff() const { return currentCutoff; }
    float getResonance() const { return currentResonance; }
};
