#ifndef EFFECTS_H
#define EFFECTS_H

/**
 * Effects: turns the sound (sound_t, from features) into LED pixels, one
 * render per hop.
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
#define EFFECTS_RIPPLE_MAX_PULSES 8

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

// What the strip shows, see the README for each mode
typedef enum {
    EFFECTS_MODE_SPECTRUM,
    EFFECTS_MODE_SPECTRUM_MIRRORED,
    EFFECTS_MODE_RIVER,
    EFFECTS_MODE_RIPPLES,
    EFFECTS_MODE_VU,
    EFFECTS_MODE_GLOW,
    EFFECTS_MODE_COUNT
} effects_mode_t;

// One pulse of the ripples mode, launched from the centre by a beat
typedef struct {
    // Distance from the centre of the leading edge, in LEDs
    float position;
    // Beat strength 0..1, sets width and brightness
    float strength;
    // Palette position, steps by 1/8 per beat
    float color_position;
    // False once it left the strip, the slot is free
    bool active;
} effects_pulse_t;

// Render state. Allocated by effects_init(), the render path never allocates
typedef struct {
    size_t led_count;
    size_t band_count;
    // LEDs from the centre to one end, (led_count + 1) / 2
    size_t half_led_count;
    // Time between two renders
    float hop_period_s;

    effects_mode_t mode;
    palette_t palette;

    // Time since init modulo two drift periods, drives the drift
    float time_s;

    // Current beat flash level and its fade per frame
    float flash;
    float flash_k;

    // Colours being rendered, led_count
    rgb_t *frame;

    // Per mode state
    struct {
        // History, half_led_count, index 0 at the centre
        rgb_t *history;
    } river;

    struct {
        effects_pulse_t pulses[EFFECTS_RIPPLE_MAX_PULSES];
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
 * band_count: length of sound_t.bands (>= 2)
 * seed: for the sparkles, renders are deterministic for a seed
 *
 * Starts in EFFECTS_MODE_RIVER with PALETTE_SYNTHWAVE. False if a count is too
 * small, hop_period_s is not positive or memory runs out
 */
[[nodiscard]] bool effects_init(effects_t *this, size_t led_count,
                                size_t band_count, float hop_period_s,
                                uint32_t seed);

// Takes effect on the next render. Out of range values are ignored
void effects_set_mode(effects_t *this, effects_mode_t mode);

// Takes effect on the next render. Out of range values are ignored
void effects_set_palette(effects_t *this, palette_t palette);

/**
 * Renders one frame into led_count color_ws2812_t words
 */
void effects_render(effects_t *this, const sound_t *sound, uint32_t *pixels);

// Only after a successful effects_init()
void effects_deinit(effects_t *this);

#endif
