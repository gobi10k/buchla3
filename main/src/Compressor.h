#pragma once

#include "config.h"
#include <string>
#include "AudioEffect.h"
#include <cmath>

class Compressor : public AudioEffect {
public:
    Compressor(float threshold = -20.0f, float ratio = 4.0f, float attack = 0.01f, float release = 0.1f);

    void process(float& sample) override;

    void setParameter(const std::string& key, float value) override;
    float getParameter(const std::string& key) const override;

    void setThreshold(float threshold);
    void setRatio(float ratio);
    void setAttack(float attack);
    void setRelease(float release);

private:
    void updateCoefficients();

    float thresholdDb;
    float ratio;
    float attackMs;
    float releaseMs;

    float attackCoeff;
    float releaseCoeff;
    float envelope;
};
