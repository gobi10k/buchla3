#include "FlangerEffect.h"
#include "config.h" // For SAMPLE_RATE, TWO_PI (assuming they are there)
#include <Arduino.h> // For uint8_t, sinf, constrain
#include <cmath>     // For sinf, fmodf, floorf, ceilf
#include <string>
#include <string>

// Assuming SAMPLE_RATE and TWO_PI are defined in config.h
#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100 // Default if not defined
#warning "SAMPLE_RATE not defined in config.h, using default 44100"
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#warning "TWO_PI not defined in config.h, using default"
#endif

FlangerEffect::FlangerEffect(float rate, float depthMs, float delayMs, float fb, float mix)
    : combFilter(1024, 0.0f, fb, 0.0f), // Max delay, initial delay, feedback, feedforward
      lfoRate(rate),
      lfoPhase(0.0f),
      lfoDepthMs(depthMs),
      baseDelayMs(delayMs),
      feedback(fb),
      dryWetMix(mix),
      currentMaxCombDelaySamples(0) // Will be initialized by updateCombFilterSettings
{
    // Initial setup of comb filter
    updateCombFilterSettings(); // This will set combFilter's max delay and initial delay
    combFilter.setFeedback(feedback);
    combFilter.clear();

    // Serial.printf("FlangerEffect created: Rate=%.2fHz, Depth=%.1fms, Delay=%.1fms, Mix=%.2f, FB=%.2f\n",
                //   lfoRate, lfoDepthMs, baseDelayMs, dryWetMix, feedback);
}

void FlangerEffect::updateCombFilterSettings() {
    // Max delay is base delay + LFO depth
    float maxTotalDelayMs = baseDelayMs + lfoDepthMs;
    // Add a small margin for safety / interpolation and ensure minimum practical size
    int requiredMaxDelaySamples = static_cast<int>((maxTotalDelayMs / 1000.0f) * SAMPLE_RATE) + 16;
    if (requiredMaxDelaySamples < 32) requiredMaxDelaySamples = 32; // Sensible minimum

    if (requiredMaxDelaySamples > currentMaxCombDelaySamples) {
        currentMaxCombDelaySamples = requiredMaxDelaySamples;
        combFilter.setMaxDelay(currentMaxCombDelaySamples);
        // Serial.printf("FlangerEffect: CombFilter max delay resized to %d samples.\n", currentMaxCombDelaySamples);
    }
    // Base delay for the LFO center point
    // float initialCombDelaySamples = (baseDelayMs / 1000.0f) * SAMPLE_RATE;
    // combFilter.setDelay(initialCombDelaySamples); // LFO will override this in process()
}

void FlangerEffect::process(float& sample) {
    if (!enabled) return;

    lfoPhase += (TWO_PI * lfoRate) / SAMPLE_RATE;
    if (lfoPhase >= TWO_PI) lfoPhase -= TWO_PI;

    float lfoValue = sinf(lfoPhase);

    float modulatedDelayMs = baseDelayMs + (lfoDepthMs * 0.5f * lfoValue);
    if (modulatedDelayMs < 0.1f) modulatedDelayMs = 0.1f;
    float delaySamplesFloat = (modulatedDelayMs / 1000.0f) * SAMPLE_RATE;
    delaySamplesFloat = constrain(delaySamplesFloat, 0.0f, static_cast<float>(combFilter.getMaxDelay() - 1.0001f));
    combFilter.setDelay(delaySamplesFloat);

    float wetSignal = combFilter.process(sample);

    sample = (sample * (1.0f - dryWetMix)) + (wetSignal * dryWetMix);
}

void FlangerEffect::setRate(float rate) {
    lfoRate = rate;
}

void FlangerEffect::setDepth(float depth) {
    lfoDepthMs = depth;
    updateCombFilterSettings();
}

void FlangerEffect::setBaseDelay(float delay) {
    baseDelayMs = delay;
    updateCombFilterSettings();
}

void FlangerEffect::setFeedback(float fb) {
    feedback = fb;
    combFilter.setFeedback(feedback);
}

void FlangerEffect::setMix(float mix) {
    dryWetMix = mix;
}

void FlangerEffect::setParameter(const std::string& name, float value) {
    if (name == "rate") {
        setRate(value);
    } else if (name == "depth") {
        setDepth(value);
    } else if (name == "delay") {
        setBaseDelay(value);
    } else if (name == "feedback") {
        setFeedback(value);
    } else if (name == "mix") {
        setMix(value);
    }
}

float FlangerEffect::getParameter(const std::string& name) const {
    if (name == "rate") return lfoRate;
    if (name == "depth") return lfoDepthMs;
    if (name == "delay") return baseDelayMs;
    if (name == "feedback") return feedback;
    if (name == "mix") return dryWetMix;
    return 0.0f;
}

void FlangerEffect::reset() {
    lfoPhase = 0.0f;
    combFilter.clear();
    // Serial.println("FlangerEffect reset.");
    // Re-apply current settings to comb filter if necessary
    updateCombFilterSettings();
    combFilter.setFeedback(feedback);
}
