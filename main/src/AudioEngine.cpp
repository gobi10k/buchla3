#include "AudioEngine.h"
#include "config.h"
#include <Arduino.h>
#include "driver/dac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>
#include <string>
#include <string>

AudioEngine* AudioEngine::instance = nullptr;

AudioEngine::AudioEngine()
    : audioTaskHandle(nullptr), outputBuffer(nullptr),
      currentAudioSource(nullptr), effectCount(0), running(false),
      sampleTimer(nullptr), underrunCounter(0), overrunCounter(0), processedSampleCount(0),
      masterVolume(1.0f), dacDithererLeft(), dacDithererRight() { // Initialize masterVolume
    instance = this;
    memset(effects, 0, sizeof(effects)); // Clear effects array
    runningSemaphore = xSemaphoreCreateMutex();
    audioSourceMutex = xSemaphoreCreateMutex();
    effectsMutex = xSemaphoreCreateMutex();
    resetStats();
}

AudioEngine::~AudioEngine() {
    stop();
    cleanup();
    if (instance == this) {
        instance = nullptr;
    }
    vSemaphoreDelete(runningSemaphore);
    vSemaphoreDelete(audioSourceMutex);
    vSemaphoreDelete(effectsMutex);
}

bool AudioEngine::initialize() {
    if (!validateConfiguration()) {
        return false;
    }

    Serial.println("AudioEngine: Initializing DAC...");
    if (dac_output_enable(DAC_CHANNEL_1) != ESP_OK) {
        Serial.println("AudioEngine: DAC enable failed!");
        return false;
    }
    // Brief DAC test tone/sequence (optional)
    dac_output_voltage(DAC_CHANNEL_1, 128); // Set to midpoint initially
    Serial.println("AudioEngine: DAC initialized.");

    outputBuffer = std::make_unique<AudioBuffer>(AUDIO_BUFFER_SIZE); // AUDIO_BUFFER_SIZE from config.h
    if (!outputBuffer || !outputBuffer->isValid()) {
        Serial.println("AudioEngine: Buffer allocation failed!");
        cleanup();
        return false;
    }
    Serial.printf("AudioEngine: Buffer created (size: %d samples)\n", AUDIO_BUFFER_SIZE);

    BaseType_t taskResult = xTaskCreatePinnedToCore(
        audioProcessingTask,    // Task function
        "AudioProcTask",        // Name of task
        AUDIO_TASK_STACK_SIZE,  // Stack size
        this,                   // Parameter to pass to task
        AUDIO_TASK_PRIORITY,    // Priority
        &audioTaskHandle,       // Task handle
        AUDIO_CORE              // Core to pin to
    );

    if (taskResult != pdPASS) {
        Serial.println("AudioEngine: Audio processing task creation failed!");
        cleanup();
        return false;
    }
    Serial.printf("AudioEngine: Task created (core %d, prio %d)\n", AUDIO_CORE, AUDIO_TASK_PRIORITY);

    esp_timer_create_args_t timerConfig = {
        .callback = onSampleTimer,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "AudioSampleTimer"
    };

    if (esp_timer_create(&timerConfig, &sampleTimer) != ESP_OK) {
        Serial.println("AudioEngine: Sample timer creation failed!");
        cleanup();
        return false;
    }
    Serial.println("AudioEngine: Sample timer created.");
    return true;
}

bool AudioEngine::start() {
    if (running) return true;
    if (!audioTaskHandle || !sampleTimer || !outputBuffer) {
        Serial.println("AudioEngine: Not properly initialized, cannot start.");
        return false;
    }

    xSemaphoreTake(runningSemaphore, portMAX_DELAY);
    running = true;
    xSemaphoreGive(runningSemaphore);
    uint64_t timerPeriodMicroseconds = 1000000ULL / SAMPLE_RATE;
    if (esp_timer_start_periodic(sampleTimer, timerPeriodMicroseconds) != ESP_OK) {
        Serial.println("AudioEngine: Failed to start sample timer!");
        running = false;
        handleTimerError();
        return false;
    }
    Serial.printf("AudioEngine: Started. Sample timer period: %llu us (Target: %d Hz)\n", timerPeriodMicroseconds, SAMPLE_RATE);
    return true;
}

void AudioEngine::stop() {
    if (!running) return;
    xSemaphoreTake(runningSemaphore, portMAX_DELAY);
    running = false;
    xSemaphoreGive(runningSemaphore);

    if (sampleTimer) {
        esp_timer_stop(sampleTimer);
        Serial.println("AudioEngine: Sample timer stopped.");
    }
    dac_output_voltage(DAC_CHANNEL_1, 128); // Output silence (midpoint)
    vTaskDelay(pdMS_TO_TICKS(50)); // Allow task to complete current cycle
    Serial.println("AudioEngine: Stopped.");
}

void AudioEngine::setAudioSource(AudioSource* source) {
    xSemaphoreTake(audioSourceMutex, portMAX_DELAY);
    currentAudioSource = source;
    xSemaphoreGive(audioSourceMutex);
    if (source) {
        Serial.printf("AudioEngine: Audio source set to %p\n", source);
    } else {
        Serial.println("AudioEngine: Audio source cleared.");
    }
}

bool AudioEngine::addEffect(AudioEffect* effect) {
    xSemaphoreTake(effectsMutex, portMAX_DELAY);
    bool result = false;
    if (effectCount < MAX_EFFECTS) {
        effects[effectCount++] = effect;
        result = true;
    }
    xSemaphoreGive(effectsMutex);
    if(result) {
        Serial.printf("AudioEngine: Effect %p added. Total effects: %d\n", effect, effectCount);
    } else {
        Serial.println("AudioEngine: Max effects reached, cannot add more.");
    }
    return result;
}

void AudioEngine::removeAllEffects() {
    xSemaphoreTake(effectsMutex, portMAX_DELAY);
    effectCount = 0;
    xSemaphoreGive(effectsMutex);
    // Optionally: memset(effects, 0, sizeof(effects)); but not strictly needed if using effectCount.
    Serial.println("AudioEngine: All effects removed.");
}

size_t AudioEngine::getBufferLevel() const {
    return outputBuffer ? outputBuffer->available() : 0;
}

size_t AudioEngine::getBufferCapacity() const {
    return outputBuffer ? outputBuffer->getCapacity() : 0;
}


void AudioEngine::resetCounters() {
    underrunCounter = 0;
    overrunCounter = 0;
    processedSampleCount = 0;
    Serial.println("AudioEngine: Performance counters reset.");
}

void AudioEngine::audioProcessingTask(void* parameter) {
    static_cast<AudioEngine*>(parameter)->runAudioTask();
}

void AudioEngine::setMasterVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 2.0f); // Allow up to 2x boost, min 0.0 (mute)
    Serial.printf("AudioEngine: Master Volume set to %.2f\n", masterVolume);
}

float AudioEngine::getMasterVolume() const {
    return masterVolume;
}

void AudioEngine::runAudioTask() {
    float currentSample;
    uint32_t processingStartTime;
    uint32_t processingTime;

    while (true) {
        // Wait for a notification from the timer ISR.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        processingStartTime = micros();

        xSemaphoreTake(runningSemaphore, portMAX_DELAY);
        bool is_running = running;
        xSemaphoreGive(runningSemaphore);

        if (!is_running) {
            continue;
        }

        // Generate one sample
        xSemaphoreTake(audioSourceMutex, portMAX_DELAY);
        if (currentAudioSource && currentAudioSource->isActive()) {
            currentAudioSource->generateSample(currentSample);
            xSemaphoreGive(audioSourceMutex);

            xSemaphoreTake(effectsMutex, portMAX_DELAY);
            for (size_t j = 0; j < effectCount; ++j) {
                if (effects[j] && effects[j]->isEnabled()) {
                    effects[j]->process(currentSample);
                }
            }
            xSemaphoreGive(effectsMutex);
        } else {
            xSemaphoreGive(audioSourceMutex);
            currentSample = 0.0f; // Silence
        }

        if (outputBuffer->space() > 0) {
            if (!outputBuffer->write(currentSample)) {
                overrunCounter++;
            }
        }

        // Output the sample
        float sampleToOutput_float;
        if (outputBuffer && outputBuffer->read(sampleToOutput_float)) {
            float finalSample = sampleToOutput_float * masterVolume;
            // Constrain before dithering and conversion.
            finalSample = constrain(finalSample, -1.0f, 1.0f);

            uint8_t dac_sample;
            if (fabsf(finalSample) < 0.0001f) { // Threshold for near-silence
                dac_sample = 128; // Output midpoint directly, bypassing dither
            } else {
                // Use DitherNoiseShaping for float to uint8_t conversion
                dac_sample = dacDithererLeft.process(finalSample);
            }

            dac_output_voltage(DAC_CHANNEL_1, dac_sample);
            processedSampleCount++;
        } else {
            dac_output_voltage(DAC_CHANNEL_1, 128); // Underrun: output silence (midpoint)
            underrunCounter++;
            stats.bufferUnderruns++;
        }

        processingTime = micros() - processingStartTime;
        if (processingTime > stats.maxProcessingTime) {
            stats.maxProcessingTime = processingTime;
        }
        // A simple moving average could be implemented here for avgProcessingTime
        stats.avgProcessingTime = (stats.avgProcessingTime + processingTime) / 2;
        stats.bufferOverruns = overrunCounter;
        // More complex CPU usage calculation would be needed for accurate stats
        stats.cpuUsage = (float)stats.avgProcessingTime / (1000000.0f / SAMPLE_RATE) * 100.0f;
    }
}

void AudioEngine::onSampleTimer(void* arg) {
    AudioEngine* engine = static_cast<AudioEngine*>(arg);
    if (engine && engine->running) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(engine->audioTaskHandle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

void AudioEngine::cleanup() {
    Serial.println("AudioEngine: Cleaning up resources...");
    if (audioTaskHandle) {
        // Ensure 'running' is false so the task can exit its loop if it checks.
        // Then delete the task.
        running = false; // Signal task to stop its work
        vTaskDelay(pdMS_TO_TICKS(20)); // Allow time for task to yield or finish current iteration
        vTaskDelete(audioTaskHandle);
        audioTaskHandle = nullptr;
        Serial.println("AudioEngine: Audio task deleted.");
    }

    if (sampleTimer) {
        esp_timer_stop(sampleTimer); // Must stop before deleting
        esp_timer_delete(sampleTimer);
        sampleTimer = nullptr;
        Serial.println("AudioEngine: Sample timer deleted.");
    }

    outputBuffer.reset();
    Serial.println("AudioEngine: Output buffer deleted.");

    dac_output_disable(DAC_CHANNEL_1);
    Serial.println("AudioEngine: DAC disabled.");

    currentAudioSource = nullptr;
    effectCount = 0;
    Serial.println("AudioEngine: Cleanup complete.");
}

bool AudioEngine::validateConfiguration() {
    if (AUDIO_BUFFER_SIZE < SAMPLE_RATE / 100) { // Less than 10ms buffer
        Serial.println("AudioEngine: Buffer too small for stable operation!");
        return false;
    }

    if (AUDIO_TASK_PRIORITY >= configMAX_PRIORITIES) {
        Serial.println("AudioEngine: Invalid task priority!");
        return false;
    }

    return true;
}

void AudioEngine::handleTimerError() {
    Serial.println("AudioEngine: Timer error detected, attempting recovery...");

    if (esp_timer_stop(sampleTimer) == ESP_OK) {
        uint64_t period = 1000000ULL / SAMPLE_RATE;
        if (esp_timer_start_periodic(sampleTimer, period) == ESP_OK) {
            Serial.println("AudioEngine: Timer recovery successful");
            return;
        }
    }

    Serial.println("AudioEngine: Timer recovery failed, stopping engine");
    stop();
}

void AudioEngine::resetStats() {
    memset(&stats, 0, sizeof(stats));
}
