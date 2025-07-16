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
#include "DitherNoiseShaping.h"
#include "RoutingManager.h"

class AudioEngine {
private:
    static AudioEngine* instance;
    TaskHandle_t audioTaskHandle;
    AudioBuffer* outputBuffer;
    AudioSource* currentAudioSource;
    AudioEffect* effects[MAX_EFFECTS];
    size_t effectCount;
    volatile bool running;
    esp_timer_handle_t sampleTimer;
    SemaphoreHandle_t runningSemaphore;
    DitherNoiseShaping dacDithererLeft;
    DitherNoiseShaping dacDithererRight;
    RoutingManager* routingManager;

    volatile uint32_t underrunCounter;
    volatile uint32_t overrunCounter;
    volatile uint32_t processedSampleCount;

    float masterVolume;

public:
    AudioEngine();
    ~AudioEngine();

    bool initialize(RoutingManager& rm);
    bool start();
    void stop();

    void setAudioSource(AudioSource* source);
    AudioSource* getCurrentAudioSource() const { return currentAudioSource; }

    void updateRouting();

    uint32_t getUnderruns() const { return underrunCounter; }
    uint32_t getOverruns() const { return overrunCounter; }
    uint32_t getSamplesProcessed() const { return processedSampleCount; }
    size_t getBufferLevel() const;
    size_t getBufferCapacity() const;
    void resetCounters();

    void setMasterVolume(float volume);
    float getMasterVolume() const;

    static AudioEngine* getInstancePtr() { return instance; }

private:
    static void audioProcessingTask(void* parameter);
    void runAudioTask();
    static void onSampleTimer(void* arg);
    void generateAndOutputSample();
    void cleanup();
};
