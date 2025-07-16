#include "GeneticController.h"
#include "config.h"
#include <Arduino.h>
#include "esp_random.h"
#include <string>

GeneticController::GeneticController()
    : numParameters(0), currentGenomeIndex(0), generation(0),
      currentControlMode(BYPASS), currentEvolutionRate(0.001f),
      currentMutationRate(0.1f), currentMorphSpeed(0.01f),
      targetSource(nullptr), targetEffect(nullptr),
      samplesSinceLastEvolution(0), evolutionIntervalSamples(SAMPLE_RATE / 10),
      morphFromGenomeIndex(0), morphToGenomeIndex(1), currentMorphPosition(0.0f),
      numMacroPoints(0), macroControlValue(0.0f)
       {
    for (int i = 0; i < MAX_CONTROLLABLE_PARAMETERS; i++) {
        parameters[i].name[0] = '\0';
        parameters[i].minValue = 0.0f;
        parameters[i].maxValue = 1.0f;
        parameters[i].currentValue = 0.5f;
        parameters[i].active = false;
    }
    initializePopulation();
}

void GeneticController::process(float& sample) {
    if (!enabled || currentControlMode == BYPASS || numParameters == 0) {
        return;
    }
    samplesSinceLastEvolution++;
    switch (currentControlMode) {
        case EVOLVE_CONTINUOUS:
            if (samplesSinceLastEvolution >= evolutionIntervalSamples) {
                if (randomFloat(0.0f, 1.0f) < currentEvolutionRate) {
                    evolvePopulation();
                }
                samplesSinceLastEvolution = 0;
            }
            updateAndApplyCurrentParameters();
            break;
        case EVOLVE_DISCRETE:
            if (samplesSinceLastEvolution >= evolutionIntervalSamples * 5) {
                evolvePopulation();
                updateAndApplyCurrentParameters();
                samplesSinceLastEvolution = 0;
            }
            break;
        case MORPH_BETWEEN:
            performMorph();
            updateAndApplyCurrentParameters();
            break;
        case MACRO:
            applyMacroControl();
            break;
        default: break;
    }
    if (currentGenomeIndex < CONTROLLER_POPULATION_SIZE) {
        population[currentGenomeIndex].age++;
    }
}

void GeneticController::setParameter(const std::string& name, float value) {
    if (name == "control_mode") {
        setControlMode(static_cast<ControlMode>(static_cast<int>(value)));
    } else if (name == "evolution_rate") {
        currentEvolutionRate = constrain(value, 0.0f, 1.0f);
        if (currentEvolutionRate > 0.0001f) {
            evolutionIntervalSamples = static_cast<uint32_t>( (AUDIO_BUFFER_CHUNK_SIZE / currentEvolutionRate) );
            if (evolutionIntervalSamples < AUDIO_BUFFER_CHUNK_SIZE) evolutionIntervalSamples = AUDIO_BUFFER_CHUNK_SIZE;
        } else {
            evolutionIntervalSamples = UINT32_MAX;
        }
    } else if (name == "mutation_rate") {
        currentMutationRate = constrain(value, 0.0f, 1.0f);
    } else if (name == "morph_speed") {
        currentMorphSpeed = constrain(value, 0.0001f, 0.1f);
    } else if (name == "genome_index") {
        int index = static_cast<int>(value);
        if (index >= 0 && index < CONTROLLER_POPULATION_SIZE) {
            currentGenomeIndex = index;
            updateAndApplyCurrentParameters();
        }
    } else if (name == "fitness") {
        rateCurrentGenome(value);
    } else if (name == "macro_control") {
        macroControlValue = constrain(value, 0.0f, 1.0f);
    }
}

void GeneticController::reset() {
    samplesSinceLastEvolution = 0;
    currentMorphPosition = 0.0f;
    for (int i = 0; i < CONTROLLER_POPULATION_SIZE; i++) population[i].age = 0;
}

bool GeneticController::addParameter(const char* name, float minVal, float maxVal) {
    if (numParameters >= MAX_CONTROLLABLE_PARAMETERS) return false;
    for (int i = 0; i < numParameters; i++) if (strcmp(parameters[i].name, name) == 0) return false;

    strncpy(parameters[numParameters].name, name, 15);
    parameters[numParameters].name[15] = '\0';
    parameters[numParameters].minValue = minVal;
    parameters[numParameters].maxValue = maxVal;
    parameters[numParameters].currentValue = (minVal + maxVal) * 0.5f;
    parameters[numParameters].active = true;
    numParameters++;
    return true;
}

void GeneticController::clearParameters() {
    numParameters = 0;
}

void GeneticController::setTarget(AudioSource* source) { targetSource = source; targetEffect = nullptr; }
void GeneticController::setTarget(AudioEffect* effect) { targetEffect = effect; targetSource = nullptr; }
void GeneticController::clearTarget() { targetSource = nullptr; targetEffect = nullptr; }

void GeneticController::initializePopulation() {
    for (int i = 0; i < CONTROLLER_POPULATION_SIZE; i++) {
        randomizeGenome(i);
    }
    generation = 0;
    currentGenomeIndex = 0;
    updateAndApplyCurrentParameters();
}

void GeneticController::evolvePopulation() {
    if (numParameters == 0) return;
    ParameterGenome newPopulation[CONTROLLER_POPULATION_SIZE];

    int bestFitIdx = 0;
    for (int i = 1; i < CONTROLLER_POPULATION_SIZE; ++i) {
        if (population[i].fitness > population[bestFitIdx].fitness) bestFitIdx = i;
    }
    newPopulation[0] = population[bestFitIdx];
    newPopulation[0].age = 0;

    for (int i = 1; i < CONTROLLER_POPULATION_SIZE; ++i) {
        int p1_idx = selectParentByTournament();
        int p2_idx = selectParentByTournament();
        crossoverGenomes(population[p1_idx], population[p2_idx], newPopulation[i]);
        mutateGenome(newPopulation[i]);
        constrainGenomeValues(newPopulation[i]);
        newPopulation[i].fitness = 0.5f;
        newPopulation[i].age = 0;
    }
    memcpy(population, newPopulation, sizeof(population));
    generation++;
    currentGenomeIndex = 0;
}

void GeneticController::mutateGenome(ParameterGenome& genome) {
    for (int i = 0; i < numParameters; i++) {
        if (parameters[i].active && randomFloat(0.0f, 1.0f) < currentMutationRate) {
            genome.values[i] += randomFloat(-0.15f, 0.15f);
        }
    }
}

void GeneticController::crossoverGenomes(const ParameterGenome& parent1, const ParameterGenome& parent2, ParameterGenome& child) {
    for (int i = 0; i < numParameters; i++) {
        child.values[i] = (randomFloat(0.0f, 1.0f) < 0.5f) ? parent1.values[i] : parent2.values[i];
    }
}

void GeneticController::updateAndApplyCurrentParameters() {
    if (numParameters == 0 || currentGenomeIndex >= CONTROLLER_POPULATION_SIZE) return;

    const ParameterGenome* activeGenome = &population[currentGenomeIndex];
    ParameterGenome tempMorphedGenome;

    if (currentControlMode == MORPH_BETWEEN &&
        morphFromGenomeIndex < CONTROLLER_POPULATION_SIZE &&
        morphToGenomeIndex < CONTROLLER_POPULATION_SIZE) {

        const ParameterGenome& from = population[morphFromGenomeIndex];
        const ParameterGenome& to = population[morphToGenomeIndex];
        for (int i = 0; i < numParameters; i++) {
            tempMorphedGenome.values[i] = from.values[i] + (to.values[i] - from.values[i]) * currentMorphPosition;
        }
        activeGenome = &tempMorphedGenome;
    }

    for (int i = 0; i < numParameters; i++) {
        if (parameters[i].active) {
            float normalizedValue = activeGenome->values[i];
            float range = parameters[i].maxValue - parameters[i].minValue;
            parameters[i].currentValue = parameters[i].minValue + (normalizedValue * range);

            if (targetSource) {
                targetSource->setParameter(parameters[i].name, parameters[i].currentValue);
            } else if (targetEffect) {
                targetEffect->setParameter(parameters[i].name, parameters[i].currentValue);
            }
        }
    }
}

void GeneticController::rateCurrentGenome(float rating) {
    if (currentGenomeIndex < CONTROLLER_POPULATION_SIZE) {
        population[currentGenomeIndex].fitness = constrain(rating, 0.0f, 1.0f);
    }
}

void GeneticController::randomizeGenome(int index) {
    if (index < 0 || index >= CONTROLLER_POPULATION_SIZE) return;
    ParameterGenome& genome = population[index];
    for (int i = 0; i < MAX_CONTROLLABLE_PARAMETERS; i++) genome.values[i] = randomFloat(0.0f, 1.0f);
    genome.fitness = 0.5f; genome.age = 0;
    if (index == currentGenomeIndex) updateAndApplyCurrentParameters();
}

void GeneticController::loadPreset(int presetNumber) {
    clearParameters(); // Clear existing parameters before loading a preset
    bool success = false;
    switch (presetNumber) {
        case 0: // Filter control
            success = addParameter("cutoff", 200.0f, 8000.0f);
            if(success) success = addParameter("resonance", 0.0f, 0.95f);
            if(success) Serial.println("GC: Loaded Filter Control preset");
            break;
        case 1: // FM control (example parameters)
            success = addParameter("mod_ratio", 0.1f, 8.0f);
            if(success) success = addParameter("mod_index", 0.0f, 20.0f);
            if(success) Serial.println("GC: Loaded FM Control preset");
            break;
        // Add more presets as needed
        default:
            Serial.printf("GC: Invalid preset number %d\n", presetNumber);
            return;
    }
    if (!success) Serial.println("GC: Error loading preset, max parameters possibly reached.");

    initializePopulation(); // Re-initialize population for new parameter set
}

void GeneticController::setControlMode(ControlMode mode) {
    if (mode < NUM_CONTROL_MODES) {
        currentControlMode = mode;
        if (mode == MORPH_BETWEEN) {
            // Ensure morphFrom/To are valid before starting morph
            if (morphFromGenomeIndex >= CONTROLLER_POPULATION_SIZE) morphFromGenomeIndex = 0;
            if (morphToGenomeIndex >= CONTROLLER_POPULATION_SIZE) morphToGenomeIndex = (morphFromGenomeIndex + 1) % CONTROLLER_POPULATION_SIZE;
            if (morphFromGenomeIndex == morphToGenomeIndex) morphToGenomeIndex = (morphFromGenomeIndex + 1) % CONTROLLER_POPULATION_SIZE;
            currentMorphPosition = 0.0f;
        }
        // Serial.printf("GC: Control Mode to %s\n", getControlModeName(mode));
    }
}


void GeneticController::constrainGenomeValues(ParameterGenome& genome) {
    for (int i = 0; i < MAX_CONTROLLABLE_PARAMETERS; i++) {
        genome.values[i] = constrain(genome.values[i], 0.0f, 1.0f);
    }
}

int GeneticController::selectParentByTournament() {
    int t_size = 3;
    int bestParentIndex = randomInt(0, CONTROLLER_POPULATION_SIZE - 1);
    for (int i = 1; i < t_size; ++i) {
        int contenderIndex = randomInt(0, CONTROLLER_POPULATION_SIZE - 1);
        float contenderFitness = population[contenderIndex].fitness;
        float bestKnownFitness = population[bestParentIndex].fitness;
        // Basic age penalty (older genomes are slightly less likely to be chosen if fitness is similar)
        contenderFitness *= (1.0f - (float)population[contenderIndex].age / (SAMPLE_RATE * 300.0f)); // Penalty after 5 mins
        bestKnownFitness *= (1.0f - (float)population[bestParentIndex].age / (SAMPLE_RATE * 300.0f));

        if (contenderFitness > bestKnownFitness) {
            bestParentIndex = contenderIndex;
        }
    }
    return bestParentIndex;
}

void GeneticController::performMorph() {
    currentMorphPosition += currentMorphSpeed;
    if (currentMorphPosition >= 1.0f) {
        currentMorphPosition = 1.0f;
        currentControlMode = EVOLVE_CONTINUOUS;
        currentGenomeIndex = morphToGenomeIndex;
    }
}

bool GeneticController::addMacroControlPoint(float controlValue, const float* paramValues, int numValues) {
    if (numMacroPoints >= MAX_MACRO_CONTROL_POINTS || numValues > numParameters) {
        return false;
    }
    macroMap[numMacroPoints].controlValue = controlValue;
    for (int i = 0; i < numValues; ++i) {
        macroMap[numMacroPoints].paramValues[i] = paramValues[i];
    }
    numMacroPoints++;
    return true;
}

void GeneticController::clearMacroControlPoints() {
    numMacroPoints = 0;
}

void GeneticController::applyMacroControl() {
    if (numMacroPoints < 2) return;

    // Find the two control points to interpolate between
    int p1_idx = -1, p2_idx = -1;
    for (int i = 0; i < numMacroPoints; ++i) {
        if (macroMap[i].controlValue <= macroControlValue) {
            if (p1_idx == -1 || macroMap[i].controlValue > macroMap[p1_idx].controlValue) {
                p1_idx = i;
            }
        }
        if (macroMap[i].controlValue >= macroControlValue) {
            if (p2_idx == -1 || macroMap[i].controlValue < macroMap[p2_idx].controlValue) {
                p2_idx = i;
            }
        }
    }

    if (p1_idx == -1) p1_idx = p2_idx;
    if (p2_idx == -1) p2_idx = p1_idx;

    float factor = 0.0f;
    if (p1_idx != p2_idx) {
        factor = (macroControlValue - macroMap[p1_idx].controlValue) / (macroMap[p2_idx].controlValue - macroMap[p1_idx].controlValue);
    }

    for (int i = 0; i < numParameters; ++i) {
        if (parameters[i].active) {
            float val1 = macroMap[p1_idx].paramValues[i];
            float val2 = macroMap[p2_idx].paramValues[i];
            float interpolatedValue = val1 + factor * (val2 - val1);

            float range = parameters[i].maxValue - parameters[i].minValue;
            parameters[i].currentValue = parameters[i].minValue + (interpolatedValue * range);

            if (targetSource) {
                targetSource->setParameter(parameters[i].name, parameters[i].currentValue);
            } else if (targetEffect) {
                targetEffect->setParameter(parameters[i].name, parameters[i].currentValue);
            }
        }
    }
}

const char* GeneticController::getControlModeName(ControlMode mode) {
    switch (mode) {
        case BYPASS: return "BYPASS"; case EVOLVE_CONTINUOUS: return "EVOLVE_CONTINUOUS";
        case EVOLVE_DISCRETE: return "EVOLVE_DISCRETE"; case MORPH_BETWEEN: return "MORPH_BETWEEN";
        case MACRO: return "MACRO";
        default: return "UNKNOWN_CTRL_MODE";
    }
}

float GeneticController::randomFloat(float minVal, float maxVal) {
    return minVal + (static_cast<float>(esp_random()) / static_cast<float>(UINT32_MAX)) * (maxVal - minVal);
}
int GeneticController::randomInt(int minVal, int maxVal) {
    if (minVal > maxVal) { int temp = minVal; minVal = maxVal; maxVal = temp; }
    if (minVal == maxVal) return minVal;
    return minVal + (esp_random() % (maxVal - minVal + 1));
}

float GeneticController::getCurrentGenomeFitness() const {
    if (currentGenomeIndex < CONTROLLER_POPULATION_SIZE) return population[currentGenomeIndex].fitness;
    return 0.0f;
}
const GeneticController::Parameter& GeneticController::getConfiguredParameter(int index) const {
    if (index >= 0 && index < numParameters) return parameters[index];
    static Parameter emptyParam = {""}; // Should not happen with correct usage
    return emptyParam;
}
const GeneticController::ParameterGenome& GeneticController::getGenomeData(int index) const {
    if (index >= 0 && index < CONTROLLER_POPULATION_SIZE) return population[index];
    return population[0];
}

void GeneticController::printCurrentParameters() {
    Serial.println("--- GeneticController Current Params ---");
    for (int i = 0; i < numParameters; i++) {
        if(parameters[i].active) {
            Serial.printf("  %s: %.3f (norm: %.3f)\n", parameters[i].name, parameters[i].currentValue,
                         (currentGenomeIndex < CONTROLLER_POPULATION_SIZE) ? population[currentGenomeIndex].values[i] : 0.0f);
        }
    }
}
void GeneticController::printPopulationStatus() {
    Serial.println("--- GeneticController Population ---");
    for (int i = 0; i < CONTROLLER_POPULATION_SIZE; i++) {
        Serial.printf("  G%d: Fit=%.3f Age=%u %s", i, population[i].fitness, population[i].age, (i == currentGenomeIndex) ? "<-\n" : "\n");
    }
    Serial.printf("Generation: %d, Mode: %s\n", generation, getControlModeName(currentControlMode));
}
