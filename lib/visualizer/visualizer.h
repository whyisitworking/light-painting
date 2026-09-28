#ifndef VISUALIZER_H
#define VISUALIZER_H

/**
 * The visualizer pipeline, hardware independent: I2S frames in, pixels out.
 *
 *   frames ──► spectrum ──bins──► features ──sound_t──► effects ──► pixels
 *
 * Once per hop: visualizer_analyze() with the newest frames, then
 * visualizer_render() into the next LED frame.
 */

#include "effects.h"
#include "features.h"
#include "spectrum.h"

#include <stddef.h>
#include <stdint.h>

// On top of the microphone's SPECTRUM_INPUT_GAIN, x12 in total: the features'
// FEATURES_MIN_CEILING_DB is tuned for it
constexpr float VISUALIZER_GAIN = 1.5f;

// How the pipeline is built, read by visualizer_init() only. What can
// change while running is in visualizer_tuning_t
typedef struct {
    // Actual I2S sample rate in Hz
    float sample_rate;
    // Mono samples per analysis (a power of two >= 4), and new ones per hop
    size_t fft_size;
    size_t hop_size;
    size_t led_count;
    // Renders are deterministic for a seed
    uint32_t seed;
} visualizer_config_t;

// What can be changed while running: the gain and each stage's tuning,
// the mode and palette among the effects'
typedef struct {
    // VISUALIZER_GAIN: positive. Applied after the window, on top of the
    // microphone's SPECTRUM_INPUT_GAIN
    float gain;
    features_tuning_t features;
    effects_tuning_t effects;
} visualizer_tuning_t;

// The pipeline stages, public for inspection (e.g. features.ceiling_db)
typedef struct {
    float gain;
    spectrum_t spectrum;
    features_t features;
    effects_t effects;
} visualizer_t;

/**
 * Sets up all stages from the config, on visualizer_default_tuning(). False
 * if any rejects it or memory runs out, with nothing left allocated
 */
[[nodiscard]] bool visualizer_init(visualizer_t *this,
                                   const visualizer_config_t *config);

// VISUALIZER_GAIN and the stages' default tunings
visualizer_tuning_t visualizer_default_tuning(void);

/**
 * Takes effect on the next hop, without resetting the analysis or the
 * effects. A value out of range keeps its current one, see the stages'
 * tune functions
 */
void visualizer_tune(visualizer_t *this, const visualizer_tuning_t *tuning);

/**
 * Analyzes hop new stereo frames, left and right words as the I2S driver
 * delivers them
 */
void visualizer_analyze(visualizer_t *this, const int32_t *frames);

/**
 * Renders the newest analysis into led_count color_ws2812_t words.
 * Returns the sound it was rendered from, valid until the next call
 */
const sound_t *visualizer_render(visualizer_t *this, uint32_t *pixels);

// Only after a successful visualizer_init()
void visualizer_deinit(visualizer_t *this);

#endif
