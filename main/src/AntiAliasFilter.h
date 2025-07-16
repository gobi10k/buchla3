#pragma once

#include "AudioEffect.h"

class AntiAliasFilter : public AudioEffect {
private:
    float prev_sample;
    float filter_coeff; // Filter coefficient (alpha)

public:
    AntiAliasFilter(float coeff = 0.45f);
    void process(float& sample) override;
    void reset() override;
    void setParameter(const std::string& name, float value) override;
};
