#include "check.h"
#include "settings.h"
#include "visualizer.h"

// Every default and every end of a range sits on its grid
static void test_ranges(void) {
    for (int id = 0; id < SETTINGS_ID_COUNT; id++) {
        const settings_range_t *range = settings_range(id);

        CHECK(range != nullptr);
        CHECK(range->step > 0);
        CHECK(range->divisor > 0);
        CHECK(range->min < range->max);
        CHECK((range->max - range->min) % range->step == 0);
        CHECK(range->initial >= range->min && range->initial <= range->max);
        CHECK((range->initial - range->min) % range->step == 0);
    }

    CHECK(settings_range(SETTINGS_ID_COUNT) == nullptr);
}

// The defaults stand for the constants they replace, exactly
static void test_defaults_are_the_constants(void) {
    settings_t settings;

    settings_reset(&settings);

    CHECK(settings_get(&settings, SETTINGS_MODE) == (int)VISUALIZER_MODE);
    CHECK(settings_get(&settings, SETTINGS_PALETTE) == (int)VISUALIZER_PALETTE);
    CHECK(settings_value(&settings, SETTINGS_BRIGHTNESS) == 1.f);
    CHECK(settings_value(&settings, SETTINGS_GAIN) == VISUALIZER_GAIN);
    CHECK(settings_value(&settings, SETTINGS_BEAT_THRESHOLD) ==
          FEATURES_BEAT_THRESHOLD);
    CHECK(settings_value(&settings, SETTINGS_QUIET_FLOOR) ==
          FEATURES_MIN_CEILING_DB);
    CHECK(settings_value(&settings, SETTINGS_ATTACK) == FEATURES_ATTACK_MS);
    CHECK(settings_value(&settings, SETTINGS_DECAY) == FEATURES_DECAY_MS);
    CHECK(settings_value(&settings, SETTINGS_DRIFT) == EFFECTS_DRIFT_PERIOD_S);
    CHECK(settings_value(&settings, SETTINGS_WARMTH) == EFFECTS_WARMTH);
    CHECK(settings_value(&settings, SETTINGS_FLASH) == EFFECTS_FLASH_LEVEL);
    CHECK(settings_value(&settings, SETTINGS_SPARKLES) == EFFECTS_SPARKLE_RATE);
    CHECK(settings_value(&settings, SETTINGS_RIVER_SPEED) ==
          (float)EFFECTS_RIVER_SPEED);
    CHECK(settings_value(&settings, SETTINGS_RIPPLE_SPEED) ==
          EFFECTS_RIPPLE_SPEED);
    CHECK(settings_value(&settings, SETTINGS_PEAK_HOLD) == EFFECTS_PEAK_HOLD_MS);
}

static void test_set_snaps_and_clamps(void) {
    settings_t settings;

    settings_reset(&settings);

    // Beyond the ends
    CHECK(settings_set(&settings, SETTINGS_GAIN, 99));
    CHECK(settings_get(&settings, SETTINGS_GAIN) == 40);
    CHECK(settings_set(&settings, SETTINGS_GAIN, -5));
    CHECK(settings_get(&settings, SETTINGS_GAIN) == 5);

    // Between two steps: the nearest, halves up
    CHECK(settings_set(&settings, SETTINGS_ATTACK, 13));
    CHECK(settings_get(&settings, SETTINGS_ATTACK) == 14);
    CHECK(settings_set(&settings, SETTINGS_DECAY, 126));
    CHECK(settings_get(&settings, SETTINGS_DECAY) == 130);
    CHECK(settings_set(&settings, SETTINGS_DECAY, 124));
    CHECK(settings_get(&settings, SETTINGS_DECAY) == 120);

    // Negative ranges
    CHECK(settings_set(&settings, SETTINGS_QUIET_FLOOR, -50));
    CHECK(settings_get(&settings, SETTINGS_QUIET_FLOOR) == -45);

    // Unchanged
    CHECK(!settings_set(&settings, SETTINGS_QUIET_FLOOR, -45));

    // An id out of range changes nothing
    CHECK(!settings_set(&settings, SETTINGS_ID_COUNT, 1));
    CHECK(settings_get(&settings, SETTINGS_ID_COUNT) == 0);
}

static void test_step_stops_at_the_ends(void) {
    settings_t settings;

    settings_reset(&settings);

    CHECK(settings_step(&settings, SETTINGS_BRIGHTNESS, -1));
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 95);
    CHECK(settings_step(&settings, SETTINGS_BRIGHTNESS, 1));
    CHECK(!settings_step(&settings, SETTINGS_BRIGHTNESS, 1));
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 100);

    CHECK(settings_step(&settings, SETTINGS_BRIGHTNESS, -100));
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 10);
}

static void test_step_wraps_modes_and_palettes(void) {
    settings_t settings;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_MODE, EFFECTS_MODE_COUNT - 1);

    CHECK(settings_step(&settings, SETTINGS_MODE, 1));
    CHECK(settings_get(&settings, SETTINGS_MODE) == 0);
    CHECK(settings_step(&settings, SETTINGS_MODE, -1));
    CHECK(settings_get(&settings, SETTINGS_MODE) == EFFECTS_MODE_COUNT - 1);

    settings_set(&settings, SETTINGS_PALETTE, 0);
    CHECK(settings_step(&settings, SETTINGS_PALETTE, -1));
    CHECK(settings_get(&settings, SETTINGS_PALETTE) == PALETTE_COUNT - 1);
    // A whole round comes back to the same palette
    CHECK(!settings_step(&settings, SETTINGS_PALETTE, PALETTE_COUNT));
    CHECK(settings_get(&settings, SETTINGS_PALETTE) == PALETTE_COUNT - 1);
}

// Values read back from somewhere else land on the grid
static void test_clamp(void) {
    settings_t settings;

    settings_reset(&settings);
    settings.values[SETTINGS_PEAK_HOLD] = 5000;
    settings.values[SETTINGS_ATTACK] = 3;
    settings.values[SETTINGS_MODE] = -7;

    settings_clamp(&settings);

    CHECK(settings_get(&settings, SETTINGS_PEAK_HOLD) == 2000);
    CHECK(settings_get(&settings, SETTINGS_ATTACK) == 4);
    CHECK(settings_get(&settings, SETTINGS_MODE) == 0);
}

int main(void) {
    test_ranges();
    test_defaults_are_the_constants();
    test_set_snaps_and_clamps();
    test_step_stops_at_the_ends();
    test_step_wraps_modes_and_palettes();
    test_clamp();

    return CHECK_REPORT();
}
