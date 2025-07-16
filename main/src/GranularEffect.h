#pragma once

#include "AudioEffect.h"
#include "config.h" // For SAMPLE_RATE, PI
#include <Arduino.h> // For uint8_t, size_t, memset, constrain, Serial, strcmp, sqrt, exp, fmod

// ============================================================================
// Granular Effect - Real-time granular processing of input audio
// ============================================================================
class GranularEffect : public AudioEffect {
public:
    enum WindowType {
        HANN_WINDOW = 0,
        TRIANGLE_WINDOW,
        GAUSSIAN_WINDOW,
        NUM_WINDOW_TYPES
    };

private:
    static const size_t MAX_GRAINS = 6;
    static const size_t INPUT_BUFFER_SIZE = 4096;
    static const size_t WINDOW_TABLE_SIZE = 256;

    struct Grain {
        bool active;
        float bufferPosition;
        float playbackRate;
        uint32_t grainPosition;
        uint32_t grainSize;
        float amplitude;
        void reset();
    };

    float inputBuffer[INPUT_BUFFER_SIZE]; // Changed to float
    volatile size_t bufferWriteIndex; // Made volatile as it might be accessed by ISR or different thread in future

    static float windowTables[NUM_WINDOW_TYPES][WINDOW_TABLE_SIZE];
    static bool windowTablesInitialized;

    Grain grains[MAX_GRAINS];
    size_t activeGrainCount;

    float grainDensity;
    uint32_t baseGrainSize;
    float grainSizeVariation;
    float playbackRate;
    float pitchVariation;
    float positionSpray;
    float timeShift;
    float dryWetMix;
    WindowType windowType;

    float samplesSinceLastGrain;
    float samplesPerGrain;

    uint32_t randomSeed;

    static void initializeWindowTables(); // Should be private, called by constructor or public static method

public:
    GranularEffect(float density = 10.0f, uint32_t grainSize = 512, float mix = 0.5f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const override;
    void reset() override;

    void setWindowType(WindowType type);
    WindowType getWindowType() const { return windowType; }
    void loadPreset(int presetNumber);

    float getGrainDensity() const { return grainDensity; }
    uint32_t getBaseGrainSize() const { return baseGrainSize; }
    float getPlaybackRate() const { return playbackRate; }
    float getDryWetMix() const { return dryWetMix; }
    size_t getActiveGrainCount() const { return activeGrainCount; }

    static void ensureWindowTablesInitialized() { // Public static method
        if (!windowTablesInitialized) {
            initializeWindowTables();
        }
    }

private:
    void updateGrainTiming();
    void triggerNewGrain();
    void initializeGrain(Grain& grain);
    float processGrain(Grain& grain);
    uint32_t fastRandom(uint32_t min, uint32_t max);
    float fastRandomFloat();
    const char* getWindowTypeName(WindowType type) const;
};
