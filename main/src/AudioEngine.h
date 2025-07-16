#pragma once

#include <Arduino.h>
#include "driver/dac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "AudioBuffer.h"
#include "AudioSource.h"
#include "AudioEffect.h"
#include "DitherNoiseShaping.h" // Added for DAC ditherer

// ============================================================================
// Audio Engine - Manages audio generation, processing, and output.
// ============================================================================
class AudioEngine {
private:
    static AudioEngine* instance;
    TaskHandle_t audioTaskHandle;
    AudioBuffer* outputBuffer;
    AudioSource* currentAudioSource; // Renamed for clarity
    AudioEffect* effects[MAX_EFFECTS];  // MAX_EFFECTS from config.h
    size_t effectCount;
    volatile bool running; // Renamed for clarity
    esp_timer_handle_t sampleTimer;
    SemaphoreHandle_t runningSemaphore;
    // For final float to uint8_t conversion (one for each channel)
    DitherNoiseShaping dacDithererLeft;
    DitherNoiseShaping dacDithererRight;

    volatile uint32_t underrunCounter; // Renamed for clarity
    volatile uint32_t overrunCounter;  // Renamed for clarity
    volatile uint32_t processedSampleCount; // Renamed for clarity

    float masterVolume; // New: Master volume control (0.0 to N.N)

public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    bool start();
    void stop();

    void setAudioSource(AudioSource* source);
    AudioSource* getCurrentAudioSource() const { return currentAudioSource; }
    bool addEffect(AudioEffect* effect);
    void removeAllEffects(); // Clears the effect chain

    uint32_t getUnderruns() const { return underrunCounter; }
    uint32_t getOverruns() const { return overrunCounter; }
    uint32_t getSamplesProcessed() const { return processedSampleCount; }
    size_t getBufferLevel() const; // Number of samples currently in the buffer
    size_t getBufferCapacity() const; // Total capacity of the buffer
    void resetCounters(); // Resets underrun, overrun, and processed sample counters

    void setMasterVolume(float volume);
    float getMasterVolume() const;

    static AudioEngine* getInstancePtr() { return instance; } // For C-style callbacks

private:
    static void audioProcessingTask(void* parameter); // Renamed task wrapper
    void runAudioTask(); // Renamed actual task method
    static void onSampleTimer(void* arg); // Renamed timer callback
    void generateAndOutputSample(); // Renamed sample output method
    void cleanup(); // Internal cleanup method
};
