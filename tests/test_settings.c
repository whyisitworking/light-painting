#include "check.h"
#include "settings.h"
#include "visualizer.h"

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

    CHECK(settings_get(&settings, SETTINGS_MODE) == (int)EFFECTS_MODE);
    CHECK(settings_get(&settings, SETTINGS_PALETTE) == (int)EFFECTS_PALETTE);
    CHECK(settings_value(&settings, SETTINGS_BRIGHTNESS) == 1.f);
    CHECK(settings_value(&settings, SETTINGS_GAIN) == VISUALIZER_GAIN);
    CHECK(settings_value(&settings, SETTINGS_BEAT_THRESHOLD) ==
          FEATURES_HIT_THRESHOLD);
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
    CHECK(settings_value(&settings, SETTINGS_PEAK_HOLD) ==
          EFFECTS_PEAK_HOLD_MS);
    CHECK(settings_value(&settings, SETTINGS_TRAILS) == EFFECTS_TRAILS_MS);
    CHECK(settings_value(&settings, SETTINGS_DIFFUSE) == EFFECTS_DIFFUSE);
    CHECK(settings_get(&settings, SETTINGS_SYMMETRY) == (int)EFFECTS_SYMMETRY);
    CHECK(settings_value(&settings, SETTINGS_CHASE) ==
          EFFECTS_CHASE_LEDS_PER_S);
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

// The defaults make the visualizer's default tuning, field for field
static void test_default_tuning(void) {
    settings_t settings;
    visualizer_tuning_t tuning, defaults = visualizer_default_tuning();

    settings_reset(&settings);
    tuning = settings_tuning(&settings);

    CHECK(tuning.gain == defaults.gain);
    CHECK(tuning.features.attack_ms == defaults.features.attack_ms);
    CHECK(tuning.features.decay_ms == defaults.features.decay_ms);
    CHECK(tuning.features.min_ceiling_db == defaults.features.min_ceiling_db);
    CHECK(tuning.features.hit_threshold ==
          defaults.features.hit_threshold);
    CHECK(tuning.effects.mode == defaults.effects.mode);
    CHECK(tuning.effects.palette == defaults.effects.palette);
    CHECK(tuning.effects.brightness == defaults.effects.brightness);
    CHECK(tuning.effects.river_speed == defaults.effects.river_speed);
    CHECK(tuning.effects.ripple_speed == defaults.effects.ripple_speed);
    CHECK(tuning.effects.peak_hold_ms == defaults.effects.peak_hold_ms);
    CHECK(tuning.effects.drift_period_s == defaults.effects.drift_period_s);
    CHECK(tuning.effects.warmth == defaults.effects.warmth);
    CHECK(tuning.effects.flash_level == defaults.effects.flash_level);
    CHECK(tuning.effects.sparkle_rate == defaults.effects.sparkle_rate);
    CHECK(tuning.effects.trails_ms == defaults.effects.trails_ms);
    CHECK(tuning.effects.diffuse == defaults.effects.diffuse);
    CHECK(tuning.effects.symmetry == defaults.effects.symmetry);
    CHECK(tuning.effects.chase_leds_per_s ==
          defaults.effects.chase_leds_per_s);
}

// Each setting lands in its field, divided by its divisor
static void test_tuning_follows_the_settings(void) {
    settings_t settings;
    visualizer_tuning_t tuning;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_MODE, EFFECTS_MODE_GLOW);
    settings_set(&settings, SETTINGS_BRIGHTNESS, 50);
    settings_set(&settings, SETTINGS_GAIN, 25);
    settings_set(&settings, SETTINGS_DRIFT, 0);
    settings_set(&settings, SETTINGS_RIVER_SPEED, 3);
    settings_set(&settings, SETTINGS_SPARKLES, 55);
    settings_set(&settings, SETTINGS_TRAILS, 300);
    settings_set(&settings, SETTINGS_DIFFUSE, 45);
    settings_set(&settings, SETTINGS_SYMMETRY, 3);
    settings_set(&settings, SETTINGS_CHASE, -80);
    tuning = settings_tuning(&settings);

    CHECK(tuning.effects.mode == EFFECTS_MODE_GLOW);
    CHECK(tuning.effects.brightness == 0.5f);
    CHECK(tuning.gain == 2.5f);
    CHECK(tuning.effects.drift_period_s == 0.f);
    CHECK(tuning.effects.river_speed == 3);
    CHECK(tuning.effects.sparkle_rate == 0.055f);
    CHECK(tuning.effects.trails_ms == 300.f);
    CHECK(tuning.effects.diffuse == 0.45f);
    CHECK(tuning.effects.symmetry == 3);
    CHECK(tuning.effects.chase_leds_per_s == -80.f);
}

static void test_record_round_trip(void) {
    settings_t saved, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence = 0;

    settings_reset(&saved);
    settings_set(&saved, SETTINGS_MODE, EFFECTS_MODE_VU);
    settings_set(&saved, SETTINGS_QUIET_FLOOR, -40);
    settings_set(&saved, SETTINGS_PEAK_HOLD, 1250);

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

// The record of the firmware before the layers: the layers come up off
static void test_record_before_the_layers(void) {
    settings_t settings, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_MODE, EFFECTS_MODE_VU);
    settings_set(&settings, SETTINGS_BACKLIGHT, 60);
    settings_set(&settings, SETTINGS_TRAILS, 500);
    settings_set(&settings, SETTINGS_SYMMETRY, 4);
    settings_encode(&settings, 9, record);
    cut_record(record, SETTINGS_BACKLIGHT + 1);

    CHECK(settings_decode(&loaded, &sequence, record));
    CHECK(settings_get(&loaded, SETTINGS_MODE) == EFFECTS_MODE_VU);
    CHECK(settings_get(&loaded, SETTINGS_BACKLIGHT) == 60);
    CHECK(settings_get(&loaded, SETTINGS_TRAILS) == 0);
    CHECK(settings_get(&loaded, SETTINGS_DIFFUSE) == 0);
    CHECK(settings_get(&loaded, SETTINGS_SYMMETRY) == 1);
    CHECK(settings_get(&loaded, SETTINGS_CHASE) == 0);
}

// Negative values survive a record
static void test_record_keeps_negative_chase(void) {
    settings_t saved, loaded;
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint32_t sequence;

    settings_reset(&saved);
    settings_set(&saved, SETTINGS_CHASE, -120);
    settings_encode(&saved, 3, record);

    CHECK(settings_decode(&loaded, &sequence, record));
    CHECK(settings_get(&loaded, SETTINGS_CHASE) == -120);
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
        settings_set(&settings, SETTINGS_PEAK_HOLD, (i % 40) * 50);
        settings_set(&settings, SETTINGS_MODE, i % EFFECTS_MODE_COUNT);
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
        settings_set(&settings, SETTINGS_WARMTH, (i % 20) * 5);
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

// Shuffles change the look on its grids, never the brightness, the sound
// response or the backlight, and always show a new mode or palette
static void test_shuffle(void) {
    settings_t settings, before;
    uint32_t random = 12345;
    int changed_drift = 0, same_look = 0, off_grid = 0, kept = 0;
    int shuffled_trails = 0, shuffled_diffuse = 0, kept_layers = 0;

    settings_reset(&settings);
    settings_set(&settings, SETTINGS_BRIGHTNESS, 40);
    settings_set(&settings, SETTINGS_GAIN, 25);
    settings_set(&settings, SETTINGS_BACKLIGHT, 60);

    for (int i = 0; i < 1000; i++) {
        before = settings;
        settings_shuffle(&settings, &random);

        same_look += settings_get(&settings, SETTINGS_MODE) ==
                         settings_get(&before, SETTINGS_MODE) &&
                     settings_get(&settings, SETTINGS_PALETTE) ==
                         settings_get(&before, SETTINGS_PALETTE);
        changed_drift += settings_get(&settings, SETTINGS_DRIFT) !=
                         settings_get(&before, SETTINGS_DRIFT);
        shuffled_trails += settings_get(&settings, SETTINGS_TRAILS) !=
                           settings_get(&before, SETTINGS_TRAILS);
        shuffled_diffuse += settings_get(&settings, SETTINGS_DIFFUSE) !=
                            settings_get(&before, SETTINGS_DIFFUSE);
        kept_layers += settings_get(&settings, SETTINGS_SYMMETRY) == 1 &&
                       settings_get(&settings, SETTINGS_CHASE) == 0;

        for (int id = 0; id < SETTINGS_ID_COUNT; id++) {
            const settings_range_t *range = settings_range(id);
            int value = settings_get(&settings, id);

            off_grid += value < range->min || value > range->max ||
                        (value - range->min) % range->step != 0;
        }

        kept += settings_get(&settings, SETTINGS_BRIGHTNESS) == 40 &&
                settings_get(&settings, SETTINGS_GAIN) == 25 &&
                settings_get(&settings, SETTINGS_BACKLIGHT) == 60 &&
                settings_get(&settings, SETTINGS_QUIET_FLOOR) == -32 &&
                settings_get(&settings, SETTINGS_BEAT_THRESHOLD) == 30 &&
                settings_get(&settings, SETTINGS_ATTACK) == 10 &&
                settings_get(&settings, SETTINGS_DECAY) == 120;
    }

    CHECK(same_look == 0);
    CHECK(off_grid == 0);
    CHECK(kept == 1000);
    // The layers vary too: drift has 31 values
    CHECK(changed_drift > 900);
    // The trails and the blur are shuffled, the folding and the sliding not
    CHECK(shuffled_trails > 900);
    CHECK(shuffled_diffuse > 900);
    CHECK(kept_layers == 1000);
}

// Every setting is global or in exactly one mode's list, and no mode has more
// than two rows: nothing is unreachable in the menu, the Look page stays short
static void test_mode_ids(void) {
    static const settings_id_t global[] = {
        SETTINGS_MODE,    SETTINGS_PALETTE,        SETTINGS_BRIGHTNESS,
        SETTINGS_GAIN,    SETTINGS_BEAT_THRESHOLD, SETTINGS_QUIET_FLOOR,
        SETTINGS_ATTACK,  SETTINGS_DECAY,          SETTINGS_DRIFT,
        SETTINGS_WARMTH,  SETTINGS_FLASH,          SETTINGS_SPARKLES,
        SETTINGS_BACKLIGHT, SETTINGS_TRAILS,       SETTINGS_DIFFUSE,
        SETTINGS_SYMMETRY, SETTINGS_CHASE,
    };
    int owners[SETTINGS_ID_COUNT] = {0};
    size_t river_count, ripples_count, vu_count;
    const settings_id_t *river, *ripples, *vu;

    // The 17 that apply to every mode never grow
    CHECK(sizeof(global) / sizeof(global[0]) == 17);

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
        size_t count = 99;
        const settings_id_t *ids =
            settings_mode_ids((effects_mode_t)mode, &count);

        CHECK(count <= SETTINGS_MODE_IDS_MAX);
        CHECK(count == 0 || ids != nullptr);
        for (size_t k = 0; k < count; k++)
            owners[ids[k]]++;
    }

    for (int id = 0; id < SETTINGS_ID_COUNT; id++) {
        bool is_global = false;

        for (size_t k = 0; k < sizeof(global) / sizeof(global[0]); k++)
            is_global = is_global || global[k] == (settings_id_t)id;

        CHECK(owners[id] == (is_global ? 0 : 1));
    }

    // The three that moved off the Effects page
    river = settings_mode_ids(EFFECTS_MODE_RIVER, &river_count);
    ripples = settings_mode_ids(EFFECTS_MODE_RIPPLES, &ripples_count);
    vu = settings_mode_ids(EFFECTS_MODE_VU, &vu_count);
    CHECK(river_count == 1 && river[0] == SETTINGS_RIVER_SPEED);
    CHECK(ripples_count == 1 && ripples[0] == SETTINGS_RIPPLE_SPEED);
    CHECK(vu_count == 1 && vu[0] == SETTINGS_PEAK_HOLD);

    // Out of range: none
    CHECK(settings_mode_ids(EFFECTS_MODE_COUNT, &river_count) == nullptr);
    CHECK(river_count == 0);
}

int main(void) {
    test_ranges();
    test_defaults_are_the_constants();
    test_set_snaps_and_clamps();
    test_step_stops_at_the_ends();
    test_step_wraps_modes_and_palettes();
    test_clamp();
    test_default_tuning();
    test_tuning_follows_the_settings();
    test_record_round_trip();
    test_record_damage_detected();
    test_record_rejects_erased_and_other_versions();
    test_record_from_older_firmware();
    test_record_before_the_layers();
    test_record_keeps_negative_chase();
    test_log_empty();
    test_log_saves_and_loads();
    test_log_torn_record();
    test_log_garbage();
    test_log_power_lost_after_erase();
    test_shuffle();
    test_mode_ids();

    return CHECK_REPORT();
}
