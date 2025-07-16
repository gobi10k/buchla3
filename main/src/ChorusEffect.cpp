#include "ChorusEffect.h"
#include "config.h"  // For SAMPLE_RATE, TWO_PI
#include <Arduino.h> // For sinf, constrain, etc.
#include <cmath>     // For sinf, fmodf, floorf, ceilf
#include <string>
#include <string>
#include <string>

ChorusEffect::ChorusEffect(float rate, float modDepthMs, float staticDelayMs, float mix, float fb)
    : lfoRate(rate), lfoPhase(0.0f), depth(modDepthMs), baseDelayMs(staticDelayMs),
      dryWetMix(mix), feedback(fb), delayBufferPos(0) {

    updateMaxDelaySamples(); // Initialize buffer size based on initial delay settings
    delayBuffer.assign(maxDelaySamples, 0.0f); // Fill with zeros

    Serial.printf("ChorusEffect created: Rate=%.2fHz, Depth=%.1fms, Delay=%.1fms, Mix=%.2f, FB=%.2f\n",
                  lfoRate, depth, baseDelayMs, dryWetMix, feedback);
}

void ChorusEffect::updateMaxDelaySamples() {
    // Max delay is base delay + depth
    float maxTotalDelayMs = baseDelayMs + depth;
    // Add a small margin for safety / interpolation
    maxDelaySamples = static_cast<int>((maxTotalDelayMs / 1000.0f) * SAMPLE_RATE) + 16;

    if (maxDelaySamples < 16) maxDelaySamples = 16; // Minimum practical size
    // Resize buffer if needed, preserving content if possible (though simple assign is easier for now)
    if (delayBuffer.size() != maxDelaySamples) {
        delayBuffer.assign(maxDelaySamples, 0.0f); // Resize and clear
        delayBufferPos = 0; // Reset position
        Serial.printf("ChorusEffect: Delay buffer resized to %d samples.\n", maxDelaySamples);
    }
}

void ChorusEffect::process(float& sample) {
    if (!enabled) return;

    lfoPhase += (TWO_PI * lfoRate) / SAMPLE_RATE;
    if (lfoPhase >= TWO_PI) lfoPhase -= TWO_PI;
    float lfoValue = sinf(lfoPhase);

    float modulatedDelayMs = baseDelayMs + (depth * lfoValue);
    float delaySamplesFloat = (modulatedDelayMs / 1000.0f) * SAMPLE_RATE;
    delaySamplesFloat = constrain(delaySamplesFloat, 0.0f, static_cast<float>(maxDelaySamples - 2));

    int delaySamplesInt = static_cast<int>(floorf(delaySamplesFloat));
    float fraction = delaySamplesFloat - delaySamplesInt;

    int readPos1 = delayBufferPos - delaySamplesInt;
    while (readPos1 < 0) readPos1 += maxDelaySamples;

    int readPos2 = delayBufferPos - delaySamplesInt - 1;
    while (readPos2 < 0) readPos2 += maxDelaySamples;

    float delayedSample1 = delayBuffer[readPos1 % maxDelaySamples];
    float delayedSample2 = delayBuffer[readPos2 % maxDelaySamples];

    float interpolatedDelayedSample = delayedSample1 * (1.0f - fraction) + delayedSample2 * fraction;

    float bufferInput = sample + interpolatedDelayedSample * feedback;
    bufferInput = constrain(bufferInput, -1.0f, 1.0f);

    delayBuffer[delayBufferPos] = bufferInput;
    delayBufferPos = (delayBufferPos + 1) % maxDelaySamples;

    sample = (sample * (1.0f - dryWetMix)) + (interpolatedDelayedSample * dryWetMix);
}

void ChorusEffect::setRate(float rate) {
    lfoRate = constrain(rate, 0.01f, 10.0f);
}

void ChorusEffect::setDepth(float d) {
    depth = constrain(d, 0.1f, 20.0f);
    updateMaxDelaySamples();
}

void ChorusEffect::setBaseDelay(float delay) {
    baseDelayMs = constrain(delay, 1.0f, 50.0f);
    updateMaxDelaySamples();
}

void ChorusEffect::setMix(float mix) {
    dryWetMix = constrain(mix, 0.0f, 1.0f);
}

void ChorusEffect::setFeedback(float fb) {
    feedback = constrain(fb, 0.0f, 0.95f);
}

void ChorusEffect::setParameter(const std::string& name, float value) {
    if (name == "rate") {
        setRate(value);
    } else if (name == "depth") {
        setDepth(value);
    } else if (name == "delay") {
        setBaseDelay(value);
    } else if (name == "mix") {
        setMix(value);
    } else if (name == "feedback") {
        setFeedback(value);
    } else if (name == "enabled") {
        enabled = (value > 0.5f);
    }
}

void ChorusEffect::reset() {
    std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    lfoPhase = 0.0f;
}
