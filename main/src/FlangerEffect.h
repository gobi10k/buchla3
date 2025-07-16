#pragma once

#include <string>

#include <string>

#include "AudioEffect.h"
#include "CombFilter.h"
#include "config.h" // For SAMPLE_RATE, TWO_PI (assuming they are there)
#include <Arduino.h> // For uint8_t, sinf, constrain
#include <cmath>     // For sinf, fmodf, floorf, ceilf

// ============================================================================
// Flanger Effect - Creates a sweeping "jet engine" sound.
// ============================================================================
class FlangerEffect : public AudioEffect {
private:
    CombFilter combFilter;

    // LFO parameters
    float lfoRate;      // Hz (e.g., 0.05 to 2 Hz)
    float lfoPhase;
    float lfoDepthMs;   // Modulation depth in milliseconds (e.g., 0.5 to 5 ms)

    // Flanger parameters
    float baseDelayMs;  // Base (center) delay in milliseconds (e.g., 1 to 10 ms)
    float feedback;     // Feedback for the comb filter (-0.9 to 0.9)
    float dryWetMix;    // 0.0 (dry) to 1.0 (wet)

    // Internal state
    int currentMaxCombDelaySamples; // To track if comb filter needs resizing

    void updateCombFilterSettings(); // Recalculates max delay for comb filter

public:
    FlangerEffect(float rate = 0.2f, float depthMs = 2.5f, float delayMs = 5.0f,
                  float fb = 0.7f, float mix = 0.5f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void reset() override;

    // Getters for potential UI display
    float getRate() const { return lfoRate; }
    float getDepth() const { return lfoDepthMs; }
    float getBaseDelay() const { return baseDelayMs; }
    float getFeedback() const { return feedback; }
    float getMix() const { return dryWetMix; }
};
