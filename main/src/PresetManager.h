#pragma once

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

struct WavetablePreset {
    float frequency;
    float amplitude;
    WT_WaveformType waveform;
};

struct FMSynthPreset {
    float frequency;
    float amplitude;
    float mod_ratio;
    float mod_index;
    float feedback;
    FM_Algorithm algorithm;
    FM_Waveform mod_waveform;
    FM_Waveform car_waveform;
    float master_attack;
    float master_decay;
    float master_sustain;
    float master_release;
    float mod_attack;
    float mod_decay;
    float mod_sustain;
    float mod_release;
    float car_attack;
    float car_decay;
    float car_sustain;
    float car_release;
};

struct KarplusStrongPreset {
    float frequency;
    float amplitude;
    float feedback;
};

struct GeneticSynthPreset {
    float frequency;
    float amplitude;
};

struct VocoderPreset {
    float q_factor;
    float attack_time;
    float release_time;
    float output_gain;
};

struct EnhancedLowPassFilterPreset {
    float cutoff;
    float resonance;
};

struct ChorusPreset {
    float rate;
    float depth;
    float delay;
    float mix;
    float feedback;
};

struct FlangerPreset {
    float rate;
    float depth;
    float delay;
    float mix;
    float feedback;
};

struct ReverbPreset {
    float room_size;
    float damping;
    float mix;
};

struct BuchlaLPGPreset {
    float cv;
    float resonance;
    float mode;
};

struct LimiterPreset {
    float threshold;
};

struct CompressorPreset {
    float threshold;
    float ratio;
    float attack;
    float release;
};

struct GranularPreset {
    float dry_wet;
    float density;
    float grain_size;
    float grain_variation;
    float playback_rate;
    float pitch_variation;
    float time_shift;
    float spray;
    GranularEffect::WindowType window_type;
};

struct Preset {
    WavetablePreset wavetable;
    FMSynthPreset fm;
    KarplusStrongPreset karplus;
    GeneticSynthPreset genetic;
    VocoderPreset vocoder;
    EnhancedLowPassFilterPreset lpf;
    ChorusPreset chorus;
    FlangerPreset flanger;
    ReverbPreset reverb;
    BuchlaLPGPreset lpg;
    LimiterPreset limiter;
    CompressorPreset compressor;
    GranularPreset granular;
};

class PresetManager {
public:
    PresetManager(WavetableSynth& wt, FMSynth& fm, KarplusStrongSynth& ks, GeneticSynth& gen, VocoderSynth& voc,
                  EnhancedLowPassFilter& lpf, ChorusEffect& cho, FlangerEffect& fl, ReverbEffect& rev,
                  BuchlaLPG& lpg, Limiter& lim, Compressor& comp, GranularEffect& gran);

    void savePreset(uint8_t index);
    void loadPreset(uint8_t index);
    void interpolatePresets(uint8_t index1, uint8_t index2, float factor);

private:
    void applyPreset(const Preset& preset);

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

    static const int MAX_PRESETS = 16;
    Preset presets[MAX_PRESETS];
};
