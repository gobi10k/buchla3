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
    float lfoRate;      // Hz (e.g., 0.1 to 5 Hz)
    float lfoPhase;
    float depth;        // Modulated delay depth in milliseconds (e.g., 1 to 10 ms)
    float baseDelayMs;  // Base static delay in milliseconds (e.g., 10 to 30 ms)
    float dryWetMix;    // 0.0 (dry) to 1.0 (wet)
    float feedback;     // 0.0 to <1.0 for flanger-like effects, typically low for chorus

    std::vector<float> delayBuffer;
    int delayBufferPos;
    int maxDelaySamples;

    void updateMaxDelaySamples();

public:
    ChorusEffect(float rate = 0.5f, float modDepthMs = 5.0f, float staticDelayMs = 20.0f, float mix = 0.5f, float fb = 0.0f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;

    // Getters for status display
    float getRate() const { return lfoRate; }
    float getDepth() const { return depth; }
    float getBaseDelay() const { return baseDelayMs; }
    float getMix() const { return dryWetMix; }
    float getFeedback() const { return feedback; }
};
