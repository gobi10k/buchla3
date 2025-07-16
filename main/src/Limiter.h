
#pragma once

#include "config.h"
#include <string>
#include <string>
#include "AudioEffect.h"
#include <cmath>

class Limiter : public AudioEffect {
public:
    Limiter(float threshold = -0.1f, float releaseTime = 0.1f)
        : thresholdDb(threshold), releaseMs(releaseTime), attackMs(0.001f), envelope(0.0f) {
        updateCoefficients();
    }

    void process(float& sample) override {
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

    void setParameter(const std::string& key, float value) override {
        if (key == "threshold") {
            thresholdDb = value;
            updateCoefficients();
        } else if (key == "release") {
            releaseMs = value;
            updateCoefficients();
        }
    }

private:
    void updateCoefficients() {
        threshold = powf(10.0f, thresholdDb / 20.0f);
        attackCoeff = expf(-1.0f / (attackMs * SAMPLE_RATE * 0.001f));
        releaseCoeff = expf(-1.0f / (releaseMs * SAMPLE_RATE * 0.001f));
    }

    float thresholdDb;
    float releaseMs;
    float attackMs;

    float threshold;
    float attackCoeff;
    float releaseCoeff;
    float envelope;
};
