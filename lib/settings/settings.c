#include "settings.h"

/*
 * The initial values are the constants they replace, written on the grid:
 * dividing one by its divisor gives exactly that constant, as float division
 * rounds to the nearest float and so does the constant's literal (28 / 10
 * and 2.8f are the same float). test_settings checks every one
 */
static const settings_range_t ranges[SETTINGS_ID_COUNT] = {
    [SETTINGS_MODE] = {0, EFFECTS_MODE_COUNT - 1, 1, EFFECTS_MODE, 1, true},
    [SETTINGS_PALETTE] = {0, PALETTE_COUNT - 1, 1, EFFECTS_PALETTE, 1,
                          true},
    // Perceptual, before the strip's 8-bit gamma: at 10 % full white is
    // 2 / 255, below about 20 % colours lose their shading, and below 6 %
    // everything rounds to off
    [SETTINGS_BRIGHTNESS] = {10, 100, 5, 100, 100, false},
    // VISUALIZER_GAIN. Much above 3, a quiet room's self-noise nears the
    // floor of FEATURES_MIN_CEILING_DB - FEATURES_RANGE_DB
    [SETTINGS_GAIN] = {5, 40, 1, 15, 10, false},
    // FEATURES_BEAT_THRESHOLD: steady noise reaches 2.6 times its average,
    // kicks 6.5 times
    [SETTINGS_BEAT_THRESHOLD] = {15, 60, 1, 28, 10, false},
    // FEATURES_MIN_CEILING_DB
    [SETTINGS_QUIET_FLOOR] = {-45, -10, 1, -32, 1, false},
    // FEATURES_ATTACK_MS, FEATURES_DECAY_MS. A hop is 5.2 ms
    [SETTINGS_ATTACK] = {2, 60, 2, 10, 1, false},
    [SETTINGS_DECAY] = {20, 600, 10, 120, 1, false},
    // EFFECTS_DRIFT_PERIOD_S, 0 disables it
    [SETTINGS_DRIFT] = {0, 300, 10, 60, 1, false},
    // EFFECTS_WARMTH, EFFECTS_FLASH_LEVEL, in percent
    [SETTINGS_WARMTH] = {0, 100, 5, 25, 100, false},
    [SETTINGS_FLASH] = {0, 100, 5, 35, 100, false},
    // EFFECTS_SPARKLE_RATE, in per mille
    [SETTINGS_SPARKLES] = {0, 100, 5, 30, 1000, false},
    // EFFECTS_RIVER_SPEED, EFFECTS_RIPPLE_SPEED
    [SETTINGS_RIVER_SPEED] = {1, 4, 1, 1, 1, false},
    [SETTINGS_RIPPLE_SPEED] = {5, 60, 5, 20, 10, false},
    // EFFECTS_PEAK_HOLD_MS
    [SETTINGS_PEAK_HOLD] = {0, 2000, 50, 300, 1, false},
    [SETTINGS_BACKLIGHT] = {10, 100, 10, 80, 100, false},
};

// The nearest value on the grid within the range, halves rounding up
static int on_grid(const settings_range_t *range, int value) {
    if (value <= range->min)
        return range->min;
    if (value >= range->max)
        return range->max;

    return range->min +
           (value - range->min + range->step / 2) / range->step * range->step;
}

const settings_range_t *settings_range(settings_id_t id) {
    return id < SETTINGS_ID_COUNT ? &ranges[id] : nullptr;
}

void settings_reset(settings_t *this) {
    for (size_t id = 0; id < SETTINGS_ID_COUNT; id++)
        this->values[id] = ranges[id].initial;
}

int settings_get(const settings_t *this, settings_id_t id) {
    return id < SETTINGS_ID_COUNT ? this->values[id] : 0;
}

float settings_value(const settings_t *this, settings_id_t id) {
    if (id >= SETTINGS_ID_COUNT)
        return 0.f;

    return (float)this->values[id] / (float)ranges[id].divisor;
}

bool settings_set(settings_t *this, settings_id_t id, int value) {
    int16_t previous;

    if (id >= SETTINGS_ID_COUNT)
        return false;

    previous = this->values[id];
    this->values[id] = (int16_t)on_grid(&ranges[id], value);

    return this->values[id] != previous;
}

bool settings_step(settings_t *this, settings_id_t id, int steps) {
    const settings_range_t *range;
    int count, index;

    if (id >= SETTINGS_ID_COUNT)
        return false;

    range = &ranges[id];

    if (!range->wraps)
        return settings_set(this, id,
                            this->values[id] + steps * range->step);

    // Positions on the grid, cycled through
    count = (range->max - range->min) / range->step + 1;
    index = (on_grid(range, this->values[id]) - range->min) / range->step;
    index = ((index + steps) % count + count) % count;

    return settings_set(this, id, range->min + index * range->step);
}

// What a shuffle changes: the look, not how bright or how sensitive it is
static const settings_id_t look_ids[] = {
    SETTINGS_MODE,         SETTINGS_PALETTE,      SETTINGS_DRIFT,
    SETTINGS_WARMTH,       SETTINGS_FLASH,        SETTINGS_SPARKLES,
    SETTINGS_RIVER_SPEED,  SETTINGS_RIPPLE_SPEED, SETTINGS_PEAK_HOLD,
};

static uint32_t next_random(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

// A random point of a setting's grid
static int random_value(settings_id_t id, uint32_t *random) {
    const settings_range_t *range = &ranges[id];
    uint32_t count = (uint32_t)((range->max - range->min) / range->step) + 1;

    return range->min + (int)(next_random(random) % count) * range->step;
}

void settings_shuffle(settings_t *this, uint32_t *random) {
    int mode = this->values[SETTINGS_MODE];
    int palette = this->values[SETTINGS_PALETTE];

    for (size_t i = 0; i < sizeof(look_ids) / sizeof(look_ids[0]); i++)
        settings_set(this, look_ids[i], random_value(look_ids[i], random));

    // Something visibly new: another mode or palette than before
    while (this->values[SETTINGS_MODE] == mode &&
           this->values[SETTINGS_PALETTE] == palette) {
        settings_set(this, SETTINGS_MODE,
                     random_value(SETTINGS_MODE, random));
        settings_set(this, SETTINGS_PALETTE,
                     random_value(SETTINGS_PALETTE, random));
    }
}

visualizer_tuning_t settings_tuning(const settings_t *this) {
    visualizer_tuning_t tuning = visualizer_default_tuning();

    tuning.gain = settings_value(this, SETTINGS_GAIN);

    tuning.features.attack_ms = settings_value(this, SETTINGS_ATTACK);
    tuning.features.decay_ms = settings_value(this, SETTINGS_DECAY);
    tuning.features.min_ceiling_db = settings_value(this, SETTINGS_QUIET_FLOOR);
    tuning.features.beat_threshold =
        settings_value(this, SETTINGS_BEAT_THRESHOLD);

    tuning.effects.mode = (effects_mode_t)settings_get(this, SETTINGS_MODE);
    tuning.effects.palette = (palette_t)settings_get(this, SETTINGS_PALETTE);
    tuning.effects.brightness = settings_value(this, SETTINGS_BRIGHTNESS);
    tuning.effects.river_speed =
        (size_t)settings_get(this, SETTINGS_RIVER_SPEED);
    tuning.effects.ripple_speed = settings_value(this, SETTINGS_RIPPLE_SPEED);
    tuning.effects.peak_hold_ms = settings_value(this, SETTINGS_PEAK_HOLD);
    tuning.effects.drift_period_s = settings_value(this, SETTINGS_DRIFT);
    tuning.effects.warmth = settings_value(this, SETTINGS_WARMTH);
    tuning.effects.flash_level = settings_value(this, SETTINGS_FLASH);
    tuning.effects.sparkle_rate = settings_value(this, SETTINGS_SPARKLES);

    return tuning;
}

void settings_clamp(settings_t *this) {
    for (size_t id = 0; id < SETTINGS_ID_COUNT; id++)
        this->values[id] = (int16_t)on_grid(&ranges[id], this->values[id]);
}
