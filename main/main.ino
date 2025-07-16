#include <Arduino.h>
#include "src/config.h"
#include "src/globals.h"
#include "src/AudioEngine.h"
#include "src/WavetableSynth.h"
#include "src/FMSynth.h"
#include "src/GranularEffect.h"
#include "src/EnhancedLowPassFilter.h"
#include "src/ChorusEffect.h"
#include "src/DCBlocker.h"
#include "src/AntiAliasFilter.h"
#include "src/KarplusStrongSynth.h"
#include "src/GeneticSynth.h"
#include "src/GeneticController.h"
#include "src/FlangerEffect.h"
#include "src/ReverbEffect.h"
#include "src/VocoderSynth.h"
#include "src/Limiter.h"
#include "src/Compressor.h"
#include "src/BuchlaLPG.h"
#include "src/CommandHandler.h"
#include "src/PresetManager.h"
#include "src/RoutingManager.h"
#include "esp_random.h"

AudioEngine audioEngine;
WavetableSynth wavetableSynth(DEFAULT_SYNTH_FREQUENCY, 100, WavetableSynth::SINE);
FMSynth fmSynth(DEFAULT_SYNTH_FREQUENCY, 1.0f, 1.0f, 100);
GranularEffect granularEffect(10.0f, 512, 0.3f);
EnhancedLowPassFilter lowpass(5000.0f, 0.1f);
ChorusEffect chorusEffect(0.5f, 5.0f, 20.0f, 0.4f, 0.1f);
FlangerEffect flangerEffect(0.15f, 3.0f, 2.0f, 0.6f, 0.5f);
ReverbEffect reverbEffect(0.8f, 0.6f, 0.35f);
VocoderSynth vocoderSynth;
DCBlocker dcBlocker;
AntiAliasFilter antiAliasFilter;
KarplusStrongSynth karplusSynth(SAMPLE_RATE);
GeneticSynth geneticSynth(DEFAULT_SYNTH_FREQUENCY, 100);
GeneticController geneticController;
Limiter limiter;
Compressor compressor;
BuchlaLPG buchlaLPG;
RoutingManager routingManager(dcBlocker, antiAliasFilter, granularEffect, lowpass, chorusEffect, flangerEffect, reverbEffect, buchlaLPG, compressor, limiter);
Presets::PresetManager presetManager(wavetableSynth, fmSynth, karplusSynth, geneticSynth, vocoderSynth, lowpass, chorusEffect, flangerEffect, reverbEffect, buchlaLPG, limiter, compressor, granularEffect);
CommandHandler commandHandler(audioEngine, wavetableSynth, fmSynth, karplusSynth, geneticSynth, vocoderSynth, lowpass, chorusEffect, flangerEffect, reverbEffect, buchlaLPG, limiter, compressor, granularEffect, geneticController, dcBlocker, antiAliasFilter, presetManager, routingManager);

char serialBuffer[256];
uint8_t serialBufferPos = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\nESP32 Polyphonic Synthesizer - Professional Edition Starting...");
    
    setCpuFrequencyMhz(240);
    Serial.printf("CPU frequency: %d MHz, Free heap: %u bytes\n", getCpuFrequencyMhz(), esp_get_free_heap_size());
    
    if (!audioEngine.initialize(routingManager)) {
        Serial.println("FATAL: Audio engine initialization failed!");
        while(1);
    }
    
    audioEngine.setAudioSource(&wavetableSynth);
    
    vocoderSynth.setCarrierSource(&wavetableSynth);
    vocoderSynth.setModulatorSource(&fmSynth);

    geneticController.clearParameters();
    geneticController.addParameter("cutoff", 100.0f, 10000.0f);
    geneticController.addParameter("resonance", 0.0f, 0.95f);
    geneticController.setTarget(&lowpass);
    geneticController.setControlMode(GeneticController::BYPASS);
    
    if (audioEngine.start()) {
        Serial.println("Audio engine started successfully. Type 'h' for help.");
    } else {
        Serial.println("FATAL: Failed to start audio engine!");
        while(1);
    }
}

void loop() {
    while (Serial.available()) {
        char inChar = Serial.read();
        if (inChar == '\n' || inChar == '\r') {
            if (serialBufferPos > 0) {
                serialBuffer[serialBufferPos] = '\0';
                commandHandler.handleCommand(serialBuffer);
                serialBufferPos = 0;
            }
        } else if (serialBufferPos < sizeof(serialBuffer) - 1) {
            serialBuffer[serialBufferPos++] = inChar;
        }
    }
    
    static uint32_t lastPerfCheck = 0;
    if (millis() - lastPerfCheck > 5000) {
        lastPerfCheck = millis();
        size_t bufferLevel = audioEngine.getBufferLevel();
        uint32_t underruns = audioEngine.getUnderruns();
        
        if (bufferLevel < (audioEngine.getBufferCapacity() / 8)) {
             Serial.printf("WARN: Low buffer (%u/%u)\n", bufferLevel, audioEngine.getBufferCapacity());
        }
        if (underruns > 0) {
            Serial.printf("WARN: %u underruns\n", underruns);
        }
    }
    
    delay(20);
}