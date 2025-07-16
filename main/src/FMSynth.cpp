#include "FMSynth.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>

// Operator method implementations
void FMSynth::Operator::updatePhaseIncrement(float sampleRate) {
    phaseIncrement = (TWO_PI * frequency) / sampleRate;
}

float FMSynth::Operator::generateSample(float modulation, float feedbackInput) {
    float currentPhase = phase + modulation;
    float sample = 0.0f;

    if (feedbackInput != 0.0f) {
        currentPhase += lastOutput * feedbackInput;
    }

    while (currentPhase >= TWO_PI) currentPhase -= TWO_PI;
    while (currentPhase < 0.0f) currentPhase += TWO_PI;

    switch (waveform) {
        case SINE:      sample = sinf(currentPhase); break;
        case TRIANGLE: {
            float normalizedPhase = currentPhase / TWO_PI;
            if (normalizedPhase < 0.25f) sample = 4.0f * normalizedPhase;
            else if (normalizedPhase < 0.75f) sample = 2.0f - 4.0f * normalizedPhase;
            else sample = -4.0f + 4.0f * normalizedPhase;
            break;
        }
        case SAWTOOTH:  sample = (currentPhase / PI) - 1.0f; break;
        case SQUARE:    sample = (currentPhase < PI) ? 1.0f : -1.0f; break;
        default:        sample = sinf(currentPhase); break;
    }

    phase += phaseIncrement;
    if (phase >= TWO_PI) phase -= TWO_PI;

    lastOutput = sample;
    return sample * amplitude * envelope.getAmplitude();
}

void FMSynth::Operator::setWaveform(FM_Waveform wf) {
    if (wf < NUM_WAVEFORMS) waveform = wf;
}

void FMSynth::Operator::triggerEnvelope() { envelope.noteOn(); }
void FMSynth::Operator::releaseEnvelope() { envelope.noteOff(); }
float FMSynth::Operator::getEnvelopeValue() { return envelope.getAmplitude(); }


// FMSynth method implementations
FMSynth::FMSynth(float freq, float ratio, float index, uint8_t level)
    : baseFrequency(freq), modRatio(ratio), modIndex(index),
      feedbackAmount(0.0f), outputLevel(level), currentAlgorithm(SIMPLE_FM),
      prevCarrierOutput(0.0f) {

    carrier.amplitude = 1.0f;
    modulator.amplitude = 1.0f;

    masterAmpEnvelope.setAttack(10);
    masterAmpEnvelope.setDecay(100);
    masterAmpEnvelope.setSustain(0.7f);
    masterAmpEnvelope.setRelease(200);

    carrier.envelope.setAttack(10); carrier.envelope.setDecay(100);
    carrier.envelope.setSustain(1.0); carrier.envelope.setRelease(200);

    modulator.envelope.setAttack(10); modulator.envelope.setDecay(100);
    modulator.envelope.setSustain(1.0); modulator.envelope.setRelease(200);

    updateOperatorFrequencies();
    Serial.printf("FMSynth created: %.1fHz, Ratio:%.2f, Idx:%.2f, Lvl:%d\n",
                 baseFrequency, modRatio, modIndex, outputLevel);
}

void FMSynth::noteOn(float frequency, float velocity) {
    setFrequency(frequency); // Use the setter to also update operator freqs
    masterAmpEnvelope.noteOn();
    carrier.triggerEnvelope();
    modulator.triggerEnvelope();

    carrier.phase = 0.0f; modulator.phase = 0.0f;
    carrier.lastOutput = 0.0f; modulator.lastOutput = 0.0f;
    prevCarrierOutput = 0.0f;

    Serial.printf("FMSynth Note ON: Freq=%.1f Hz\n", baseFrequency);
}

void FMSynth::noteOff() {
    masterAmpEnvelope.noteOff();
    carrier.releaseEnvelope();
    modulator.releaseEnvelope();
    Serial.println("FMSynth Note OFF");
}

// AudioSource interface methods
void FMSynth::setFrequency(float freq) {
    baseFrequency = constrain(freq, 20.0f, 20000.0f);
    updateOperatorFrequencies();
}
float FMSynth::getFrequency() const { return baseFrequency; }

void FMSynth::setAmplitude(float amp) { // Expects 0-127
    outputLevel = static_cast<uint8_t>(constrain(amp, 0.0f, 127.0f));
}
float FMSynth::getAmplitude() const { return static_cast<float>(outputLevel); }


void FMSynth::generateSample(float& sample) {
    if (masterAmpEnvelope.getState() == ADSREnvelope::OFF && masterAmpEnvelope.getAmplitude() < 0.001f) {
        sample = 0.0f;
        return;
    }

    float modOutput = 0.0f;
    float carrierInputModulation = 0.0f;
    float finalOutput = 0.0f;
    float modFeedbackSignal = 0.0f;
    float carrierFeedbackSignal = 0.0f;

    if (currentAlgorithm == MOD_FEEDBACK_FM) {
        modFeedbackSignal = modulator.lastOutput * feedbackAmount;
    }
    modOutput = modulator.generateSample(0.0f, modFeedbackSignal);

    switch (currentAlgorithm) {
        case SIMPLE_FM:
            carrierInputModulation = modOutput * modIndex;
            finalOutput = carrier.generateSample(carrierInputModulation);
            break;
        case PARALLEL_FM:
            finalOutput = carrier.generateSample() + (modOutput * modIndex * 0.5f);
            break;
        case FEEDBACK_FM:
            carrierFeedbackSignal = prevCarrierOutput * feedbackAmount;
            carrierInputModulation = modOutput * modIndex;
            finalOutput = carrier.generateSample(carrierInputModulation, carrierFeedbackSignal);
            prevCarrierOutput = finalOutput;
            break;
        case MOD_FEEDBACK_FM:
            carrierInputModulation = modOutput * modIndex;
            finalOutput = carrier.generateSample(carrierInputModulation);
            break;
    }

    finalOutput *= masterAmpEnvelope.getAmplitude();
    float scaledToLevel = finalOutput * (static_cast<float>(outputLevel) / 127.0f);

    // Output should be in -1.0 to 1.0 range.
    // The operator outputs are already in -1.0 to 1.0 (scaled by their amplitude & envelope).
    // Master envelope and outputLevel scale this further.
    // So, scaledToLevel should inherently be in a range that can be considered -1.0 to 1.0 if outputLevel is ~127.
    // Add a constrain to be safe, as FM synthesis can produce high amplitudes.
    sample = constrain(scaledToLevel, -1.0f, 1.0f);
    // sample = tanhf(scaledToLevel); // Optional soft clipping if more aggressive limiting is needed
}

void FMSynth::setParameter(const std::string& name, float value) {
    if (name == "frequency") {
        setFrequency(value);
    } else if (name == "output_level" || name == "amplitude") {
        setAmplitude(value);
    } else if (name == "mod_ratio") {
        modRatio = constrain(value, 0.01f, 50.0f);
        updateOperatorFrequencies();
    } else if (name == "mod_index") {
        modIndex = constrain(value, 0.0f, 100.0f);
    } else if (name == "feedback") {
        feedbackAmount = constrain(value, 0.0f, 0.99f);
    } else if (name == "algorithm") {
        setAlgorithm(static_cast<FM_Algorithm>(static_cast<int>(constrain(value, 0, NUM_ALGORITHMS - 1))));
    } else if (name == "mod_waveform") {
        setModulatorWaveform(static_cast<FM_Waveform>(static_cast<int>(constrain(value, 0, NUM_WAVEFORMS - 1))));
    } else if (name == "car_waveform") {
        setCarrierWaveform(static_cast<FM_Waveform>(static_cast<int>(constrain(value, 0, NUM_WAVEFORMS - 1))));
    }
    else if (name == "attack") { setAttack(value); }
    else if (name == "decay") { setDecay(value); }
    else if (name == "sustain") { setSustain(value); }
    else if (name == "release") { setRelease(value); }
    else if (name == "mod_attack") { setModulatorAttack(value); }
    else if (name == "mod_decay") { setModulatorDecay(value); }
    else if (name == "mod_sustain") { setModulatorSustain(value); }
    else if (name == "mod_release") { setModulatorRelease(value); }
    else if (name == "car_attack") { setCarrierAttack(value); }
    else if (name == "car_decay") { setCarrierDecay(value); }
    else if (name == "car_sustain") { setCarrierSustain(value); }
    else if (name == "car_release") { setCarrierRelease(value); }
}

void FMSynth::setAlgorithm(FM_Algorithm alg) {
    if (alg < NUM_ALGORITHMS) {
        currentAlgorithm = alg;
        Serial.printf("FM algorithm: %s\n", getAlgorithmName(alg));
    }
}

void FMSynth::setModulatorWaveform(FM_Waveform wf) { modulator.setWaveform(wf); }
void FMSynth::setCarrierWaveform(FM_Waveform wf) { carrier.setWaveform(wf); }

// Master Envelope setters are in the header (inline)

// Modulator Envelope setters are in the header (inline)

// Carrier Envelope setters are in the header (inline)

void FMSynth::reset() {
    carrier.phase = 0.0f; modulator.phase = 0.0f;
    carrier.lastOutput = 0.0f; modulator.lastOutput = 0.0f;
    prevCarrierOutput = 0.0f;
    masterAmpEnvelope.reset();
    carrier.envelope.reset();
    modulator.envelope.reset();
    // dither.reset(); // Dither instance removed
}

void FMSynth::updateOperatorFrequencies() {
    carrier.frequency = baseFrequency;
    modulator.frequency = baseFrequency * modRatio;
    carrier.updatePhaseIncrement(SAMPLE_RATE);
    modulator.updatePhaseIncrement(SAMPLE_RATE);
}

// applyOperatorEnvelopes removed as envelopes are self-managing

const char* FMSynth::getAlgorithmName(FM_Algorithm alg) {
    switch (alg) {
        case SIMPLE_FM: return "Simple (M->C)";
        case PARALLEL_FM: return "Parallel (M+C)";
        case FEEDBACK_FM: return "Feedback C (M->C fb(C))";
        case MOD_FEEDBACK_FM: return "Feedback M (M fb(M)->C)";
        default: return "Unknown Algorithm";
    }
}

const char* FMSynth::getWaveformName(FM_Waveform wf) {
    switch (wf) {
        case SINE: return "Sine";
        case TRIANGLE: return "Triangle";
        case SAWTOOTH: return "Sawtooth";
        case SQUARE: return "Square";
        default: return "Unknown Wave";
    }
}
