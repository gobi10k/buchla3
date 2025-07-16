#pragma once

#include "config.h"
#include <string>
#include "AudioEffect.h"
#include <cmath>

class Limiter : public AudioEffect {
public:
    Limiter(float threshold = -0.1f, float releaseTime = 0.1f);

    void process(float& sample) override;

    void setParameter(const std::string& key, float value) override;

    void setThreshold(float threshold);
    void setRelease(float release);

private:
    void updateCoefficients();

    float thresholdDb;
    float releaseMs;
    float attackMs;

    float threshold;
    float attackCoeff;
    float releaseCoeff;
    float envelope;
};
