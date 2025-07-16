
#pragma once

#include "config.h"
#include <string>
#include <string>
#include "AudioEffect.h"
#include <cmath>

class Compressor : public AudioEffect {
public:
    Compressor(float threshold = -20.0f, float ratio = 4.0f, float attack = 0.01f, float release = 0.1f)
        : thresholdDb(threshold), ratio(ratio), attackMs(attack), releaseMs(release), envelope(0.0f) {
        updateCoefficients();
    }

    void process(float& sample) override {
        if (!enabled) return;

        float inputDb = 20.0f * log10f(fabsf(sample) + 1e-6);
        float gain = 1.0f;

        if (inputDb > envelope) {
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputDb;
        } else {
            envelope = releaseCoeff * envelope;
        }

        if (envelope > thresholdDb) {
            float gainDb = thresholdDb + (envelope - thresholdDb) / ratio;
            gain = powf(10.0f, (gainDb - inputDb) / 20.0f);
        }

        sample *= gain;
    }

    void setParameter(const std::string& key, float value) override {
        if (key == "threshold") {
            thresholdDb = value;
        } else if (key == "ratio") {
            ratio = value;
        } else if (key == "attack") {
            attackMs = value;
            updateCoefficients();
        } else if (key == "release") {
            releaseMs = value;
            updateCoefficients();
        }
    }

    float getParameter(const std::string& key) const override {
        if (key == "threshold") return thresholdDb;
        if (key == "ratio") return ratio;
        if (key == "attack") return attackMs;
        if (key == "release") return releaseMs;
        return 0.0f;
    }

private:
    void updateCoefficients() {
        attackCoeff = expf(-1.0f / (attackMs * SAMPLE_RATE * 0.001f));
        releaseCoeff = expf(-1.0f / (releaseMs * SAMPLE_RATE * 0.001f));
    }

    float thresholdDb;
    float ratio;
    float attackMs;
    float releaseMs;

    float attackCoeff;
    float releaseCoeff;
    float envelope;
};
