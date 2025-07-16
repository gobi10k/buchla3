#pragma once

#include "AudioSource.h"
#include <Arduino.h>
#include "DitherNoiseShaping.h"
#include "ADSREnvelope.h" // For internal envelope

// ============================================================================
// Wavetable Synthesizer - High-quality wavetable synthesis
// ============================================================================
class WavetableSynth : public AudioSource {
public:
    enum WaveformType {
        SINE = 0, SAW, SQUARE, TRIANGLE, NOISE, CUSTOM, NUM_WAVEFORMS
    };

private:
    static const size_t WAVETABLE_SIZE = 256;
    static const size_t WAVETABLE_MASK = WAVETABLE_SIZE - 1;

    static int16_t wavetables[NUM_WAVEFORMS_WT][WAVETABLE_SIZE];
    static bool wavetablesInitialized;

    uint32_t phase;
    uint32_t phaseIncrement;
    float currentFrequency; // Renamed from 'frequency' for clarity
    uint8_t currentAmplitude; // Renamed from 'amplitude'
    WaveformType currentWaveform;
    bool gate; // For note on/off state

    // DitherNoiseShaping dither; // Removed, global ditherer in AudioEngine
    ADSREnvelope ampEnvelope; // Internal amplitude envelope

    static void initializeWavetables();
    void updatePhaseIncrement();

public:
    WavetableSynth(float freq = DEFAULT_SYNTH_FREQUENCY, uint8_t amp = 100, WaveformType waveform = SINE_WT);

    // AudioSource overrides
    void generateSample(float& sample) override;
    void reset() override;
    bool isActive() const override { return ampEnvelope.getState() != ADSREnvelope::OFF; }
    void setParameter(const std::string& name, float value) override;
    SynthType getSynthType() const override { return SYNTH_TYPE_WAVETABLE; }

    // Note control and parameter setting
    void noteOn(float frequency, float velocity = 1.0f) override;
    void noteOff() override;

    void setFrequency(float freq) override;
    float getFrequency() const override;
    void setAmplitude(float amp) override; // Expects 0-127
    float getAmplitude() const override; // Returns 0-127

    void setWaveform(WaveformType waveform);
    WaveformType getWaveformType() const { return currentWaveform; } // Changed from getWaveform

    bool loadCustomWavetable(const int16_t* data, size_t size_bytes);
    void generateCustomWaveform(float (*waveFunction)(float phaseZeroToOne)); // Phase 0.0 to 1.0

    const int16_t* getWavetableData(WaveformType waveform = NUM_WAVEFORMS_WT) const;
    size_t getWavetableSize() const { return WAVETABLE_SIZE; }

    static void ensureWavetablesInitialized();
    static const char* getWaveformName(WaveformType waveform); // Made static for use in main.ino

    // Envelope parameter setters
    void setAttack(float ms) { ampEnvelope.setAttack(ms); }
    void setDecay(float ms) { ampEnvelope.setDecay(ms); }
    void setSustain(float level) { ampEnvelope.setSustain(level); }
    void setRelease(float ms) { ampEnvelope.setRelease(ms); }
};
