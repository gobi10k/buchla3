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
#include "GeneticController.h"

namespace Presets {

struct Wavetable {
    float frequency;
    float amplitude;
    WavetableSynth::WaveformType waveform;
};

struct FMSynth {
    float frequency;
    float amplitude;
    float mod_ratio;
    float mod_index;
    float feedback;
    FMSynth::Algorithm algorithm;
    FMSynth::Waveform mod_waveform;
    FMSynth::Waveform car_waveform;
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

struct KarplusStrong {
    float frequency;
    float amplitude;
    float feedback;
};

struct Genetic {
    float frequency;
    float amplitude;
};

struct Vocoder {
    float q_factor;
    float attack_time;
    float release_time;
    float output_gain;
};

struct EnhancedLowPassFilter {
    float cutoff;
    float resonance;
};

struct Chorus {
    float rate;
    float depth;
    float delay;
    float mix;
    float feedback;
};

struct Flanger {
    float rate;
    float depth;
    float delay;
    float mix;
    float feedback;
};

struct Reverb {
    float room_size;
    float damping;
    float mix;
};

struct BuchlaLPG {
    float cv;
    float resonance;
    float mode;
};

struct Limiter {
    float threshold;
};

struct Compressor {
    float threshold;
    float ratio;
    float attack;
    float release;
};

struct Granular {
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
    Wavetable wavetable;
    FMSynth fm;
    KarplusStrong karplus;
    Genetic genetic;
    Vocoder vocoder;
    EnhancedLowPassFilter lpf;
    Chorus chorus;
    Flanger flanger;
    Reverb reverb;
    BuchlaLPG lpg;
    Limiter limiter;
    Compressor compressor;
    Granular granular;
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

} // namespace Presets
