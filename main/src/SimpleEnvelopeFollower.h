#pragma once

#include <cmath>    // For fabsf, expf
#include "config.h" // For SAMPLE_RATE

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

class SimpleEnvelopeFollower {
public:
    SimpleEnvelopeFollower() : envelope(0.0f), attackSamples(100), releaseSamples(1000), sampleRate(SAMPLE_RATE) {
        setAttackTime(0.01f); // 10ms default attack
        setReleaseTime(0.1f); // 100ms default release
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        // Recalculate coefficients if sample rate changes
        setAttackTime(attackTimeS);
        setReleaseTime(releaseTimeS);
    }

    // time in seconds
    void setAttackTime(float time_s) {
        attackTimeS = time_s;
        if (time_s <= 0.0f) {
            attackCoeff = 0.0f; // Instantaneous attack
        } else {
            attackCoeff = expf(-1.0f / (sampleRate * time_s));
        }
    }

    // time in seconds
    void setReleaseTime(float time_s) {
        releaseTimeS = time_s;
        if (time_s <= 0.0f) {
            releaseCoeff = 0.0f; // Instantaneous release
        } else {
            releaseCoeff = expf(-1.0f / (sampleRate * time_s));
        }
    }

    float process(float inputSample) {
        float rectifiedInput = fabsf(inputSample);
        if (rectifiedInput > envelope) {
            // Attack phase
            envelope = (1.0f - attackCoeff) * rectifiedInput + attackCoeff * envelope;
        } else {
            // Release phase
            envelope = (1.0f - releaseCoeff) * rectifiedInput + releaseCoeff * envelope;
        }
        return envelope;
    }

    float getEnvelope() const {
        return envelope;
    }

    void reset() {
        envelope = 0.0f;
    }

private:
    float envelope;
    float attackCoeff;
    float releaseCoeff;
    float attackTimeS;    // Store for sample rate changes
    float releaseTimeS;   // Store for sample rate changes
    float sampleRate;
    unsigned int attackSamples;  // For older coefficient calculation method if needed
    unsigned int releaseSamples; // For older coefficient calculation method if needed
};
