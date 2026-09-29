#ifndef PREVIEW_API_H
#define PREVIEW_API_H

/**
 * The preview tool's engine: the firmware's analysis and effects driven by
 * samples from a browser, for every mode at once. Compiled natively for the
 * host tests and to WebAssembly for the page (tools/preview/build.sh).
 *
 *   samples ─► hops of 256 ─► spectrum ─► features ─► effects, one per mode
 *
 * It repeats the few lines of visualizer.c that chain the stages, because a
 * visualizer_t holds a single effects instance; test_preview checks that the
 * pixels are the visualizer's. Settings go through settings_set() and
 * settings_tuning(), so a menu state looks here as it does on the board.
 *
 * The page talks to it through plain integers, floats and pointers: the
 * functions marked PREVIEW_EXPORT are the WebAssembly exports.
 */

#include "effects.h"
#include "settings.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define PREVIEW_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define PREVIEW_EXPORT
#endif

constexpr size_t PREVIEW_LED_COUNT = 300;
constexpr size_t PREVIEW_FFT_SIZE = 512;
constexpr size_t PREVIEW_HOP_SIZE = 256;

// Samples the input buffer holds: what preview_push() takes at once
constexpr size_t PREVIEW_INPUT_CAPACITY = 8192;

/**
 * Starts with the default settings at this sample rate (the firmware's
 * 48828.125 Hz, or the browser's). 1 if it worked, 0 if the rate is not
 * positive or memory ran out. Calling it again starts over
 */
PREVIEW_EXPORT int preview_init(float sample_rate);

// Frees everything, preview_init() may follow
PREVIEW_EXPORT void preview_deinit(void);

/**
 * Where the samples go: PREVIEW_INPUT_CAPACITY floats, mono, -1 to 1. Write
 * count of them, then call preview_push(count)
 */
PREVIEW_EXPORT float *preview_input(void);

/**
 * Takes count samples from the input buffer (at most PREVIEW_INPUT_CAPACITY),
 * and renders one hop for every 256 completed. Returns how many hops that
 * was
 */
PREVIEW_EXPORT int preview_push(int count);

/**
 * How loud the microphones would hear it, in dB: applied to the samples
 * before the analysis. A song at full volume is much louder than the board's
 * microphones hear a room, and the quiet floor depends on absolute level
 */
PREVIEW_EXPORT void preview_set_input_trim_db(float db);

// On: every mode renders each hop (the gallery). Off: only the selected mode.
// A mode's pixels are then stale, and each mode keeps its own effects state:
// one selected after being idle resumes its old trails, unlike the board,
// where one effects instance carries the previous mode's trails over
PREVIEW_EXPORT void preview_set_gallery(int on);

// The menu's settings: a stored value, its grid, the defaults
PREVIEW_EXPORT int preview_setting_count(void);
// min, max, step, initial, divisor, wraps: six int16, valid until the next
// call. nullptr for an id out of range
PREVIEW_EXPORT const int16_t *preview_setting_range(int id);
PREVIEW_EXPORT int preview_get(int id);
// Snaps to the grid like the menu. Returns the stored value
PREVIEW_EXPORT int preview_set(int id, int value);
// Every setting to its default
PREVIEW_EXPORT void preview_reset(void);

// The menu's names
PREVIEW_EXPORT int preview_mode_count(void);
PREVIEW_EXPORT const char *preview_mode_name(int mode);
PREVIEW_EXPORT int preview_palette_count(void);
PREVIEW_EXPORT const char *preview_palette_name(int palette);

// The newest frame of a mode: PREVIEW_LED_COUNT WS2812 words, after gamma.
// nullptr for a mode out of range. Only the selected mode renders unless the
// gallery is on
PREVIEW_EXPORT const uint32_t *preview_pixels(int mode);

// The newest sound
PREVIEW_EXPORT float preview_loudness(void);
PREVIEW_EXPORT float preview_centroid(void);
// Beats detected since preview_init()
PREVIEW_EXPORT int preview_beats(void);
PREVIEW_EXPORT int preview_hops(void);
// Band levels, FEATURES_BAND_COUNT floats
PREVIEW_EXPORT const float *preview_bands(void);

#endif
