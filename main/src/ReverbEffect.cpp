#include "ReverbEffect.h"
#include "config.h"  // For SAMPLE_RATE
#include <Arduino.h> // For constrain, etc.
#include <cmath>     // For roundf
#include <string>
#include <string>
#include <string>

// Assuming SAMPLE_RATE is defined in config.h
#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100 // Default if not defined
#warning "SAMPLE_RATE not defined in config.h, using default 44100 for ReverbEffect"
#endif

// Define fixed feedback for comb filters (can be tuned)
const float COMB_FEEDBACK_BASE = 0.84f;
// Damping will reduce this feedback more for higher damping values.

ReverbEffect::ReverbEffect(float initialRoomSize, float initialDamping, float initialMix)
    : roomSize(constrain(initialRoomSize, 0.0f, 1.0f)),
      damping(constrain(initialDamping, 0.0f, 1.0f)),
      dryWetMix(constrain(initialMix, 0.0f, 1.0f)),
      currentSampleRate(static_cast<float>(SAMPLE_RATE))
{
    // Initialize Comb Filters
    combFilters.reserve(NUM_COMB_FILTERS);
    for (int i = 0; i < NUM_COMB_FILTERS; ++i) {
        combFilters.emplace_back(1, 0.0f, 0.0f); // Placeholder maxDelay, actual set in configure
    }

    // Initialize All-Pass Filters
    allPassFilters.reserve(NUM_ALLPASS_FILTERS);
    for (int i = 0; i < NUM_ALLPASS_FILTERS; ++i) {
        allPassFilters.emplace_back(1, 0.0f, ALLPASS_GAINS[i]);// Placeholder maxDelay
    }

    configureFilters(); // Apply initial parameters
    reset();
}

void ReverbEffect::configureFilters() {
    // Configure Comb Filters
    for (int i = 0; i < NUM_COMB_FILTERS; ++i) {
        float delayMs = COMB_DELAYS_MS[i] * (0.25f + roomSize * 0.75f); // Scale delay by roomSize (min 25% of base)
        int delaySamples = static_cast<int>((delayMs / 1000.0f) * currentSampleRate);
        if (delaySamples < 16) delaySamples = 16; // Minimum practical delay

        // Max delay should accommodate the largest possible delay for this filter
        float maxDelayMs = COMB_DELAYS_MS[i] * 1.0f; // Max at roomSize = 1.0
        int maxDelaySamples = static_cast<int>((maxDelayMs / 1000.0f) * currentSampleRate) + 16;
        if (maxDelaySamples < 32) maxDelaySamples = 32;

        combFilters[i].setMaxDelay(maxDelaySamples);
        combFilters[i].setDelay(static_cast<float>(delaySamples));

        // Damping effect: lower feedback for higher frequencies (simulated by reducing overall feedback)
        // Higher damping value means more reduction in feedback.
        float feedback = COMB_FEEDBACK_BASE * (1.0f - (damping * 0.25f)); // Damping reduces feedback up to 25%
        feedback *= (0.5f + roomSize * 0.5f); // Room size also affects feedback (decay time)
        combFilters[i].setFeedback(constrain(feedback, 0.1f, 0.99f));
        combFilters[i].setFeedforward(0.0f); // Standard IIR comb for reverb
    }

    // Configure All-Pass Filters
    for (int i = 0; i < NUM_ALLPASS_FILTERS; ++i) {
        // All-pass delays are often less dramatically scaled by roomSize, or fixed
        float delayMs = ALLPASS_DELAYS_MS[i] * (0.5f + roomSize * 0.5f);
        int delaySamples = static_cast<int>((delayMs / 1000.0f) * currentSampleRate);
        if (delaySamples < 2) delaySamples = 2; // Minimum for all-pass

        float maxDelayMs = ALLPASS_DELAYS_MS[i] * 1.0f;
        int maxDelaySamples = static_cast<int>((maxDelayMs / 1000.0f) * currentSampleRate) + 16;
        if (maxDelaySamples < 32) maxDelaySamples = 32;

        allPassFilters[i].setMaxDelay(maxDelaySamples);
        allPassFilters[i].setDelay(static_cast<float>(delaySamples));
        allPassFilters[i].setGain(ALLPASS_GAINS[i] * (0.75f + roomSize * 0.25f)); // Roomsize can slightly affect allpass character
    }
}


void ReverbEffect::process(float& sample) {
    if (!enabled || dryWetMix == 0.0f) return;

    float wetSignal = 0.0f;

    for (size_t i = 0; i < combFilters.size(); ++i) {
        wetSignal += combFilters[i].process(sample);
    }
    if (!combFilters.empty()) {
        wetSignal *= (1.0f / sqrtf(static_cast<float>(combFilters.size())));
    }

    for (size_t i = 0; i < allPassFilters.size(); ++i) {
        wetSignal = allPassFilters[i].process(wetSignal);
    }

    wetSignal = constrain(wetSignal, -1.0f, 1.0f);

    sample = (sample * (1.0f - dryWetMix)) + (wetSignal * dryWetMix);
}

void ReverbEffect::setParameter(const std::string& name, float value) {
    bool needsReconfig = false;
    if (name == "roomSize") {
        roomSize = constrain(value, 0.0f, 1.0f);
        needsReconfig = true;
    } else if (name == "damping") {
        damping = constrain(value, 0.0f, 1.0f);
        needsReconfig = true;
    } else if (name == "mix") {
        dryWetMix = constrain(value, 0.0f, 1.0f);
    } else if (name == "enabled") {
        enabled = (value > 0.5f);
    }

    if (needsReconfig) {
        configureFilters();
    }
}

void ReverbEffect::reset() {
    for (size_t i = 0; i < combFilters.size(); ++i) {
        combFilters[i].clear();
    }
    for (size_t i = 0; i < allPassFilters.size(); ++i) {
        allPassFilters[i].clear();
    }
}
