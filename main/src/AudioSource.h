#pragma once

#include <string>
#include <Arduino.h> // For uint8_t

// Enum to identify different synth types without RTTI
enum SynthType {
    SYNTH_TYPE_UNKNOWN = 0,
    SYNTH_TYPE_WAVETABLE,
    SYNTH_TYPE_FM,
    SYNTH_TYPE_KARPLUS_STRONG,
    SYNTH_TYPE_GENETIC
};

// ============================================================================
// Audio Source Interface - Base class for all audio sources
// ============================================================================
class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual void generateSample(float& sample) = 0;
    virtual void reset() {}
    virtual bool isActive() const { return true; } // Default to active

    // Generic parameter setting
    virtual void setParameter(const std::string& name, float value) {}
    // Generic note on/off, frequency, and amplitude controls
    // Derived classes should override these if they support them.
    virtual void noteOn(float frequency, float velocity = 1.0f) {}
    virtual void noteOff() {}
    virtual void setFrequency(float freq) {}
    virtual float getFrequency() const { return 0.0f; } // Default implementation
    virtual void setAmplitude(float amp) {} // Typically 0-127 for 8-bit context, or 0-1.0 float
    virtual float getAmplitude() const { return 0.0f; } // Default implementation

    virtual SynthType getSynthType() const { return SYNTH_TYPE_UNKNOWN; }
};
