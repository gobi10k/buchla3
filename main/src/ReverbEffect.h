#pragma once

#include "AudioEffect.h"
#include "CombFilter.h"
#include "AllPassFilter.h"
#include "config.h" // For SAMPLE_RATE
#include <vector>
#include <Arduino.h> // For uint8_t, constrain

// ============================================================================
// Reverb Effect - Simulates acoustic spaces.
// Based on a Schroeder reverb model: Parallel comb filters -> Serial all-pass filters
// ============================================================================

// Define some typical prime-ish delay lengths in milliseconds for a natural sound
// These will be scaled by roomSize and SAMPLE_RATE
const float COMB_DELAYS_MS[] = {29.7f, 37.1f, 41.1f, 43.7f}; // ~20-50ms range
const int NUM_COMB_FILTERS = sizeof(COMB_DELAYS_MS) / sizeof(float);

const float ALLPASS_DELAYS_MS[] = {5.0f, 1.7f}; // ~1-10ms range (shorter than combs)
const float ALLPASS_GAINS[] = {0.7f, 0.7f}; // Typical all-pass gain
const int NUM_ALLPASS_FILTERS = sizeof(ALLPASS_DELAYS_MS) / sizeof(float);


class ReverbEffect : public AudioEffect {
private:
    std::vector<CombFilter> combFilters;
    std::vector<AllPassFilter> allPassFilters;

    float roomSize;
    float damping;
    float dryWetMix;

    float currentSampleRate;

    void configureFilters();

public:
    ReverbEffect(float initialRoomSize = 0.75f, float initialDamping = 0.5f, float initialMix = 0.3f);

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;

    void setRoomSize(float size);
    void setDamping(float damping);
    void setMix(float mix);

    float getRoomSize() const { return roomSize; }
    float getDamping() const { return damping; }
    float getMix() const { return dryWetMix; }
};
