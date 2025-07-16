#include "Limiter.h"
#include <string>
#include <cmath>

Limiter::Limiter(float threshold, float releaseTime)
    : thresholdDb(threshold), releaseMs(releaseTime), attackMs(0.001f), envelope(0.0f) {
    updateCoefficients();
}

void Limiter::process(float& sample) {
    if (!enabled) return;

    float inputAbs = fabsf(sample);
    float gain = 1.0f;

    if (inputAbs > envelope) {
        // Attack phase
        envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputAbs;
    } else {
        // Release phase
        envelope = releaseCoeff * envelope;
    }

    if (envelope > threshold) {
        gain = threshold / envelope;
    }

    sample *= gain;
}

void Limiter::setThreshold(float t) {
    thresholdDb = t;
    updateCoefficients();
}

void Limiter::setRelease(float r) {
    releaseMs = r;
    updateCoefficients();
}

void Limiter::setParameter(const std::string& key, float value) {
    if (key == "threshold") {
        setThreshold(value);
    } else if (key == "release") {
        setRelease(value);
    }
}

void Limiter::updateCoefficients() {
    threshold = powf(10.0f, thresholdDb / 20.0f);
    attackCoeff = expf(-1.0f / (attackMs * SAMPLE_RATE * 0.001f));
    releaseCoeff = expf(-1.0f / (releaseMs * SAMPLE_RATE * 0.001f));
}
