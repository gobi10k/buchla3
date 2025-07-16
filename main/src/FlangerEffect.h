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

    float lfoRate;
    float lfoPhase;
    float lfoDepthMs;

    float baseDelayMs;
    float feedback;
    float dryWetMix;

    int currentMaxCombDelaySamples;

    void updateCombFilterSettings();

public:
    FlangerEffect(float rate = 0.2f, float depthMs = 2.5f, float delayMs = 5.0f,
                  float fb = 0.7f, float mix = 0.5f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void reset() override;

    void setRate(float rate);
    void setDepth(float depth);
    void setBaseDelay(float delay);
    void setFeedback(float feedback);
    void setMix(float mix);

    float getRate() const { return lfoRate; }
    float getDepth() const { return lfoDepthMs; }
    float getBaseDelay() const { return baseDelayMs; }
    float getFeedback() const { return feedback; }
    float getMix() const { return dryWetMix; }
};
