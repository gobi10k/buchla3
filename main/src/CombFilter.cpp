#include "CombFilter.h"
#include <algorithm> // For std::fill
#include <Arduino.h> // For Serial.printf if debugging, and constrain

CombFilter::CombFilter(int maxDelay, float initialDelay, float fbGain, float ffGain)
    : writePos(0),
      maxDelaySamples(0), // Will be set by setMaxDelay
      currentDelaySamples(0.0f), // Will be set by setDelay
      feedbackGain(0.0f),   // Will be set by setFeedback
      feedforwardGain(0.0f) // Will be set by setFeedforward
{
    setMaxDelay(maxDelay > 0 ? maxDelay : 1); // Ensure maxDelay is at least 1
    setDelay(initialDelay);
    setFeedback(fbGain);
    setFeedforward(ffGain);
    clear(); // Initialize buffer with zeros
}

void CombFilter::setMaxDelay(int maxSamples) {
    if (maxSamples <= 0) maxSamples = 1; // Minimum size

    if (maxDelaySamples != maxSamples) {
        maxDelaySamples = maxSamples;
        delayBuffer.assign(maxDelaySamples, 0.0f); // Resize and fill with 0
        writePos = 0; // Reset write position
        // Ensure currentDelaySamples is still valid
        currentDelaySamples = constrain(currentDelaySamples, 0.0f, static_cast<float>(maxDelaySamples -1));
        // Serial.printf("CombFilter: Max delay set to %d samples.\n", maxDelaySamples);
    }
}

void CombFilter::setDelay(float delaySamples) {
    // Constrain delay to be within [0, maxDelaySamples - 1.00001] to avoid issues with readPos at max boundary
    currentDelaySamples = constrain(delaySamples, 0.0f, static_cast<float>(maxDelaySamples - 1.00001f));
    if (currentDelaySamples < 0) currentDelaySamples = 0; // Should be handled by constrain, but just in case
}

void CombFilter::setFeedback(float gain) {
    feedbackGain = constrain(gain, -0.999f, 0.999f); // Keep feedback stable
}

void CombFilter::setFeedforward(float gain) {
    feedforwardGain = gain;
}

float CombFilter::getInterpolatedSample(float readPosFractional) const {
    if (delayBuffer.empty()) return 0.0f;

    int readPosInt = static_cast<int>(floorf(readPosFractional));
    float fraction = readPosFractional - readPosInt;

    // Ensure positive indices after wrapping
    int index1 = (writePos - 1 - readPosInt + maxDelaySamples) % maxDelaySamples;
    int index2 = (writePos - 1 - (readPosInt + 1) + maxDelaySamples) % maxDelaySamples;

    // In a standard setup, readPosFractional is calculated from currentDelaySamples.
    // The actual read head is `writePos - currentDelaySamples`.
    // Let's adjust the typical interpolation logic for a circular buffer.
    // `delaySamplesFloat` would be `currentDelaySamples`.
    // `delaySamplesInt` is `floorf(currentDelaySamples)`.
    // `fraction` is `currentDelaySamples - delaySamplesInt`.

    // `readPos1` = `(writePos - delaySamplesInt + maxDelaySamples) % maxDelaySamples`
    // `readPos2` = `(writePos - delaySamplesInt - 1 + maxDelaySamples) % maxDelaySamples`
    // This seems more standard. The `getInterpolatedSample` was intended to be a generic helper,
    // let's refine it in the context of `process`.

    // For now, let's assume readPosFractional is the exact fractional index *from the write head*.
    // This means if currentDelaySamples = 5.5, we want to read 5.5 samples ago.
    // read_idx_exact = writePos - currentDelaySamples

    // Corrected interpolation logic for a delay line:
    int d_int = static_cast<int>(floorf(currentDelaySamples));
    float frac = currentDelaySamples - d_int;

    int r_idx1 = (writePos - d_int + maxDelaySamples) % maxDelaySamples;
    int r_idx2 = (writePos - d_int - 1 + maxDelaySamples) % maxDelaySamples;

    if (r_idx1 < 0 || r_idx1 >= maxDelaySamples || r_idx2 < 0 || r_idx2 >= maxDelaySamples) {
        // This should not happen with proper modulo and buffer size > 0
        // Serial.printf("CombFilter: Invalid read index. r_idx1=%d, r_idx2=%d, maxDelay=%d\n", r_idx1, r_idx2, maxDelaySamples);
        return 0.0f;
    }

    float s1 = delayBuffer[r_idx1];
    float s2 = delayBuffer[r_idx2];

    return s1 * (1.0f - frac) + s2 * frac;
}


float CombFilter::process(float inputSample) {
    if (maxDelaySamples <= 0) return inputSample; // Should not happen if constructed properly

    // Get delayed sample using interpolation
    float delayedSample = getInterpolatedSample(currentDelaySamples);

    // Calculate output (IIR comb filter structure)
    // y[n] = x[n] * ff + d[n] (where d[n] is the delayed signal from buffer)
    // The content of the buffer for next iteration is x[n] + fb * d[n]
    // Or for a more standard definition: y[n] = x[n] + feedback * y[n-D] (recursive)
    // Or y[n] = x[n-D] + feedback * y[n-D] (feedforward comb, often for FIR)
    // Let's use the common definition: y(n) = x(n) + feedbackGain * y(n - delay)
    // This means the sample written to the buffer includes feedback.
    // Or for Flanger/Chorus: y(n) = x(n) + wet_signal; wet_signal = delayed_signal.
    // And buffer_input = x(n) + feedback * delayed_signal.

    // Chorus/Flanger style (feedback from the delayed signal, not output):
    float output = inputSample * (1.0f - fabsf(feedforwardGain)) + delayedSample * feedforwardGain; // If using ff as a mix
    if (feedforwardGain == 0.0f && feedbackGain != 0.0f) { // Typical IIR comb (reverb style)
        // y[n] = x[n-D_eff] where D_eff is the content of delay line
        // and x[n-D_eff] = inputSample_delayed + feedbackGain * x[n-D_eff-D_actual]
        // More simply: output is the delayed sample. The value written to the buffer includes feedback.
        output = delayedSample; // Output is simply what's read from the delay line.
    } else if (feedforwardGain != 0.0f) { // FIR comb or mixed
        output = inputSample * feedforwardGain + delayedSample; // Simple FIR: y[n] = a*x[n] + b*x[n-D]
                                                               // Here, ff is 'a', and implicit 'b' is 1.
                                                               // Let's make ff the coefficient for input, and 1 for delayed.
        output = inputSample * feedforwardGain + delayedSample;
    } else { // Default passthrough if unclear
        output = inputSample;
    }

    // For a standard IIR comb filter used in reverb/flanger:
    // The output y[n] = delayed_sample_value.
    // The value written into the delay line is inputSample + feedbackGain * delayed_sample_value.
    // Let's stick to this, assuming ffGain = 0 for typical IIR usage.
    // If ffGain is non-zero, it implies an FIR structure or a more complex filter.

    // Standard IIR Comb: output is the content of the delay line.
    // Input to delay line: current input + feedback * content of delay line.
    output = delayedSample; // y[n] = x[n-D_actual] (where x means content of buffer)

    float bufferInput = inputSample + feedbackGain * delayedSample;
    bufferInput = constrain(bufferInput, -1.0f, 1.0f); // Basic limiting to prevent explosion

    if (writePos < 0 || writePos >= maxDelaySamples) {
        // Should not happen
        // Serial.printf("CombFilter: Invalid writePos: %d, maxDelay: %d\n", writePos, maxDelaySamples);
        writePos = 0;
    }
    delayBuffer[writePos] = bufferInput;

    writePos = (writePos + 1) % maxDelaySamples;

    return output;
}

void CombFilter::clear() {
    if (!delayBuffer.empty()) {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
    }
    writePos = 0;
    // Serial.println("CombFilter: Buffer cleared.");
}
