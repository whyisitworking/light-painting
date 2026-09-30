#include "check.h"
#include "settings.h"
#include "visualizer.h"

#include <math.h>
#include <string.h>

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

    CHECK(settings_get(&settings, SETTINGS_LOOK) == (int)SHOW_LOOK);
    CHECK(settings_get(&settings, SETTINGS_SCENE) == (int)SHOW_SCENE);
    CHECK(settings_value(&settings, SETTINGS_BRIGHTNESS) == 1.f);
    CHECK(settings_get(&settings, SETTINGS_SONG_PARTS) == 1);
    CHECK(settings_value(&settings, SETTINGS_GAIN) == VISUALIZER_GAIN);
    // Sensitivity 3.0 stands for the threshold 9 / 3.0, the constant
    CHECK(settings_value(&settings, SETTINGS_HIT_SENSITIVITY) == 3.f);
    CHECK(settings_tuning(&settings).features.hit_threshold ==
          FEATURES_HIT_THRESHOLD);
    CHECK(settings_value(&settings, SETTINGS_QUIET_FLOOR) ==
          FEATURES_MIN_CEILING_DB);
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
    CHECK(settings_set(&settings, SETTINGS_BRIGHTNESS, 47));
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 45);
    CHECK(settings_set(&settings, SETTINGS_BRIGHTNESS, 48));
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 50);
    CHECK(settings_set(&settings, SETTINGS_BACKLIGHT, 64));
    CHECK(settings_get(&settings, SETTINGS_BACKLIGHT) == 60);

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

static void test_step_wraps_looks_scenes_and_song_parts(void) {
    settings_t settings;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_LOOK, SHOW_LOOK_COUNT - 1);

    CHECK(settings_step(&settings, SETTINGS_LOOK, 1));
    CHECK(settings_get(&settings, SETTINGS_LOOK) == 0);
    CHECK(settings_step(&settings, SETTINGS_LOOK, -1));
    CHECK(settings_get(&settings, SETTINGS_LOOK) == SHOW_LOOK_COUNT - 1);

    settings_set(&settings, SETTINGS_SCENE, 0);
    CHECK(settings_step(&settings, SETTINGS_SCENE, -1));
    CHECK(settings_get(&settings, SETTINGS_SCENE) == SCENE_COUNT - 1);
    // A whole round comes back to the same scene
    CHECK(!settings_step(&settings, SETTINGS_SCENE, SCENE_COUNT));
    CHECK(settings_get(&settings, SETTINGS_SCENE) == SCENE_COUNT - 1);

    // On and off toggle both ways
    CHECK(settings_step(&settings, SETTINGS_SONG_PARTS, 1));
    CHECK(settings_get(&settings, SETTINGS_SONG_PARTS) == 0);
    CHECK(settings_step(&settings, SETTINGS_SONG_PARTS, -1));
    CHECK(settings_get(&settings, SETTINGS_SONG_PARTS) == 1);
}

// Values read back from somewhere else land on the grid
static void test_clamp(void) {
    settings_t settings;

    settings_reset(&settings);
    settings.values[SETTINGS_QUIET_FLOOR] = 5000;
    settings.values[SETTINGS_BRIGHTNESS] = 3;
    settings.values[SETTINGS_LOOK] = -7;

    settings_clamp(&settings);

    CHECK(settings_get(&settings, SETTINGS_QUIET_FLOOR) == -10);
    CHECK(settings_get(&settings, SETTINGS_BRIGHTNESS) == 10);
    CHECK(settings_get(&settings, SETTINGS_LOOK) == 0);
}

// The defaults make the visualizer's default tuning, field for field
static void test_default_tuning(void) {
    settings_t settings;
    visualizer_tuning_t tuning, defaults = visualizer_default_tuning();

    settings_reset(&settings);
    tuning = settings_tuning(&settings);

    CHECK(tuning.gain == defaults.gain);
    CHECK(tuning.features.min_ceiling_db == defaults.features.min_ceiling_db);
    CHECK(tuning.features.hit_threshold ==
          defaults.features.hit_threshold);
    CHECK(tuning.features.song_parts == defaults.features.song_parts);
    CHECK(tuning.show.look == defaults.show.look);
    CHECK(tuning.show.scene == defaults.show.scene);
    CHECK(tuning.show.brightness == defaults.show.brightness);
}

// Each setting lands in its field, divided by its divisor
static void test_tuning_follows_the_settings(void) {
    settings_t settings;
    visualizer_tuning_t tuning;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_LOOK, SHOW_LOOK_STORM);
    settings_set(&settings, SETTINGS_SCENE, SCENE_ICE);
    settings_set(&settings, SETTINGS_BRIGHTNESS, 50);
    settings_set(&settings, SETTINGS_SONG_PARTS, 0);
    settings_set(&settings, SETTINGS_GAIN, 25);
    settings_set(&settings, SETTINGS_HIT_SENSITIVITY, 45);
    settings_set(&settings, SETTINGS_QUIET_FLOOR, -40);
    tuning = settings_tuning(&settings);

    CHECK(tuning.show.look == SHOW_LOOK_STORM);
    CHECK(tuning.show.scene == SCENE_ICE);
    CHECK(tuning.show.brightness == 0.5f);
    CHECK(!tuning.features.song_parts);
    CHECK(tuning.gain == 2.5f);
    CHECK(tuning.features.hit_threshold == 2.f);
    CHECK(tuning.features.min_ceiling_db == -40.f);
}

// Hit sensitivity turns the threshold around: higher, a lower threshold and
// more hits, from 6.0 at the lowest to 1.5 at the highest
static void test_hit_sensitivity_is_inverse(void) {
    const settings_range_t *range = settings_range(SETTINGS_HIT_SENSITIVITY);
    settings_t settings;
    float previous = INFINITY;

    settings_reset(&settings);
    for (int value = range->min; value <= range->max; value += range->step) {
        float threshold;

        settings_set(&settings, SETTINGS_HIT_SENSITIVITY, value);
        threshold = settings_tuning(&settings).features.hit_threshold;
        CHECK(threshold < previous);
        previous = threshold;
    }
    settings_set(&settings, SETTINGS_HIT_SENSITIVITY, range->min);
    CHECK(settings_tuning(&settings).features.hit_threshold == 6.f);
    settings_set(&settings, SETTINGS_HIT_SENSITIVITY, range->max);
    CHECK(settings_tuning(&settings).features.hit_threshold == 1.5f);
}

static void test_record_round_trip(void) {
    settings_t saved, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence = 0;

    settings_reset(&saved);
    settings_set(&saved, SETTINGS_LOOK, SHOW_LOOK_SWEEP);
    settings_set(&saved, SETTINGS_QUIET_FLOOR, -40);
    settings_set(&saved, SETTINGS_SONG_PARTS, 0);

    settings_encode(&saved, 42, record);
    CHECK(settings_decode(&loaded, &sequence, record));
    CHECK(sequence == 42);
    CHECK(memcmp(&saved, &loaded, sizeof(saved)) == 0);

    // Unused bytes as in erased flash
    CHECK(record[SETTINGS_RECORD_SIZE - 1] == 0xFF);
}

// Any byte changed in the part that counts: rejected
static void test_record_damage_detected(void) {
    settings_t settings, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;
    size_t used = 12 + 2 * SETTINGS_ID_COUNT + 4;
    size_t accepted = 0;

    settings_reset(&settings);
    settings_encode(&settings, 1, record);

    for (size_t i = 0; i < used; i++) {
        record[i] ^= 0x10;
        accepted += settings_decode(&loaded, &sequence, record);
        record[i] ^= 0x10;
    }

    CHECK(accepted == 0);
}

static void test_record_rejects_erased_and_other_versions(void) {
    settings_t settings;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    memset(record, 0xFF, sizeof(record));
    CHECK(!settings_decode(&settings, &sequence, record));

    settings_reset(&settings);
    settings_encode(&settings, 1, record);
    record[4] = (uint8_t)(SETTINGS_VERSION + 1);
    CHECK(!settings_decode(&settings, &sequence, record));
}

// The record cut down to its first count values, with a matching CRC, as an
// older firmware with that many settings wrote it
static void cut_record(uint8_t record[SETTINGS_RECORD_SIZE], size_t count) {
    size_t size = 12 + 2 * count;
    uint32_t crc = 0xFFFFFFFFu;

    record[6] = (uint8_t)count;
    record[7] = 0;
    memset(record + size, 0xFF, SETTINGS_RECORD_SIZE - size);
    for (size_t i = 0; i < size; i++) {
        crc ^= record[i];
        for (int bit = 0; bit < 8; bit++)
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    crc = ~crc;
    for (int b = 0; b < 4; b++)
        record[size + b] = (uint8_t)(crc >> (8 * b));
}

// A record from a firmware with fewer settings: the rest are defaults
static void test_record_from_older_firmware(void) {
    settings_t settings, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_BRIGHTNESS, 40);
    settings_set(&settings, SETTINGS_GAIN, 30);
    settings_encode(&settings, 7, record);
    cut_record(record, 3);

    CHECK(settings_decode(&loaded, &sequence, record));
    CHECK(settings_get(&loaded, SETTINGS_BRIGHTNESS) == 40);
    CHECK(settings_get(&loaded, SETTINGS_GAIN) == 15);
}

// The quiet floor is negative: it survives a record
static void test_record_keeps_negative_values(void) {
    settings_t saved, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    settings_reset(&saved);
    settings_set(&saved, SETTINGS_QUIET_FLOOR, -44);
    settings_encode(&saved, 3, record);

    CHECK(settings_decode(&loaded, &sequence, record));
    CHECK(settings_get(&loaded, SETTINGS_QUIET_FLOOR) == -44);
}

// A record of the firmware before the show engine (version 1): ignored, the
// defaults load
static void test_record_of_version_1_ignored(void) {
    settings_t settings;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    settings_reset(&settings);
    settings_encode(&settings, 5, record);
    record[4] = 1;
    record[5] = 0;
    CHECK(!settings_decode(&settings, &sequence, record));
}

// NOR flash as the log sees it: erasing sets a block to 0xFF, programming
// can only clear bits
static uint8_t flash[SETTINGS_LOG_SIZE];
static size_t erases;

static void flash_erase(size_t offset) {
    memset(flash + offset, 0xFF, SETTINGS_LOG_BLOCK_SIZE);
    erases++;
}

static void flash_program(size_t offset, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++)
        flash[offset + i] &= data[i];
}

// What the firmware does to save: scan, erase if needed, program
static void save(const settings_t *settings) {
    settings_log_t log;
    settings_t current;
    uint8_t record[SETTINGS_RECORD_SIZE];

    settings_log_scan(&log, &current, flash);
    if (log.erase_first)
        flash_erase(log.next_offset);
    settings_encode(settings, settings_log_next_sequence(&log), record);
    flash_program(log.next_offset, record, sizeof(record));
}

static void test_log_empty(void) {
    settings_log_t log;
    settings_t settings, defaults;

    memset(flash, 0xFF, sizeof(flash));
    settings_log_scan(&log, &settings, flash);
    settings_reset(&defaults);

    CHECK(!log.found);
    CHECK(memcmp(&settings, &defaults, sizeof(settings)) == 0);
    CHECK(log.next_offset == 0);
    CHECK(!log.erase_first);
    CHECK(settings_log_next_sequence(&log) == 1);
}

// Many saves: always the last one back, one erase per block filled
static void test_log_saves_and_loads(void) {
    settings_log_t log;
    settings_t settings, loaded;
    int wrong = 0;

    memset(flash, 0xFF, sizeof(flash));
    erases = 0;
    settings_reset(&settings);

    for (int i = 0; i < 100; i++) {
        settings_set(&settings, SETTINGS_QUIET_FLOOR, -45 + i % 36);
        settings_set(&settings, SETTINGS_LOOK, i % SHOW_LOOK_COUNT);
        save(&settings);

        settings_log_scan(&log, &loaded, flash);
        wrong += !log.found || log.sequence != (uint32_t)i + 1 ||
                 memcmp(&settings, &loaded, sizeof(settings)) != 0;
    }

    CHECK(wrong == 0);
    // Block 1 is still erased when block 0 fills up. From then on a block
    // is erased every 16 saves: saves 33, 49, 65, 81 and 97
    CHECK(erases == 5);
}

// A record cut off halfway, as by a power loss: skipped, the previous one
// loads, and the next save goes past it
static void test_log_torn_record(void) {
    settings_log_t log;
    settings_t first, second, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];

    memset(flash, 0xFF, sizeof(flash));
    settings_reset(&first);
    settings_set(&first, SETTINGS_GAIN, 20);
    save(&first);

    second = first;
    settings_set(&second, SETTINGS_GAIN, 30);
    settings_encode(&second, 2, record);
    flash_program(SETTINGS_RECORD_SIZE, record, 20);

    settings_log_scan(&log, &loaded, flash);
    CHECK(log.found && log.sequence == 1);
    CHECK(settings_get(&loaded, SETTINGS_GAIN) == 20);
    CHECK(log.next_offset == 2 * SETTINGS_RECORD_SIZE);

    save(&second);
    settings_log_scan(&log, &loaded, flash);
    CHECK(settings_get(&loaded, SETTINGS_GAIN) == 30);
}

// Whatever a fresh chip held: defaults, and block 0 is erased first
static void test_log_garbage(void) {
    settings_log_t log;
    settings_t settings, defaults;
    uint32_t state = 99;

    for (size_t i = 0; i < sizeof(flash); i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        flash[i] = (uint8_t)state;
    }

    settings_log_scan(&log, &settings, flash);
    settings_reset(&defaults);

    CHECK(!log.found);
    CHECK(memcmp(&settings, &defaults, sizeof(settings)) == 0);
    CHECK(log.next_offset == 0);
    CHECK(log.erase_first);
}

// Power lost after erasing the other block, before programming it: the
// newest record is still there
static void test_log_power_lost_after_erase(void) {
    settings_log_t log;
    settings_t settings, loaded;

    memset(flash, 0xFF, sizeof(flash));
    erases = 0;
    settings_reset(&settings);

    // Both blocks full: records 1 to 16 in block 0, 17 to 32 in block 1
    for (int i = 0; i < 32; i++) {
        settings_set(&settings, SETTINGS_BRIGHTNESS, 10 + (i % 19) * 5);
        save(&settings);
    }

    // The next save erases block 0, with the older records
    settings_log_scan(&log, &loaded, flash);
    CHECK(log.erase_first);
    CHECK(log.next_offset == 0);

    flash_erase(log.next_offset);

    settings_log_scan(&log, &loaded, flash);
    CHECK(log.found && log.sequence == 32);
    CHECK(memcmp(&settings, &loaded, sizeof(settings)) == 0);
}

// A shuffle changes the look or the scene, always, and nothing else
static void test_shuffle(void) {
    settings_t settings, before;
    uint32_t random = 12345;
    int same_look = 0, off_grid = 0, kept = 0, changed_look = 0;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_BRIGHTNESS, 40);
    settings_set(&settings, SETTINGS_SONG_PARTS, 0);
    settings_set(&settings, SETTINGS_GAIN, 25);
    settings_set(&settings, SETTINGS_BACKLIGHT, 60);

    for (int i = 0; i < 1000; i++) {
        before = settings;
        settings_shuffle(&settings, &random);

        same_look += settings_get(&settings, SETTINGS_LOOK) ==
                         settings_get(&before, SETTINGS_LOOK) &&
                     settings_get(&settings, SETTINGS_SCENE) ==
                         settings_get(&before, SETTINGS_SCENE);
        changed_look += settings_get(&settings, SETTINGS_LOOK) !=
                        settings_get(&before, SETTINGS_LOOK);

        for (int id = 0; id < SETTINGS_ID_COUNT; id++) {
            const settings_range_t *range = settings_range(id);
            int value = settings_get(&settings, id);

            off_grid += value < range->min || value > range->max ||
                        (value - range->min) % range->step != 0;
        }

        kept += settings_get(&settings, SETTINGS_BRIGHTNESS) == 40 &&
                settings_get(&settings, SETTINGS_SONG_PARTS) == 0 &&
                settings_get(&settings, SETTINGS_GAIN) == 25 &&
                settings_get(&settings, SETTINGS_BACKLIGHT) == 60 &&
                settings_get(&settings, SETTINGS_QUIET_FLOOR) == -32 &&
                settings_get(&settings, SETTINGS_HIT_SENSITIVITY) == 30;
    }

    CHECK(same_look == 0);
    CHECK(off_grid == 0);
    CHECK(kept == 1000);
    // Five looks: most shuffles bring another
    CHECK(changed_look > 700);
}

int main(void) {
    test_ranges();
    test_defaults_are_the_constants();
    test_set_snaps_and_clamps();
    test_step_stops_at_the_ends();
    test_step_wraps_looks_scenes_and_song_parts();
    test_clamp();
    test_default_tuning();
    test_tuning_follows_the_settings();
    test_hit_sensitivity_is_inverse();
    test_record_round_trip();
    test_record_damage_detected();
    test_record_rejects_erased_and_other_versions();
    test_record_from_older_firmware();
    test_record_keeps_negative_values();
    test_record_of_version_1_ignored();
    test_log_empty();
    test_log_saves_and_loads();
    test_log_torn_record();
    test_log_garbage();
    test_log_power_lost_after_erase();
    test_shuffle();

    return CHECK_REPORT();
}
