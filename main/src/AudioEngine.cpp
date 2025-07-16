#include "AudioEngine.h"
#include "config.h"
#include <Arduino.h>
#include "driver/dac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

AudioEngine* AudioEngine::instance = nullptr;

AudioEngine::AudioEngine()
    : audioTaskHandle(nullptr), outputBuffer(nullptr),
      currentAudioSource(nullptr), effectCount(0), running(false),
      sampleTimer(nullptr), underrunCounter(0), overrunCounter(0), processedSampleCount(0),
      masterVolume(1.0f), dacDithererLeft(), dacDithererRight(), routingManager(nullptr) {
    instance = this;
    memset(effects, 0, sizeof(effects));
    runningSemaphore = xSemaphoreCreateMutex();
}

AudioEngine::~AudioEngine() {
    stop();
    cleanup();
    if (instance == this) {
        instance = nullptr;
    }
    vSemaphoreDelete(runningSemaphore);
}

bool AudioEngine::initialize(RoutingManager& rm) {
    routingManager = &rm;
    Serial.println("AudioEngine: Initializing DAC...");
    if (dac_output_enable(DAC_CHANNEL_1) != ESP_OK) {
        Serial.println("AudioEngine: DAC enable failed!");
        return false;
    }
    dac_output_voltage(DAC_CHANNEL_1, 128);
    Serial.println("AudioEngine: DAC initialized.");

    outputBuffer = new AudioBuffer(AUDIO_BUFFER_SIZE);
    if (!outputBuffer || !outputBuffer->isValid()) {
        Serial.println("AudioEngine: Buffer allocation failed!");
        cleanup();
        return false;
    }
    Serial.printf("AudioEngine: Buffer created (size: %d samples)\n", AUDIO_BUFFER_SIZE);

    BaseType_t taskResult = xTaskCreatePinnedToCore(
        audioProcessingTask,
        "AudioProcTask",
        AUDIO_TASK_STACK_SIZE,
        this,
        AUDIO_TASK_PRIORITY,
        &audioTaskHandle,
        AUDIO_CORE
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
    updateRouting();
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
    dac_output_voltage(DAC_CHANNEL_1, 128);
    vTaskDelay(pdMS_TO_TICKS(50));
    Serial.println("AudioEngine: Stopped.");
}

void AudioEngine::setAudioSource(AudioSource* source) {
    currentAudioSource = source;
    if (source) {
        Serial.printf("AudioEngine: Audio source set to %p\n", source);
    } else {
        Serial.println("AudioEngine: Audio source cleared.");
    }
}

void AudioEngine::updateRouting() {
    if (routingManager) {
        routingManager->applyRouting(effects, effectCount);
    }
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
    float currentSample_float; // Changed to float

    while (true) {
        xSemaphoreTake(runningSemaphore, portMAX_DELAY);
        bool is_running = running;
        xSemaphoreGive(runningSemaphore);
        if (!is_running) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (outputBuffer && outputBuffer->space() >= (AUDIO_BUFFER_CHUNK_SIZE) ) {
            for (int i = 0; i < AUDIO_BUFFER_CHUNK_SIZE; ++i) {
                if (!running) break;

                if (currentAudioSource && currentAudioSource->isActive()) {
                    currentAudioSource->generateSample(currentSample_float);

                    for (size_t j = 0; j < effectCount; ++j) {
                        if (effects[j] && effects[j]->isEnabled()) {
                            effects[j]->process(currentSample_float);
                        }
                    }
                    if (!outputBuffer->write(currentSample_float)) { // Write float
                        overrunCounter++;
                        break;
                    }
                } else {
                    outputBuffer->write(0.0f); // Silence as float
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void AudioEngine::onSampleTimer(void* arg) {
    AudioEngine* engine = static_cast<AudioEngine*>(arg);
    if (engine && engine->running) {
        engine->generateAndOutputSample();
    }
}

void AudioEngine::generateAndOutputSample() {
    float sampleToOutput_float; // Changed to float
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
            // dacDitherer.process already handles the scaling from -1..1 float to 0..255 uint8_t
            // and applies dither/noise shaping. It also constrains the output to 0-255.
        }

        dac_output_voltage(DAC_CHANNEL_1, dac_sample);
        processedSampleCount++;
    } else {
        // For underrun, output silence.
        // Bypassing dither here too, as it's an explicit silence due to buffer empty.
        dac_output_voltage(DAC_CHANNEL_1, 128); // Underrun: output silence (midpoint)
        underrunCounter++;
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

    if (outputBuffer) {
        delete outputBuffer;
        outputBuffer = nullptr;
        Serial.println("AudioEngine: Output buffer deleted.");
    }

    dac_output_disable(DAC_CHANNEL_1);
    Serial.println("AudioEngine: DAC disabled.");

    currentAudioSource = nullptr;
    effectCount = 0;
    Serial.println("AudioEngine: Cleanup complete.");
}
