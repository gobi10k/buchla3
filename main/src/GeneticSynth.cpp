#include "GeneticSynth.h"
#include "config.h"
#include <Arduino.h>
#include "esp_random.h"
#include <string>
#include <string>
#include <string>

// Oscillator method implementations
float GeneticSynth::Oscillator::generateSample() {
    float sampleVal = 0.0f;
    switch (currentWaveTypeSetting) {
        case SINE:     sampleVal = sinf(phase); break;
        case SAW:      sampleVal = (2.0f * phase / TWO_PI) - 1.0f; break;
        case SQUARE:   sampleVal = (phase < PI) ? 1.0f : -1.0f; break;
        case TRIANGLE:
            if (phase < PI) sampleVal = (4.0f * phase / TWO_PI) - 1.0f;
            else sampleVal = 3.0f - (4.0f * phase / TWO_PI);
            break;
        case NOISE:    sampleVal = ((float)(esp_random() % 65536) / 32768.0f) - 1.0f; break;
    }
    sampleVal *= currentAmplitudeSetting;
    phase += phaseIncrement;
    if (phase >= TWO_PI) phase -= TWO_PI;
    return sampleVal;
}

void GeneticSynth::Oscillator::updatePhaseIncrement(float actualFrequency, float sampleRate) {
    phaseIncrement = (TWO_PI * actualFrequency) / sampleRate;
}

// GeneticSynth method implementations
GeneticSynth::GeneticSynth(float freq, uint8_t amp)
    : currentBaseFrequency(freq), currentOutputLevel(amp), currentGenomeIdx(0), generationCount(0), gateActive(false),
      currentEvoMode(MANUAL), currentMutationRate(0.1f), currentCrossoverRate(0.7f), // Default crossover rate
      lastEvoTimeMs(0), evoIntervalMs(10000), lfoPhaseValue(0.0f),
      svf_low_pass_state(0.0f), svf_band_pass_state(0.0f), // Initialize SVF states
      evolution_due(false) { // Initialize evolution_due flag - true when evolution should run outside audio thread
    
    ampEnvelope.setAttack(10);
    ampEnvelope.setDecay(100);
    ampEnvelope.setSustain(0.7f);
    ampEnvelope.setRelease(200);

    initializePopulation();
    applyGenomeToSynth(population[currentGenomeIdx]);
    
    // Serial.printf("GeneticSynth created: %.1f Hz, Amp %d, Pop %d\n",
    //              currentBaseFrequency, currentOutputLevel, POPULATION_SIZE);
}

void GeneticSynth::generateSample(float& sample) {
    if (!gateActive && ampEnvelope.getState() == ADSREnvelope::OFF && ampEnvelope.getAmplitude() < 0.001f) {
        sample = 0.0f;
        return;
    }
    float envAmp = ampEnvelope.getAmplitude();
    if (envAmp < 0.001f && ampEnvelope.getState() == ADSREnvelope::OFF && !gateActive) { // Ensure gate is also considered
        sample = 0.0f;
        return;
    }

    const Genome& currentGenome = population[currentGenomeIdx];
    float output = 0.0f;
    
    float lfo_sample_val = sinf(lfoPhaseValue) * currentGenome.lfo_depth_genome;
    lfoPhaseValue += (TWO_PI * currentGenome.lfo_rate_genome) / SAMPLE_RATE;
    if (lfoPhaseValue >= TWO_PI) lfoPhaseValue -= TWO_PI;
    
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        if (oscillators[i].currentAmplitudeSetting > 0.001f) {
            float oscOutput = oscillators[i].generateSample();
            // LFO typically modulates pitch or filter, not usually direct amplitude of each osc this way
            // For now, let's assume LFO affects global aspects or filter, not per-osc amplitude here.
            // float modAmp = oscillators[i].currentAmplitudeSetting * (1.0f + lfo_sample_val * 0.5f);
            // output += oscOutput * modAmp;
            output += oscOutput; // Individual oscillator amplitudes are already set in applyGenomeToSynth
        }
    }
    
    if (currentGenome.noise_level_genome > 0.001f) {
        output += generateNoiseSample() * currentGenome.noise_level_genome;
    }
    
    float effectiveFilterCutoffRatio = currentGenome.filter_cutoff_ratio_genome * (1.0f + lfo_sample_val * 0.3f);
    output = applySynthFilter(output, effectiveFilterCutoffRatio, currentGenome.filter_resonance_genome);
    output *= currentGenome.master_amp_genome * envAmp;
    
    // If in an automatic evolution mode, check if it's time to schedule an evolution.
    // Instead of running evolvePopulation() directly (which is too slow for audio thread),
    // set a flag (evolution_due) that can be checked by a lower-priority task.
    if (currentEvoMode != MANUAL && !evolution_due && (millis() - lastEvoTimeMs > evoIntervalMs) ) {
        evolution_due = true; // Signal that an evolution step should be performed.
        lastEvoTimeMs = millis(); // Reset the timer for the next evolution schedule.

        // Update the evolution interval for the next cycle, especially for AUTO_CHAOTIC mode.
        if(currentEvoMode == AUTO_CHAOTIC) evoIntervalMs = 1000 + (esp_random() % 8000);
        else if(currentEvoMode == AUTO_FAST) evoIntervalMs = 2000;
        else if(currentEvoMode == AUTO_SLOW) evoIntervalMs = 10000;
    }
    
    float finalScaled = output * (static_cast<float>(currentOutputLevel) / 127.0f);
    sample = constrain(finalScaled, -1.0f, 1.0f);
}

void GeneticSynth::noteOn(float frequency, float velocity) {
    gateActive = true;
    setFrequency(frequency);
    ampEnvelope.noteOn();
    // Serial.printf("GeneticSynth Note ON: Freq=%.1f Hz\n", currentBaseFrequency);
}

void GeneticSynth::noteOff() {
    gateActive = false;
    ampEnvelope.noteOff();
    // Serial.println("GeneticSynth Note OFF");
}

void GeneticSynth::reset() {
    for (int i = 0; i < NUM_OSCILLATORS; i++) oscillators[i].phase = 0.0f;
    lfoPhaseValue = 0.0f;
    svf_low_pass_state = 0.0f;  // Reset SVF states
    svf_band_pass_state = 0.0f; // Reset SVF states
    ampEnvelope.reset();
    gateActive = false;
}

void GeneticSynth::setParameter(const std::string& name, float value) {
    if (name == "frequency") setFrequency(value);
    else if (name == "amplitude") setAmplitude(value);
    else if (name == "evolution_mode") setEvolutionMode(static_cast<EvolutionMode>(static_cast<int>(value)));
    else if (name == "mutation_rate") setMutationRate(value);
    else if (name == "crossover_rate") setCrossoverRate(value);
    else if (name == "genome_index") setGenome(static_cast<int>(value));
    else if (name == "attack") ampEnvelope.setAttack(value);
    else if (name == "decay") ampEnvelope.setDecay(value);
    else if (name == "sustain") ampEnvelope.setSustain(value);
    else if (name == "release") ampEnvelope.setRelease(value);
}

void GeneticSynth::setFrequency(float freq) {
    currentBaseFrequency = constrain(freq, 20.0f, 20000.0f);
    if (currentGenomeIdx < POPULATION_SIZE) { // Ensure index is valid
        applyGenomeToSynth(population[currentGenomeIdx]);
    }
}

void GeneticSynth::setAmplitude(float amp) {
    currentOutputLevel = static_cast<uint8_t>(constrain(amp, 0.0f, 127.0f));
}

void GeneticSynth::initializePopulation() {
    for (int i = 0; i < POPULATION_SIZE; i++) {
        randomizeGenome(i);
    }
    generationCount = 0;
    // Serial.printf("GeneticSynth: Population initialized (%d genomes)\n", POPULATION_SIZE);
}

void GeneticSynth::evolvePopulation() {
    Genome newPopulation[POPULATION_SIZE];
    selectNextGeneration();

    int bestIdx = 0;
    for(int i=1; i<POPULATION_SIZE; ++i) if(population[i].fitness > population[bestIdx].fitness) bestIdx = i;
    newPopulation[0] = population[bestIdx];

    for (int i = 1; i < POPULATION_SIZE; i += 2) {
        int p1_idx = randomInt(0, POPULATION_SIZE - 1);
        int p2_idx = randomInt(0, POPULATION_SIZE - 1);
        
        if (i + 1 < POPULATION_SIZE) {
            crossoverGenomes(population[p1_idx], population[p2_idx], newPopulation[i], newPopulation[i+1]);
            mutateGenome(newPopulation[i]);
            mutateGenome(newPopulation[i+1]);
            constrainGenomeParameters(newPopulation[i]);
            constrainGenomeParameters(newPopulation[i+1]);
            newPopulation[i].fitness = 0.5f;
            newPopulation[i+1].fitness = 0.5f;
        } else {
            newPopulation[i] = population[p1_idx];
            mutateGenome(newPopulation[i]);
            constrainGenomeParameters(newPopulation[i]);
            newPopulation[i].fitness = 0.5f;
        }
    }
    
    memcpy(population, newPopulation, sizeof(population));
    generationCount++;
    currentGenomeIdx = 0;
    applyGenomeToSynth(population[currentGenomeIdx]);
    // Serial.printf("GeneticSynth: Evolved to generation %d. Best fitness: %.3f\n", generationCount, population[0].fitness);
}

void GeneticSynth::mutateGenome(Genome& genome) {
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.osc_freq_ratios[i] += randomFloat(-0.5f, 0.5f);
        if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.osc_amps[i] += randomFloat(-0.2f, 0.2f);
        if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.osc_waves[i] = randomInt(0, NUM_WAVE_TYPES - 1);
    }
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.master_amp_genome += randomFloat(-0.1f, 0.1f);
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.filter_cutoff_ratio_genome += randomFloat(-0.3f, 0.3f);
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.filter_resonance_genome += randomFloat(-0.2f, 0.2f);
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.noise_level_genome += randomFloat(-0.1f, 0.1f);
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.lfo_rate_genome += randomFloat(-2.0f, 2.0f);
    if (randomFloat(0.0f, 1.0f) < currentMutationRate) genome.lfo_depth_genome += randomFloat(-0.2f, 0.2f);
}

void GeneticSynth::crossoverGenomes(const Genome& parent1, const Genome& parent2, Genome& child1, Genome& child2) {
    // This function was updated to use currentCrossoverRate.
    // The original loop that did a 50/50 split was removed.
    // Now, currentCrossoverRate determines the probability of child1 taking a gene from parent2 (else parent1),
    // and for child2, it's the probability of taking from parent1 (else parent2).
    // This ensures that currentCrossoverRate has a direct impact on gene inheritance.

    // Helper lambda for deciding if a gene should be swapped based on currentCrossoverRate.
    // If randomFloat < currentCrossoverRate, the gene is taken from the "second" parent argument.
    // Otherwise, it's taken from the "first" parent argument.
    auto do_crossover = [&](float p1_gene, float p2_gene) {
        return (randomFloat(0.0f, 1.0f) < currentCrossoverRate) ? p2_gene : p1_gene;
    };
    auto do_crossover_u8 = [&](uint8_t p1_gene, uint8_t p2_gene) {
        return (randomFloat(0.0f, 1.0f) < currentCrossoverRate) ? p2_gene : p1_gene;
    };

    for (int i = 0; i < NUM_OSCILLATORS; ++i) {
        child1.osc_freq_ratios[i] = do_crossover(parent1.osc_freq_ratios[i], parent2.osc_freq_ratios[i]);
        child2.osc_freq_ratios[i] = do_crossover(parent2.osc_freq_ratios[i], parent1.osc_freq_ratios[i]); // Swap p1/p2 for child2 for diversity
        child1.osc_amps[i]  = do_crossover(parent1.osc_amps[i], parent2.osc_amps[i]);
        child2.osc_amps[i]  = do_crossover(parent2.osc_amps[i], parent1.osc_amps[i]);
        child1.osc_waves[i]   = do_crossover_u8(parent1.osc_waves[i], parent2.osc_waves[i]);
        child2.osc_waves[i]   = do_crossover_u8(parent2.osc_waves[i], parent1.osc_waves[i]);
    }
    child1.master_amp_genome = do_crossover(parent1.master_amp_genome, parent2.master_amp_genome);
    child2.master_amp_genome = do_crossover(parent2.master_amp_genome, parent1.master_amp_genome);
    child1.filter_cutoff_ratio_genome = do_crossover(parent1.filter_cutoff_ratio_genome, parent2.filter_cutoff_ratio_genome);
    child2.filter_cutoff_ratio_genome = do_crossover(parent2.filter_cutoff_ratio_genome, parent1.filter_cutoff_ratio_genome);
    child1.filter_resonance_genome = do_crossover(parent1.filter_resonance_genome, parent2.filter_resonance_genome);
    child2.filter_resonance_genome = do_crossover(parent2.filter_resonance_genome, parent1.filter_resonance_genome);
    child1.noise_level_genome = do_crossover(parent1.noise_level_genome, parent2.noise_level_genome);
    child2.noise_level_genome = do_crossover(parent2.noise_level_genome, parent1.noise_level_genome);
    child1.lfo_rate_genome = do_crossover(parent1.lfo_rate_genome, parent2.lfo_rate_genome);
    child2.lfo_rate_genome = do_crossover(parent2.lfo_rate_genome, parent1.lfo_rate_genome);
    child1.lfo_depth_genome = do_crossover(parent1.lfo_depth_genome, parent2.lfo_depth_genome);
    child2.lfo_depth_genome = do_crossover(parent2.lfo_depth_genome, parent1.lfo_depth_genome);
}

void GeneticSynth::selectNextGeneration() {
    // Placeholder
}

void GeneticSynth::setGenome(int index) {
    if (index >= 0 && index < POPULATION_SIZE) {
        currentGenomeIdx = index;
        applyGenomeToSynth(population[index]);
        // Serial.printf("GeneticSynth: Switched to genome %d (Fitness: %.3f)\n", index, population[index].fitness);
    }
}

void GeneticSynth::rateCurrentGenome(float rating){
    if(currentGenomeIdx < POPULATION_SIZE) {
        population[currentGenomeIdx].fitness = constrain(rating, 0.0f, 1.0f);
    }
}

void GeneticSynth::randomizeGenome(int index) {
    if (index < 0 || index >= POPULATION_SIZE) return;
    Genome& genome = population[index];
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        genome.osc_freq_ratios[i] = randomFloat(0.25f, 4.0f);
        genome.osc_amps[i] = randomFloat(0.0f, 1.0f);
        genome.osc_waves[i] = randomInt(0, NUM_WAVE_TYPES - 1);
    }
    genome.master_amp_genome = randomFloat(0.5f, 1.0f);
    genome.filter_cutoff_ratio_genome = randomFloat(0.2f, 3.0f);
    genome.filter_resonance_genome = randomFloat(0.0f, 0.9f);
    genome.noise_level_genome = randomFloat(0.0f, 0.15f);
    genome.lfo_rate_genome = randomFloat(0.1f, 5.0f);
    genome.lfo_depth_genome = randomFloat(0.0f, 0.5f);
    genome.fitness = 0.5f;
    constrainGenomeParameters(genome);
    if (index == currentGenomeIdx) applyGenomeToSynth(genome);
}

void GeneticSynth::setEvolutionMode(EvolutionMode mode) {
    if (mode < NUM_EVOLUTION_MODES) {
        currentEvoMode = mode; // Renamed member variable
        lastEvoTimeMs = millis();
        if(mode == AUTO_CHAOTIC) evoIntervalMs = 1000 + (esp_random() % 8000);
        else if(mode == AUTO_FAST) evoIntervalMs = 2000;
        else if(mode == AUTO_SLOW) evoIntervalMs = 10000;
        // Serial.printf("GeneticSynth: Evolution mode to %s\n", getEvolutionModeName(mode));
    }
}

void GeneticSynth::loadPreset(int presetNumber) {
    initializePopulation();
    for(int i=0; i < POPULATION_SIZE; ++i) {
        Genome& g = population[i];
        switch (presetNumber) {
            case 0:
                for(int j=0; j<NUM_OSCILLATORS; ++j) { g.osc_freq_ratios[j] = 1.0f + j; g.osc_waves[j] = SINE; }
                g.filter_resonance_genome = 0.1f; g.noise_level_genome = 0.01f; break;
            case 1:
                for(int j=0; j<NUM_OSCILLATORS; ++j) { g.osc_waves[j] = (j%2==0) ? SAW : NOISE; }
                g.noise_level_genome = 0.1f; g.lfo_depth_genome = 0.3f; g.lfo_rate_genome = 0.2f; break;
        }
        constrainGenomeParameters(g);
    }
    currentGenomeIdx = 0;
    applyGenomeToSynth(population[currentGenomeIdx]);
    // Serial.printf("GeneticSynth: Loaded preset %d\n", presetNumber);
}

void GeneticSynth::applyGenomeToSynth(const Genome& genome) {
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        oscillators[i].currentAmplitudeSetting = genome.osc_amps[i]; // Use renamed member
        oscillators[i].currentWaveTypeSetting = static_cast<WaveType>(genome.osc_waves[i]); // Use renamed member
        float freq = currentBaseFrequency * genome.osc_freq_ratios[i]; // Use renamed member
        oscillators[i].updatePhaseIncrement(freq, SAMPLE_RATE);
    }
}

float GeneticSynth::generateNoiseSample() {
    return (static_cast<float>(esp_random()) / static_cast<float>(UINT32_MAX)) * 2.0f - 1.0f;
}

// State Variable Filter (SVF) implementation
// Output is low_pass_out. band_pass_out and high_pass_out are also available if needed.
// This filter replaces the previous 1-pole LPF to enable resonance control.
float GeneticSynth::applySynthFilter(float input, float cutoffRatio, float resonance) {
    // Convert resonance (0-1 range from genome, constrained 0-0.98) to q value for SVF.
    // q = 1/Q. Higher resonance means a smaller q value.
    // A resonance value of 0 gives q=1.0 (less resonant).
    // A resonance value of 0.98 gives q=0.02 (highly resonant).
    float q_val = 1.0f - constrain(resonance, 0.0f, 0.98f);

    // Calculate the effective cutoff frequency for the filter.
    // It's based on the synth's base frequency, the genome's cutoff ratio,
    // and further modulated by the LFO in generateSample().
    float actualCutoffFreq = currentBaseFrequency * cutoffRatio; // cutoffRatio already includes LFO modulation
    actualCutoffFreq = constrain(actualCutoffFreq, 20.0f, SAMPLE_RATE * 0.45f); // Ensure cutoff is within reasonable audio range and below Nyquist.

    // Calculate filter coefficient 'f' (related to cutoff)
    // For SVF, f = 2 * sin(PI * cutoff / sample_rate)
    // This can be approximated for low frequencies by f = 2 * PI * cutoff / sample_rate
    // Let's use the approximation as it's common and less computationally intensive.
    float f = (TWO_PI * actualCutoffFreq) / SAMPLE_RATE;
    f = constrain(f, 0.0001f, TWO_PI * 0.45f); // Ensure f is positive and below Nyquist approximation limit

    // SVF processing steps:
    // Input to the filter is 'input'
    // svf_low_pass_state and svf_band_pass_state are the z^-1 states

    // Standard SVF equations:
    // 1. Calculate high-pass output (can be considered an intermediate value)
    float high_pass_out = input - svf_low_pass_state - q_val * svf_band_pass_state;
    // 2. Update band-pass state using the high-pass output
    svf_band_pass_state = svf_band_pass_state + f * high_pass_out;
    // 3. Update low-pass state using the new band-pass state
    svf_low_pass_state = svf_low_pass_state + f * svf_band_pass_state;

    // svf_low_pass_state now holds the low-pass filtered output for this sample.
    // Other outputs like band-pass (svf_band_pass_state) or high-pass (high_pass_out)
    // are available if the filter needed to provide them.
    // For this synth, only the low-pass output is used.

    // Clamp internal SVF states to prevent them from growing unbounded at high resonance,
    // which could lead to NaN/Inf values or extreme audio artifacts.
    // A limit of 10.0 should be well above typical audio signal ranges (-1 to 1)
    // but prevent runaway floating point values.
    const float svf_state_limit = 10.0f;
    svf_band_pass_state = constrain(svf_band_pass_state, -svf_state_limit, svf_state_limit);
    svf_low_pass_state = constrain(svf_low_pass_state, -svf_state_limit, svf_state_limit);

    return svf_low_pass_state;
}

// This method should be called periodically from a context outside the audio interrupt/task
// (e.g., from the main application loop) to perform the evolution if it has been scheduled.
void GeneticSynth::checkAndPerformEvolution() {
    if (evolution_due) {
        evolvePopulation(); // Execute the computationally intensive evolution process.
        evolution_due = false; // Reset the flag once done.
    }
}

void GeneticSynth::constrainGenomeParameters(Genome& genome) {
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        genome.osc_freq_ratios[i] = constrain(genome.osc_freq_ratios[i], 0.1f, 16.0f);
        genome.osc_amps[i] = constrain(genome.osc_amps[i], 0.0f, 1.0f);
        genome.osc_waves[i] = constrain(genome.osc_waves[i], 0, NUM_WAVE_TYPES - 1);
    }
    genome.master_amp_genome = constrain(genome.master_amp_genome, 0.0f, 1.0f);
    genome.filter_cutoff_ratio_genome = constrain(genome.filter_cutoff_ratio_genome, 0.05f, 5.0f);
    genome.filter_resonance_genome = constrain(genome.filter_resonance_genome, 0.0f, 0.98f);
    genome.noise_level_genome = constrain(genome.noise_level_genome, 0.0f, 0.5f);
    genome.lfo_rate_genome = constrain(genome.lfo_rate_genome, 0.05f, 20.0f);
    genome.lfo_depth_genome = constrain(genome.lfo_depth_genome, 0.0f, 1.0f);
    genome.fitness = constrain(genome.fitness, 0.0f, 1.0f);
}

const char* GeneticSynth::getEvolutionModeName(EvolutionMode mode) {
    switch (mode) {
        case MANUAL: return "MANUAL"; case AUTO_SLOW: return "AUTO_SLOW";
        case AUTO_FAST: return "AUTO_FAST"; case AUTO_CHAOTIC: return "AUTO_CHAOTIC";
        default: return "UNKNOWN_EVO_MODE";
    }
}

const char* GeneticSynth::getWaveTypeName(WaveType type) {
    switch (type) {
        case SINE: return "SINE"; case SAW: return "SAW"; case SQUARE: return "SQUARE";
        case TRIANGLE: return "TRIANGLE"; case NOISE: return "NOISE";
        default: return "UNKNOWN_WAVE";
    }
}

float GeneticSynth::randomFloat(float minVal, float maxVal) {
    return minVal + (static_cast<float>(esp_random()) / static_cast<float>(UINT32_MAX)) * (maxVal - minVal);
}

int GeneticSynth::randomInt(int minVal, int maxVal) {
    if (minVal > maxVal) { int temp = minVal; minVal = maxVal; maxVal = temp; }
    if (minVal == maxVal) return minVal;
    return minVal + (esp_random() % (maxVal - minVal + 1));
}

void GeneticSynth::printStatus() {
    Serial.printf("--- GeneticSynth Status ---\n");
    Serial.printf("Current Genome: %d, Generation: %d, Fitness: %.3f\n", currentGenomeIdx, generationCount, getCurrentGenomeFitness());
    Serial.printf("Base Freq: %.2f Hz, Output Level: %d\n", currentBaseFrequency, currentOutputLevel);
    Serial.printf("Evolution Mode: %s, Mutate: %.2f, XOver: %.2f\n", getEvolutionModeName(currentEvoMode), currentMutationRate, currentCrossoverRate);
    const Genome& g = population[currentGenomeIdx];
    Serial.printf("  Genome Master Amp: %.2f, Noise: %.2f\n", g.master_amp_genome, g.noise_level_genome);
    Serial.printf("  Filter: CutoffRatio=%.2f, Res=%.2f\n", g.filter_cutoff_ratio_genome, g.filter_resonance_genome);
    Serial.printf("  LFO: Rate=%.2f Hz, Depth=%.2f\n", g.lfo_rate_genome, g.lfo_depth_genome);
    for(int i=0; i<NUM_OSCILLATORS; ++i) {
        Serial.printf("  Osc %d: FreqRatio=%.2f, Amp=%.2f, Wave=%s\n", i, g.osc_freq_ratios[i], g.osc_amps[i], getWaveTypeName(static_cast<WaveType>(g.osc_waves[i])));
    }
    Serial.printf("  Envelope: A=%.1f D=%.1f S=%.1f R=%.1f State=%s Level=%.2f\n",
        ampEnvelope.getAttack(), ampEnvelope.getDecay(), ampEnvelope.getSustain(), ampEnvelope.getRelease(),
        "N/A", ampEnvelope.getAmplitude()
    );
}

float GeneticSynth::getCurrentGenomeFitness() const {
    if (currentGenomeIdx >= 0 && currentGenomeIdx < POPULATION_SIZE) {
        return population[currentGenomeIdx].fitness;
    }
    return 0.0f; // Should not happen if index is always valid
}
