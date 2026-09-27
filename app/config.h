#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * Build time configuration: the board wiring and the visualizer settings
 */

#include "effects.h"

#include <stddef.h>
#include <stdint.h>

// Each analysis covers the last AUDIO_FFT_SIZE mono samples and runs every
// AUDIO_HOP_SIZE new ones. At fs = 48828 Hz, 512 / 256:
//   window 10.5 ms, bin width fs / size = 95 Hz, a new analysis every 5.2 ms
// Larger sizes resolve lower frequencies, smaller ones react faster
constexpr size_t AUDIO_FFT_SIZE = 512;
constexpr size_t AUDIO_HOP_SIZE = 256;

// One mono sample per stereo frame: a left and a right word
constexpr size_t AUDIO_WORDS_PER_FRAME = 2;

static_assert(AUDIO_FFT_SIZE >= 4 &&
                  (AUDIO_FFT_SIZE & (AUDIO_FFT_SIZE - 1)) == 0,
              "AUDIO_FFT_SIZE must be a power of two >= 4");
static_assert(AUDIO_HOP_SIZE >= 1 && AUDIO_HOP_SIZE <= AUDIO_FFT_SIZE,
              "AUDIO_HOP_SIZE must be between 1 and AUDIO_FFT_SIZE");
// The audio DMA streams into a hardware ring of two hops: a power of two, at
// most 32 KB
static_assert((AUDIO_HOP_SIZE & (AUDIO_HOP_SIZE - 1)) == 0 &&
                  AUDIO_HOP_SIZE <= 2048,
              "AUDIO_HOP_SIZE must be a power of two, at most 2048");

constexpr size_t LED_COUNT = 300;

// Header pins, SCK and WS must be consecutive
constexpr unsigned MIC_SCK_PIN = 26;
constexpr unsigned MIC_WS_PIN = 27;
constexpr unsigned MIC_DATA_PIN = 28;

static_assert(MIC_WS_PIN == MIC_SCK_PIN + 1,
              "MIC_WS_PIN must follow MIC_SCK_PIN, one side-set drives both");

constexpr unsigned LED_DATA_PIN = 8;

// Visualizer look, see lib/effects/effects.h and lib/effects/palette.h
constexpr effects_mode_t VISUALIZER_MODE = EFFECTS_MODE_RIVER;
constexpr palette_t VISUALIZER_PALETTE = PALETTE_SYNTHWAVE;

// On top of the microphone's SPECTRUM_INPUT_GAIN, x12 in total: the features'
// FEATURES_MIN_CEILING_DB is tuned for it
constexpr float VISUALIZER_GAIN = 1.5f;

// Sparkle pattern, renders are deterministic for a seed
constexpr uint32_t VISUALIZER_SEED = 1;

#endif
