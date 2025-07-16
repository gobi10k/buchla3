#pragma once

#include "AudioEffect.h"
#include "AudioSource.h"
#include "config.h"
#include <Arduino.h>

class GeneticController : public AudioEffect {
public:
    enum ControlMode {
        BYPASS = 0, EVOLVE_CONTINUOUS, EVOLVE_DISCRETE, MORPH_BETWEEN, MACRO, NUM_CONTROL_MODES
    };
    
private:
    struct Parameter {
        char name[16];
        float minValue;
        float maxValue;
        float currentValue;
        bool active;
    };
    
    struct ParameterGenome {
        float values[MAX_CONTROLLABLE_PARAMETERS];
        float fitness;
        uint32_t age;
    };

    struct MacroControlPoint {
        float controlValue;
        float paramValues[MAX_CONTROLLABLE_PARAMETERS];
    };
    
    ParameterGenome population[CONTROLLER_POPULATION_SIZE];
    Parameter parameters[MAX_CONTROLLABLE_PARAMETERS];
    MacroControlPoint macroMap[MAX_MACRO_CONTROL_POINTS];
    int numMacroPoints;
    int numParameters;
    int currentGenomeIndex;
    int generation;
    
    ControlMode currentControlMode;
    float currentEvolutionRate;
    float currentMutationRate;
    float currentMorphSpeed;
    float macroControlValue;
    
    AudioSource* targetSource;
    AudioEffect* targetEffect;
    
    uint32_t samplesSinceLastEvolution;
    uint32_t evolutionIntervalSamples;
    
    int morphFromGenomeIndex;
    int morphToGenomeIndex;
    float currentMorphPosition;
    
public:
    GeneticController();

    void process(float& sample) override;
    void setParameter(const std::string& name, float value) override;
    void reset() override;
    
    bool addParameter(const char* name, float minVal, float maxVal);
    void clearParameters();
    
    void setTarget(AudioSource* source);
    void setTarget(AudioEffect* effect);
    AudioEffect* getTargetEffect() const { return targetEffect; }
    AudioSource* getTargetSource() const { return targetSource; }
    void clearTarget();
    
    void initializePopulation();
    void evolvePopulation();
    void evolveOnce() { evolvePopulation(); updateAndApplyCurrentParameters(); }

    void rateCurrentGenome(float rating);
    void loadPreset(int presetNumber);
    void setControlMode(ControlMode mode);

    bool addMacroControlPoint(float controlValue, const float* paramValues, int numValues);
    void clearMacroControlPoints();

    // Getters
    ControlMode getControlMode() const { return currentControlMode; }
    int getCurrentGenome() const { return currentGenomeIndex; }
    int getGenerationCount() const { return generation; }
    float getEvolutionRate() const { return currentEvolutionRate; }
    float getMutationRate() const { return currentMutationRate; }
    float getMorphSpeed() const { return currentMorphSpeed; }
    float getCurrentGenomeFitness() const;
    int getNumConfiguredParameters() const { return numParameters; }
    const Parameter& getConfiguredParameter(int index) const;
    const ParameterGenome& getGenomeData(int index) const;
    
    void printCurrentParameters();
    void printPopulationStatus();
    void randomizeGenome(int index);

private:
    void mutateGenome(ParameterGenome& genome);
    void crossoverGenomes(const ParameterGenome& parent1, const ParameterGenome& parent2, ParameterGenome& child);
    void updateAndApplyCurrentParameters();
    void applyMacroControl();
    void constrainGenomeValues(ParameterGenome& genome);
    int selectParentByTournament();
    void performMorph();
    static const char* getControlModeName(ControlMode mode);

    float randomFloat(float minVal, float maxVal);
    int randomInt(int minVal, int maxVal);
};
