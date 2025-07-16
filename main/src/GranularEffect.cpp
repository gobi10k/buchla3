#include "GranularEffect.h"
#include "config.h"  // For SAMPLE_RATE, PI
#include <Arduino.h> // For memset, constrain, Serial, strcmp, sqrt, exp, fmod

// Initialize static members
float GranularEffect::windowTables[GranularEffect::NUM_WINDOW_TYPES][GranularEffect::WINDOW_TABLE_SIZE];
bool GranularEffect::windowTablesInitialized = false;

void GranularEffect::Grain::reset() {
    active = false;
    bufferPosition = 0.0f;
    playbackRate = 1.0f;
    grainPosition = 0;
    grainSize = 0;
    amplitude = 0.0f;
}

void GranularEffect::initializeWindowTables() {
    if (windowTablesInitialized) return;

    Serial.println("Initializing granular effect window tables...");

    // Hann window
    for (size_t i = 0; i < WINDOW_TABLE_SIZE; i++) {
        float phase = (float)i / (WINDOW_TABLE_SIZE - 1);
        windowTables[HANN_WINDOW][i] = 0.5f * (1.0f - cos(2.0f * PI * phase));
    }

    // Triangle window
    for (size_t i = 0; i < WINDOW_TABLE_SIZE; i++) {
        float phase = (float)i / (WINDOW_TABLE_SIZE - 1);
        if (phase <= 0.5f) {
            windowTables[TRIANGLE_WINDOW][i] = 2.0f * phase;
        } else {
            windowTables[TRIANGLE_WINDOW][i] = 2.0f * (1.0f - phase);
        }
    }

    // Gaussian window (approximation)
    for (size_t i = 0; i < WINDOW_TABLE_SIZE; i++) {
        float phase = (float)i / (WINDOW_TABLE_SIZE - 1);
        float x = (phase - 0.5f) * 6.0f; // -3 to +3 range
        windowTables[GAUSSIAN_WINDOW][i] = exp(-0.5f * x * x);
    }

    windowTablesInitialized = true;
    Serial.println("Granular effect window tables initialized");
}

GranularEffect::GranularEffect(float density, uint32_t grainSize, float mix)
    : bufferWriteIndex(0), activeGrainCount(0),
      grainDensity(density), baseGrainSize(grainSize), grainSizeVariation(0.2f),
      playbackRate(1.0f), pitchVariation(0.1f), positionSpray(0.3f), timeShift(0.1f),
      dryWetMix(mix), windowType(HANN_WINDOW), samplesSinceLastGrain(0.0f),
      randomSeed(54321) { // Or use a more random seed, e.g. analogRead(0) if available early

    // ensureWindowTablesInitialized(); // Call public static method
    initializeWindowTables(); // Or call private static method directly from constructor

    for (size_t i = 0; i < INPUT_BUFFER_SIZE; ++i) { inputBuffer[i] = 0.0f; } // Initialize float buffer
    for (size_t i = 0; i < MAX_GRAINS; i++) {
        grains[i].reset();
    }
    updateGrainTiming();

    Serial.printf("Granular effect created: %.1f grains/sec, %u samples/grain, %.1f%% wet\n",
                 density, grainSize, mix * 100.0f);
}

void GranularEffect::process(float& sample) {
    if (!enabled) return;

    inputBuffer[bufferWriteIndex] = sample;
    bufferWriteIndex = (bufferWriteIndex + 1) % INPUT_BUFFER_SIZE;

    samplesSinceLastGrain += 1.0f;
    if (samplesSinceLastGrain >= samplesPerGrain) {
        triggerNewGrain();
        samplesSinceLastGrain -= samplesPerGrain;
    }

    float granularOutput = 0.0f;
    size_t currentActiveGrains = 0;

    for (size_t i = 0; i < MAX_GRAINS; i++) {
        if (grains[i].active) {
            currentActiveGrains++;
            float grainSample = processGrain(grains[i]);
            granularOutput += grainSample;

            grains[i].grainPosition++;
            if (grains[i].grainPosition >= grains[i].grainSize) {
                grains[i].active = false;
            }
        }
    }
    activeGrainCount = currentActiveGrains;

    if (activeGrainCount > 0) {
        granularOutput /= sqrt((float)activeGrainCount);
    }

    sample = sample * (1.0f - dryWetMix) + granularOutput * dryWetMix;
}

float GranularEffect::getParameter(const std::string& key) const {
    if (key == "dry_wet") return dryWetMix;
    if (key == "density") return grainDensity;
    if (key == "grain_size") return (float)baseGrainSize;
    if (key == "grain_variation") return grainSizeVariation;
    if (key == "playback_rate") return playbackRate;
    if (key == "pitch_variation") return pitchVariation;
    if (key == "time_shift") return timeShift;
    if (key == "spray") return positionSpray;
    return 0.0f;
}

void GranularEffect::setDensity(float d) {
    grainDensity = constrain(d, 0.1f, 100.0f);
    updateGrainTiming();
}

void GranularEffect::setGrainSize(uint32_t size) {
    baseGrainSize = constrain(size, (uint32_t)16, (uint32_t)INPUT_BUFFER_SIZE / 2);
    updateGrainTiming();
}

void GranularEffect::setGrainSizeVariation(float variation) {
    grainSizeVariation = constrain(variation, 0.0f, 1.0f);
}

void GranularEffect::setPlaybackRate(float rate) {
    playbackRate = constrain(rate, -4.0f, 4.0f);
}

void GranularEffect::setPitchVariation(float variation) {
    pitchVariation = constrain(variation, 0.0f, 1.0f);
}

void GranularEffect::setPositionSpray(float spray) {
    positionSpray = constrain(spray, 0.0f, 1.0f);
}

void GranularEffect::setTimeShift(float shift) {
    timeShift = constrain(shift, -1.0f, 1.0f);
}

void GranularEffect::setMix(float m) {
    dryWetMix = constrain(m, 0.0f, 1.0f);
}

void GranularEffect::setParameter(const std::string& key, float value) {
    if (key == "dry_wet") {
        setMix(value);
    } else if (key == "density") {
        setDensity(value);
    } else if (key == "grain_size") {
        setGrainSize((uint32_t)value);
    } else if (key == "grain_variation") {
        setGrainSizeVariation(value);
    } else if (key == "playback_rate") {
        setPlaybackRate(value);
    } else if (key == "pitch_variation") {
        setPitchVariation(value);
    } else if (key == "time_shift") {
        setTimeShift(value);
    } else if (key == "spray") {
        setPositionSpray(value);
    } else if (key == "enabled") {
        enabled = (value > 0.5f);
    }
}

void GranularEffect::setWindowType(WindowType type) {
    if (type < NUM_WINDOW_TYPES) {
        this->windowType = type;
        Serial.printf("Granular effect window changed to: %s\n", getWindowTypeName(type));
    }
}

void GranularEffect::loadPreset(int presetNumber) {
    switch (presetNumber) {
        case 0: // Shimmer
            grainDensity = 15.0f; baseGrainSize = 256; grainSizeVariation = 0.2f;
            playbackRate = 1.0f; pitchVariation = 0.3f; positionSpray = 0.2f;
            timeShift = 0.05f; dryWetMix = 0.3f; windowType = HANN_WINDOW;
            Serial.println("Loaded Shimmer preset");
            break;
        case 1: // Freeze
            grainDensity = 8.0f; baseGrainSize = 1024; grainSizeVariation = 0.1f;
            playbackRate = 0.3f; pitchVariation = 0.1f; positionSpray = 0.5f;
            timeShift = 0.2f; dryWetMix = 0.7f; windowType = HANN_WINDOW;
            Serial.println("Loaded Freeze preset");
            break;
        case 2: // Stutter
            grainDensity = 30.0f; baseGrainSize = 128; grainSizeVariation = 0.6f;
            playbackRate = 1.2f; pitchVariation = 0.2f; positionSpray = 0.8f;
            timeShift = 0.1f; dryWetMix = 0.5f; windowType = TRIANGLE_WINDOW;
            Serial.println("Loaded Stutter preset");
            break;
        case 3: // Reverse
            grainDensity = 12.0f; baseGrainSize = 512; grainSizeVariation = 0.3f;
            playbackRate = -0.8f; pitchVariation = 0.15f; positionSpray = 0.4f;
            timeShift = 0.3f; dryWetMix = 0.6f; windowType = GAUSSIAN_WINDOW;
            Serial.println("Loaded Reverse preset");
            break;
        case 4: // Octave
            grainDensity = 10.0f; baseGrainSize = 384; grainSizeVariation = 0.2f;
            playbackRate = 2.0f; pitchVariation = 0.1f; positionSpray = 0.3f;
            timeShift = 0.15f; dryWetMix = 0.4f; windowType = HANN_WINDOW;
            Serial.println("Loaded Octave preset");
            break;
        default:
            Serial.printf("Invalid granular effect preset: %d (valid: 0-4)\n", presetNumber);
            return;
    }
    updateGrainTiming();
}

void GranularEffect::reset() {
    for (size_t i = 0; i < INPUT_BUFFER_SIZE; ++i) { inputBuffer[i] = 0.0f; } // Initialize float buffer
    bufferWriteIndex = 0;
    for (size_t i = 0; i < MAX_GRAINS; i++) {
        grains[i].reset();
    }
    activeGrainCount = 0;
    samplesSinceLastGrain = 0.0f;
}

void GranularEffect::updateGrainTiming() {
    if (grainDensity > 0) {
        samplesPerGrain = SAMPLE_RATE / grainDensity;
    } else {
        samplesPerGrain = SAMPLE_RATE * 1000; // Effectively infinite if density is zero
    }
    Serial.printf("Granular effect timing updated: %.1f samples between grains\n", samplesPerGrain);
}

void GranularEffect::triggerNewGrain() {
    for (size_t i = 0; i < MAX_GRAINS; i++) {
        if (!grains[i].active) {
            initializeGrain(grains[i]);
            return; // Found a slot
        }
    }
    // Optional: if all grains active, could replace oldest or quietest
}

void GranularEffect::initializeGrain(Grain& grain) {
    grain.active = true;
    grain.grainPosition = 0;

    float sizeMultiplier = 1.0f + (fastRandomFloat() - 0.5f) * 2.0f * grainSizeVariation;
    grain.grainSize = (uint32_t)(baseGrainSize * sizeMultiplier);
    grain.grainSize = constrain(grain.grainSize, 64, INPUT_BUFFER_SIZE / 2); // Max grain size protection

    float pitchMultiplier = 1.0f + (fastRandomFloat() - 0.5f) * 2.0f * pitchVariation;
    grain.playbackRate = playbackRate * pitchMultiplier;

    float sprayOffset = (fastRandomFloat() - 0.5f) * 2.0f * positionSpray;
    float effectiveTimeShift = constrain(timeShift + sprayOffset, 0.0f, 1.0f);

    uint32_t samplesBack = (uint32_t)(effectiveTimeShift * (INPUT_BUFFER_SIZE - grain.grainSize)); // Ensure grain can be read
    grain.bufferPosition = (float)((bufferWriteIndex - samplesBack + INPUT_BUFFER_SIZE) % INPUT_BUFFER_SIZE);
    // Ensure bufferPosition is also adjusted if playbackRate is negative to start from the "end" of the segment in buffer
    if (grain.playbackRate < 0) {
         grain.bufferPosition = (grain.bufferPosition + grain.grainSize * fabs(grain.playbackRate));
         if (grain.bufferPosition >= INPUT_BUFFER_SIZE) grain.bufferPosition -= INPUT_BUFFER_SIZE;

    }

    grain.amplitude = 1.0f; // Could be randomized too
}

float GranularEffect::processGrain(Grain& grain) {
    // Calculate actual read index with integer part
    uint32_t intPos = (uint32_t)floor(grain.bufferPosition);
    float fracPos = grain.bufferPosition - intPos;

    // Ensure positive modulo for negative bufferPosition values (can happen with negative playbackRate)
    intPos = (intPos % INPUT_BUFFER_SIZE + INPUT_BUFFER_SIZE) % INPUT_BUFFER_SIZE;
    uint32_t nextIntPos = (intPos + 1) % INPUT_BUFFER_SIZE;

    // Read float samples directly from the buffer
    float sample1_float = inputBuffer[intPos];
    float sample2_float = inputBuffer[nextIntPos];

    // Interpolate between the two float samples
    // These samples are already in the -1.0 to 1.0 range.
    float interpolatedSample_float = sample1_float + fracPos * (sample2_float - sample1_float);
    // No need to convert from uint8_t to signed float here.
    float signedSample = interpolatedSample_float;

    float windowPhase = (float)grain.grainPosition / (float)(grain.grainSize > 1 ? grain.grainSize - 1 : 1);
    windowPhase = constrain(windowPhase, 0.0f, 1.0f); // Ensure phase is within [0,1]
    uint32_t windowTableIndex = (uint32_t)(windowPhase * (WINDOW_TABLE_SIZE - 1));
    float windowValue = windowTables[windowType][windowTableIndex];

    float grainOutput = signedSample * windowValue * grain.amplitude;

    grain.bufferPosition += grain.playbackRate;
    // Simpler wrap-around for bufferPosition, assuming it can go negative
    while(grain.bufferPosition < 0) grain.bufferPosition += INPUT_BUFFER_SIZE;
    grain.bufferPosition = fmod(grain.bufferPosition, (float)INPUT_BUFFER_SIZE);

    return grainOutput;
}

uint32_t GranularEffect::fastRandom(uint32_t min, uint32_t max) {
    randomSeed = randomSeed * 1664525 + 1013904223; // LCG
    if (max <= min) return min; // Avoid issues if max not greater than min
    return min + (randomSeed % (max - min));
}

float GranularEffect::fastRandomFloat() {
    return (float)fastRandom(0, 10001) / 10000.0f; // Range [0.0, 1.0]
}

const char* GranularEffect::getWindowTypeName(WindowType type) const {
    switch (type) {
        case HANN_WINDOW: return "HANN";
        case TRIANGLE_WINDOW: return "TRIANGLE";
        case GAUSSIAN_WINDOW: return "GAUSSIAN";
        default: return "UNKNOWN";
    }
}
