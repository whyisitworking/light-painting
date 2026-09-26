#ifndef VISUALIZER_H
#define VISUALIZER_H

/**
 * The visualizer pipeline, hardware independent: I2S frames in, pixels out.
 *
 *   frames ──► audio ──bins──► features ──features_t──► effects ──► pixels
 *
 * Once per hop: visualizer_analyze() with the newest frames, then
 * visualizer_render() into the next LED frame.
 */

#include "audio.h"
#include "effects.h"
#include "features.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Everything the pipeline needs, read by visualizer_init() only
typedef struct {
    // Actual I2S sample rate in Hz
    float sample_rate;
    // Mono samples per analysis (a power of two >= 4), and new ones per hop
    size_t fft_size;
    size_t hop;
    size_t led_count;
    // Applied after the window, on top of the microphone's AUDIO_INPUT_GAIN
    float gain;
    effects_mode_t mode;
    effects_palette_t palette;
    // Renders are deterministic for a seed
    uint32_t seed;
} visualizer_config_t;

// The pipeline stages, public for inspection (e.g. features.ceiling_db)
typedef struct {
    float gain;
    audio_t audio;
    features_state_t features;
    effects_t effects;
} visualizer_t;

/**
 * Sets up all stages from the config. False if any rejects it or memory
 * runs out, with nothing left allocated
 */
bool visualizer_init(visualizer_t *this, const visualizer_config_t *config);

/**
 * Analyzes hop new stereo frames, left and right words as the I2S driver
 * delivers them
 */
void visualizer_analyze(visualizer_t *this, const int32_t *frames);

/**
 * Renders the newest analysis into led_count color_neopixel_t words.
 * Returns the features it was rendered from, valid until the next call
 */
const features_t *visualizer_render(visualizer_t *this, uint32_t *pixels);

// Only after a successful visualizer_init()
void visualizer_deinit(visualizer_t *this);

#endif
