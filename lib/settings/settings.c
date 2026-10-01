#include "settings.h"

/*
 * The initial values are the constants they replace, written on the grid:
 * dividing one by its divisor gives exactly that constant, as float division
 * rounds to the nearest float and so does the constant's literal (28 / 10
 * and 2.8f are the same float). test_settings checks every one. Hit
 * sensitivity alone is turned around, see HIT_SENSITIVITY_PRODUCT
 */

// Hit sensitivity s stands for the hit threshold (FEATURES_HIT_THRESHOLD,
// times the rise's average) HIT_SENSITIVITY_PRODUCT / s, so a higher
// sensitivity gives more hits. The default threshold squared: sensitivity
// 3.0 is the threshold 3.0, and 1.5 to 6.0 stand for thresholds 6.0 to 1.5
constexpr float HIT_SENSITIVITY_PRODUCT =
    FEATURES_HIT_THRESHOLD * FEATURES_HIT_THRESHOLD;

static const settings_range_t ranges[SETTINGS_ID_COUNT] = {
    [SETTINGS_LOOK] = {0, SHOW_LOOK_COUNT - 1, 1, SHOW_LOOK, 1, true},
    [SETTINGS_SCENE] = {0, SCENE_COUNT - 1, 1, SHOW_SCENE, 1, true},
    // Perceptual, before the strip's 8-bit gamma: at 10 % full white is
    // 2 / 255, below about 20 % colours lose their shading, and below 6 %
    // everything rounds to off
    [SETTINGS_BRIGHTNESS] = {10, 100, 5, 100, 100, false},
    [SETTINGS_SONG_PARTS] = {0, 1, 1, 1, 1, true},
    // VISUALIZER_GAIN. Much above 3, a quiet room's self-noise nears the
    // floor of FEATURES_MIN_CEILING_DB - FEATURES_RANGE_DB
    [SETTINGS_GAIN] = {5, 40, 1, 15, 10, false},
    // FEATURES_HIT_THRESHOLD, turned around: see HIT_SENSITIVITY_PRODUCT
    [SETTINGS_HIT_SENSITIVITY] = {15, 60, 1, 30, 10, false},
    // FEATURES_MIN_CEILING_DB
    [SETTINGS_QUIET_FLOOR] = {-45, -10, 1, -32, 1, false},
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
    int look = this->values[SETTINGS_LOOK];
    int scene = this->values[SETTINGS_SCENE];

    // Something visibly new: another look or scene than before
    do {
        settings_set(this, SETTINGS_LOOK, random_value(SETTINGS_LOOK, random));
        settings_set(this, SETTINGS_SCENE,
                     random_value(SETTINGS_SCENE, random));
    } while (this->values[SETTINGS_LOOK] == look &&
             this->values[SETTINGS_SCENE] == scene);
}

visualizer_tuning_t settings_tuning(const settings_t *this) {
    visualizer_tuning_t tuning = visualizer_default_tuning();

    tuning.gain = settings_value(this, SETTINGS_GAIN);

    tuning.features.min_ceiling_db = settings_value(this, SETTINGS_QUIET_FLOOR);
    tuning.features.hit_threshold =
        HIT_SENSITIVITY_PRODUCT /
        settings_value(this, SETTINGS_HIT_SENSITIVITY);
    tuning.features.song_parts = settings_get(this, SETTINGS_SONG_PARTS) != 0;

    tuning.show.look = (show_look_t)settings_get(this, SETTINGS_LOOK);
    tuning.show.scene = (scene_t)settings_get(this, SETTINGS_SCENE);
    tuning.show.brightness = settings_value(this, SETTINGS_BRIGHTNESS);

    return tuning;
}

void settings_clamp(settings_t *this) {
    for (size_t id = 0; id < SETTINGS_ID_COUNT; id++)
        this->values[id] = (int16_t)on_grid(&ranges[id], this->values[id]);
}
