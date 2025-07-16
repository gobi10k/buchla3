#include "KarplusStrongSynth.h"
#include "config.h" // For SAMPLE_RATE, DEFAULT_SYNTH_FREQUENCY
#include <Arduino.h> // For Serial, constrain, strcmp, random (if esp_random not available/preferred)
#include <cmath>     // For fabsf
#include "esp_random.h" // For esp_random()

KarplusStrongSynth::KarplusStrongSynth(float sampleRate)
    : m_sampleRate(sampleRate),
      m_buffer(nullptr),
      m_buffer_size(0),
      m_delay_length(static_cast<int>(sampleRate / DEFAULT_SYNTH_FREQUENCY + 0.5f)),
      m_write_index(0),
      m_feedback(0.996f),
      m_current_pluck_strength(1.0f),
      m_active(false) {
    
    m_buffer_size = static_cast<int>(m_sampleRate / 20.0f) + 2; // Max buffer for 20Hz + safety
    m_buffer = new float[m_buffer_size];
    if (m_buffer) {
        for (int i = 0; i < m_buffer_size; ++i) {
            m_buffer[i] = 0.0f;
        }
    } else {
        m_buffer_size = 0;
    }
    if (m_delay_length >= m_buffer_size && m_buffer_size > 1) m_delay_length = m_buffer_size -1;
    else if (m_delay_length < 2) m_delay_length = 2;
}

KarplusStrongSynth::~KarplusStrongSynth() {
    delete[] m_buffer;
}

void KarplusStrongSynth::reset() {
    if (m_buffer) {
        for (int i = 0; i < m_buffer_size; ++i) m_buffer[i] = 0.0f;
    }
    m_write_index = 0;
    m_active = false;
}

void KarplusStrongSynth::generateSample(float& sample) {
    if (!m_active || !m_buffer || m_delay_length < 2) {
        sample = 0.0f;
    } else {
        int readIndex = (m_write_index - m_delay_length + m_buffer_size) % m_buffer_size;
        float current_sample_val = m_buffer[readIndex];
        int prevFeedbackSampleReadIndex = (readIndex - 1 + m_buffer_size) % m_buffer_size;
        float feedback_signal = (m_buffer[readIndex] + m_buffer[prevFeedbackSampleReadIndex]) * 0.5f;
        m_buffer[m_write_index] = feedback_signal * m_feedback;
        m_write_index = (m_write_index + 1) % m_buffer_size;
        sample = constrain(current_sample_val * m_current_pluck_strength, -1.0f, 1.0f);
        if (fabsf(sample) < 0.0001f) m_active = false;
    }
}

void KarplusStrongSynth::noteOn(float frequency, float velocity) {
    setFrequency(frequency);
    float pluckStrength = 0.1f + velocity * 1.4f;
    pluck(pluckStrength);
    m_active = true;
}

void KarplusStrongSynth::noteOff() {
    m_active = false;
}

void KarplusStrongSynth::setParameter(const std::string& name, float value) {
    if (name == "frequency") {
        setFrequency(value);
    } else if (name == "feedback") {
        setFeedback(value);
    } else if (name == "amplitude") {
        setAmplitude(value); // This controls pluck strength
    }
}

void KarplusStrongSynth::setFrequency(float freq) {
    if (freq < 20.0f) freq = 20.0f;
    if (freq > m_sampleRate / 2.0f) freq = m_sampleRate / 2.0f;

    m_delay_length = static_cast<int>(m_sampleRate / freq + 0.5f);
    if (m_delay_length < 2) m_delay_length = 2;
    if (m_delay_length >= m_buffer_size && m_buffer_size > 1) {
        m_delay_length = m_buffer_size -1;
    }
}

float KarplusStrongSynth::getFrequency() const {
    if (m_delay_length == 0) return DEFAULT_SYNTH_FREQUENCY; // Avoid division by zero
    return m_sampleRate / static_cast<float>(m_delay_length);
}

void KarplusStrongSynth::setAmplitude(float amp) {
    m_current_pluck_strength = constrain(amp, 0.01f, 2.0f);
}

float KarplusStrongSynth::getAmplitude() const {
    return m_current_pluck_strength;
}

void KarplusStrongSynth::setFeedback(float fb) {
    m_feedback = constrain(fb, 0.0f, 1.0f);
}

void KarplusStrongSynth::pluck(float strength) {
    if (!m_buffer || m_buffer_size == 0 || m_delay_length < 2) return;

    m_current_pluck_strength = constrain(strength, 0.01f, 2.0f);

    for (int i = 0; i < m_delay_length; i++) {
        m_buffer[i] = (2.0f * (static_cast<float>(esp_random()) / UINT32_MAX) - 1.0f);
    }
    m_write_index = 0;
    m_active = true;
}

// getCurrentFrequency() is now getFrequency()
// getCurrentFeedback() is in header
// getCurrentDelayLength() is in header
