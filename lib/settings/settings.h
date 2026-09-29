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
    // Layers. ms, 0 disables
    SETTINGS_TRAILS,
    // Fraction
    SETTINGS_DIFFUSE,
    // Segments, 1 disables
    SETTINGS_SYMMETRY,
    // LEDs per second, negative slides the other way, 0 disables
    SETTINGS_CHASE,
    SETTINGS_ID_COUNT
} settings_id_t;

// Stands for every setting at once, e.g. when a reset changed them all.
// Not a setting: the functions below treat it as out of range
constexpr settings_id_t SETTINGS_ALL = SETTINGS_ID_COUNT;

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

// Most rows of the menu's Look page a mode adds below Brightness
constexpr size_t SETTINGS_MODE_IDS_MAX = 2;

/**
 * The settings only one mode uses, in the order the menu shows them: at most
 * SETTINGS_MODE_IDS_MAX. Every setting is either in exactly one mode's list
 * or applies to every mode (test_settings checks it). nullptr and 0 for a
 * mode without any, or out of range
 */
const settings_id_t *settings_mode_ids(effects_mode_t mode, size_t *count);

/**
 * A random look: the mode, the palette and every effect layer at random on
 * their grids, the mode and palette together always different from before.
 * The brightness, the sound response and the backlight are left alone.
 * random is xorshift32 state, not 0
 */
void settings_shuffle(settings_t *this, uint32_t *random);

/*
 * Records: the settings as saved, in one flash page, little endian
 *
 *   offset 0   "LPST"
 *          4   u16 SETTINGS_VERSION
 *          6   u16 number of values, n
 *          8   u32 sequence number, counting the saves
 *         12   n x i16 values, in settings_id_t order
 *   12 + 2 n   u32 CRC-32 of all the bytes before it
 *
 * and 0xFF after it, as in erased flash
 */

// A flash page
constexpr size_t SETTINGS_RECORD_SIZE = 256;

// Changes when a stored value's meaning does (e.g. reordered modes): older
// records are ignored then. New settings appended do not need it
constexpr uint16_t SETTINGS_VERSION = 1;

// Writes the settings as a record numbered sequence
void settings_encode(const settings_t *this, uint32_t sequence,
                     uint8_t record[SETTINGS_RECORD_SIZE]);

/**
 * Reads a record, false if it is none: erased, cut short, damaged or of
 * another version. Settings missing from a record of an older firmware get
 * their defaults, values past ours from a newer one are skipped, and all
 * are brought onto their grids
 */
[[nodiscard]] bool settings_decode(settings_t *this, uint32_t *sequence,
                                   const uint8_t record[SETTINGS_RECORD_SIZE]);

/*
 * The log: records one after the other in two erase blocks of flash, so a
 * save only programs an erased page and a block is erased once per 16
 * saves. The block holding the newest record is never erased: power lost
 * during a save leaves the previous record, and the block erased is always
 * the other one, whose records are older
 */

// A flash sector, the smallest part that can be erased
constexpr size_t SETTINGS_LOG_BLOCK_SIZE = 4096;
constexpr size_t SETTINGS_LOG_BLOCK_COUNT = 2;
constexpr size_t SETTINGS_LOG_SIZE =
    SETTINGS_LOG_BLOCK_SIZE * SETTINGS_LOG_BLOCK_COUNT;

// What settings_log_scan() found, and where the next record goes
typedef struct {
    // Whether there is a valid record, and the newest one's sequence
    bool found;
    uint32_t sequence;
    // Where the next record goes, from the start of the log, and whether
    // its block has to be erased first
    size_t next_offset;
    bool erase_first;
} settings_log_t;

/**
 * Reads the log, SETTINGS_LOG_SIZE bytes as in flash: the newest valid
 * record into settings, or the defaults if there is none. Damaged or
 * partly written records are skipped
 */
void settings_log_scan(settings_log_t *this, settings_t *settings,
                       const uint8_t *log);

// The sequence number of the next record
uint32_t settings_log_next_sequence(const settings_log_t *this);

#endif
