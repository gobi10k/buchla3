#include <Arduino.h>
// Keep these ESP-specific includes as they are used by setup() for CPU frequency
// and potentially by other low-level Arduino functions.
// However, AudioEngine now includes its own ESP-specific headers.

// Include project-specific configuration and global declarations
#include "src/config.h"      // Defines SAMPLE_RATE, BUFFER_SIZE, etc.
#include "src/globals.h"     // Extern declarations for global objects

// Include class headers (AudioEngine will include Buffer, Source, Effect)
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
// New includes
#include "src/FlangerEffect.h"
#include "src/ReverbEffect.h"
#include "src/VocoderSynth.h"
#include "src/Limiter.h"
#include "src/Compressor.h"
#include "src/BuchlaLPG.h"
#include "esp_random.h"

AudioEngine audioEngine;
WavetableSynth wavetableSynth(DEFAULT_SYNTH_FREQUENCY, 100, SINE_WT);
FMSynth fmSynth(DEFAULT_SYNTH_FREQUENCY, 1.0f, 1.0f, 100);
// mainADSR is currently not used as an effect; synths have internal envelopes.
// ADSREnvelope mainADSR(0.02f, 0.2f, 0.8f, 0.5f);

GranularEffect granularEffect(10.0f, 512, 0.3f);
EnhancedLowPassFilter lowpass(5000.0f, 0.1f);
ChorusEffect chorusEffect(0.5f, 5.0f, 20.0f, 0.4f, 0.1f);
FlangerEffect flangerEffect(0.15f, 3.0f, 2.0f, 0.6f, 0.5f); // rate, depthMs, delayMs, fb, mix
ReverbEffect reverbEffect(0.8f, 0.6f, 0.35f);                // roomSize, damping, mix
VocoderSynth vocoderSynth;
DCBlocker dcBlocker;
AntiAliasFilter antiAliasFilter;
KarplusStrongSynth karplusSynth(SAMPLE_RATE);
GeneticSynth geneticSynth(DEFAULT_SYNTH_FREQUENCY, 100);
GeneticController geneticController;
Limiter limiter;
Compressor compressor;
BuchlaLPG buchlaLPG;

SynthMode currentSynthMode = WAVETABLE_MODE;

void testDACDirect();
void printDetailedStatus();
AudioSource* getAudioSourceForMode(SynthMode mode);
const char* getSynthModeName(SynthMode mode);

float customSawtooth(float phase) {
    return 2.0f * (phase / TWO_PI) - 1.0f;
}

float customPulse(float phase) {
    return (phase < PI) ? 1.0f : -1.0f;
}

float customSine3rd(float phase) {
    float fundamental = sin(phase);
    float third = 0.3f * sin(phase * 3.0f);
    return fundamental + third;
}

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
    
    // Vocoder setup: Set default carrier and modulator (e.g., wavetable and fm)
    // Users can change this via serial commands later.
    vocoderSynth.setCarrierSource(&wavetableSynth);
    vocoderSynth.setModulatorSource(&fmSynth); // Or another source like Karplus, or even itself if careful!

    geneticController.clearParameters();
    geneticController.addParameter("cutoff", 100.0f, 10000.0f);
    geneticController.addParameter("resonance", 0.0f, 0.95f);
    geneticController.setTarget(&lowpass);
    geneticController.setControlMode(GeneticController::BYPASS);
    
    if (audioEngine.start()) {
        Serial.println("Audio engine started successfully.");
        Serial.println("--- Commands ---");
        Serial.println("m<mode>     - Mode: 0=Wave, 1=FM, 2=Karplus, 3=Genetic, 4=Vocoder");
        Serial.println("f<freq>     - Freq (Hz) for current synth");
        Serial.println("a<level>    - Amplitude/Level (0-127) for current synth");
        Serial.println("n           - Note ON (triggers synth's envelope)");
        Serial.println("o           - Note OFF (releases synth's envelope)");
        Serial.println("--- LowPass Filter ---");
        Serial.println("c<freq>     - Filter Cutoff (Hz)");
        Serial.println("Q<res>      - Filter Resonance (0.0-1.0)");
        Serial.println("--- Chorus Effect ---");
        Serial.println("CHrate<Hz> CHdepth<ms> CHdelay<ms> CHmix<0-1> CHfb<0-1> CHen<0/1>");
        Serial.println("--- Flanger Effect ---");
        Serial.println("FLrate<Hz> FLdepth<ms> FLdelay<ms> FLfb<0-1> FLmix<0-1> FLen<0/1>");
        Serial.println("--- Reverb Effect ---");
        Serial.println("RVsize<0-1> RVdamp<0-1> RVmix<0-1> RVen<0/1>");
        Serial.println("--- Buchla LPG ---");
        Serial.println("LPGcv<0-1> - CV");
        Serial.println("LPGres<0-1> - Resonance");
        Serial.println("LPGmode<0-2> - Mode (0=Both, 1=VCA, 2=LP)");
        Serial.println("LPGen<0/1> - Enable");
        Serial.println("--- Limiter ---");
        Serial.println("L<thresh_db> - Limiter Threshold (dB)");
        Serial.println("--- Compressor ---");
        Serial.println("C<thresh_db> - Compressor Threshold (dB)");
        Serial.println("R<ratio> - Compressor Ratio");
        Serial.println("A<attack_ms> - Compressor Attack (ms)");
        Serial.println("X<release_ms> - Compressor Release (ms)");
        Serial.println("--- Wavetable (mode 0) ---");
        Serial.println("w<0-5>      - Waveform: Sine,Saw,Square,Tri,Noise,Custom");
        Serial.println("g<0-2>      - Gen Custom: Saw,Pulse,Sine3rd");
        Serial.println("--- FM (mode 1) ---");
        Serial.println("r<ratio> i<index> b<feedback> l<alg> mw<wave> cw<wave>");
        Serial.println("FMA<ms> FMD<ms> FMS<lvl> FMR<ms> - Master Env");
        Serial.println("FMa<ms> FMd<ms> FMs<lvl> FMr<ms> - Modulator Env");
        Serial.println("FMc<ms> FMk<ms> FMx<lvl> FMy<ms> - Carrier Env");
        Serial.println("--- Karplus-Strong (mode 2) ---");
        Serial.println("k<feedback> v<amp> P pluck<str>");
        Serial.println("--- Granular Effect ---");
        Serial.println("gw<mix> gd<dens> gk<size> gv<var> gu<rate> gy<pitchvar> gt<time> gx<spray> gq<win> gg<preset>");
        Serial.println("--- Genetic Synth (mode 3) & Controller ---");
        Serial.println("GA<mode> GE GG<idx> GR<rate> GM<rate> GC<rate> GP<preset> GZ<idx> GS");
        Serial.println("CA<mode> CE CG<idx> CR<rate> CM<rate> CV<rate> CP<preset> CT<target> CS CC");
        Serial.println("--- Vocoder (mode 4) ---");
        Serial.println("VCq<Q> VCatk<sec> VCrel<sec> VCgain<0-2> VCcar<idx> VCmod<idx>");
        Serial.println("--- AntiAliasFilter ---");
        Serial.println("AA<coeff>   - Set AntiAliasFilter coefficient (0.001-1.0)");
        Serial.println("--- System ---");
        Serial.println("V<vol>      - Master Volume (0.0-2.0)");
        Serial.println("s - Status, z - Reset Counters, t - DAC Test");
    } else {
        Serial.println("FATAL: Failed to start audio engine!");
        while(1);
    }
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.length() > 0) {
            Serial.print("CMD: "); Serial.println(command);
            char firstChar = command.charAt(0);
            String paramStr = "";

            int valStartIndex = 1;
            if (command.length() > 1) {
                bool complexPrefix = false;
                // Extend prefix detection for new commands
                if ((command.startsWith("CH") || command.startsWith("FL") || command.startsWith("RV") || command.startsWith("VC")) && command.length() > 2) {
                    complexPrefix = true;
                    for(int i = 2; i < command.length(); ++i) { if(isdigit(command.charAt(i)) || command.charAt(i) == '.' || command.charAt(i) == '-') { valStartIndex = i; break;} }
                    // If no digit found after prefix (e.g. "CHen"), valStartIndex might remain at prefix length or go to end
                    if (valStartIndex == 2 && !(isdigit(command.charAt(2)) || command.charAt(2) == '.' || command.charAt(2) == '-')) {
                         // Check if it's a short command like "FLen" or "RVen"
                         if (command.length() > 3 && (command.charAt(3) == '0' || command.charAt(3) == '1')) { // FLen0, FLen1
                            valStartIndex = 3;
                         } else {
                            valStartIndex = command.length(); // No numeric part, e.g. for toggles without explicit value
                         }
                    }
                } else if (command.startsWith("LPG") && command.length() > 3) {
                    complexPrefix = true;
                    for(int i = 3; i < command.length(); ++i) { if(isdigit(command.charAt(i)) || command.charAt(i) == '.' || command.charAt(i) == '-') { valStartIndex = i; break;} }
                } else if (command.startsWith("FM") && command.length() > 3 && isalpha(command.charAt(2))) {
                    complexPrefix = true;
                    valStartIndex = 3;
                } else if (command.startsWith("pluck") && command.length() > 5) {
                    complexPrefix = true;
                    valStartIndex = 5;
                } else if (isalpha(command.charAt(0)) && command.length() > 1 && isalpha(command.charAt(1))) {
                     complexPrefix = true; // For GA, CA, gw, gd etc.
                     valStartIndex = 2;
                }
                if (valStartIndex < command.length()) {
                    paramStr = command.substring(valStartIndex);
                }
            }

            float val = paramStr.toFloat();
            int intVal = paramStr.toInt();

            AudioSource* src = getAudioSourceForMode(currentSynthMode);

            if (command.length() == 1) {
                 switch (firstChar) {
                    case 'n':
                        if (src) src->noteOn(src->getFrequency(), 1.0f);
                        Serial.println("Note ON triggered"); return;
                    case 'o':
                        if (src) src->noteOff();
                        Serial.println("Note OFF triggered"); return;
                    case 'P':
                        if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) static_cast<KarplusStrongSynth*>(src)->pluck();
                        else Serial.println("Command for Karplus-Strong mode only.");
                        return;
                    case 's': printDetailedStatus(); return;
                    case 'z': audioEngine.resetCounters(); Serial.println("Perf counters reset."); return;
                    case 't': testDACDirect(); return;
                }
            }

            if (command.startsWith("CH")) {
                String chorusCmdPart = command.substring(2);
                String paramName = "";
                // Correctly find the split point for parameter name
                int splitPoint = 0;
                for(int i=0; i < chorusCmdPart.length(); ++i) {
                    if(isdigit(chorusCmdPart.charAt(i)) || chorusCmdPart.charAt(i) == '.' || chorusCmdPart.charAt(i) == '-') {
                        splitPoint = i;
                        break;
                    }
                    paramName += chorusCmdPart.charAt(i); // Build paramName char by char
                }
                 // The value 'val' is already parsed from paramStr at the top, which should be correct if valStartIndex was set right
                if (paramName.length() > 0) {
                    if (paramName == "rate") chorusEffect.setParameter("rate", val);
                    else if (paramName == "depth") chorusEffect.setParameter("depth", val);
                    else if (paramName == "delay") chorusEffect.setParameter("delay", val);
                    else if (paramName == "mix") chorusEffect.setParameter("mix", val);
                    else if (paramName == "fb") chorusEffect.setParameter("feedback", val);
                    else if (paramName == "en") chorusEffect.setParameter("enabled", val > 0.5f); // Handle 0/1 for enabled
                    else Serial.println("Unknown Chorus parameter.");
                } else { Serial.println("Invalid Chorus command format.");}
                return;
            }

            if (command.startsWith("FL")) { // Flanger commands
                String flangerCmdPart = command.substring(2);
                String paramName = "";
                int splitPoint = 0;
                for(int i=0; i < flangerCmdPart.length(); ++i) {
                    if(isdigit(flangerCmdPart.charAt(i)) || flangerCmdPart.charAt(i) == '.' || flangerCmdPart.charAt(i) == '-') {
                        splitPoint = i;
                        break;
                    }
                    paramName += flangerCmdPart.charAt(i);
                }
                if (paramName.length() > 0) {
                    if (paramName == "rate") flangerEffect.setParameter("rate", val);
                    else if (paramName == "depth") flangerEffect.setParameter("depth", val);
                    else if (paramName == "delay") flangerEffect.setParameter("delay", val);
                    else if (paramName == "fb") flangerEffect.setParameter("feedback", val);
                    else if (paramName == "mix") flangerEffect.setParameter("mix", val);
                    else if (paramName == "en") flangerEffect.setParameter("enabled", val > 0.5f);
                    else Serial.println("Unknown Flanger parameter.");
                } else { Serial.println("Invalid Flanger command format.");}
                return;
            }

            if (command.startsWith("LPG")) { // Buchla LPG commands
                String lpgCmdPart = command.substring(3);
                String paramName = "";
                int splitPoint = 0;
                for(int i=0; i < lpgCmdPart.length(); ++i) {
                    if(isdigit(lpgCmdPart.charAt(i)) || lpgCmdPart.charAt(i) == '.' || lpgCmdPart.charAt(i) == '-') {
                        splitPoint = i;
                        break;
                    }
                    paramName += lpgCmdPart.charAt(i);
                }
                if (paramName.length() > 0) {
                    if (paramName == "cv") buchlaLPG.setParameter("cv", val);
                    else if (paramName == "res") buchlaLPG.setParameter("resonance", val);
                    else if (paramName == "mode") buchlaLPG.setParameter("mode", val);
                    else if (paramName == "en") buchlaLPG.setParameter("enabled", val > 0.5f);
                    else Serial.println("Unknown LPG parameter.");
                } else { Serial.println("Invalid LPG command format.");}
                return;
            }

            if (command.startsWith("RV")) { // Reverb commands
                String reverbCmdPart = command.substring(2);
                String paramName = "";
                int splitPoint = 0;
                for(int i=0; i < reverbCmdPart.length(); ++i) {
                    if(isdigit(reverbCmdPart.charAt(i)) || reverbCmdPart.charAt(i) == '.' || reverbCmdPart.charAt(i) == '-') {
                        splitPoint = i;
                        break;
                    }
                    paramName += reverbCmdPart.charAt(i);
                }
                if (paramName.length() > 0) {
                    if (paramName == "size") reverbEffect.setParameter("roomSize", val);
                    else if (paramName == "damp") reverbEffect.setParameter("damping", val);
                    else if (paramName == "mix") reverbEffect.setParameter("mix", val);
                    else if (paramName == "en") reverbEffect.setParameter("enabled", val > 0.5f);
                    else Serial.println("Unknown Reverb parameter.");
                } else { Serial.println("Invalid Reverb command format.");}
                return;
            }

            if (command.startsWith("VC")) { // Vocoder commands
                String vocoderCmdPart = command.substring(2);
                String paramName = "";
                int splitPoint = 0;
                for(int i=0; i < vocoderCmdPart.length(); ++i) {
                    if(isdigit(vocoderCmdPart.charAt(i)) || vocoderCmdPart.charAt(i) == '.' || vocoderCmdPart.charAt(i) == '-') {
                        splitPoint = i;
                        break;
                    }
                    paramName += vocoderCmdPart.charAt(i);
                }
                if (paramName.length() > 0) {
                    if (paramName == "q") vocoderSynth.setParameter("qFactor", val);
                    else if (paramName == "atk") vocoderSynth.setParameter("attackTime", val);
                    else if (paramName == "rel") vocoderSynth.setParameter("releaseTime", val);
                    else if (paramName == "gain") vocoderSynth.setParameter("outputGain", val);
                    else if (paramName == "car") {
                        AudioSource* newCarrier = getAudioSourceForMode((SynthMode)intVal);
                        if (newCarrier && newCarrier != &vocoderSynth) vocoderSynth.setCarrierSource(newCarrier);
                        else Serial.println("Invalid carrier source for vocoder.");
                    } else if (paramName == "mod") {
                        AudioSource* newModulator = getAudioSourceForMode((SynthMode)intVal);
                        if (newModulator && newModulator != &vocoderSynth) vocoderSynth.setModulatorSource(newModulator);
                        else Serial.println("Invalid modulator source for vocoder.");
                    }
                    else Serial.println("Unknown Vocoder parameter.");
                } else { Serial.println("Invalid Vocoder command format.");}
                return;
            }

            if (command.length() > 3 && command.startsWith("FM") && isalpha(command.charAt(2))) {
                if (src && src->getSynthType() == SYNTH_TYPE_FM) {
                    FMSynth* fm = static_cast<FMSynth*>(src);
                    char type = command.charAt(2);
                    switch(type) {
                        case 'A': fm->setAttack(val); break; case 'D': fm->setDecay(val); break;
                        case 'S': fm->setSustain(val); break; case 'R': fm->setRelease(val); break;
                        case 'a': fm->setModulatorAttack(val); break; case 'd': fm->setModulatorDecay(val); break;
                        case 's': fm->setModulatorSustain(val); break; case 'r': fm->setModulatorRelease(val); break;
                        case 'c': fm->setCarrierAttack(val); break; case 'k': fm->setCarrierDecay(val); break;
                        case 'x': fm->setCarrierSustain(val); break; case 'y': fm->setCarrierRelease(val); break;
                        default: Serial.println("Unknown FM envelope param."); break;
                    }
                } else { Serial.println("FM mode command only."); }
                return;
            }

            if (command.startsWith("pluck")) {
                 if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) {
                    static_cast<KarplusStrongSynth*>(src)->pluck(constrain(val, 0.1f, 2.0f));
                } else { Serial.println("Karplus-Strong command only."); }
                return;
            }

            if (command.length() > 1 && isalpha(command.charAt(1))) {
                String prefix = command.substring(0, 2);

                if (prefix == "gw") granularEffect.setParameter("dry_wet", val);
                else if (prefix == "gd") granularEffect.setParameter("density", val);
                else if (prefix == "gk") granularEffect.setParameter("grain_size", val);
                else if (prefix == "gv") granularEffect.setParameter("grain_variation", val);
                else if (prefix == "gu") granularEffect.setParameter("playback_rate", val);
                else if (prefix == "gy") granularEffect.setParameter("pitch_variation", val);
                else if (prefix == "gt") granularEffect.setParameter("time_shift", val);
                else if (prefix == "gx") granularEffect.setParameter("spray", val);
                else if (prefix == "gq") granularEffect.setWindowType((GranularEffect::WindowType)intVal);
                else if (prefix == "gg") granularEffect.loadPreset(intVal);
                else if (src && src->getSynthType() == SYNTH_TYPE_GENETIC) {
                    GeneticSynth* gs = static_cast<GeneticSynth*>(src);
                    if (prefix == "GA") gs->setEvolutionMode(static_cast<GeneticSynth::EvolutionMode>(intVal));
                    else if (command == "GE") gs->evolveOnce();
                    else if (prefix == "GG") gs->setGenome(intVal);
                    else if (prefix == "GR") gs->rateCurrentGenome(val);
                    else if (prefix == "GM") gs->setMutationRate(val);
                    else if (prefix == "GC") gs->setCrossoverRate(val);
                    else if (prefix == "GP") gs->loadPreset(intVal);
                    else if (prefix == "GZ") gs->randomizeGenome(intVal);
                    else if (command == "GS") gs->printStatus();
                }
                else if (prefix == "CA") geneticController.setControlMode(static_cast<GeneticController::ControlMode>(intVal));
                else if (command == "CE") geneticController.evolveOnce();
                else if (prefix == "CG") geneticController.setParameter("genome_index", val);
                else if (prefix == "CR") geneticController.rateCurrentGenome(val);
                else if (prefix == "CM") geneticController.setParameter("mutation_rate", val); // Corrected
                else if (prefix == "CV") geneticController.setParameter("evolution_rate", val); // Corrected
                else if (prefix == "CP") geneticController.loadPreset(intVal);
                else if (prefix == "CT") {
                    AudioEffect* targetPtr = nullptr; String targetName = "None";
                    if(intVal == 1) { targetPtr = &lowpass; targetName = "Lowpass"; }
                    else if(intVal == 2) { targetPtr = &granularEffect; targetName = "Granular"; }
                    else if(intVal == 3) { targetPtr = &chorusEffect; targetName = "Chorus"; }
                    if(targetPtr) geneticController.setTarget(targetPtr); else geneticController.clearTarget();
                    Serial.printf("Genetic Controller target set to %s\n", targetName.c_str());
                }
                else if (command == "CS") geneticController.printCurrentParameters();
                else if (command == "CC") geneticController.clearParameters();
                else if (src && src->getSynthType() == SYNTH_TYPE_FM) {
                    FMSynth* fm = static_cast<FMSynth*>(src);
                    if (prefix == "mw") fm->setModulatorWaveform((FMSynth::Waveform)intVal);
                    else if (prefix == "cw") fm->setCarrierWaveform((FMSynth::Waveform)intVal);
                }
                else if (prefix == "AA") { // AntiAliasFilter coefficient
                    antiAliasFilter.setParameter("coeff", val);
                    Serial.printf("AntiAliasFilter coeff set to: %.4f\n", val); // Provide feedback
                }
            }

            // This re-parse was likely causing issues if paramStr was already correctly set by the prefix logic.
            // if (command.length() > 1 && !isalpha(command.charAt(1))) {
            //      paramStr = command.substring(1);
            //      val = paramStr.toFloat();
            //      intVal = paramStr.toInt();
            // }

            switch (firstChar) {
                case 'm':
                    if (intVal >= 0 && intVal < NUM_SYNTH_MODES) {
                        currentSynthMode = (SynthMode)intVal;
                        audioEngine.setAudioSource(getAudioSourceForMode(currentSynthMode));
                        Serial.printf("Mode -> %s\n", getSynthModeName(currentSynthMode));
                        AudioSource* newSrc = getAudioSourceForMode(currentSynthMode);
                        if (newSrc) { // Check if newSrc is not null
                           if (newSrc->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) {
                                static_cast<KarplusStrongSynth*>(newSrc)->pluck();
                           } else {
                                newSrc->noteOn(newSrc->getFrequency(), 1.0f);
                           }
                        }
                    } else { Serial.printf("Invalid mode %d\n", intVal); }
                    break;
                case 'f':
                    if(src) src->setFrequency(val);
                    // Serial.printf("Freq -> %.1f Hz for %s\n", val, getSynthModeName(currentSynthMode)); // Redundant if setter prints
                    break;
                case 'a':
                    if(src) src->setAmplitude(val);
                    // Serial.printf("Level -> %.0f for %s\n", val, getSynthModeName(currentSynthMode)); // Redundant
                    break;
                case 'c': lowpass.setParameter("cutoff", val); break;
                case 'Q': lowpass.setParameter("resonance", val); break;
                case 'w':
                    if (src && src->getSynthType() == SYNTH_TYPE_WAVETABLE) static_cast<WavetableSynth*>(src)->setWaveform((WavetableSynth::WaveformType)intVal);
                    else Serial.println("Wavetable command only.");
                    break;
                case 'g':
                    if (src && src->getSynthType() == SYNTH_TYPE_WAVETABLE) {
                        WavetableSynth* wt = static_cast<WavetableSynth*>(src);
                        if (intVal == 0) wt->generateCustomWaveform(customSawtooth);
                        else if (intVal == 1) wt->generateCustomWaveform(customPulse);
                        else if (intVal == 2) wt->generateCustomWaveform(customSine3rd);
                    } else { Serial.println("Wavetable command only."); }
                    break;
                case 'r': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("mod_ratio", val); else Serial.println("FM cmd only."); break;
                case 'i': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("mod_index", val); else Serial.println("FM cmd only."); break;
                case 'b': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("feedback", val); else Serial.println("FM cmd only."); break;
                case 'l': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setAlgorithm((FMSynth::Algorithm)intVal); else Serial.println("FM cmd only."); break;
                case 'k': if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) static_cast<KarplusStrongSynth*>(src)->setFeedback(constrain(val, 0.9f, 1.0f)); else Serial.println("Karplus cmd only."); break;
                // case 'v': // 'v' was for Karplus amplitude, now 'V' is master volume. Ensure no conflict or repurpose if Karplus still needs it.
                // For now, Karplus 'v' command is removed to avoid conflict with master Volume 'V'.
                // If Karplus still needs an amplitude command, it should be different, e.g., "KA<amp>"
                // Or, if 'v' is truly essential, then Master Volume needs a different char.
                // Assuming 'v' for Karplus amplitude is less critical than 'V' for Master Volume for now.
                // If Karplus setAmplitude was using 'v', it will need a new command.
                // Let's assume Karplus setAmplitude is like other synths via 'a' command for now.
                case 'V': // Master Volume
                    audioEngine.setMasterVolume(val);
                    // audioEngine.setMasterVolume already prints the new volume
                    break;
                case 'L': limiter.setParameter("threshold", val); break;
                case 'C': compressor.setParameter("threshold", val); break;
                case 'R': compressor.setParameter("ratio", val); break;
                case 'A': compressor.setParameter("attack", val); break;
                case 'X': compressor.setParameter("release", val); break;
                default:
                    bool known_prefix = false;
                    if(command.length() > 1 && isalpha(command.charAt(1))) {
                        String pf = command.substring(0,2);
                        if (pf=="gw"||pf=="gd"||pf=="gk"||pf=="gv"||pf=="gu"||pf=="gy"||pf=="gt"||pf=="gx"||pf=="gq"||pf=="gg"||
                            pf=="GA"||pf=="GE"||pf=="GG"||pf=="GR"||pf=="GM"||pf=="GC"||pf=="GP"||pf=="GZ"||pf=="GS"||
                            pf=="CA"||pf=="CE"||pf=="CG"||pf=="CR"||pf=="CM"||pf=="CV"||pf=="CP"||pf=="CT"||pf=="CS"||pf=="CC"||
                            pf=="mw"||pf=="cw"||pf=="AA" ) known_prefix = true; // Added AA
                    }
                    if (!known_prefix && command.length() > 0 && command.charAt(0) != ' ' && command.charAt(0) != '\n' && command.charAt(0) != '\r') {
                         if(command.length() > 3 && command.startsWith("FM") && isalpha(command.charAt(2))) {} // Handled
                         else if (command.startsWith("CH") || command.startsWith("FL") || command.startsWith("RV") || command.startsWith("VC")) {} // Handled
                         else if (command.startsWith("pluck")) {} // Handled
                         else if (command.length() !=1 &&
                                  !(firstChar == 'n' || firstChar == 'o' || firstChar == 'P' || firstChar == 's' || firstChar == 'z' || firstChar == 't')) {
                            Serial.println("Unknown command.");
                         }
                    }
                    break;
            }
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

void printDetailedStatus() {
    uint32_t totalSamples = audioEngine.getSamplesProcessed();
    uint32_t uptime = millis() / 1000;
    float avgSampleRate = uptime > 0 ? (float)totalSamples / uptime : 0;

    Serial.println("\n=== ESP32 Polyphonic Synthesizer Status ===");

    AudioSource* activeSource = audioEngine.getCurrentAudioSource();
    Serial.printf("Mode: %s\n", getSynthModeName(currentSynthMode));

    if (activeSource) {
        SynthType type = activeSource->getSynthType();
        if (type == SYNTH_TYPE_WAVETABLE) {
            WavetableSynth* wt = static_cast<WavetableSynth*>(activeSource);
            Serial.printf("  Wavetable: Wave=%s, Freq=%.1f Hz, Amp=%.0f\n",
                WavetableSynth::getWaveformName(wt->getWaveformType()), wt->getFrequency(), wt->getAmplitude());
        } else if (type == SYNTH_TYPE_FM) {
            FMSynth* fm = static_cast<FMSynth*>(activeSource);
            Serial.printf("  FM: Alg=%s, BaseFreq=%.1f Hz, Lvl=%.0f\n",
                FMSynth::getAlgorithmName(fm->getAlgorithm()), fm->getFrequency(), fm->getAmplitude());
            Serial.printf("    Mod: Ratio=%.2f, Idx=%.2f, Wave=%s, FB=%.2f\n",
                fm->getModulatorRatio(), fm->getModulationIndex(), FMSynth::getWaveformName(fm->getModulatorWaveform()), fm->getFeedback());
            Serial.printf("    Car: Wave=%s\n", FMSynth::getWaveformName(fm->getCarrierWaveform()));
        } else if (type == SYNTH_TYPE_KARPLUS_STRONG) {
            KarplusStrongSynth* ks = static_cast<KarplusStrongSynth*>(activeSource);
            Serial.printf("  Karplus: Freq=%.1f Hz, FB=%.4f, Amp=%.2f, Active=%s\n",
                ks->getFrequency(), ks->getCurrentFeedback(), ks->getAmplitude(), ks->isActive() ? "Y" : "N");
        } else if (type == SYNTH_TYPE_GENETIC) {
            GeneticSynth* gs = static_cast<GeneticSynth*>(activeSource);
            gs->printStatus();
        } else if (type == SYNTH_TYPE_UNKNOWN && activeSource == &vocoderSynth) { // Assuming VocoderSynth returns UNKNOWN or new type
            Serial.printf("  Vocoder: Q=%.1f, Atk=%.3fs, Rel=%.3fs, Gain=%.2f\n",
                vocoderSynth.getQFactor(),
                vocoderSynth.getAttackTime(),
                vocoderSynth.getReleaseTime(),
                vocoderSynth.getOutputGain());
            AudioSource* cSrc = vocoderSynth.getCarrierSource();
            AudioSource* mSrc = vocoderSynth.getModulatorSource();
            Serial.printf("    Carrier: %s, Modulator: %s\n",
                cSrc ? getSynthModeName(getSynthModeFromPointer(cSrc)) : "None",
                mSrc ? getSynthModeName(getSynthModeFromPointer(mSrc)) : "None");
        }
    }

    Serial.printf("LPF: Cutoff=%.1f Hz, Res=%.2f, En=%s\n", lowpass.getCutoff(), lowpass.getResonance(), lowpass.isEnabled() ? "Y":"N");
    Serial.printf("Chorus: Rate=%.2fHz, Depth=%.1fms, Delay=%.1fms, Mix=%.2f, FB=%.2f, En=%s\n",
                  chorusEffect.getRate(), chorusEffect.getDepth(), chorusEffect.getBaseDelay(),
                  chorusEffect.getMix(), chorusEffect.getFeedback(), chorusEffect.isEnabled() ? "Y":"N");
    Serial.printf("Flanger: Rate=%.2fHz, Depth=%.1fms, Delay=%.1fms, Mix=%.2f, FB=%.2f, En=%s\n",
                  flangerEffect.getRate(), flangerEffect.getDepth(), flangerEffect.getBaseDelay(),
                  flangerEffect.getMix(), flangerEffect.getFeedback(), flangerEffect.isEnabled() ? "Y":"N");
    Serial.printf("Reverb: Size=%.2f, Damp=%.2f, Mix=%.2f, En=%s\n",
                  reverbEffect.getRoomSize(), reverbEffect.getDamping(),
                  reverbEffect.getMix(), reverbEffect.isEnabled() ? "Y":"N");
    Serial.printf("BuchlaLPG: CV=%.2f, Resonance=%.2f, Mode=%d, En=%s\n",
buchlaLPG.getParameter("cv"), buchlaLPG.getParameter("resonance"), (int)buchlaLPG.getParameter("mode"), buchlaLPG.isEnabled() ? "Y" : "N");
    Serial.printf("Limiter: Thresh=%.1f dB, En=%s\n", limiter.getParameter("threshold"), limiter.isEnabled() ? "Y":"N");
    Serial.printf("Compressor: Thresh=%.1f dB, Ratio=%.1f:1, Atk=%.2fms, Rel=%.2fms, En=%s\n", compressor.getParameter(std::string("threshold")), compressor.getParameter(std::string("ratio")), compressor.getParameter(std::string("attack")), compressor.getParameter(std::string("release")), compressor.isEnabled() ? "Y":"N");
    Serial.printf("Granular: Wet=%.2f, Dens=%.1f, Size=%u, Rate=%.2f, En=%s\n",
                  granularEffect.getDryWetMix(), granularEffect.getGrainDensity(),
                  granularEffect.getBaseGrainSize(), granularEffect.getPlaybackRate(), granularEffect.isEnabled() ? "Y":"N");

    if (geneticController.getTargetEffect() || geneticController.getTargetSource()) {
        Serial.printf("GeneticCtrl: Targetting, Genome=%d, Gen=%d, En=%s\n",
                      geneticController.getCurrentGenome(), geneticController.getGenerationCount(), geneticController.isEnabled() ? "Y":"N");
    } else {
        Serial.println("GeneticCtrl: No target set.");
    }

    Serial.printf("Audio Engine: MasterVol=%.2f, Buf=%u/%u (%.0f%%), Underrun=%u, Overrun=%u\n",
                  audioEngine.getMasterVolume(),
                  audioEngine.getBufferLevel(), audioEngine.getBufferCapacity(),
                  audioEngine.getBufferCapacity() > 0 ? ((float)audioEngine.getBufferLevel() / audioEngine.getBufferCapacity() * 100.0f) : 0.0f,
                  audioEngine.getUnderruns(), audioEngine.getOverruns());
    Serial.printf("Sys: Samples=%u, AvgRate=%.1f Hz, Heap=%u, CPU=%u MHz\n",
                  totalSamples, avgSampleRate, esp_get_free_heap_size(), getCpuFrequencyMhz());
    Serial.println("============================================");
}

void testDACDirect() {
    Serial.println("DAC Test: Stopping audio engine...");
    audioEngine.stop();

    dac_output_enable(DAC_CHANNEL_1);
    Serial.println("Outputting 1kHz square wave for 3s on DAC...");
    unsigned long startTime = millis();
    bool high = false;
    while (millis() - startTime < 3000) {
        dac_output_voltage(DAC_CHANNEL_1, high ? 220 : 30);
        high = !high;
        delayMicroseconds(500);
    }
    dac_output_voltage(DAC_CHANNEL_1, 128);
    Serial.println("DAC test done. Restarting audio engine...");
    audioEngine.start();
}

AudioSource* getAudioSourceForMode(SynthMode mode) {
    switch (mode) {
        case WAVETABLE_MODE: return &wavetableSynth;
        case FM_MODE: return &fmSynth;
        case KARPLUS_STRONG_MODE: return &karplusSynth;
        case GENETIC_MODE: return &geneticSynth;
        case VOCODER_MODE: return &vocoderSynth; // New
        default:
            Serial.printf("Error: Unknown synth mode %d in getAudioSourceForMode\n", mode);
            return &wavetableSynth; // Default to a known safe source
    }
}

// Helper to get SynthMode from a pointer, for Vocoder status display
SynthMode getSynthModeFromPointer(AudioSource* src) {
    if (src == &wavetableSynth) return WAVETABLE_MODE;
    if (src == &fmSynth) return FM_MODE;
    if (src == &karplusSynth) return KARPLUS_STRONG_MODE;
    if (src == &geneticSynth) return GENETIC_MODE;
    if (src == &vocoderSynth) return VOCODER_MODE;
    return NUM_SYNTH_MODES; // Represents an unknown or unmapped source
}


const char* getSynthModeName(SynthMode mode) {
    switch (mode) {
        case WAVETABLE_MODE: return "Wavetable";
        case FM_MODE: return "FM";
        case KARPLUS_STRONG_MODE: return "Karplus-Strong";
        case GENETIC_MODE: return "Genetic";
        case VOCODER_MODE: return "Vocoder"; // New
        default: return "Unknown";
    }
}