#ifndef CONFIG_H
#define CONFIG_H

/**
 * Build time configuration: the board wiring and the visualizer settings
 */

// Each analysis covers the last AUDIO_FFT_SIZE mono samples and runs every
// AUDIO_FFT_HOP new ones. At fs = 48828 Hz, 512 / 256:
//   window 10.5 ms, bin width fs / size = 95 Hz, a new analysis every 5.2 ms
// Larger sizes resolve lower frequencies, smaller ones react faster
#define AUDIO_FFT_SIZE 512
#define AUDIO_FFT_HOP 256

// One mono sample per stereo frame: a left and a right word
#define AUDIO_WORDS_PER_FRAME 2

_Static_assert(AUDIO_FFT_SIZE >= 4 &&
                   (AUDIO_FFT_SIZE & (AUDIO_FFT_SIZE - 1)) == 0,
               "AUDIO_FFT_SIZE must be a power of two >= 4");
_Static_assert(AUDIO_FFT_HOP >= 1 && AUDIO_FFT_HOP <= AUDIO_FFT_SIZE,
               "AUDIO_FFT_HOP must be between 1 and AUDIO_FFT_SIZE");
// The audio DMA streams into a hardware ring of two hops: a power of two, at
// most 32 KB
_Static_assert((AUDIO_FFT_HOP & (AUDIO_FFT_HOP - 1)) == 0 &&
                   AUDIO_FFT_HOP <= 2048,
               "AUDIO_FFT_HOP must be a power of two, at most 2048");

#define LED_COUNT 300

// Pico 2 header pins, SCK and WS must be consecutive
#define MIC_SCK_PIN 26
#define MIC_WS_PIN 27
#define MIC_DATA_PIN 28

#define LED_DATA_PIN 8

// Visualizer look, see lib/effects/effects.h and lib/effects/palette.h
#define VISUALIZER_MODE EFFECTS_RIVER
#define VISUALIZER_PALETTE PALETTE_SYNTHWAVE

// On top of the microphone's AUDIO_INPUT_GAIN, x12 in total: the features'
// FEATURES_MIN_CEILING_DB is tuned for it
#define VISUALIZER_GAIN 1.5f

// Sparkle pattern, renders are deterministic for a seed
#define VISUALIZER_SEED 1

#endif
