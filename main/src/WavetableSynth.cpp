#include "WavetableSynth.h"
#include "config.h"
#include <Arduino.h>
#include <string>
#include <string>
#include <string>

int16_t WavetableSynth::wavetables[NUM_WAVEFORMS_WT][WavetableSynth::WAVETABLE_SIZE];
bool WavetableSynth::wavetablesInitialized = false;

void WavetableSynth::initializeWavetables() {
    if (wavetablesInitialized) return;
    Serial.println("WavetableSynth: Initializing 16-bit wavetables...");
    for (size_t i = 0; i < WAVETABLE_SIZE; ++i) {
        float phaseNorm = static_cast<float>(i) / WAVETABLE_SIZE; // 0.0 to <1.0
        wavetables[SINE_WT][i]     = static_cast<int16_t>(32767.0f * sinf(TWO_PI * phaseNorm));
        wavetables[SAW_WT][i]      = static_cast<int16_t>(32767.0f * (2.0f * phaseNorm - 1.0f)); // Ramps -1 to 1
        wavetables[SQUARE_WT][i]   = (phaseNorm < 0.5f) ? 16384 : -16384; // Reduced amplitude
        float triSample = 0.0f;
        if (phaseNorm < 0.25f) triSample = 4.0f * phaseNorm;
        else if (phaseNorm < 0.75f) triSample = 2.0f - 4.0f * phaseNorm;
        else triSample = -4.0f + 4.0f * phaseNorm;
        wavetables[TRIANGLE_WT][i] = static_cast<int16_t>(32767.0f * triSample);
        wavetables[NOISE_WT][i]    = static_cast<int16_t>(random(-32767, 32768));
    }
    memcpy(wavetables[CUSTOM_WT], wavetables[SINE_WT], WAVETABLE_SIZE * sizeof(int16_t));
    wavetablesInitialized = true;
    Serial.println("WavetableSynth: Wavetables initialized.");
}

void WavetableSynth::ensureWavetablesInitialized() {
    if (!wavetablesInitialized) {
        initializeWavetables();
    }
}

WavetableSynth::WavetableSynth(float freq, uint8_t amp, WaveformType waveform)
    : phase(0), currentFrequency(freq), currentAmplitude(amp), currentWaveform(waveform),
      gate(false), /* dither(), */ ampEnvelope(0.005f, 0.1f, 1.0f, 0.2f) { // Default ADSR for wavetable
      // Dither instance removed
    ensureWavetablesInitialized();
    updatePhaseIncrement();
    Serial.printf("WavetableSynth created: %.1f Hz, Amp %d, Wave %s\n",
                 currentFrequency, currentAmplitude, getWaveformName(currentWaveform));
}

void WavetableSynth::generateSample(float& sample) {
    if (!gate && ampEnvelope.getState() == ADSREnvelope::OFF) {
        sample = 0.0f;
        return;
    }

    float envAmp = ampEnvelope.getAmplitude();
    if (envAmp <= 0.001f && ampEnvelope.getState() == ADSREnvelope::OFF) {
        sample = 0.0f;
        return;
    }

    const int16_t* wavetable = wavetables[currentWaveform];
    uint32_t integerPart = (phase >> 8) & WAVETABLE_MASK;
    uint8_t fractionalPart = phase & 0xFF;

    int32_t s1 = wavetable[integerPart];
    int32_t s2 = wavetable[(integerPart + 1) & WAVETABLE_MASK];

    int32_t interpolated_sample = s1 + (((s2 - s1) * fractionalPart) >> 8);

    float scaled_float = (static_cast<float>(interpolated_sample) / 32767.0f) *
                         (static_cast<float>(currentAmplitude) / 127.0f) *
                         envAmp;

    scaled_float = constrain(scaled_float, -1.0f, 1.0f);

    phase += phaseIncrement;

    sample = scaled_float;
}

void WavetableSynth::noteOn(float frequency, float velocity) {
    gate = true;
    setFrequency(frequency); // Set frequency on note on
    // Velocity could scale currentAmplitude or an internal gain factor for the envelope
    // For now, let's use velocity to scale the main amplitude, if ampEnvelope supports velocity scaling
    // ampEnvelope.noteOn(velocity); // If ADSR had velocity support
    ampEnvelope.noteOn();
    Serial.printf("WavetableSynth Note ON: Freq=%.1f Hz\n", currentFrequency);
}

void WavetableSynth::noteOff() {
    gate = false;
    ampEnvelope.noteOff();
    Serial.println("WavetableSynth Note OFF");
}

void WavetableSynth::reset() {
    phase = 0;
    ampEnvelope.reset();
    gate = false;
    // dither.reset(); // Dither instance removed
}

void WavetableSynth::setParameter(const std::string& name, float value) {
    if (name == "frequency") {
        setFrequency(value);
    } else if (name == "amplitude") {
        setAmplitude(value);
    } else if (name == "waveform") {
        setWaveform(static_cast<WaveformType>(static_cast<int>(value)));
    } else if (name == "attack") {
        ampEnvelope.setAttack(value);
    } else if (name == "decay") {
        ampEnvelope.setDecay(value);
    } else if (name == "sustain") {
        ampEnvelope.setSustain(value);
    } else if (name == "release") {
        ampEnvelope.setRelease(value);
    }
}

void WavetableSynth::setFrequency(float freq) {
    currentFrequency = constrain(freq, 20.0f, 20000.0f);
    updatePhaseIncrement();
}

float WavetableSynth::getFrequency() const {
    return currentFrequency;
}

void WavetableSynth::setAmplitude(float amp) { // Expects 0-127
    currentAmplitude = static_cast<uint8_t>(constrain(amp, 0.0f, 127.0f));
}

float WavetableSynth::getAmplitude() const { // Returns 0-127
    return static_cast<float>(currentAmplitude);
}

void WavetableSynth::setWaveform(WT_WaveformType waveform) {
    if (waveform < NUM_WAVEFORMS_WT) {
        currentWaveform = waveform;
        Serial.printf("WavetableSynth Waveform: %s\n", getWaveformName(waveform));
    }
}

bool WavetableSynth::loadCustomWavetable(const int16_t* data, size_t size_bytes) {
    if (size_bytes != (WAVETABLE_SIZE * sizeof(int16_t))) {
        Serial.printf("WavetableSynth: Custom table size mismatch. Expected %u, got %u\n",
                      WAVETABLE_SIZE * sizeof(int16_t), size_bytes);
        return false;
    }
    memcpy(wavetables[CUSTOM], data, size_bytes);
    Serial.println("WavetableSynth: Custom table loaded.");
    return true;
}

void WavetableSynth::generateCustomWaveform(float (*waveFunction)(float)) {
    ensureWavetablesInitialized(); // Ensure other tables are there if needed as base
    for (size_t i = 0; i < WAVETABLE_SIZE; i++) {
        float phaseValue = static_cast<float>(i) / WAVETABLE_SIZE;  // 0.0 to <1.0
        float func_output = waveFunction(phaseValue);
        wavetables[CUSTOM][i] = static_cast<int16_t>(constrain(func_output, -1.0f, 1.0f) * 32767.0f);
    }
    Serial.println("WavetableSynth: Custom waveform generated.");
}

const int16_t* WavetableSynth::getWavetableData(WT_WaveformType waveform) const {
    WT_WaveformType typeToGet = (waveform == NUM_WAVEFORMS_WT) ? currentWaveform : waveform;
    if (typeToGet < NUM_WAVEFORMS_WT) {
        return wavetables[typeToGet];
    }
    return nullptr;
}

void WavetableSynth::updatePhaseIncrement() {
    phaseIncrement = static_cast<uint32_t>((currentFrequency * WAVETABLE_SIZE * 256.0f) / SAMPLE_RATE);
}

const char* WavetableSynth::getWaveformName(WT_WaveformType waveform) {
    switch (waveform) {
        case SINE_WT: return "SINE";
        case SAW_WT: return "SAW";
        case SQUARE_WT: return "SQUARE";
        case TRIANGLE_WT: return "TRIANGLE";
        case NOISE_WT: return "NOISE";
        case CUSTOM_WT: return "CUSTOM";
        default: return "UNKNOWN";
    }
}
