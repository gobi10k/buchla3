#pragma once

// Audio configuration constants
#define SAMPLE_RATE 44100        // Hz - optimized for ESP32
#define AUDIO_BUFFER_SIZE 1024   // Increased buffer size for stability (was BUFFER_SIZE 512, now double buffer of 512 = 1024 total)
#define AUDIO_BUFFER_CHUNK_SIZE (AUDIO_BUFFER_SIZE / 4) // Size of chunks to process in audio task
#define AUDIO_CORE 1             // Dedicated core for audio processing task (Core 1 often preferred if Core 0 handles WiFi/BT)
#define DAC_CHANNEL_1 DAC_CHANNEL_1 // GPIO25 for DAC output
#define DAC_CHANNEL_2 DAC_CHANNEL_2 // GPIO26 for DAC output
#define AUDIO_TASK_PRIORITY (configMAX_PRIORITIES - 1) // Highest possible priority for audio task
#define AUDIO_TASK_STACK_SIZE 4096 // Stack size for the audio processing task

#define MAX_EFFECTS 8            // Maximum number of effects in the audio chain

// Genetic Controller specific constants
#define MAX_CONTROLLABLE_PARAMETERS 8
#define CONTROLLER_POPULATION_SIZE 6


// Math constants (if not available or for consistency)
#ifndef PI
#define PI 3.14159265358979323846f // Use f suffix for float constants
#endif

#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif

// Debounce delay for serial commands or UI interactions (in milliseconds)
#define DEBOUNCE_DELAY_MS 50

// Default frequency for synths if not specified
#define DEFAULT_SYNTH_FREQUENCY 440.0f

// MIDI Configuration (if MIDI input is added later)
// #define MIDI_CHANNEL 1
// #define MIDI_BAUD_RATE 31250
