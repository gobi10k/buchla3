#pragma once

#include <string>

#include <Arduino.h> // For uint8_t

// ============================================================================
// Audio Effect Interface - Base class for all audio effects
// ============================================================================
class AudioEffect {
public:
    virtual ~AudioEffect() = default;
    // Process a mono sample. Effects can modify the sample in place.
    virtual void process(float& sample) = 0;
    virtual void reset() {}
    virtual void setParameter(const std::string& name, float value) {}
    virtual float getParameter(const std::string& name) const { return 0.0f; }
    virtual bool isEnabled() const { return enabled; }
    virtual void setEnabled(bool state) { enabled = state; }

protected:
    bool enabled = true;
};
