#include "AntiAliasFilter.h"
#include <Arduino.h> // For constrain()

AntiAliasFilter::AntiAliasFilter(float coeff)
    : prev_sample(0.0f),
      filter_coeff(constrain(coeff, 0.001f, 1.0f)) { // Constrain initial value
    enabled = true;
}

void AntiAliasFilter::process(float& sample) {
    if (!enabled) return;
    float filtered_sample = filter_coeff * sample + (1.0f - filter_coeff) * prev_sample;
    prev_sample = filtered_sample;
    sample = filtered_sample;
}

void AntiAliasFilter::reset() {
    prev_sample = 0.0f;
}

void AntiAliasFilter::setParameter(const std::string& name, float value) {
    if (name == "coeff") {
        filter_coeff = constrain(value, 0.001f, 1.0f);
    } else if (name == "enabled") {
        enabled = (value > 0.5f);
    }
}
