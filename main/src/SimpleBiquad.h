#pragma once

#include <cmath> // For sinf, cosf, tanf, etc.
#include "config.h" // For SAMPLE_RATE, TWO_PI (assuming they are there)

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

#ifndef TWO_PI
#define TWO_PI (6.283185307179586f)
#endif

enum class BiquadFilterType {
    LOWPASS,
    HIGHPASS,
    BANDPASS_SKIRT, // Constant skirt gain
    BANDPASS_PEAK,  // Constant peak gain
    NOTCH,
    PEAK,
    LOWSHELF,
    HIGHSHELF
};

class SimpleBiquad {
public:
    SimpleBiquad() : x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f),
                     a0(1.0f), a1(0.0f), a2(0.0f),
                     b0(1.0f), b1(0.0f), b2(0.0f),
                     sampleRate(SAMPLE_RATE) {}

    void setSampleRate(float sr) {
        sampleRate = sr;
    }

    // Common parameters:
    // freq: Center/cutoff frequency in Hz
    // Q: Quality factor / resonance. Higher Q = narrower band / more resonance.
    // peakGainDB: For PEAK, LOWSHELF, HIGHSHELF filters (in dB)
    void calculateCoefficients(BiquadFilterType type, float freq, float Q, float peakGainDB = 0.0f) {
        if (freq <= 0 || Q <= 0) { // Basic validation
            // Set to passthrough
            a0 = 1.0f; a1 = 0.0f; a2 = 0.0f;
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            return;
        }
        // Clamp frequency to just below Nyquist
        if (freq >= sampleRate / 2.0f) {
            freq = sampleRate / 2.0f - 1.0f;
        }


        float A = 0.0f;
        if (type == BiquadFilterType::PEAK || type == BiquadFilterType::LOWSHELF || type == BiquadFilterType::HIGHSHELF) {
            A = powf(10.0f, peakGainDB / 40.0f); // For PEAK and SHELF, A = sqrt(10^(peakGainDB/20))
        } else {
            A = sqrtf(powf(10.0f, peakGainDB / 20.0f)); // Not used by LPF/HPF/BPF_constant_skirt/Notch
        }


        float w0 = TWO_PI * freq / sampleRate;
        float cos_w0 = cosf(w0);
        float sin_w0 = sinf(w0);
        float alpha = 0.0f; // Default, will be calculated based on type

        switch (type) {
            case BiquadFilterType::LOWPASS:
            case BiquadFilterType::HIGHPASS:
                 alpha = sin_w0 / (2.0f * Q);
                break;
            case BiquadFilterType::BANDPASS_SKIRT: // constant skirt gain
            case BiquadFilterType::BANDPASS_PEAK:  // constant peak gain
            case BiquadFilterType::NOTCH:
                alpha = sin_w0 / (2.0f * Q); // Same alpha for these BPF/Notch
                break;
            case BiquadFilterType::PEAK:
            case BiquadFilterType::LOWSHELF:
            case BiquadFilterType::HIGHSHELF:
                alpha = sin_w0 / (2.0f * Q * A); // Different alpha for shelving/peaking with A
                // For shelf, can also use: alpha = sin_w0/2.0f * sqrt( (A + 1.0f/A)*(1.0f/S - 1.0f) + 2.0f ) where S is slope
                // But the RBJ cookbook uses simpler Q definition for shelves.
                break;
        }
        if (alpha == 0.0f && (type == BiquadFilterType::PEAK || type == BiquadFilterType::LOWSHELF || type == BiquadFilterType::HIGHSHELF)) {
             alpha = sin_w0 / (2.0f * Q); // Fallback if A was 1 (0dB gain)
        }


        float b0_temp = 0, b1_temp = 0, b2_temp = 0, a0_temp = 0, a1_temp = 0, a2_temp = 0;

        switch (type) {
            case BiquadFilterType::LOWPASS:
                b0_temp = (1.0f - cos_w0) / 2.0f;
                b1_temp = 1.0f - cos_w0;
                b2_temp = (1.0f - cos_w0) / 2.0f;
                a0_temp = 1.0f + alpha;
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - alpha;
                break;
            case BiquadFilterType::HIGHPASS:
                b0_temp = (1.0f + cos_w0) / 2.0f;
                b1_temp = -(1.0f + cos_w0);
                b2_temp = (1.0f + cos_w0) / 2.0f;
                a0_temp = 1.0f + alpha;
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - alpha;
                break;
            case BiquadFilterType::BANDPASS_SKIRT: // Bandpass filter (constant skirt gain, Q gives bandwidth)
                b0_temp = sin_w0 / 2.0f; // or Q*alpha
                b1_temp = 0;
                b2_temp = -sin_w0 / 2.0f; // or -Q*alpha
                a0_temp = 1.0f + alpha;
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - alpha;
                break;
            case BiquadFilterType::BANDPASS_PEAK: // Bandpass filter (constant peak gain 0dB)
                b0_temp = alpha; // Q determines bandwidth
                b1_temp = 0;
                b2_temp = -alpha;
                a0_temp = 1.0f + alpha;
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - alpha;
                break;
            case BiquadFilterType::NOTCH:
                b0_temp = 1.0f;
                b1_temp = -2.0f * cos_w0;
                b2_temp = 1.0f;
                a0_temp = 1.0f + alpha;
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - alpha;
                break;
            case BiquadFilterType::PEAK:
                b0_temp = 1.0f + (alpha * A);
                b1_temp = -2.0f * cos_w0;
                b2_temp = 1.0f - (alpha * A);
                a0_temp = 1.0f + (alpha / A);
                a1_temp = -2.0f * cos_w0;
                a2_temp = 1.0f - (alpha / A);
                break;
            case BiquadFilterType::LOWSHELF:
                b0_temp = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
                b1_temp = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                b2_temp = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha);
                a0_temp = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha;
                a1_temp = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                a2_temp = (A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha;
                break;
            case BiquadFilterType::HIGHSHELF:
                b0_temp = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha);
                b1_temp = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                b2_temp = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha);
                a0_temp = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sqrtf(A) * alpha;
                a1_temp = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                a2_temp = (A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sqrtf(A) * alpha;
                break;
            default: // Passthrough
                b0_temp = 1.0f; a0_temp = 1.0f;
                break;
        }

        // Normalize coefficients by a0_temp
        // (b0, b1, b2, a1, a2)
        this->b0 = b0_temp / a0_temp;
        this->b1 = b1_temp / a0_temp;
        this->b2 = b2_temp / a0_temp;
        this->a1 = a1_temp / a0_temp; // a0 is implicitly 1 after normalization
        this->a2 = a2_temp / a0_temp;
        this->a0 = 1.0f; // Normalized
    }

    float process(float x0) {
        // Direct Form I: y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
        //                       - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]
        // Since we pre-normalized by a0_temp, our a0 is 1.
        float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        // Update states (Direct Form II Transposed is often better for precision, but DF1 is fine for floats)
        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = y0;

        return y0;
    }

    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
    }

private:
    // Filter coefficients
    float a0, a1, a2; // Denominator (feedback)
    float b0, b1, b2; // Numerator (feedforward)

    // Filter state (delays)
    float x1, x2; // Input history
    float y1, y2; // Output history

    float sampleRate;
};
