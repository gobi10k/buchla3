#include "AllPassFilter.h"
#include <algorithm> // For std::fill
#include <Arduino.h> // For Serial.printf if debugging, and constrain

AllPassFilter::AllPassFilter(int maxDelay, float initialDelay, float g)
    : writePos(0),
      maxDelaySamples(0), // Will be set by setMaxDelay
      currentDelaySamples(0.0f), // Will be set by setDelay
      gain(0.0f)          // Will be set by setGain
{
    setMaxDelay(maxDelay > 0 ? maxDelay : 1); // Ensure maxDelay is at least 1
    setDelay(initialDelay);
    setGain(g);
    clear(); // Initialize buffer with zeros
}

void AllPassFilter::setMaxDelay(int maxSamples) {
    if (maxSamples <= 0) maxSamples = 1; // Minimum size

    if (maxDelaySamples != maxSamples) {
        maxDelaySamples = maxSamples;
        delayBuffer.assign(maxDelaySamples, 0.0f); // Resize and fill with 0
        writePos = 0; // Reset write position
        // Ensure currentDelaySamples is still valid
        currentDelaySamples = constrain(currentDelaySamples, 0.0f, static_cast<float>(maxDelaySamples - 1.00001f));
        // Serial.printf("AllPassFilter: Max delay set to %d samples.\n", maxDelaySamples);
    }
}

void AllPassFilter::setDelay(float delaySamples) {
    // Constrain delay to be within [0, maxDelaySamples - 1.00001] to avoid issues with readPos at max boundary
    currentDelaySamples = constrain(delaySamples, 0.0f, static_cast<float>(maxDelaySamples - 1.00001f));
    if (currentDelaySamples < 0) currentDelaySamples = 0;
}

void AllPassFilter::setGain(float g) {
    // Gain for all-pass can theoretically be -1 to 1, but values around 0.5 to 0.7 are common.
    // Reducing the max magnitude slightly to prevent extreme resonance that might be perceived as instability.
    gain = constrain(g, -0.95f, 0.95f);
}

// Internal helper for reading from the delay line with interpolation
float AllPassFilter::getInterpolatedSample(float delaySamples) const {
    if (delayBuffer.empty() || maxDelaySamples == 0) return 0.0f;

    int d_int = static_cast<int>(floorf(delaySamples));
    float frac = delaySamples - d_int;

    // Calculate read indices, wrapping around the circular buffer
    int r_idx1 = (writePos - d_int + maxDelaySamples) % maxDelaySamples;
    int r_idx2 = (writePos - d_int - 1 + maxDelaySamples) % maxDelaySamples;

    // Boundary checks for indices (should ideally not be hit with correct modulo)
    if (r_idx1 < 0 || r_idx1 >= maxDelaySamples) r_idx1 = 0;
    if (r_idx2 < 0 || r_idx2 >= maxDelaySamples) r_idx2 = 0;


    float s1 = delayBuffer[r_idx1];
    float s2 = delayBuffer[r_idx2];

    return s1 * (1.0f - frac) + s2 * frac;
}

float AllPassFilter::process(float inputSample) {
    if (maxDelaySamples <= 0) return inputSample;

    // Using the Gardner/Dattorro all-pass filter structure.
    // This structure is common in digital reverberators.
    //
    // Equations:
    //   dn_out = delayBuffer[readPos] (interpolated value from D samples ago)
    //   ap_out (filter output) = dn_out - gain * inputSample
    //   delayBuffer[writePos] (value to store in delay line) = inputSample + gain * ap_out

    float dn_out = getInterpolatedSample(currentDelaySamples); // Output of the delay line (w[n-D])
    float ap_out = dn_out - (gain * inputSample);              // Filter output y[n] = w[n-D] - g*x[n]

    float buffer_input_val = inputSample + (gain * ap_out);    // Input to the delay line w[n] = x[n] + g*y[n]
    buffer_input_val = constrain(buffer_input_val, -1.f, 1.f); // Ensure stability

    if (writePos < 0 || writePos >= maxDelaySamples) {
        // This case should ideally not be reached if writePos is always managed by modulo.
        // However, as a safeguard:
        writePos = 0;
    }
    delayBuffer[writePos] = buffer_input_val;
    writePos = (writePos + 1) % maxDelaySamples; // Circular buffer increment

    return constrain(ap_out, -1.f, 1.f); // Ensure output is within bounds
}

void AllPassFilter::clear() {
    if (!delayBuffer.empty()) {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    }
    writePos = 0;
    // Serial.println("AllPassFilter: Buffer cleared.");
}
