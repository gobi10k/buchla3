#include "ADSREnvelope.h"
#include "config.h"  // For SAMPLE_RATE
#include <Arduino.h> // For constrain, Serial, strcmp, expf, logf, fabsf
#include <cmath>     // For expf, logf, fabsf

ADSREnvelope::ADSREnvelope(float attackSec, float decaySec, float sustain, float releaseSec)
    : sustainLevel(constrain(sustain, 0.0f, 1.0f)),
      currentState(OFF), currentLevel(0.0f), gate(false) {

    setAttack(attackSec);
    setDecay(decaySec);
    setRelease(releaseSec);
    // calculateRates will be called by the setters.

    Serial.printf("ADSR Envelope created: A=%.3fs, D=%.3fs, S=%.2f, R=%.3fs\n",
                 attackTimeSec, decayTimeSec, sustainLevel, releaseTimeSec);
}

void ADSREnvelope::setAttack(float seconds) {
    attackTimeSec = constrain(seconds, 0.001f, 20.0f); // Min attack 1ms, Max 20s
    calculateRates();
}

void ADSREnvelope::setDecay(float seconds) {
    decayTimeSec = constrain(seconds, 0.001f, 20.0f);
    calculateRates();
}

void ADSREnvelope::setSustain(float level) {
    sustainLevel = constrain(level, 0.0f, 1.0f);
    // No need to recalculate rates, but if in SUSTAIN state, update currentLevel
    if (currentState == SUSTAIN) {
        currentLevel = sustainLevel;
    }
}

void ADSREnvelope::setRelease(float seconds) {
    releaseTimeSec = constrain(seconds, 0.001f, 20.0f);
    calculateRates();
}

// Calculate internal per-sample rates from time values
void ADSREnvelope::calculateRates() {
    // Attack: linear increase from 0 to 1.0
    float attackSamples = attackTimeSec * SAMPLE_RATE;
    if (attackSamples < 1.0f) attackSamples = 1.0f; // Ensure at least one sample for attack
    attackRate = 1.0f / attackSamples;

    // Decay: exponential decrease from 1.0 to sustainLevel
    // We want currentLevel = currentLevel * decayRate until it reaches sustainLevel
    // sustainLevel = 1.0 * (decayRate ^ decaySamples)
    // log(sustainLevel) = decaySamples * log(decayRate)
    // decayRate = exp(log(sustainLevel) / decaySamples)
    float decaySamples = decayTimeSec * SAMPLE_RATE;
    if (decaySamples < 1.0f) decaySamples = 1.0f;
    if (sustainLevel <= TARGET_RATIO_DEFAULT) { // Avoid log(0) or very small numbers if sustain is effectively zero
        decayRate = calculateRate(decayTimeSec, 1.0f, TARGET_RATIO_DEFAULT);
    } else {
        decayRate = calculateRate(decayTimeSec, 1.0f, sustainLevel);
    }

    // Release: exponential decrease from currentLevel (usually sustainLevel) to 0
    // Similar calculation for releaseRate
    // 0 (+epsilon) = currentLevel * (releaseRate ^ releaseSamples)
    float releaseSamples = releaseTimeSec * SAMPLE_RATE;
    if (releaseSamples < 1.0f) releaseSamples = 1.0f;
    releaseRate = calculateRate(releaseTimeSec, 1.0f, TARGET_RATIO_DEFAULT); // Rate to go from 1 to almost 0
}

float ADSREnvelope::calculateRate(float timeInSeconds, float startLevel, float endLevel) {
    // Calculates a multiplicative rate for exponential segments.
    // rate = exp(log(endLevel / startLevel) / (timeInSeconds * SAMPLE_RATE))
    // Assumes startLevel and endLevel are positive.
    // If endLevel is 0 (or very close), we aim for a target ratio.

    float numSamples = timeInSeconds * SAMPLE_RATE;
    if (numSamples < 1.0f) numSamples = 1.0f;

    if (startLevel <= 0.0f) startLevel = TARGET_RATIO_DEFAULT; // Avoid issues with log(0) or division by zero
    if (endLevel <= 0.0f) endLevel = TARGET_RATIO_DEFAULT;


    // If start and end levels are very close, or time is extremely short,
    // it might be better to use a linear approach or a fixed small multiplier.
    // For now, direct calculation:
    if (fabsf(startLevel - endLevel) < 0.00001f) { // Effectively same level
        return 1.0f; // No change
    }

    // Ensure ratio is > 0 for log
    float ratio = endLevel / startLevel;
    if (ratio <= 0.0f) { // Should not happen if inputs are positive
        // This case means we are decaying/releasing towards zero from a positive value.
        // We want to reach a small target (TARGET_RATIO_DEFAULT) relative to the startLevel.
        // So, the effective endLevel for calculation is startLevel * TARGET_RATIO_DEFAULT
        // And the ratio is TARGET_RATIO_DEFAULT
        ratio = TARGET_RATIO_DEFAULT;
    }


    // The rate should be < 1 for decay/release, > 1 for rise (though attack is linear here)
    // log(ratio) will be negative if ratio < 1 (decaying)
    return expf(logf(ratio) / numSamples);
}


void ADSREnvelope::noteOn() {
    gate = true;
    if (currentState == OFF || currentState == RELEASE) { // Retrigger
        currentLevel = 0.0f; // Start attack from zero or current level if retriggering during release
    }
    // If in Attack, Decay, or Sustain, retriggering might mean starting attack again from current level
    // For simplicity now, always start attack from 0 on noteOn if it was off or releasing.
    // More advanced retrigger options could be added.
    currentState = ATTACK;
    Serial.println("ADSR: Note ON");
}

void ADSREnvelope::noteOff() {
    gate = false;
    if (currentState != OFF) { // Only start release if not already off
        currentState = RELEASE;
        Serial.println("ADSR: Note OFF -> Release");
    }
}

void ADSREnvelope::reset() {
    currentState = OFF;
    currentLevel = 0.0f;
    gate = false;
    Serial.println("ADSR: Reset");
}


float ADSREnvelope::getAmplitude() {
    switch (currentState) {
        case OFF:
            currentLevel = 0.0f;
            break;

        case ATTACK:
            currentLevel += attackRate; // Linear attack
            if (currentLevel >= 1.0f) {
                currentLevel = 1.0f;
                currentState = DECAY;
                // Serial.println("ADSR: Attack -> Decay");
            }
            break;

        case DECAY:
            currentLevel *= decayRate; // Exponential decay
            if (currentLevel <= sustainLevel) {
                currentLevel = sustainLevel;
                if (sustainLevel <= TARGET_RATIO_DEFAULT && gate) { // If sustain is effectively zero, but gate is on
                    // This handles cases like percussive sounds with no sustain.
                    // If gate is still on, it should stay at this near-zero level.
                    // If gate turns off, it will go to RELEASE.
                    currentState = SUSTAIN; // Go to sustain even if it's zero
                } else if (sustainLevel <= TARGET_RATIO_DEFAULT && !gate) {
                     currentState = RELEASE; // If sustain is zero and gate is off, go to release
                } else {
                    currentState = SUSTAIN;
                    // Serial.println("ADSR: Decay -> Sustain");
                }
            }
            break;

        case SUSTAIN:
            // currentLevel remains sustainLevel
            // If gate is released during sustain, transition to RELEASE is handled by noteOff()
            if (!gate) { // Should be handled by noteOff, but as a fallback
                 currentState = RELEASE;
            }
            break;

        case RELEASE:
            currentLevel *= releaseRate; // Exponential release
            if (currentLevel <= TARGET_RATIO_DEFAULT) { // Check if level is practically zero
                currentLevel = 0.0f;
                currentState = OFF;
                // Serial.println("ADSR: Release -> OFF");
            }
            break;
    }
    return constrain(currentLevel, 0.0f, 1.0f);
}

// Removed loadPreset as it's better handled by a synth/controller layer.
// Removed process() and setParameter(char*, float) as this class is no longer an AudioEffect.
// Individual setters are now used.
