#pragma once

// #include "AudioEffect.h" // No longer an AudioEffect, but a component used by AudioSources/Effects
#include "config.h" // For SAMPLE_RATE
#include <Arduino.h> // For uint8_t, uint32_t, constrain, Serial, strcmp, expf

// ============================================================================
// ADSR Envelope - Attack, Decay, Sustain, Release envelope generator
// Provides a more musically useful exponential curve for decay and release.
// ============================================================================
class ADSREnvelope {
public:
    enum EnvelopeState {
        OFF = 0, // Changed from IDLE to OFF for clarity
        ATTACK,
        DECAY,
        SUSTAIN,
        RELEASE
    };

private:
    float attackRate;    // Rate: amount to add per sample (linear attack)
    float decayRate;     // Rate: multiplier per sample (exponential decay)
    float releaseRate;   // Rate: multiplier per sample (exponential release)
    float sustainLevel;  // 0.0 to 1.0

    EnvelopeState currentState;
    float currentLevel;     // Current envelope output (0.0 to 1.0)

    // Parameters stored in seconds for user interface, converted to rates internally
    float attackTimeSec;
    float decayTimeSec;
    float releaseTimeSec;

    bool gate; // True if note is held

    // Constants for exponential curve calculation
    // Target value for decay/release to stop (practically zero)
    static constexpr float TARGET_RATIO_DEFAULT = 0.0001f;


public:
    ADSREnvelope(float attackSec = 0.01f, float decaySec = 0.3f, float sustain = 0.7f, float releaseSec = 1.0f);

    // Removed: uint8_t process(uint8_t input) override;
    // Envelope will be queried for its value using getAmplitude()
    // void setParameter(const char* name, float value) override; // Will be individual setters

    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float level); // 0.0 to 1.0
    void setRelease(float seconds);

    void noteOn();  // Triggers the envelope
    void noteOff(); // Starts the release phase
    void reset();   // Resets to OFF state, level 0

    float getAmplitude(); // Calculates and returns the current envelope amplitude
    EnvelopeState getState() const { return currentState; }
    bool isActive() const { return currentState != OFF; }


    // Getters for parameters (optional, for UI/state saving)
    float getAttack() const { return attackTimeSec; }
    float getDecay() const { return decayTimeSec; }
    float getSustain() const { return sustainLevel; }
    float getRelease() const { return releaseTimeSec; }


private:
    void calculateRates(); // Calculate internal rates from time values
    float calculateRate(float timeInSeconds, float startLevel, float endLevel); // For exponential stages
};
