#pragma once

#include "AudioEngine.h"
#include "WavetableSynth.h"
#include "FMSynth.h"
#include "KarplusStrongSynth.h"
#include "GeneticSynth.h"
#include "VocoderSynth.h"
#include "EnhancedLowPassFilter.h"
#include "ChorusEffect.h"
#include "FlangerEffect.h"
#include "ReverbEffect.h"
#include "BuchlaLPG.h"
#include "Limiter.h"
#include "Compressor.h"
#include "GranularEffect.h"
#include "GeneticController.h"
#include "DCBlocker.h"
#include "AntiAliasFilter.h"
#include "PresetManager.h"
#include "RoutingManager.h"
#include "globals.h"

class CommandHandler {
public:
    CommandHandler(AudioEngine& engine,
                   WavetableSynth& wavetable,
                   FMSynth& fm,
                   KarplusStrongSynth& karplus,
                   GeneticSynth& genetic,
                   VocoderSynth& vocoder,
                   EnhancedLowPassFilter& lpf,
                   ChorusEffect& chorus,
                   FlangerEffect& flanger,
                   ReverbEffect& reverb,
                   BuchlaLPG& lpg,
                   Limiter& limiter,
                   Compressor& compressor,
                   GranularEffect& granular,
                   GeneticController& geneticController,
                   DCBlocker& dcBlocker,
                   AntiAliasFilter& antiAlias,
                   Presets::PresetManager& presetManager,
                   RoutingManager& routingManager);

    void handleCommand(char* command);

private:
    void printHelp();
    void printDetailedStatus();
    void testDACDirect();
    AudioSource* getAudioSourceForMode(SynthMode mode);
    SynthMode getSynthModeFromPointer(AudioSource* src);
    const char* getSynthModeName(SynthMode mode);

    AudioEngine& audioEngine;
    WavetableSynth& wavetableSynth;
    FMSynth& fmSynth;
    KarplusStrongSynth& karplusSynth;
    GeneticSynth& geneticSynth;
    VocoderSynth& vocoderSynth;
    EnhancedLowPassFilter& lowpass;
    ChorusEffect& chorusEffect;
    FlangerEffect& flangerEffect;
    ReverbEffect& reverbEffect;
    BuchlaLPG& buchlaLPG;
    Limiter& limiter;
    Compressor& compressor;
    GranularEffect& granularEffect;
    GeneticController& geneticController;
    DCBlocker& dcBlocker;
    AntiAliasFilter& antiAliasFilter;
    Presets::PresetManager& presetManager;
    RoutingManager& routingManager;

    SynthMode currentSynthMode = WAVETABLE_MODE;
};
