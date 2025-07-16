#pragma once
#include "AudioSource.h"
#include "config.h" // For SAMPLE_RATE, TWO_PI, DEFAULT_SYNTH_FREQUENCY
#include <cmath>   // For fabsf
#include <cstdlib> // For random or esp_random if not in Arduino.h for this target

class KarplusStrongSynth : public AudioSource {
public:
    explicit KarplusStrongSynth(float sampleRate = SAMPLE_RATE);
    ~KarplusStrongSynth() override;

    // AudioSource overrides
    void generateSample(float& sample) override;
    void reset() override;
    bool isActive() const override { return m_active; }
    void setParameter(const std::string& name, float value) override;
    SynthType getSynthType() const override { return SYNTH_TYPE_KARPLUS_STRONG; }

    void noteOn(float frequency, float velocity = 1.0f) override;
    void noteOff() override;

    void setFrequency(float freq) override;
    float getFrequency() const override;
    void setAmplitude(float amp) override; // Expects 0.0 to 2.0+ for pluck strength
    float getAmplitude() const override;   // Returns current pluck strength/amplitude scaling

    // Specific methods
    void pluck(float strength = 1.0f);
    
    // Getters for status display (can be specific or use generic getFrequency etc.)
    float getCurrentFeedback() const { return m_feedback; }
    void setFeedback(float fb); // Specific setter for feedback
    int getCurrentDelayLength() const { return m_delay_length; }


private:
    const float m_sampleRate;
    float* m_buffer;
    int m_buffer_size;
    int m_delay_length;
    int m_write_index;
    float m_feedback;
    float m_current_pluck_strength;
    bool m_active;

    // initializeBuffer is effectively done in constructor/reset/pluck
};
