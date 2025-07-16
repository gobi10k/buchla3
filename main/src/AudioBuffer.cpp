#include "AudioBuffer.h"
#include <Arduino.h> // For Serial if used in debugging, memset

AudioBuffer::AudioBuffer(size_t size) : capacity(size), writeIndex(0), readIndex(0) {
    // Ensure power of 2 for efficient modulo operations
    while ((capacity & (capacity - 1)) != 0) capacity++;
    mask = capacity - 1;
    // Allocate buffer for floats. MALLOC_CAP_SPIRAM might be an option if heap is tight.
    buffer = (float*)heap_caps_malloc(capacity * sizeof(float), MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
    if (buffer) {
        // Initialize to 0.0f (silence for float samples)
        for (size_t i = 0; i < capacity; ++i) {
            buffer[i] = 0.0f;
        }
    }
    // else { Serial.println("AudioBuffer: Failed to allocate float buffer!"); } // Optional: Error handling
}

AudioBuffer::~AudioBuffer() {
    if (buffer) heap_caps_free(buffer);
}

bool AudioBuffer::write(float sample) { // Changed to float
    size_t nextWrite = (writeIndex + 1) & mask;
    if (nextWrite == readIndex) return false; // Buffer full
    buffer[writeIndex] = sample;
    writeIndex = nextWrite;
    return true;
}

bool AudioBuffer::read(float& sample) { // Changed to float
    if (readIndex == writeIndex) return false; // Buffer empty
    sample = buffer[readIndex];
    readIndex = (readIndex + 1) & mask;
    return true;
}

void AudioBuffer::clear() {
    readIndex = writeIndex = 0;
    if (buffer) {
        for (size_t i = 0; i < capacity; ++i) {
            buffer[i] = 0.0f; // Initialize to 0.0f
        }
    }
}
