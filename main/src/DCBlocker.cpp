#include "DCBlocker.h"
#include <string>

DCBlocker::DCBlocker() : prev_input(0.0f), prev_output(0.0f) {
    // For float pipeline (-1.0 to 1.0), initial states should be 0.0f
    enabled = true; // Effects are enabled by default
}

void DCBlocker::process(float& sample) {
    if (!enabled) {
        return;
    }

    // The DC blocker algorithm y[n] = x[n] - x[n-1] + R * y[n-1] works directly on this.
    float current_output = sample - prev_input + R_dc * prev_output;

    prev_input = sample;
    prev_output = current_output;

    // Output should also be in -1.0 to 1.0 range.
    // Constrain to prevent potential drift issues, though a well-behaved DC blocker shouldn't need aggressive clamping.
    sample = constrain(current_output, -1.0f, 1.0f);
}

void DCBlocker::reset() {
    // Reset to 0.0 for float pipeline
    prev_input = 0.0f;
    prev_output = 0.0f;
}

void DCBlocker::setParameter(const std::string& name, float value) {
    // This effect does not have tunable parameters via this method in the current design.
    // Call base class or handle if needed in future.
    AudioEffect::setParameter(name, value);
}
