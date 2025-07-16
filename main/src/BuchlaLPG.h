#pragma once

#include "AudioEffect.h"
#include "config.h" // For SAMPLE_RATE
#include <Arduino.h>
#include <cmath>

class BuchlaLPG : public AudioEffect {
public:
    enum Mode {
        BOTH,     // Lowpass filter mode
        VCA,      // Voltage-controlled amplifier mode
        LOWPASS   // Resonant lowpass mode
    };

    BuchlaLPG();
    void process(float& sample) override;
    void reset() override;
    void setParameter(const std::string& name, float value) override;
    float getParameter(const std::string& name) const;

private:
    void updateVactrol();
    void updateCoefficients();
    bool isStable() const;
    void clampStates();

    // Filter states
    float s_d = 0.0f, s_x = 0.0f, s_o = 0.0f;

    // Component values
    float C1 = 1e-9f;    // 1nF
    float C2 = 220e-12f; // 220pF
    float C3 = 0.0f;     // Initially disabled
    float Ra = 5e6f;     // 5MΩ (Both/Lowpass mode)

    // Control parameters
    float cv = 0.0f;           // Control voltage [0,1]
    float resonance = 0.0f;    // Normalized resonance [0,1]
    Mode mode = BOTH;

    // Vactrol model
    float currentIf = 10e-6f;  // LED current (10μA to 40mA)
    float targetIf = 10e-6f;
    const float minIf = 10e-6f;
    const float maxIf = 40e-3f;
    const float attackTime = 0.012f; // 12ms
    const float decayTime = 0.25f;   // 250ms
    float coeffAttack = 0.0f;
    float coeffDecay = 0.0f;

    // Constants
    const float A = 3.464f;     // Ω·A^1.4
    const float B = 1136.212f;  // Ω
    const float Fs = static_cast<float>(SAMPLE_RATE);
    
    // Stability helpers
    int resetCounter = 0;  // Counter to skip processing after instability
};
