#pragma once

#include "AudioEffect.h"
#include "config.h" // For SAMPLE_RATE, TWO_PI
#include <Arduino.h> // For uint8_t, sinf, constrain
#include <vector>    // For delay buffer

// ============================================================================
// Chorus Effect - Adds richness and movement to the sound.
// ============================================================================
class ChorusEffect : public AudioEffect {
private:
    float lfoRate;
    float lfoPhase;
    float depth;
    float baseDelayMs;
    float dryWetMix;
    float feedback;

    std::vector<float> delayBuffer;
    int delayBufferPos;
    int maxDelaySamples;

    void updateMaxDelaySamples();

public:
    ChorusEffect(float rate = 0.5f, float modDepthMs = 5.0f, float staticDelayMs = 20.0f, float mix = 0.5f, float fb = 0.0f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;

    void setRate(float rate);
    void setDepth(float depth);
    void setBaseDelay(float delay);
    void setMix(float mix);
    void setFeedback(float feedback);

    float getRate() const { return lfoRate; }
    float getDepth() const { return depth; }
    float getBaseDelay() const { return baseDelayMs; }
    float getMix() const { return dryWetMix; }
    float getFeedback() const { return feedback; }
};
