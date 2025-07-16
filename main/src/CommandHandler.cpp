#include "CommandHandler.h"
#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

// Helper function to trim leading/trailing whitespace from a C-style string
void trim(char* str) {
    if (!str) return;
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

CommandHandler::CommandHandler(AudioEngine& engine,
                               WavetableSynth& wavetable,
                               FMSynth& fm,
                               KarplusStrongSynth& karplus,
                               GeneticSynth& genetic,
                               VocoderSynth& vocoder,
                               EnhancedLowPassFilter& lpf,
                               ChorusEffect& chorus,
                               FlangerEffect& flanger,
                               ReverbEffect& reverb,
                               BuchlaLPG& lpg,
                               Limiter& limiter,
                               Compressor& compressor,
                               GranularEffect& granular,
                               GeneticController& geneticCtrl,
                               DCBlocker& dc,
                               AntiAliasFilter& antiAlias,
                               Presets::PresetManager& presets,
                               RoutingManager& routing)
    : audioEngine(engine),
      wavetableSynth(wavetable),
      fmSynth(fm),
      karplusSynth(karplus),
      geneticSynth(genetic),
      vocoderSynth(vocoder),
      lowpass(lpf),
      chorusEffect(chorus),
      flangerEffect(flanger),
      reverbEffect(reverb),
      buchlaLPG(lpg),
      limiter(limiter),
      compressor(compressor),
      granularEffect(granular),
      geneticController(geneticCtrl),
      dcBlocker(dc),
      antiAliasFilter(antiAlias),
      presetManager(presets),
      routingManager(routing)
{
}

void CommandHandler::handleCommand(char* command) {
    trim(command);
    if (strlen(command) == 0) return;

    Serial.print("CMD: "); Serial.println(command);

    char firstChar = command[0];
    char* paramStr = NULL;
    float val = 0;
    int intVal = 0;

    char* token = strtok(command, " ");
    if (token == NULL) return;

    if (strcmp(token, "psave") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            presetManager.savePreset(atoi(token));
            Serial.printf("Saved preset %d\n", atoi(token));
        }
        return;
    } else if (strcmp(token, "pload") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            presetManager.loadPreset(atoi(token));
            Serial.printf("Loaded preset %d\n", atoi(token));
        }
        return;
    } else if (strcmp(token, "pinterp") == 0) {
        char* p1_str = strtok(NULL, " ");
        char* p2_str = strtok(NULL, " ");
        char* factor_str = strtok(NULL, " ");
        if (p1_str && p2_str && factor_str) {
            presetManager.interpolatePresets(atoi(p1_str), atoi(p2_str), atof(factor_str));
            Serial.printf("Interpolated between %s and %s with factor %s\n", p1_str, p2_str, factor_str);
        }
        return;
    } else if (strcmp(token, "route") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            int preset = atoi(token);
            routingManager.setRoutingPreset((RoutingManager::RoutingPreset)preset);
            audioEngine.updateRouting();
            Serial.printf("Routing preset set to %d\n", preset);
        }
        return;
    } else if (strcmp(token, "Cmacadd") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            float controlValue = atof(token);
            float paramValues[MAX_CONTROLLABLE_PARAMETERS];
            int numValues = 0;
            token = strtok(NULL, " ");
            while (token && numValues < MAX_CONTROLLABLE_PARAMETERS) {
                paramValues[numValues++] = atof(token);
                token = strtok(NULL, " ");
            }
            geneticController.addMacroControlPoint(controlValue, paramValues, numValues);
            Serial.printf("Added macro point at %.2f\n", controlValue);
        }
        return;
    } else if (strcmp(token, "Cmacc") == 0) {
        geneticController.clearMacroControlPoints();
        Serial.println("Cleared macro points");
        return;
    } else if (strcmp(token, "Cmacval") == 0) {
        token = strtok(NULL, " ");
        if (token) {
            geneticController.setParameter("macro_control", atof(token));
            Serial.printf("Macro control value set to %.2f\n", atof(token));
        }
        return;
    }


    // Find the start of the parameter
    for (int i = 1; i < strlen(command); ++i) {
        if (isdigit(command[i]) || command[i] == '.' || command[i] == '-') {
            paramStr = &command[i];
            break;
        }
    }

    if (paramStr == NULL && strlen(command) > 1) {
        // For commands like "CHen", "FLen"
        if ((strncmp(command, "CHen", 4) == 0 || strncmp(command, "FLen", 4) == 0 || strncmp(command, "RVen", 4) == 0) && strlen(command) > 4) {
            paramStr = &command[4];
        } else if (strncmp(command, "LPG", 3) == 0 && strlen(command) > 3) {
            // Find first digit for LPG commands e.g. LPGmode, LPGcv
            for (int i = 3; i < strlen(command); ++i) {
                if (isdigit(command[i]) || command[i] == '.' || command[i] == '-') {
                    paramStr = &command[i];
                    break;
                }
            }
        }
    }


    if (paramStr) {
        val = atof(paramStr);
        intVal = atoi(paramStr);
    }

    AudioSource* src = getAudioSourceForMode(currentSynthMode);

    if (strlen(command) == 1) {
        switch (firstChar) {
            case 'n': if (src) src->noteOn(src->getFrequency(), 1.0f); Serial.println("Note ON triggered"); return;
            case 'o': if (src) src->noteOff(); Serial.println("Note OFF triggered"); return;
            case 'P': if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) static_cast<KarplusStrongSynth*>(src)->pluck(); else Serial.println("Command for Karplus-Strong mode only."); return;
            case 's': printDetailedStatus(); return;
            case 'z': audioEngine.resetCounters(); Serial.println("Perf counters reset."); return;
            case 't': testDACDirect(); return;
            case 'h': printHelp(); return;
        }
    }

    if (strncmp(command, "CH", 2) == 0) {
        if (strstr(command, "rate")) chorusEffect.setRate(val);
        else if (strstr(command, "depth")) chorusEffect.setDepth(val);
        else if (strstr(command, "delay")) chorusEffect.setBaseDelay(val);
        else if (strstr(command, "mix")) chorusEffect.setMix(val);
        else if (strstr(command, "fb")) chorusEffect.setFeedback(val);
        else if (strstr(command, "en")) chorusEffect.setEnabled(val > 0.5f);
        else Serial.println("Unknown Chorus parameter.");
        return;
    }

    if (strncmp(command, "FL", 2) == 0) {
        if (strstr(command, "rate")) flangerEffect.setRate(val);
        else if (strstr(command, "depth")) flangerEffect.setDepth(val);
        else if (strstr(command, "delay")) flangerEffect.setBaseDelay(val);
        else if (strstr(command, "fb")) flangerEffect.setFeedback(val);
        else if (strstr(command, "mix")) flangerEffect.setMix(val);
        else if (strstr(command, "en")) flangerEffect.setEnabled(val > 0.5f);
        else Serial.println("Unknown Flanger parameter.");
        return;
    }

    if (strncmp(command, "LPG", 3) == 0) {
        if (strstr(command, "cv")) buchlaLPG.setCv(val);
        else if (strstr(command, "res")) buchlaLPG.setResonance(val);
        else if (strstr(command, "mode")) buchlaLPG.setMode((BuchlaLPG::Mode)intVal);
        else if (strstr(command, "en")) buchlaLPG.setEnabled(val > 0.5f);
        else Serial.println("Unknown LPG parameter.");
        return;
    }

    if (strncmp(command, "RV", 2) == 0) {
        if (strstr(command, "size")) reverbEffect.setRoomSize(val);
        else if (strstr(command, "damp")) reverbEffect.setDamping(val);
        else if (strstr(command, "mix")) reverbEffect.setMix(val);
        else if (strstr(command, "en")) reverbEffect.setEnabled(val > 0.5f);
        else Serial.println("Unknown Reverb parameter.");
        return;
    }

    if (strncmp(command, "VC", 2) == 0) {
        if (strstr(command, "q")) vocoderSynth.setParameter("qFactor", val);
        else if (strstr(command, "atk")) vocoderSynth.setParameter("attackTime", val);
        else if (strstr(command, "rel")) vocoderSynth.setParameter("releaseTime", val);
        else if (strstr(command, "gain")) vocoderSynth.setParameter("outputGain", val);
        else if (strstr(command, "car")) {
            AudioSource* newCarrier = getAudioSourceForMode((SynthMode)intVal);
            if (newCarrier && newCarrier != &vocoderSynth) vocoderSynth.setCarrierSource(newCarrier);
            else Serial.println("Invalid carrier source for vocoder.");
        } else if (strstr(command, "mod")) {
            AudioSource* newModulator = getAudioSourceForMode((SynthMode)intVal);
            if (newModulator && newModulator != &vocoderSynth) vocoderSynth.setModulatorSource(newModulator);
            else Serial.println("Invalid modulator source for vocoder.");
        }
        else Serial.println("Unknown Vocoder parameter.");
        return;
    }

    if (strncmp(command, "FM", 2) == 0 && strlen(command) > 2 && isalpha(command[2])) {
        if (src && src->getSynthType() == SYNTH_TYPE_FM) {
            FMSynth* fm = static_cast<FMSynth*>(src);
            char type = command[2];
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

    if (strncmp(command, "pluck", 5) == 0) {
         if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) {
            static_cast<KarplusStrongSynth*>(src)->pluck(constrain(val, 0.1f, 2.0f));
        } else { Serial.println("Karplus-Strong command only."); }
        return;
    }

    if (strlen(command) > 1 && isalpha(command[1])) {
        char prefix[3] = { command[0], command[1], '\0' };

        if (strcmp(prefix, "gw") == 0) granularEffect.setMix(val);
        else if (strcmp(prefix, "gd") == 0) granularEffect.setDensity(val);
        else if (strcmp(prefix, "gk") == 0) granularEffect.setGrainSize(intVal);
        else if (strcmp(prefix, "gv") == 0) granularEffect.setGrainSizeVariation(val);
        else if (strcmp(prefix, "gu") == 0) granularEffect.setPlaybackRate(val);
        else if (strcmp(prefix, "gy") == 0) granularEffect.setPitchVariation(val);
        else if (strcmp(prefix, "gt") == 0) granularEffect.setTimeShift(val);
        else if (strcmp(prefix, "gx") == 0) granularEffect.setPositionSpray(val);
        else if (strcmp(prefix, "gq") == 0) granularEffect.setWindowType((GranularEffect::WindowType)intVal);
        else if (strcmp(prefix, "gg") == 0) granularEffect.loadPreset(intVal);
        else if (src && src->getSynthType() == SYNTH_TYPE_GENETIC) {
            GeneticSynth* gs = static_cast<GeneticSynth*>(src);
            if (strcmp(prefix, "GA") == 0) gs->setEvolutionMode(static_cast<GeneticSynth::EvolutionMode>(intVal));
            else if (strcmp(command, "GE") == 0) gs->evolveOnce();
            else if (strcmp(prefix, "GG") == 0) gs->setGenome(intVal);
            else if (strcmp(prefix, "GR") == 0) gs->rateCurrentGenome(val);
            else if (strcmp(prefix, "GM") == 0) gs->setMutationRate(val);
            else if (strcmp(prefix, "GC") == 0) gs->setCrossoverRate(val);
            else if (strcmp(prefix, "GP") == 0) gs->loadPreset(intVal);
            else if (strcmp(prefix, "GZ") == 0) gs->randomizeGenome(intVal);
            else if (strcmp(command, "GS") == 0) gs->printStatus();
        }
        else if (strcmp(prefix, "CA") == 0) geneticController.setControlMode(static_cast<GeneticController::ControlMode>(intVal));
        else if (strcmp(command, "CE") == 0) geneticController.evolveOnce();
        else if (strcmp(prefix, "CG") == 0) geneticController.setParameter("genome_index", val);
        else if (strcmp(prefix, "CR") == 0) geneticController.rateCurrentGenome(val);
        else if (strcmp(prefix, "CM") == 0) geneticController.setParameter("mutation_rate", val);
        else if (strcmp(prefix, "CV") == 0) geneticController.setParameter("evolution_rate", val);
        else if (strcmp(prefix, "CP") == 0) geneticController.loadPreset(intVal);
        else if (strcmp(prefix, "CT") == 0) {
            AudioEffect* targetPtr = nullptr;
            if(intVal == 1) { targetPtr = &lowpass; }
            else if(intVal == 2) { targetPtr = &granularEffect; }
            else if(intVal == 3) { targetPtr = &chorusEffect; }
            if(targetPtr) geneticController.setTarget(targetPtr); else geneticController.clearTarget();
            Serial.printf("Genetic Controller target set\n");
        }
        else if (strcmp(command, "CS") == 0) geneticController.printCurrentParameters();
        else if (strcmp(command, "CC") == 0) geneticController.clearParameters();
        else if (src && src->getSynthType() == SYNTH_TYPE_FM) {
            FMSynth* fm = static_cast<FMSynth*>(src);
            if (strcmp(prefix, "mw") == 0) fm->setModulatorWaveform((FMSynth::Waveform)intVal);
            else if (strcmp(prefix, "cw") == 0) fm->setCarrierWaveform((FMSynth::Waveform)intVal);
        }
        else if (strcmp(prefix, "AA") == 0) {
            antiAliasFilter.setParameter("coeff", val);
            Serial.printf("AntiAliasFilter coeff set to: %.4f\n", val);
        }
    }

    switch (firstChar) {
        case 'm':
            if (intVal >= 0 && intVal < NUM_SYNTH_MODES) {
                currentSynthMode = (SynthMode)intVal;
                audioEngine.setAudioSource(getAudioSourceForMode(currentSynthMode));
                Serial.printf("Mode -> %s\n", getSynthModeName(currentSynthMode));
                AudioSource* newSrc = getAudioSourceForMode(currentSynthMode);
                if (newSrc) {
                   if (newSrc->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) {
                        static_cast<KarplusStrongSynth*>(newSrc)->pluck();
                   } else {
                        newSrc->noteOn(newSrc->getFrequency(), 1.0f);
                   }
                }
            } else { Serial.printf("Invalid mode %d\n", intVal); }
            break;
        case 'f': if(src) src->setFrequency(val); break;
        case 'a': if(src) src->setAmplitude(val); break;
        case 'c': lowpass.setCutoff(val); break;
        case 'Q': lowpass.setResonance(val); break;
        case 'w':
            if (src && src->getSynthType() == SYNTH_TYPE_WAVETABLE) static_cast<WavetableSynth*>(src)->setWaveform((WavetableSynth::WaveformType)intVal);
            else Serial.println("Wavetable command only.");
            break;
        case 'g':
            if (src && src->getSynthType() == SYNTH_TYPE_WAVETABLE) {
                WavetableSynth* wt = static_cast<WavetableSynth*>(src);
                // The custom waveform functions need to be accessible here.
                // For now, this feature will be disabled until a proper way to pass them is implemented.
                Serial.println("Custom waveform generation from command handler is not yet supported.");
            } else { Serial.println("Wavetable command only."); }
            break;
        case 'r': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("mod_ratio", val); else Serial.println("FM cmd only."); break;
        case 'i': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("mod_index", val); else Serial.println("FM cmd only."); break;
        case 'b': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setParameter("feedback", val); else Serial.println("FM cmd only."); break;
        case 'l': if (src && src->getSynthType() == SYNTH_TYPE_FM) static_cast<FMSynth*>(src)->setAlgorithm((FMSynth::Algorithm)intVal); else Serial.println("FM cmd only."); break;
        case 'k': if (src && src->getSynthType() == SYNTH_TYPE_KARPLUS_STRONG) static_cast<KarplusStrongSynth*>(src)->setFeedback(constrain(val, 0.9f, 1.0f)); else Serial.println("Karplus cmd only."); break;
        case 'V': audioEngine.setMasterVolume(val); break;
        case 'L': limiter.setThreshold(val); break;
        case 'C': compressor.setThreshold(val); break;
        case 'R': compressor.setRatio(val); break;
        case 'A': compressor.setAttack(val); break;
        case 'X': compressor.setRelease(val); break;
        default:
            // Don't print "Unknown command" for commands that were already handled by prefix checks
            if (strlen(command) > 1 && !isalpha(command[1])) {
                 Serial.println("Unknown command.");
            }
            break;
    }
}

void CommandHandler::printHelp() {
    Serial.println("--- Commands ---");
    Serial.println("m<mode>     - Mode: 0=Wave, 1=FM, 2=Karplus, 3=Genetic, 4=Vocoder");
    Serial.println("f<freq>     - Freq (Hz) for current synth");
    Serial.println("a<level>    - Amplitude/Level (0-127) for current synth");
    Serial.println("n           - Note ON (triggers synth's envelope)");
    Serial.println("o           - Note OFF (releases synth's envelope)");
    Serial.println("--- Presets ---");
    Serial.println("psave <idx> - Save current state to preset");
    Serial.println("pload <idx> - Load preset");
    Serial.println("pinterp <idx1> <idx2> <factor> - Interpolate between presets");
    Serial.println("--- Routing ---");
    Serial.println("route <preset> - Set routing preset (0-2)");
    Serial.println("--- Genetic Controller ---");
    Serial.println("CA<mode>    - Set control mode (0=Bypass, 1=Evolve, 2=Evolve_D, 3=Morph, 4=Macro)");
    Serial.println("CE          - Evolve once");
    Serial.println("CG<idx>     - Set genome index");
    Serial.println("CR<rate>    - Rate current genome");
    Serial.println("CM<rate>    - Set mutation rate");
    Serial.println("CV<rate>    - Set evolution rate");
    Serial.println("CP<preset>  - Load preset");
    Serial.println("CT<target>  - Set target");
    Serial.println("CS          - Print current parameters");
    Serial.println("CC          - Clear parameters");
    Serial.println("Cmacadd <val> <p1> <p2> ... - Add macro control point");
    Serial.println("Cmacc Cmacval <val> - Clear macro points, Set macro value");
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
    Serial.println("LPGcv<0-1> LPGres<0-1> LPGmode<0-2> LPGen<0/1>");
    Serial.println("--- Limiter ---");
    Serial.println("L<thresh_db> - Limiter Threshold (dB)");
    Serial.println("--- Compressor ---");
    Serial.println("C<thresh_db> R<ratio> A<attack_ms> X<release_ms>");
    Serial.println("--- Wavetable (mode 0) ---");
    Serial.println("w<0-5>      - Waveform: Sine,Saw,Square,Tri,Noise,Custom");
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
    Serial.println("s - Status, z - Reset Counters, t - DAC Test, h - Help");
}

void CommandHandler::printDetailedStatus() {
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
        } else if (activeSource == &vocoderSynth) {
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
                  totalSamples, avgSamplerate, esp_get_free_heap_size(), getCpuFrequencyMhz());
    Serial.println("============================================");
}

void CommandHandler::testDACDirect() {
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

AudioSource* CommandHandler::getAudioSourceForMode(SynthMode mode) {
    switch (mode) {
        case WAVETABLE_MODE: return &wavetableSynth;
        case FM_MODE: return &fmSynth;
        case KARPLUS_STRONG_MODE: return &karplusSynth;
        case GENETIC_MODE: return &geneticSynth;
        case VOCODER_MODE: return &vocoderSynth;
        default:
            Serial.printf("Error: Unknown synth mode %d in getAudioSourceForMode\n", mode);
            return &wavetableSynth;
    }
}

SynthMode CommandHandler::getSynthModeFromPointer(AudioSource* src) {
    if (src == &wavetableSynth) return WAVETABLE_MODE;
    if (src == &fmSynth) return FM_MODE;
    if (src == &karplusSynth) return KARPLUS_STRONG_MODE;
    if (src == &geneticSynth) return GENETIC_MODE;
    if (src == &vocoderSynth) return VOCODER_MODE;
    return NUM_SYNTH_MODES;
}

const char* CommandHandler::getSynthModeName(SynthMode mode) {
    switch (mode) {
        case WAVETABLE_MODE: return "Wavetable";
        case FM_MODE: return "FM";
        case KARPLUS_STRONG_MODE: return "Karplus-Strong";
        case GENETIC_MODE: return "Genetic";
        case VOCODER_MODE: return "Vocoder";
        default: return "Unknown";
    }
}
