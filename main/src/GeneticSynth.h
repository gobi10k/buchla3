#pragma once

#include "AudioSource.h"
#include "config.h"
#include <Arduino.h>
#include "DitherNoiseShaping.h"
#include "ADSREnvelope.h"

// ============================================================================
// Genetic Algorithm Synthesizer - Evolving synthesis parameters
// ============================================================================
class GeneticSynth : public AudioSource {
public:
    // Ensure these constants are defined, e.g., in config.h or directly here if local.
    // For now, assuming they might be local if not found in config.h by the compiler.
    static const int POPULATION_SIZE = 8;
    static const int GENOME_LENGTH = 16; // GENOME_SIZE was used before, GENOME_LENGTH might be more apt.
    static const int NUM_OSCILLATORS = 4;
    
    enum WaveType { SINE = 0, SAW, SQUARE, TRIANGLE, NOISE, NUM_WAVE_TYPES };
    enum EvolutionMode { MANUAL = 0, AUTO_SLOW, AUTO_FAST, AUTO_CHAOTIC, NUM_EVOLUTION_MODES };

private:
    struct Genome {
        float osc_freq_ratios[NUM_OSCILLATORS];
        float osc_amps[NUM_OSCILLATORS];
        uint8_t osc_waves[NUM_OSCILLATORS];
        float master_amp_genome; // Genome's master amplitude (0-1)
        float filter_cutoff_ratio_genome;
        float filter_resonance_genome;
        float noise_level_genome;
        float lfo_rate_genome;
        float lfo_depth_genome;
        float fitness;
    };
    
    struct Oscillator {
        float phase;
        float phaseIncrement;
        float currentAmplitudeSetting; // From genome
        WaveType currentWaveTypeSetting; // From genome
        
        float generateSample();
        void updatePhaseIncrement(float actualFrequency, float sampleRate);
        Oscillator() : phase(0.0f), phaseIncrement(0.0f), currentAmplitudeSetting(0.0f), currentWaveTypeSetting(SINE) {}
    };
    
    Genome population[POPULATION_SIZE]; // Use POPULATION_SIZE
    int currentGenomeIdx; // Renamed for brevity
    int generationCount;  // Renamed
    
    Oscillator oscillators[NUM_OSCILLATORS];
    float currentBaseFrequency;
    uint8_t currentOutputLevel;
    bool gateActive;

    EvolutionMode currentEvoMode;
    float currentMutationRate;
    float currentCrossoverRate;
    uint32_t lastEvoTimeMs;
    uint32_t evoIntervalMs;

    float lfoPhaseValue;
    float svf_low_pass_state;  // State variable for SVF (stores low-pass output z^-1)
    float svf_band_pass_state; // State variable for SVF (stores band-pass output z^-1)

    ADSREnvelope ampEnvelope;

public:
    GeneticSynth(float freq = DEFAULT_SYNTH_FREQUENCY, uint8_t amp = 100);

    void generateSample(float& sample) override;
    void reset() override;
    bool isActive() const override { return ampEnvelope.getState() != ADSREnvelope::OFF || gateActive; } // Active if gate or env on
    void setParameter(const std::string& name, float value) override;
    SynthType getSynthType() const override { return SYNTH_TYPE_GENETIC; }

    void noteOn(float frequency, float velocity = 1.0f) override;
    void noteOff() override;
    void setFrequency(float freq) override;
    float getFrequency() const override { return currentBaseFrequency; }
    void setAmplitude(float amp) override;
    float getAmplitude() const override { return static_cast<float>(currentOutputLevel); }

    void initializePopulation();
    void evolvePopulation();
    void mutateGenome(Genome& genome);
    void crossoverGenomes(const Genome& parent1, const Genome& parent2, Genome& child1, Genome& child2);
    void selectNextGeneration();
    
    void evolveOnce() { evolvePopulation(); }
    void setGenome(int index);
    void rateCurrentGenome(float rating);
    void randomizeGenome(int index);
    
    void setEvolutionMode(EvolutionMode mode);
    void setMutationRate(float rate) { currentMutationRate = constrain(rate, 0.0f, 1.0f); }
    void setCrossoverRate(float rate) { currentCrossoverRate = constrain(rate, 0.0f, 1.0f); }
    
    void loadPreset(int presetNumber);

    int getCurrentGenomeIndex() const { return currentGenomeIdx; }
    int getGenerationCount() const { return generationCount; }
    EvolutionMode getCurrentEvolutionMode() const { return currentEvoMode; }
    float getMutationRate() const { return currentMutationRate; }
    float getCrossoverRate() const { return currentCrossoverRate; }
    float getCurrentGenomeFitness() const;
    const Genome& getCurrentGenomeData() const; // Return by const ref
    void printStatus();

    void setAttack(float ms) { ampEnvelope.setAttack(ms); }
    void setDecay(float ms) { ampEnvelope.setDecay(ms); }
    void setSustain(float level) { ampEnvelope.setSustain(level); }
    void setRelease(float ms) { ampEnvelope.setRelease(ms); }

    void checkAndPerformEvolution(); // New public method to be called from a non-audio thread (e.g., main loop)

private:
    bool evolution_due; // Flag set by audio thread when evolution is due, processed by checkAndPerformEvolution()

    void applyGenomeToSynth(const Genome& genome);
    float generateNoiseSample();
    float applySynthFilter(float input, float cutoffRatio, float resonance);
    void constrainGenomeParameters(Genome& genome);

    static const char* getEvolutionModeName(EvolutionMode mode);
    static const char* getWaveTypeName(WaveType type);
    
    float randomFloat(float minVal, float maxVal);
    int randomInt(int minVal, int maxVal);
};
