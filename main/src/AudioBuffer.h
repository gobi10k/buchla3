#pragma once

#include <Arduino.h> // For uint8_t, size_t, memset, heap_caps_malloc, heap_caps_free
#include "soc/rtc.h" // For MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL if not in Arduino.h

// ============================================================================
// Audio Buffer Class - Optimized circular buffer
// ============================================================================
class AudioBuffer {
private:
    float* buffer; // Changed to float
    volatile size_t writeIndex;
    volatile size_t readIndex;
    size_t capacity;
    size_t mask;  // For power-of-2 optimization

public:
    AudioBuffer(size_t size);
    ~AudioBuffer();

    inline bool isValid() const { return buffer != nullptr; }

    bool write(float sample); // Changed to float
    bool read(float& sample); // Changed to float

    inline size_t available() const {
        return (writeIndex - readIndex) & mask;
    }

    inline size_t space() const {
        return capacity - available() - 1;
    }

    inline size_t getCapacity() const { // Added public getter
        return capacity;
    }

    void clear();
};
