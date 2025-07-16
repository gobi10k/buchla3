#pragma once

#include "AudioEffect.h" // Inherits from AudioEffect
#include <Arduino.h>     // For constrain, uint8_t

class DCBlocker : public AudioEffect {
private:
    float prev_input;
    float prev_output;
    // Alpha is often defined related to a cutoff frequency, but 0.995 is a common value for DC blocking.
    // For example, R = 1 - alpha. A common formula for alpha is exp(-2 * PI * cutoff_freq / sample_rate).
    // Or from the typical one-pole filter: y[n] = x[n] - x[n-1] + alpha * y[n-1]
    const float R_dc = 0.995f; // Using R from typical DC blocker formula y(n) = x(n) - x(n-1) + R * y(n-1)
                               // The user provided alpha = 0.995f, which is 'R' in this context.
public:
    DCBlocker();

    void process(float& sample) override;
    void reset() override;
    void setParameter(const std::string& name, float value) override;
};
