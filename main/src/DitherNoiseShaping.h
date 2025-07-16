#pragma once

#include <Arduino.h> // For uint8_t, int16_t, int32_t, uint32_t, constrain
#include "esp_random.h" // For esp_random()

class DitherNoiseShaping {
private:
    int32_t prev_error;
    bool initialized;

public:
    DitherNoiseShaping();

    uint8_t process(float sample_float_neg1_to_1); // Changed to take float
    void reset();
};
