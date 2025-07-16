#include "PresetManager.h"

namespace Presets {

// Helper for linear interpolation
float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

PresetManager::PresetManager(WavetableSynth& wt, FMSynth& fm, KarplusStrongSynth& ks, GeneticSynth& gen, VocoderSynth& voc,
                             EnhancedLowPassFilter& lpf, ChorusEffect& cho, FlangerEffect& fl, ReverbEffect& rev,
                             BuchlaLPG& lpg, Limiter& lim, Compressor& comp, GranularEffect& gran)
    : wavetableSynth(wt), fmSynth(fm), karplusSynth(ks), geneticSynth(gen), vocoderSynth(voc),
      lowpass(lpf), chorusEffect(cho), flangerEffect(fl), reverbEffect(rev),
      buchlaLPG(lpg), limiter(lim), compressor(comp), granularEffect(gran)
{
    // Initialize default presets
    for (int i = 0; i < MAX_PRESETS; ++i) {
        savePreset(i); // Save the current state as a default preset
    }

    // Create some example presets
    // Preset 0: Default state (already saved)

    // Preset 1: Bright Saw with Reverb
    presets[1].wavetable.waveform = WavetableSynth::SAW;
    presets[1].lpf.cutoff = 8000.0f;
    presets[1].lpf.resonance = 0.2f;
    presets[1].reverb.room_size = 0.9f;
    presets[1].reverb.damping = 0.5f;
    presets[1].reverb.mix = 0.4f;

    // Preset 2: FM Bells
    presets[2].fm.algorithm = FMSynth::Algorithm4;
    presets[2].fm.mod_ratio = 3.5f;
    presets[2].fm.mod_index = 15.0f;
    presets[2].fm.mod_decay = 0.5f;
    presets[2].fm.car_decay = 1.0f;

    // Preset 3: Plucky Karplus-Strong
    presets[3].karplus.feedback = 0.99f;
    presets[3].reverb.mix = 0.2f;

    // Preset 4: Granular Cloud
    presets[4].granular.dry_wet = 0.8f;
    presets[4].granular.density = 50.0f;
    presets[4].granular.grain_size = 1024;
    presets[4].granular.pitch_variation = 0.1f;
    presets[4].reverb.mix = 0.5f;
}

void PresetManager::savePreset(uint8_t index) {
    if (index >= MAX_PRESETS) return;

    // --- Synths ---
    presets[index].wavetable.frequency = wavetableSynth.getFrequency();
    presets[index].wavetable.amplitude = wavetableSynth.getAmplitude();
    presets[index].wavetable.waveform = wavetableSynth.getWaveformType();

    presets[index].fm.frequency = fmSynth.getFrequency();
    presets[index].fm.amplitude = fmSynth.getAmplitude();
    presets[index].fm.mod_ratio = fmSynth.getModulatorRatio();
    presets[index].fm.mod_index = fmSynth.getModulationIndex();
    presets[index].fm.feedback = fmSynth.getFeedback();
    presets[index].fm.algorithm = fmSynth.getAlgorithm();
    presets[index].fm.mod_waveform = fmSynth.getModulatorWaveform();
    presets[index].fm.car_waveform = fmSynth.getCarrierWaveform();
    // ... (rest of FM params)

    presets[index].karplus.frequency = karplusSynth.getFrequency();
    presets[index].karplus.amplitude = karplusSynth.getAmplitude();
    presets[index].karplus.feedback = karplusSynth.getCurrentFeedback();

    presets[index].genetic.frequency = geneticSynth.getFrequency();
    presets[index].genetic.amplitude = geneticSynth.getAmplitude();

    presets[index].vocoder.q_factor = vocoderSynth.getQFactor();
    presets[index].vocoder.attack_time = vocoderSynth.getAttackTime();
    presets[index].vocoder.release_time = vocoderSynth.getReleaseTime();
    presets[index].vocoder.output_gain = vocoderSynth.getOutputGain();

    // --- Effects ---
    presets[index].lpf.cutoff = lowpass.getCutoff();
    presets[index].lpf.resonance = lowpass.getResonance();

    presets[index].chorus.rate = chorusEffect.getRate();
    presets[index].chorus.depth = chorusEffect.getDepth();
    presets[index].chorus.delay = chorusEffect.getBaseDelay();
    presets[index].chorus.mix = chorusEffect.getMix();
    presets[index].chorus.feedback = chorusEffect.getFeedback();

    presets[index].flanger.rate = flangerEffect.getRate();
    presets[index].flanger.depth = flangerEffect.getDepth();
    presets[index].flanger.delay = flangerEffect.getBaseDelay();
    presets[index].flanger.mix = flangerEffect.getMix();
    presets[index].flanger.feedback = flangerEffect.getFeedback();

    presets[index].reverb.room_size = reverbEffect.getRoomSize();
    presets[index].reverb.damping = reverbEffect.getDamping();
    presets[index].reverb.mix = reverbEffect.getMix();

    presets[index].buchlaLPG.cv = buchlaLPG.getParameter("cv");
    presets[index].buchlaLPG.resonance = buchlaLPG.getParameter("resonance");
    presets[index].buchlaLPG.mode = buchlaLPG.getParameter("mode");

    presets[index].limiter.threshold = limiter.getParameter("threshold");

    presets[index].compressor.threshold = compressor.getParameter("threshold");
    presets[index].compressor.ratio = compressor.getParameter("ratio");
    presets[index].compressor.attack = compressor.getParameter("attack");
    presets[index].compressor.release = compressor.getParameter("release");

    presets[index].granular.dry_wet = granularEffect.getDryWetMix();
    presets[index].granular.density = granularEffect.getGrainDensity();
    presets[index].granular.grain_size = granularEffect.getBaseGrainSize();
    presets[index].granular.grain_variation = granularEffect.getGrainVariation();
    presets[index].granular.playback_rate = granularEffect.getPlaybackRate();
    presets[index].granular.pitch_variation = granularEffect.getPitchVariation();
    presets[index].granular.time_shift = granularEffect.getTimeShift();
    presets[index].granular.spray = granularEffect.getSpray();
    presets[index].granular.window_type = granularEffect.getWindowType();
}

void PresetManager::loadPreset(uint8_t index) {
    if (index >= MAX_PRESETS) return;
    applyPreset(presets[index]);
}

void PresetManager::interpolatePresets(uint8_t index1, uint8_t index2, float factor) {
    if (index1 >= MAX_PRESETS || index2 >= MAX_PRESETS) return;

    factor = constrain(factor, 0.0f, 1.0f);

    const Preset& p1 = presets[index1];
    const Preset& p2 = presets[index2];
    Preset result;

    // --- Synths ---
    result.wavetable.frequency = lerp(p1.wavetable.frequency, p2.wavetable.frequency, factor);
    result.wavetable.amplitude = lerp(p1.wavetable.amplitude, p2.wavetable.amplitude, factor);
    result.wavetable.waveform = factor < 0.5 ? p1.wavetable.waveform : p2.wavetable.waveform; // No interpolation for discrete values

    // ... (interpolate all other parameters)
    result.lpf.cutoff = lerp(p1.lpf.cutoff, p2.lpf.cutoff, factor);
    result.lpf.resonance = lerp(p1.lpf.resonance, p2.lpf.resonance, factor);

    result.reverb.room_size = lerp(p1.reverb.room_size, p2.reverb.room_size, factor);
    result.reverb.damping = lerp(p1.reverb.damping, p2.reverb.damping, factor);
    result.reverb.mix = lerp(p1.reverb.mix, p2.reverb.mix, factor);


    applyPreset(result);
}

void PresetManager::applyPreset(const Preset& preset) {
    // --- Synths ---
    wavetableSynth.setFrequency(preset.wavetable.frequency);
    wavetableSynth.setAmplitude(preset.wavetable.amplitude);
    wavetableSynth.setWaveform(preset.wavetable.waveform);

    fmSynth.setFrequency(preset.fm.frequency);
    fmSynth.setAmplitude(preset.fm.amplitude);
    fmSynth.setParameter("mod_ratio", preset.fm.mod_ratio);
    fmSynth.setParameter("mod_index", preset.fm.mod_index);
    fmSynth.setParameter("feedback", preset.fm.feedback);
    fmSynth.setAlgorithm(preset.fm.algorithm);
    fmSynth.setModulatorWaveform(preset.fm.mod_waveform);
    fmSynth.setCarrierWaveform(preset.fm.car_waveform);
    // ... (rest of FM params)

    karplusSynth.setFrequency(preset.karplus.frequency);
    karplusSynth.setAmplitude(preset.karplus.amplitude);
    karplusSynth.setFeedback(preset.karplus.feedback);

    geneticSynth.setFrequency(preset.genetic.frequency);
    geneticSynth.setAmplitude(preset.genetic.amplitude);

    vocoderSynth.setParameter("qFactor", preset.vocoder.q_factor);
    vocoderSynth.setParameter("attackTime", preset.vocoder.attack_time);
    vocoderSynth.setParameter("releaseTime", preset.vocoder.release_time);
    vocoderSynth.setParameter("outputGain", preset.vocoder.output_gain);

    // --- Effects ---
    lowpass.setParameter("cutoff", preset.lpf.cutoff);
    lowpass.setParameter("resonance", preset.lpf.resonance);

    chorusEffect.setParameter("rate", preset.chorus.rate);
    chorusEffect.setParameter("depth", preset.chorus.depth);
    chorusEffect.setParameter("delay", preset.chorus.delay);
    chorusEffect.setParameter("mix", preset.chorus.mix);
    chorusEffect.setParameter("feedback", preset.chorus.feedback);

    flangerEffect.setParameter("rate", preset.flanger.rate);
    flangerEffect.setParameter("depth", preset.flanger.depth);
    flangerEffect.setParameter("delay", preset.flanger.delay);
    flangerEffect.setParameter("mix", preset.flanger.mix);
    flangerEffect.setParameter("feedback", preset.flanger.feedback);

    reverbEffect.setParameter("roomSize", preset.reverb.room_size);
    reverbEffect.setParameter("damping", preset.reverb.damping);
    reverbEffect.setParameter("mix", preset.reverb.mix);

    buchlaLPG.setParameter("cv", preset.buchlaLPG.cv);
    buchlaLPG.setParameter("resonance", preset.buchlaLPG.resonance);
    buchlaLPG.setParameter("mode", preset.buchlaLPG.mode);

    limiter.setParameter("threshold", preset.limiter.threshold);

    compressor.setParameter("threshold", preset.compressor.threshold);
    compressor.setParameter("ratio", preset.compressor.ratio);
    compressor.setParameter("attack", preset.compressor.attack);
    compressor.setParameter("release", preset.compressor.release);

    granularEffect.setParameter("dry_wet", preset.granular.dry_wet);
    granularEffect.setParameter("density", preset.granular.density);
    granularEffect.setParameter("grain_size", preset.granular.grain_size);
    granularEffect.setParameter("grain_variation", preset.granular.grain_variation);
    granularEffect.setParameter("playback_rate", preset.granular.playback_rate);
    granularEffect.setParameter("pitch_variation", preset.granular.pitch_variation);
    granularEffect.setParameter("time_shift", preset.granular.time_shift);
    granularEffect.setParameter("spray", preset.granular.spray);
    granularEffect.setWindowType(preset.granular.window_type);
}

} // namespace Presets
