#pragma once

// Forward declare classes to avoid circular dependencies if they include globals.h
// Or ensure full class definitions are included before this if globals.h needs their types fully.
// For now, forward declarations should be sufficient for pointer/reference externs.
class WavetableSynth;
class FMSynth;
class ADSREnvelope;
class GranularEffect;
class EnhancedLowPassFilter;
class AudioEngine;
class KarplusStrongSynth;
class GeneticSynth;
class GeneticController;
class FlangerEffect; // New
class ReverbEffect;  // New
class VocoderSynth;  // New

// Enum definition needs to be here if it's used by extern variable.
enum SynthMode {
    WAVETABLE_MODE = 0,
    FM_MODE = 1,
    KARPLUS_STRONG_MODE = 2,
    GENETIC_MODE = 3,
    VOCODER_MODE = 4, // New
    NUM_SYNTH_MODES = 5 // Updated
};

// Declare global audio component instances as extern
// These will be defined in main.cpp
extern AudioEngine audioEngine;
extern WavetableSynth wavetableSynth;
extern FMSynth fmSynth;
extern ADSREnvelope adsrEnvelope;
extern GranularEffect granularEffect;
extern EnhancedLowPassFilter lowpass; // Kept original name 'lowpass'
extern KarplusStrongSynth karplusSynth;
extern GeneticSynth geneticSynth;
extern GeneticController geneticController;
extern FlangerEffect flangerEffect;   // New
extern ReverbEffect reverbEffect;     // New
extern VocoderSynth vocoderSynth;     // New

extern SynthMode currentSynthMode;
