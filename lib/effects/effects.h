#ifndef EFFECTS_H
#define EFFECTS_H

/**
 * Effects: turns audio features into LED pixels, one render per hop.
 * Each mode is a renderer in its own mode_*.c file, see effects_internal.h
 */

#include "features.h"
#include "palette.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// River: LEDs the history moves outward per frame
#define EFFECTS_RIVER_SPEED 1

// Ripples: LEDs a pulse travels per frame, pulses alive at once
#define EFFECTS_RIPPLE_SPEED 2.f
#define EFFECTS_RIPPLE_MAX 8

// VU: peak dot hold time, then LEDs it falls per frame
#define EFFECTS_PEAK_HOLD_MS 300.f
#define EFFECTS_PEAK_FALL 1.f

// Palette rotation period, 0 disables
#define EFFECTS_DRIFT_PERIOD_S 60.f

// Palette shift towards its end at full loudness, 0 disables
#define EFFECTS_WARMTH 0.25f

// White added on a beat of full strength, and its fade time constant
#define EFFECTS_FLASH_LEVEL 0.35f
#define EFFECTS_FLASH_MS 80.f

// Sparkles: chance per LED per frame at full treble, and fade per frame
#define EFFECTS_SPARKLE_RATE 0.03f
#define EFFECTS_SPARKLE_DECAY 0.8f

// Bands counted as bass (glow), and the top fraction counted as treble
#define EFFECTS_BASS_BANDS 5
#define EFFECTS_TREBLE_FRACTION 0.25f

typedef enum {
    EFFECTS_SPECTRUM,
    EFFECTS_SPECTRUM_MIRRORED,
    EFFECTS_RIVER,
    EFFECTS_RIPPLES,
    EFFECTS_VU,
    EFFECTS_GLOW,
    EFFECTS_MODE_COUNT
} effects_mode_t;

typedef palette_id_t effects_palette_t;

typedef struct {
    // Distance from the centre of the leading edge, in LEDs
    float position;
    float strength;
    float color_position;
    bool active;
} effects_ripple_t;

typedef struct {
    size_t led_count;
    size_t band_count;
    // LEDs from the centre to one end, (led_count + 1) / 2
    size_t half;
    float hop_seconds;

    effects_mode_t mode;
    effects_palette_t palette;

    // Time since init modulo two drift periods, drives the drift
    float time_s;

    // Current beat flash level and its fade per frame
    float flash;
    float flash_k;

    // Colours being rendered, led_count
    rgb_t *frame;

    // Per mode state
    struct {
        // History, half, index 0 at the centre
        rgb_t *history;
    } river;

    struct {
        effects_ripple_t pulses[EFFECTS_RIPPLE_MAX];
        unsigned beat_count;
    } ripples;

    struct {
        // Peak, in LEDs from the ends, and its remaining hold time
        float peak;
        float hold_s;
    } vu;

    // Shared by the modes that sparkle
    struct {
        // Levels, led_count
        float *levels;
        // xorshift32 state, never 0
        uint32_t random;
    } sparkles;
} effects_t;

/**
 * band_count: length of features_t.bands (>= 2)
 * seed: for the sparkles, renders are deterministic for a seed
 */
bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed);

void effects_set_mode(effects_t *this, effects_mode_t mode);

void effects_set_palette(effects_t *this, effects_palette_t palette);

/**
 * Renders one frame into led_count color_neopixel_t words
 */
void effects_render(effects_t *this, const features_t *features,
                    uint32_t *pixels);

void effects_deinit(effects_t *this);

#endif
