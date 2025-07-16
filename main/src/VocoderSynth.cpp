#include "VocoderSynth.h"
#include "config.h" // For SAMPLE_RATE
#include <cmath>    // For log10f, powf, roundf
#include <Arduino.h> // For Serial.printf for debugging
#include <string>
#include <string>
#include <string>

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

// Define frequency range for vocoder bands (e.g., 100 Hz to 8000 Hz)
const float VOCODER_MIN_FREQ = 200.0f;
const float VOCODER_MAX_FREQ = 7000.0f;


VocoderSynth::VocoderSynth()
    : carrierSource(nullptr),
      modulatorSource(nullptr),
      qFactor(5.0f),         // Default Q
      attackTimeS(0.005f),   // Default 5ms attack
      releaseTimeS(0.05f),  // Default 50ms release
      outputGain(1.0f),
      currentSampleRate(static_cast<float>(SAMPLE_RATE))
{
    analysisFilters.resize(VOCODER_NUM_BANDS);
    synthesisFilters.resize(VOCODER_NUM_BANDS);
    envelopeFollowers.resize(VOCODER_NUM_BANDS);

    initializeFilterBanks();
    updateParameters(); // Apply initial Q, attack, release to filters/followers
    // Serial.println("VocoderSynth created.");
}

VocoderSynth::~VocoderSynth() {
    // Serial.println("VocoderSynth destroyed.");
}

void VocoderSynth::initializeFilterBanks() {
    // Calculate center frequencies for each band (logarithmic spacing)
    // Example: Octave bands, or custom log spacing
    // log_min = log10(MIN_FREQ)
    // log_max = log10(MAX_FREQ)
    // delta_log = (log_max - log_min) / (NUM_BANDS -1 or NUM_BANDS depending on spacing goal)
    // freq[i] = 10^(log_min + i * delta_log)

    float logMinFreq = log10f(VOCODER_MIN_FREQ);
    float logMaxFreq = log10f(VOCODER_MAX_FREQ);
    float logRange = logMaxFreq - logMinFreq;

    for (int i = 0; i < VOCODER_NUM_BANDS; ++i) {
        // Calculate center frequency for the band
        // Spread bands logarithmically, ensuring the last band reaches MAX_FREQ
        float centerFreq;
        if (VOCODER_NUM_BANDS == 1) {
            centerFreq = sqrtf(VOCODER_MIN_FREQ * VOCODER_MAX_FREQ); // Geometric mean for single band
        } else {
            // Distribute N points over the log range.
            // For N points, there are N-1 intervals.
            // Position i in [0, N-1] maps to factor i/(N-1)
            float factor = static_cast<float>(i) / static_cast<float>(VOCODER_NUM_BANDS -1) ;
            centerFreq = powf(10.0f, logMinFreq + factor * logRange);
        }

        if (centerFreq > currentSampleRate / 2.0f - 10.0f) { // Ensure below Nyquist
             centerFreq = currentSampleRate / 2.0f - 10.0f;
        }
        if (centerFreq <= 10.0f) centerFreq = 10.0f;


        analysisFilters[i].setSampleRate(currentSampleRate);
        analysisFilters[i].calculateCoefficients(BiquadFilterType::BANDPASS_PEAK, centerFreq, qFactor);

        synthesisFilters[i].setSampleRate(currentSampleRate);
        synthesisFilters[i].calculateCoefficients(BiquadFilterType::BANDPASS_PEAK, centerFreq, qFactor);

        envelopeFollowers[i].setSampleRate(currentSampleRate);
        envelopeFollowers[i].setAttackTime(attackTimeS);
        envelopeFollowers[i].setReleaseTime(releaseTimeS);

        // Serial.printf("Vocoder Band %d: Freq=%.2f Hz, Q=%.2f\n", i, centerFreq, qFactor);
    }
}

void VocoderSynth::updateParameters() {
    // Update Q for all filters
    for (int i = 0; i < VOCODER_NUM_BANDS; ++i) {
        // Re-fetch frequency to pass to calculateCoefficients, as it's not stored in SimpleBiquad
        // This is a bit inefficient; ideally SimpleBiquad would store its current freq/Q/type
        // For now, we re-calculate centerFreq as in initializeFilterBanks
        float logMinFreq = log10f(VOCODER_MIN_FREQ);
        float logMaxFreq = log10f(VOCODER_MAX_FREQ);
        float logRange = logMaxFreq - logMinFreq;
        float centerFreq;
         if (VOCODER_NUM_BANDS == 1) {
            centerFreq = sqrtf(VOCODER_MIN_FREQ * VOCODER_MAX_FREQ);
        } else {
            float factor = static_cast<float>(i) / static_cast<float>(VOCODER_NUM_BANDS -1) ;
            centerFreq = powf(10.0f, logMinFreq + factor * logRange);
        }
        if (centerFreq > currentSampleRate / 2.0f - 10.0f) centerFreq = currentSampleRate / 2.0f - 10.0f;
        if (centerFreq <= 10.0f) centerFreq = 10.0f;

        analysisFilters[i].calculateCoefficients(BiquadFilterType::BANDPASS_PEAK, centerFreq, qFactor);
        synthesisFilters[i].calculateCoefficients(BiquadFilterType::BANDPASS_PEAK, centerFreq, qFactor);

        envelopeFollowers[i].setAttackTime(attackTimeS);
        envelopeFollowers[i].setReleaseTime(releaseTimeS);
    }
    // Serial.printf("Vocoder params updated: Q=%.2f, Attack=%.3fs, Release=%.3fs\n", qFactor, attackTimeS, releaseTimeS);
}


void VocoderSynth::setCarrierSource(AudioSource* source) {
    carrierSource = source;
    // Serial.printf("Vocoder: Carrier source set to %p\n", source);
}

void VocoderSynth::setModulatorSource(AudioSource* source) {
    modulatorSource = source;
    // Serial.printf("Vocoder: Modulator source set to %p\n", source);
}

void VocoderSynth::generateSample(float& sample) {
    if (!carrierSource || !modulatorSource || !carrierSource->isActive() || !modulatorSource->isActive()) {
        sample = 0.0f;
        return;
    }

    float carrierSignal = 0.0f;
    carrierSource->generateSample(carrierSignal);

    float modulatorSignal = 0.0f;
    modulatorSource->generateSample(modulatorSignal);

    float vocodedSignal = 0.0f;

    // Analysis stage (modulator)
    modulatorSignal = constrain(modulatorSignal, -1.0f, 1.0f);
    std::vector<float> bandAmplitudes(VOCODER_NUM_BANDS);
    for (int i = 0; i < VOCODER_NUM_BANDS; ++i) {
        float modulatorBandSignal = analysisFilters[i].process(modulatorSignal);
        bandAmplitudes[i] = envelopeFollowers[i].process(modulatorBandSignal);
    }

    // Synthesis stage (carrier)
    carrierSignal = constrain(carrierSignal, -1.0f, 1.0f);
    for (int i = 0; i < VOCODER_NUM_BANDS; ++i) {
        float carrierBandSignal = synthesisFilters[i].process(carrierSignal);
        vocodedSignal += carrierBandSignal * bandAmplitudes[i];
    }

    if (VOCODER_NUM_BANDS > 0) {
        vocodedSignal /= sqrtf(static_cast<float>(VOCODER_NUM_BANDS));
    }
    vocodedSignal *= outputGain;

    sample = constrain(vocodedSignal, -1.0f, 1.0f);
}

void VocoderSynth::reset() {
    for (int i = 0; i < VOCODER_NUM_BANDS; ++i) {
        analysisFilters[i].reset();
        synthesisFilters[i].reset();
        envelopeFollowers[i].reset();
    }
    if (carrierSource) carrierSource->reset();
    if (modulatorSource) modulatorSource->reset();
    // Serial.println("VocoderSynth reset.");
}

void VocoderSynth::setParameter(const std::string& name, float value) {
    bool needsUpdate = false;
    if (name == "qFactor") {
        qFactor = constrain(value, 0.5f, 20.0f);
        needsUpdate = true;
    } else if (name == "attackTime") {
        attackTimeS = constrain(value, 0.0005f, 0.5f);
        needsUpdate = true;
    } else if (name == "releaseTime") {
        releaseTimeS = constrain(value, 0.005f, 2.0f);
        needsUpdate = true;
    } else if (name == "outputGain") {
        outputGain = constrain(value, 0.0f, 4.0f);
    }

    if (needsUpdate) {
        updateParameters();
    }
}
