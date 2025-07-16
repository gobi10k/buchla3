#include "Compressor.h"
#include <cmath>
#include <string>

Compressor::Compressor(float threshold, float r, float attack, float release)
    : thresholdDb(threshold), ratio(r), attackMs(attack), releaseMs(release), envelope(0.0f) {
    updateCoefficients();
}

void Compressor::process(float& sample) {
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

void Compressor::setThreshold(float t) {
    thresholdDb = t;
}

void Compressor::setRatio(float r) {
    ratio = r;
}

void Compressor::setAttack(float a) {
    attackMs = a;
    updateCoefficients();
}

void Compressor::setRelease(float r) {
    releaseMs = r;
    updateCoefficients();
}

void Compressor::setParameter(const std::string& key, float value) {
    if (key == "threshold") {
        setThreshold(value);
    } else if (key == "ratio") {
        setRatio(value);
    } else if (key == "attack") {
        setAttack(value);
    } else if (key == "release") {
        setRelease(value);
    }
}

float Compressor::getParameter(const std::string& key) const {
    if (key == "threshold") return thresholdDb;
    if (key == "ratio") return ratio;
    if (key == "attack") return attackMs;
    if (key == "release") return releaseMs;
    return 0.0f;
}

void Compressor::updateCoefficients() {
    attackCoeff = expf(-1.0f / (attackMs * SAMPLE_RATE * 0.001f));
    releaseCoeff = expf(-1.0f / (releaseMs * SAMPLE_RATE * 0.001f));
}
