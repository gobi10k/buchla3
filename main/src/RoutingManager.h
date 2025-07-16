#pragma once

#include "AudioEffect.h"
#include "EnhancedLowPassFilter.h"
#include "ChorusEffect.h"
#include "FlangerEffect.h"
#include "ReverbEffect.h"
#include "BuchlaLPG.h"
#include "Limiter.h"
#include "Compressor.h"
#include "GranularEffect.h"
#include "DCBlocker.h"
#include "AntiAliasFilter.h"
#include "config.h"

class RoutingManager {
public:
    enum RoutingPreset {
        ROUTING_PRESET_1, // Standard
        ROUTING_PRESET_2, // LPF last
        ROUTING_PRESET_3, // Reverb first
        NUM_ROUTING_PRESETS
    };

    RoutingManager(DCBlocker& dc, AntiAliasFilter& aa, GranularEffect& gran, EnhancedLowPassFilter& lpf,
                   ChorusEffect& cho, FlangerEffect& fl, ReverbEffect& rev, BuchlaLPG& lpg,
                   Compressor& comp, Limiter& lim);

    void setRoutingPreset(RoutingPreset preset);
    void applyRouting(AudioEffect** effects, size_t& effectCount);

private:
    DCBlocker& dcBlocker;
    AntiAliasFilter& antiAliasFilter;
    GranularEffect& granularEffect;
    EnhancedLowPassFilter& lowpass;
    ChorusEffect& chorusEffect;
    FlangerEffect& flangerEffect;
    ReverbEffect& reverbEffect;
    BuchlaLPG& buchlaLPG;
    Compressor& compressor;
    Limiter& limiter;

    AudioEffect* routing[NUM_ROUTING_PRESETS][MAX_EFFECTS];
    size_t routingLengths[NUM_ROUTING_PRESETS];

    RoutingPreset currentPreset;
};
