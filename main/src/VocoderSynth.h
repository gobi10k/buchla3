#pragma once

#include "AudioSource.h"
#include "SimpleBiquad.h"
#include "SimpleEnvelopeFollower.h"
#include "config.h" // For SAMPLE_RATE
#include <vector>
#include <Arduino.h> // For Serial

// ============================================================================
// VocoderSynth - Cross-synthesis effect implemented as an AudioSource
// It uses one AudioSource as a carrier and another as a modulator.
// ============================================================================

// Define the number of bands for the vocoder
// More bands = better spectral resolution but higher CPU cost.
// 8-16 is a common range for simpler vocoders. ESP32 might handle 8-10 well.
const int VOCODER_NUM_BANDS = 8; // Configurable number of bands

class VocoderSynth : public AudioSource {
public:
    VocoderSynth();
    ~VocoderSynth() override;

    void generateSample(float& sample) override;
    void reset() override;

    // Methods to set the carrier and modulator sources
    // These should be called before the synth is expected to produce sound.
    void setCarrierSource(AudioSource* source);
    void setModulatorSource(AudioSource* source);

    AudioSource* getCarrierSource() const { return carrierSource; }
    AudioSource* getModulatorSource() const { return modulatorSource; }

    // Parameters for the vocoder
    void setParameter(const std::string& name, float value) override;
    // Example parameters:
    // "qFactor": Q for bandpass filters (e.g., 2.0 to 10.0)
    // "attackTime": Envelope follower attack time (e.g., 0.001 to 0.1 s)
    // "releaseTime": Envelope follower release time (e.g., 0.01 to 0.5 s)
    // "outputGain": Final gain adjustment (e.g., 0.0 to 2.0)

    // Getters for parameters
    float getQFactor() const { return qFactor; }
    float getAttackTime() const { return attackTimeS; }
    float getReleaseTime() const { return releaseTimeS; }
    float getOutputGain() const { return outputGain; }

    SynthType getSynthType() const override { return SYNTH_TYPE_UNKNOWN; } // Or a new VOCODER type if added to enum

private:
    AudioSource* carrierSource;
    AudioSource* modulatorSource;

    // Filter banks
    std::vector<SimpleBiquad> analysisFilters;  // For modulator
    std::vector<SimpleBiquad> synthesisFilters; // For carrier
    std::vector<SimpleEnvelopeFollower> envelopeFollowers;

    // Vocoder parameters
    float qFactor;      // Q for bandpass filters
    float attackTimeS;  // Envelope follower attack time in seconds
    float releaseTimeS; // Envelope follower release time in seconds
    float outputGain;   // Overall output gain scaling

    float currentSampleRate;

    void initializeFilterBanks(); // Sets up frequencies and Q for filters
    void updateParameters(); // Updates filter Q, envelope times
};
