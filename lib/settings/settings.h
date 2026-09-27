#ifndef SETTINGS_H
#define SETTINGS_H

/**
 * Settings: what the menu changes at runtime. Each is a whole number on a
 * fixed grid, from min to max in steps, and stands for that number divided
 * by its divisor: gain 15 is 1.5. Steps then land exactly, the menu shows
 * exactly what is stored, and a saved record holds small integers.
 *
 * The defaults are the visualizer's constants, exactly:
 * settings_value() of a default equals the constant it replaces.
 */

#include "visualizer.h"

#include <stddef.h>
#include <stdint.h>

// Saved records store the values by position: append new settings before
// SETTINGS_ID_COUNT, never reorder or remove one
typedef enum {
    // Look
    SETTINGS_MODE,
    SETTINGS_PALETTE,
    // Percent, perceptual
    SETTINGS_BRIGHTNESS,
    // Sound
    SETTINGS_GAIN,
    SETTINGS_BEAT_THRESHOLD,
    // dB, the auto-gain ceiling's lowest
    SETTINGS_QUIET_FLOOR,
    // ms
    SETTINGS_ATTACK,
    SETTINGS_DECAY,
    // Effects. Seconds, 0 disables
    SETTINGS_DRIFT,
    // Fractions
    SETTINGS_WARMTH,
    SETTINGS_FLASH,
    SETTINGS_SPARKLES,
    // LEDs per frame
    SETTINGS_RIVER_SPEED,
    SETTINGS_RIPPLE_SPEED,
    // ms
    SETTINGS_PEAK_HOLD,
    // System. Percent
    SETTINGS_BACKLIGHT,
    SETTINGS_ID_COUNT
} settings_id_t;

typedef struct {
    int16_t min;
    int16_t max;
    int16_t step;
    // The default
    int16_t initial;
    // What a value is divided by to give the real one
    int16_t divisor;
    // Cycles past its ends (the modes and palettes) instead of stopping
    bool wraps;
} settings_range_t;

typedef struct {
    int16_t values[SETTINGS_ID_COUNT];
} settings_t;

// nullptr for an id out of range
const settings_range_t *settings_range(settings_id_t id);

// Every setting to its default
void settings_reset(settings_t *this);

// The stored value, and the real value it stands for (0 out of range)
int settings_get(const settings_t *this, settings_id_t id);
float settings_value(const settings_t *this, settings_id_t id);

/**
 * Sets the nearest value on the grid, within the range. Returns whether the
 * value changed
 */
bool settings_set(settings_t *this, settings_id_t id, int value);

/**
 * Moves steps steps up (or down, if negative), cycling round or stopping at
 * the ends. Returns whether the value changed
 */
bool settings_step(settings_t *this, settings_id_t id, int steps);

// Every value onto its grid and within its range, e.g. after loading
void settings_clamp(settings_t *this);

// What the settings make of the visualizer. The backlight is not in it
visualizer_tuning_t settings_tuning(const settings_t *this);

#endif
