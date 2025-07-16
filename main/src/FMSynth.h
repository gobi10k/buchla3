#pragma once

#include "AudioSource.h"
#include "config.h"
#include <Arduino.h>
#include "DitherNoiseShaping.h"
#include "ADSREnvelope.h"

// ============================================================================
// FM Synthesizer - 2-Operator Frequency Modulation synthesis
// ============================================================================
enum FM_Algorithm {
    SIMPLE_FM = 0,
    PARALLEL_FM,
    FEEDBACK_FM,
    MOD_FEEDBACK_FM,
    NUM_ALGORITHMS
};

enum FM_Waveform {
    SINE = 0, TRIANGLE, SAWTOOTH, SQUARE, NUM_WAVEFORMS
};

class FMSynth : public AudioSource {
public:
    using Algorithm = FM_Algorithm;
    using Waveform = FM_Waveform;

private:
    struct Operator {
        float phase;
        float frequency;
        float amplitude;
        float phaseIncrement;
        Waveform waveform;
        ADSREnvelope envelope;
        float lastOutput;

        void updatePhaseIncrement(float sampleRate);
        float generateSample(float modulation = 0.0f, float feedbackInput = 0.0f);
        void setWaveform(Waveform wf);
        void triggerEnvelope();
        void releaseEnvelope();
        // updateEnvelope removed as ADSREnvelope is self-contained
        float getEnvelopeValue();

        Operator() : phase(0.0f), frequency(DEFAULT_SYNTH_FREQUENCY), amplitude(1.0f), phaseIncrement(0.0f),
                     waveform(SINE), lastOutput(0.0f) {}
    };

    Operator carrier;
    Operator modulator;

    float baseFrequency;
    float modRatio;
    float modIndex;
    float feedbackAmount;
    uint8_t outputLevel;      // 0-127
    Algorithm currentAlgorithm; // Renamed from algorithm
    float prevCarrierOutput;
    // DitherNoiseShaping dither; // Removed, global ditherer in AudioEngine
    ADSREnvelope masterAmpEnvelope; // Renamed from ampEnvelope

public:
    FMSynth(float freq = DEFAULT_SYNTH_FREQUENCY, float ratio = 1.0f, float index = 1.0f, uint8_t level = 100);

    // AudioSource overrides
    void generateSample(float& sample) override;
    void reset() override;
    bool isActive() const override { return masterAmpEnvelope.getState() != ADSREnvelope::OFF; }
    void setParameter(const std::string& name, float value) override;
    SynthType getSynthType() const override { return SYNTH_TYPE_FM; }

    void noteOn(float frequency, float velocity = 1.0f) override;
    void noteOff() override;

    // Frequency and Amplitude (as per AudioSource additions)
    void setFrequency(float freq) override; // Sets baseFrequency
    float getFrequency() const override;    // Gets baseFrequency
    void setAmplitude(float amp) override;  // Sets outputLevel (0-127)
    float getAmplitude() const override;    // Gets outputLevel (0-127)


    void setAlgorithm(Algorithm alg);
    Algorithm getAlgorithm() const { return currentAlgorithm; }
    static const char* getAlgorithmName(Algorithm alg);
    static const char* getWaveformName(Waveform wf); // Added static waveform name getter

    void setModulatorWaveform(Waveform wf);
    void setCarrierWaveform(Waveform wf);
    Waveform getModulatorWaveform() const { return modulator.waveform; }
    Waveform getCarrierWaveform() const { return carrier.waveform; }

    // Master Envelope
    void setAttack(float ms) { masterAmpEnvelope.setAttack(ms); }
    void setDecay(float ms) { masterAmpEnvelope.setDecay(ms); }
    void setSustain(float level) { masterAmpEnvelope.setSustain(level); }
    void setRelease(float ms) { masterAmpEnvelope.setRelease(ms); }

    // Modulator Envelope
    void setModulatorAttack(float ms) { modulator.envelope.setAttack(ms); }
    void setModulatorDecay(float ms) { modulator.envelope.setDecay(ms); }
    void setModulatorSustain(float level) { modulator.envelope.setSustain(level); }
    void setModulatorRelease(float ms) { modulator.envelope.setRelease(ms); }

    // Carrier Envelope
    void setCarrierAttack(float ms) { carrier.envelope.setAttack(ms); }
    void setCarrierDecay(float ms) { carrier.envelope.setDecay(ms); }
    void setCarrierSustain(float level) { carrier.envelope.setSustain(level); }
    void setCarrierRelease(float ms) { carrier.envelope.setRelease(ms); }

    // Other Getters
    float getModulatorRatio() const { return modRatio; }
    float getModulationIndex() const { return modIndex; }
    float getFeedback() const { return feedbackAmount; }
    // getOutputLevel already covered by getAmplitude()

private:
    void updateOperatorFrequencies();
    // applyOperatorEnvelopes removed as ADSREnvelope is self-contained
};
