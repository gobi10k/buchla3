#include "DitherNoiseShaping.h"

DitherNoiseShaping::DitherNoiseShaping() : prev_error(0), initialized(false) {
    // Constructor
}

// Takes a float sample in the range -1.0 to 1.0
uint8_t DitherNoiseShaping::process(float sample_float_neg1_to_1) {
    if (!initialized) {
        prev_error = 0;
        initialized = true;
    }

    // 1. Convert float sample to int16_t
    // Ensure input float is constrained to prevent overflow when converting to int16_t
    float constrained_float = constrain(sample_float_neg1_to_1, -1.0f, 1.0f);
    int16_t sample_int16 = static_cast<int16_t>(constrained_float * 32767.0f);

    // Generate TPDF dither for 16-bit to 8-bit conversion.
    // This dither has an effective range of -1 to +1 for the final 8-bit LSB.
    uint32_t rand1 = esp_random();
    uint32_t rand2 = esp_random();
    int32_t dither_val = ((rand1 & 0xFF) - (rand2 & 0xFF)); // Range approx -255 to 255

    // Apply dither and noise shaping to the 16-bit sample
    // sample_int16 is int16_t, prev_error is int32_t, dither_val is int32_t
    int32_t input_to_quantizer = (int32_t)sample_int16 + dither_val + prev_error;

    // Convert to 8-bit range (0-255) from the (effectively) 16-bit signed domain signal
    // 1. Shift to 16-bit unsigned representation: add 32768. Range becomes 0 to 65535.
    // 2. Constrain (though ideally, sample should already be in -32768 to 32767 range).
    //    If 'total' can go outside this, then constrain makes sense.
    //    The 'total' here is `sample + dither + prev_error`. `prev_error` can be large.
    int32_t shifted_to_unsigned_16bit = input_to_quantizer + 32768;

    // Constrain before downscaling to prevent overflow/underflow issues if `input_to_quantizer` is way out of 16-bit range
    shifted_to_unsigned_16bit = constrain(shifted_to_unsigned_16bit, 0, 65535);
    uint8_t out_8bit = shifted_to_unsigned_16bit >> 8; // Convert to 8-bit

    // Calculate quantization error
    // 1. Convert 8-bit output back to the 16-bit signed domain it represents
    int32_t quantized_16bit_signed = ((int32_t)out_8bit << 8) - 32768; // This centers it around 0
                                                                    // e.g. 0 -> -32768, 128 -> 0, 255 -> 32512

    // Error is the difference between the input to the quantizer and the actual quantized value
    int32_t error = input_to_quantizer - quantized_16bit_signed;

    // First-order noise shaping (feedback 90% of the error)
    // The user's code `prev_error = error * 9 / 10;` is fine.
    prev_error = (error * 9) / 10;

    return out_8bit;
}

void DitherNoiseShaping::reset() {
    prev_error = 0;
    // initialized = false; // Or keep it true once initialized for the first time
}
