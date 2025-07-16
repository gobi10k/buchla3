#include "EnhancedLowPassFilter.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>
#include <string>
#include <string>
#include <string>

// Reference for Moog Ladder Filter: ZDF (Zero Delay Feedback) style is generally preferred.
// - "The Art of VA Filter Design" by Vadim Zavalishin
// - Mystran's filter blog posts and code.

EnhancedLowPassFilter::EnhancedLowPassFilter(float cutoffFreq, float res, float sr)
    : currentCutoff(cutoffFreq), currentResonance(res), sampleRate(sr),
      y1(0.0f), y2(0.0f), y3(0.0f), y4(0.0f) { // oldx, oldy* not needed for this ZDF form
    calculateCoefficients();
    // Serial.printf("LPF created: Cutoff=%.1f Hz, Res=%.2f\n", currentCutoff, currentResonance);
}

void EnhancedLowPassFilter::calculateCoefficients() {
    // 'g' is the coefficient for each 1-pole stage. It's related to the cutoff frequency.
    // For a ZDF (Zero Delay Feedback) Moog, a common way to calculate g:
    // wd = 2 * PI * cutoff_freq
    // T = 1 / sample_rate
    // wa = (2/T) * tan(wd * T / 2)  // Pre-warping
    // g = wa * T / 2
    // Simplified: g = tan(PI * cutoff_freq / sample_rate)
    // This 'g' is then used in the feedback loop structure.

    float f_cutoff = constrain(currentCutoff, 20.0f, sampleRate / 2.5f); // Clamp cutoff
    g = tanf(PI * f_cutoff / sampleRate);
    g = constrain(g, 0.00001f, 1.0f); // Ensure g is positive and not excessively large. Max useful g is around 1.0 for Nyquist.

    // 'k' is the feedback amount, derived from resonance.
    // Resonance typically 0.0 to 1.0 from user, mapped to 0 to 4.0 for 'k'.
    // Higher 'k' values can lead to self-oscillation and more aggressive resonance.
    currentResonance = constrain(currentResonance, 0.0f, 1.0f);
    k = currentResonance * 4.0f;
    // k = constrain(k, 0.0f, 3.95f); // Max k can be slightly less than 4 to prevent instability in some models
                                    // or allow up to 4.0 or more if saturation handles it.
}

void EnhancedLowPassFilter::process(float& sample) {
    if (!enabled) return;

    float thermal = 0.0f;
    float input = sample - k * tanhf(y4);
    input = tanhf(input);

    y1 = y1 + g * (input - y1);
    y2 = y2 + g * (tanhf(y1) - y2);
    y3 = y3 + g * (tanhf(y2) - y3);
    y4 = y4 + g * (tanhf(y3) - y4);

    sample = y4;
}

void EnhancedLowPassFilter::setCutoff(float cutoff) {
    currentCutoff = constrain(cutoff, 20.0f, sampleRate / 2.5f);
    calculateCoefficients();
}

void EnhancedLowPassFilter::setResonance(float resonance) {
    currentResonance = constrain(resonance, 0.0f, 1.0f);
    calculateCoefficients();
}

void EnhancedLowPassFilter::setParameter(const std::string& name, float value) {
    if (name == "cutoff") {
        setCutoff(value);
    } else if (name == "resonance") {
        setResonance(value);
    } else if (name == "enabled") {
        enabled = (value > 0.5f);
    }
}

void EnhancedLowPassFilter::reset() {
    y1 = y2 = y3 = y4 = 0.0f;
}
