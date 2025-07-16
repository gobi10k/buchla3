#include "RoutingManager.h"

RoutingManager::RoutingManager(DCBlocker& dc, AntiAliasFilter& aa, GranularEffect& gran, EnhancedLowPassFilter& lpf,
                               ChorusEffect& cho, FlangerEffect& fl, ReverbEffect& rev, BuchlaLPG& lpg,
                               Compressor& comp, Limiter& lim)
    : dcBlocker(dc), antiAliasFilter(aa), granularEffect(gran), lowpass(lpf),
      chorusEffect(cho), flangerEffect(fl), reverbEffect(rev), buchlaLPG(lpg),
      compressor(comp), limiter(lim), currentPreset(ROUTING_PRESET_1)
{
    // Preset 1: Standard
    routing[ROUTING_PRESET_1][0] = &dcBlocker;
    routing[ROUTING_PRESET_1][1] = &antiAliasFilter;
    routing[ROUTING_PRESET_1][2] = &granularEffect;
    routing[ROUTING_PRESET_1][3] = &lowpass;
    routing[ROUTING_PRESET_1][4] = &chorusEffect;
    routing[ROUTING_PRESET_1][5] = &flangerEffect;
    routing[ROUTING_PRESET_1][6] = &reverbEffect;
    routing[ROUTING_PRESET_1][7] = &buchlaLPG;
    routing[ROUTING_PRESET_1][8] = &compressor;
    routing[ROUTING_PRESET_1][9] = &limiter;
    routingLengths[ROUTING_PRESET_1] = 10;

    // Preset 2: LPF last
    routing[ROUTING_PRESET_2][0] = &dcBlocker;
    routing[ROUTING_PRESET_2][1] = &antiAliasFilter;
    routing[ROUTING_PRESET_2][2] = &granularEffect;
    routing[ROUTING_PRESET_2][3] = &chorusEffect;
    routing[ROUTING_PRESET_2][4] = &flangerEffect;
    routing[ROUTING_PRESET_2][5] = &reverbEffect;
    routing[ROUTING_PRESET_2][6] = &buchlaLPG;
    routing[ROUTING_PRESET_2][7] = &compressor;
    routing[ROUTING_PRESET_2][8] = &limiter;
    routing[ROUTING_PRESET_2][9] = &lowpass;
    routingLengths[ROUTING_PRESET_2] = 10;

    // Preset 3: Reverb first
    routing[ROUTING_PRESET_3][0] = &dcBlocker;
    routing[ROUTING_PRESET_3][1] = &antiAliasFilter;
    routing[ROUTING_PRESET_3][2] = &reverbEffect;
    routing[ROUTING_PRESET_3][3] = &granularEffect;
    routing[ROUTING_PRESET_3][4] = &lowpass;
    routing[ROUTING_PRESET_3][5] = &chorusEffect;
    routing[ROUTING_PRESET_3][6] = &flangerEffect;
    routing[ROUTING_PRESET_3][7] = &buchlaLPG;
    routing[ROUTING_PRESET_3][8] = &compressor;
    routing[ROUTING_PRESET_3][9] = &limiter;
    routingLengths[ROUTING_PRESET_3] = 10;
}

void RoutingManager::setRoutingPreset(RoutingPreset preset) {
    if (preset < NUM_ROUTING_PRESETS) {
        currentPreset = preset;
    }
}

void RoutingManager::applyRouting(AudioEffect** effects, size_t& effectCount) {
    effectCount = routingLengths[currentPreset];
    for (size_t i = 0; i < effectCount; ++i) {
        effects[i] = routing[currentPreset][i];
    }
}
