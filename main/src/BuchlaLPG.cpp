#include "BuchlaLPG.h"

// Add stability check method
bool BuchlaLPG::isStable() const {
    return isfinite(s_d) && isfinite(s_x) && isfinite(s_o) &&
           fabsf(s_d) < 1e3f &&  // Much tighter bounds
           fabsf(s_x) < 1e3f &&
           fabsf(s_o) < 1e3f;
}

// Add state clamping method
void BuchlaLPG::clampStates() {
    s_d = constrain(s_d, -10.0f, 10.0f);  // Much tighter clamping
    s_x = constrain(s_x, -10.0f, 10.0f);
    s_o = constrain(s_o, -10.0f, 10.0f);
}

BuchlaLPG::BuchlaLPG()
    : Fs(static_cast<float>(SAMPLE_RATE)) {
    reset();
}

void BuchlaLPG::reset() {
    s_d = s_x = s_o = 0.0f;
    currentIf = minIf;
    targetIf = minIf;
    resetCounter = 0;

    // Calculate coefficients HERE after Fs is initialized
    float dt = 1.0f / Fs;
    coeffAttack = expf(-dt / attackTime);
    coeffDecay = expf(-dt / decayTime);
    
    // Pre-calculate some coefficients to avoid repeated calculations
    updateCoefficients();
}

void BuchlaLPG::updateVactrol() {
    // More robust vactrol update with rate limiting
    float diff = targetIf - currentIf;
    float maxChange = maxIf * 0.001f; // Limit rate of change
    
    if (fabsf(diff) > maxChange) {
        diff = copysignf(maxChange, diff);
    }
    
    if (targetIf > currentIf) {
        currentIf += diff * (1.0f - coeffAttack);
    } else {
        currentIf += diff * (1.0f - coeffDecay);
    }
    currentIf = constrain(currentIf, minIf, maxIf);
}

void BuchlaLPG::updateCoefficients() {
    // Update component values based on mode
    switch (mode) {
        case VCA:
            Ra = 5e3f;   // 5kΩ for VCA mode
            C3 = 0.0f;   // No feedback capacitor
            break;

        case LOWPASS:
            Ra = 5e6f;   // 5MΩ
            C3 = 4.7e-9f; // 4.7nF
            break;

        case BOTH:
        default:
            Ra = 5e6f;   // 5MΩ
            C3 = 0.0f;   // No feedback capacitor
            break;
    }
}

void BuchlaLPG::process(float& sample) {
    if (!enabled) return;

    // Rate limit instability resets
    if (resetCounter > 0) {
        resetCounter--;
        sample = 0.0f;
        return;
    }

    updateVactrol();

    // Calculate Rf with safer constraints and bounds checking
    float currentIf_safe = constrain(currentIf, minIf * 10.0f, maxIf * 0.1f);
    
    // Use lookup table or approximation instead of pow() for better performance
    float pow_term = currentIf_safe / minIf; // Simple linear approximation
    float Rf = A / (pow_term * 1.4f) + B;
    Rf = constrain(Rf, 1e3f, 1e6f);  // Much tighter bounds: 1kΩ to 1MΩ

    // Calculate coefficients with better bounds checking
    float a1 = 1.0f / (C1 * Rf);
    float a2 = -1.0f / C1 * (1.0f/Rf + 1.0f/Ra);
    float b1 = 1.0f / (C2 * Rf);
    float b2 = -2.0f / (C2 * Rf);
    float b3 = -1.0f / (C2 * Rf);
    float b4 = C3 / C2;

    // Calculate resonance (a_max from Eq. 11) with safety checks
    float a_max = (C3 > 1e-12f) ?
        (2*C1*Ra + (C2+C3)*(Ra + Rf)) / (C3 * Ra) :
        1.0f;
    a_max = constrain(a_max, 0.1f, 10.0f); // Reasonable bounds
    
    float d1 = resonance * a_max;
    float d2 = -1.0f;

    // Solve linear system with much better numerical stability
    float denom = a1*b3 - a2*b2 - 2*a1*b4*d1*Fs + 2*a2*b4*d2*Fs;

    // More robust denom handling
    if (fabsf(denom) < 1e-6f || !isfinite(denom)) {
        // Instead of continuing with unstable math, bypass processing
        sample *= 0.9f; // Simple decay
        return;
    }

    float y_x = (a2*b4*s_d + 2*Fs*(b3 - 2*b4*d1*Fs)*s_o -
                2*a2*Fs*s_x - a2*b1*sample) / denom;
    
    // Immediate stability check
    if (!isfinite(y_x) || fabsf(y_x) > 100.0f) {
        sample *= 0.9f;
        return;
    }

    float denom_o = 1.0f - a2/(2*Fs);
    if (fabsf(denom_o) < 1e-6f) {
        sample *= 0.9f;
        return;
    }
    
    float y_o = (s_o + (a1/(2*Fs)) * y_x) / denom_o;
    
    if (!isfinite(y_o) || fabsf(y_o) > 100.0f) {
        sample *= 0.9f;
        return;
    }

    float y_d = s_d + 2*Fs*(d1*y_o + d2*y_x);

    // Update states (Eqs. 17-19) with immediate bounds checking
    float s_d_next = -s_d - 4*Fs*(d1*y_o + d2*y_x);
    float s_x_next = s_x + (1.0f/Fs)*(b1*sample + b2*y_x + b3*y_o + b4*y_d);
    float s_o_next = s_o + (1.0f/Fs)*(a1*y_x + a2*y_o);

    // Tight bounds checking before updating states
    if (!isfinite(s_d_next) || fabsf(s_d_next) > 10.0f ||
        !isfinite(s_x_next) || fabsf(s_x_next) > 10.0f ||
        !isfinite(s_o_next) || fabsf(s_o_next) > 10.0f) {
        
        // Don't call reset() in audio callback - just decay states
        s_d *= 0.99f;
        s_x *= 0.99f;
        s_o *= 0.99f;
        sample *= 0.9f;
        resetCounter = 100; // Skip processing for 100 samples
        return;
    }

    // Update states with tight clamping
    s_d = constrain(s_d_next, -10.0f, 10.0f);
    s_x = constrain(s_x_next, -10.0f, 10.0f);
    s_o = constrain(s_o_next, -10.0f, 10.0f);

    // Final output bounds check
    sample = constrain(y_o, -10.0f, 10.0f);
}

void BuchlaLPG::setCv(float c) {
    cv = constrain(c, 0.0f, 1.0f);
    targetIf = minIf + cv * (maxIf - minIf);
}

void BuchlaLPG::setResonance(float r) {
    resonance = constrain(r, 0.0f, 0.95f);
}

void BuchlaLPG::setMode(Mode m) {
    mode = m;
    updateCoefficients();
}

void BuchlaLPG::setParameter(const std::string& name, float value) {
    if (name == "cv") {
        setCv(value);
    }
    else if (name == "resonance") {
        setResonance(value);
    }
    else if (name == "mode") {
        setMode(static_cast<Mode>(constrain(static_cast<int>(value), 0, 2)));
    }
    else if (name == "enabled") {
        enabled = (value > 0.5f);
        if (enabled) {
            s_d *= 0.1f;
            s_x *= 0.1f;
            s_o *= 0.1f;
        }
    }
}

float BuchlaLPG::getParameter(const std::string& name) const {
    if (name == "cv") {
        return cv;
    }
    else if (name == "resonance") {
        return resonance;
    }
    else if (name == "mode") {
        return static_cast<float>(mode);
    }
    return 0.0f;
}
