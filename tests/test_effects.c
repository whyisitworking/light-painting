#include "check.h"
#include "color.h"
#include "effects.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t LEDS = 300;
constexpr size_t BANDS = FEATURES_BAND_COUNT;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

static float bands[BANDS];
static uint32_t pixels[LEDS];

static sound_t quiet(void) {
    memset(bands, 0, sizeof(bands));
    return (sound_t){.bands = bands};
}

static unsigned brightness(uint32_t pixel) {
    color_ws2812_t color = {.value = pixel};
    return color.grba.r + color.grba.g + color.grba.b;
}

static size_t brightest(size_t from, size_t to) {
    size_t best = from;

    for (size_t i = from; i < to; i++)
        if (brightness(pixels[i]) > brightness(pixels[best]))
            best = i;

    return best;
}

// Switches mode, the rest of the tuning as it is
static void set_mode(effects_t *effects, effects_mode_t mode) {
    effects_tuning_t tuning = effects->tuning;

    tuning.mode = mode;
    effects_tune(effects, &tuning);
}

static bool all_dark(void) {
    for (size_t i = 0; i < LEDS; i++)
        if (pixels[i] != 0)
            return false;
    return true;
}

static void test_rejects_invalid(void) {
    effects_t effects;

    CHECK(!effects_init(&effects, 1, BANDS, HOP_PERIOD_S, 1));
    CHECK(!effects_init(&effects, LEDS, 1, HOP_PERIOD_S, 1));
    CHECK(!effects_init(&effects, LEDS, BANDS, 0.f, 1));
}

static void check_silence_is_dark(effects_mode_t mode) {
    effects_t effects;
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, mode);

    for (int frame = 0; frame < 50; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

static void test_spectrum_band_position(void) {
    effects_t effects;
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM);

    // Band 20 of 32 lands at 20 / 31 of the strip
    bands[20] = 1.f;
    effects_render(&effects, &sound, pixels);
    CHECK_NEAR(brightest(0, LEDS), 20.0 * (LEDS - 1) / (BANDS - 1), 1.0);

    effects_deinit(&effects);
}

static void test_mirrored_spectrum_band_position(void) {
    effects_t effects;
    sound_t sound = quiet();
    size_t half = LEDS / 2;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM_MIRRORED);

    bands[20] = 1.f;
    effects_render(&effects, &sound, pixels);

    // Bass in the centre: the same distance on both sides
    CHECK_NEAR(brightest(half, LEDS) - half,
               20.0 * (half - 1) / (BANDS - 1), 1.0);
    CHECK_NEAR(half - 1 - brightest(0, half),
               20.0 * (half - 1) / (BANDS - 1), 1.0);

    effects_deinit(&effects);
}

// A beat flashes the whole strip, then fades
static void test_beat_flash(void) {
    effects_t effects;
    sound_t sound = quiet();
    unsigned first;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM);

    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);
    first = brightness(pixels[0]);
    CHECK(first > 0);

    // White added after gamma, as it will be shown
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};
        long white = lroundf(EFFECTS_FLASH_LEVEL * 255.f);

        CHECK(color.grba.r == white && color.grba.g == white &&
              color.grba.b == white);
    }

    sound = quiet();
    for (int frame = 0; frame < 5; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(brightness(pixels[0]) < first);

    for (int frame = 0; frame < 200; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

// The flash saturates on bright pixels instead of wrapping to dark
static void test_beat_flash_saturates(void) {
    effects_t effects;
    sound_t sound = quiet();
    uint32_t plain[LEDS];
    unsigned white = (unsigned)lroundf(EFFECTS_FLASH_LEVEL * 255.f);

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 1.f;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM);
    effects_render(&effects, &sound, plain);
    effects_deinit(&effects);

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM);
    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);
    effects_deinit(&effects);

    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t before = {.value = plain[i]};
        color_ws2812_t after = {.value = pixels[i]};

        CHECK(after.grba.r == (before.grba.r + white > 255
                                   ? 255
                                   : before.grba.r + white));
        CHECK(after.grba.g == (before.grba.g + white > 255
                                   ? 255
                                   : before.grba.g + white));
        CHECK(after.grba.b == (before.grba.b + white > 255
                                   ? 255
                                   : before.grba.b + white));
    }
}

// A sound enters at the centre and flows outward one LED per frame
static void test_river_flows_outward(void) {
    effects_t effects;
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_RIVER);

    sound.loudness = 1.f;
    sound.centroid = 0.5f;
    effects_render(&effects, &sound, pixels);
    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2);

    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);

    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2 + 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightest(0, LEDS / 2) ==
          LEDS / 2 - 1 - 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightness(pixels[LEDS / 2]) == 0);

    effects_deinit(&effects);
}

// A beat launches a pulse from the centre that travels outward
static void test_ripple_travels(void) {
    effects_t effects;
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_RIPPLES);

    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);

    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);

    CHECK_NEAR(brightest(LEDS / 2, LEDS), LEDS / 2 + 10 * EFFECTS_RIPPLE_SPEED,
               0.5);

    effects_deinit(&effects);
}

// Treble sparkles, reproducibly for a seed
static void test_sparkles_deterministic(void) {
    effects_t a, b;
    sound_t sound = quiet();
    uint32_t first[LEDS];
    bool lit = false;

    for (size_t band = BANDS * 3 / 4; band < BANDS; band++)
        bands[band] = 1.f;

    CHECK(effects_init(&a, LEDS, BANDS, HOP_PERIOD_S, 7));
    CHECK(effects_init(&b, LEDS, BANDS, HOP_PERIOD_S, 7));
    set_mode(&a, EFFECTS_MODE_RIPPLES);
    set_mode(&b, EFFECTS_MODE_RIPPLES);

    for (int frame = 0; frame < 20; frame++) {
        effects_render(&a, &sound, first);
        effects_render(&b, &sound, pixels);
        CHECK(memcmp(first, pixels, sizeof(pixels)) == 0);
    }

    for (size_t i = 0; i < LEDS; i++)
        lit = lit || pixels[i] != 0;
    CHECK(lit);

    effects_deinit(&a);
    effects_deinit(&b);
}

// Twin meters from both ends, peak dot holds then falls
static void test_vu(void) {
    effects_t effects;
    sound_t sound = quiet();
    size_t half = LEDS / 2, length = half / 2;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_VU);

    sound.loudness = 0.5f;
    effects_render(&effects, &sound, pixels);

    for (size_t d = 0; d < length; d++) {
        CHECK(brightness(pixels[d]) > 0);
        CHECK(brightness(pixels[LEDS - 1 - d]) > 0);
    }
    // The peak dot right after the bar, then dark
    CHECK(brightness(pixels[length]) > 0);
    for (size_t d = length + 1; d < half; d++)
        CHECK(brightness(pixels[d]) == 0);

    // Held a while after the sound stopped
    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(brightness(pixels[length]) > 0);
    CHECK(brightness(pixels[0]) == 0);

    // Then falling
    for (int frame = 0; frame < 90; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(brightness(pixels[length]) == 0);
    CHECK(brightest(0, half) > 0);
    CHECK(brightest(0, half) < length);

    effects_deinit(&effects);
}

// The whole strip breathes with the bass
static void test_glow_follows_bass(void) {
    effects_t effects;
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_GLOW);

    for (size_t band = 0; band < EFFECTS_BASS_BANDS; band++)
        bands[band] = 1.f;
    effects_render(&effects, &sound, pixels);

    for (size_t i = 0; i < LEDS; i++)
        CHECK(brightness(pixels[i]) > 0);

    effects_deinit(&effects);
}

// The drift clock wraps every two drift periods, where the colours repeat
// for wrapping and reflecting palettes alike, instead of growing until
// adding a hop no longer changes it (~36 h)
static void test_drift_clock_wraps(void) {
    effects_t effects;
    sound_t sound = quiet();
    uint32_t early[LEDS];

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 1.f;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    set_mode(&effects, EFFECTS_MODE_SPECTRUM);
    effects.time_s = 10.f;
    effects_render(&effects, &sound, early);

    // Same colours, up to float rounding
    effects.time_s = 10.f + 2.f * EFFECTS_DRIFT_PERIOD_S;
    effects_render(&effects, &sound, pixels);
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t a = {.value = early[i]}, b = {.value = pixels[i]};

        CHECK(abs(a.grba.r - b.grba.r) <= 1 && abs(a.grba.g - b.grba.g) <= 1 &&
              abs(a.grba.b - b.grba.b) <= 1);
    }

    effects.time_s = 2.f * EFFECTS_DRIFT_PERIOD_S - HOP_PERIOD_S / 2.f;
    effects_render(&effects, &sound, pixels);
    CHECK(effects.time_s >= 0.f);
    CHECK(effects.time_s < HOP_PERIOD_S);

    effects_deinit(&effects);
}

// Tuned to the defaults, nothing changes
static void test_default_tuning_changes_nothing(void) {
    effects_t effects;
    effects_tuning_t defaults = effects_default_tuning();
    float peak_hold_s;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    peak_hold_s = effects.peak_hold_s;

    effects_tune(&effects, &defaults);
    CHECK(effects.peak_hold_s == peak_hold_s);

    // Field by field: memcmp would compare padding too, which is left
    // unset (4 bytes before river_speed on a 64-bit host)
    CHECK(effects.tuning.mode == defaults.mode);
    CHECK(effects.tuning.palette == defaults.palette);
    CHECK(effects.tuning.brightness == defaults.brightness);
    CHECK(effects.tuning.river_speed == defaults.river_speed);
    CHECK(effects.tuning.ripple_speed == defaults.ripple_speed);
    CHECK(effects.tuning.peak_hold_ms == defaults.peak_hold_ms);
    CHECK(effects.tuning.drift_period_s == defaults.drift_period_s);
    CHECK(effects.tuning.warmth == defaults.warmth);
    CHECK(effects.tuning.flash_level == defaults.flash_level);
    CHECK(effects.tuning.sparkle_rate == defaults.sparkle_rate);

    effects_deinit(&effects);
}

// Out of range fields keep their values, the others still apply
static void test_tuning_ignores_invalid(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));

    tuning.mode = EFFECTS_MODE_COUNT;
    tuning.palette = PALETTE_COUNT;
    tuning.river_speed = 0;
    tuning.ripple_speed = -1.f;
    tuning.peak_hold_ms = NAN;
    tuning.drift_period_s = -5.f;
    tuning.warmth = INFINITY;
    tuning.flash_level = 1.5f;
    tuning.sparkle_rate = 0.5f;
    effects_tune(&effects, &tuning);

    CHECK(effects.tuning.mode == EFFECTS_MODE);
    CHECK(effects.tuning.palette == EFFECTS_PALETTE);
    CHECK(effects.tuning.river_speed == EFFECTS_RIVER_SPEED);
    CHECK(effects.tuning.ripple_speed == EFFECTS_RIPPLE_SPEED);
    CHECK(effects.tuning.peak_hold_ms == EFFECTS_PEAK_HOLD_MS);
    CHECK(effects.tuning.drift_period_s == EFFECTS_DRIFT_PERIOD_S);
    CHECK(effects.tuning.warmth == EFFECTS_WARMTH);
    CHECK(effects.tuning.flash_level == EFFECTS_FLASH_LEVEL);
    CHECK(effects.tuning.sparkle_rate == 0.5f);

    effects_deinit(&effects);
}

static void test_tuned_river_speed(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_RIVER;
    tuning.river_speed = 3;
    effects_tune(&effects, &tuning);

    sound.loudness = 1.f;
    sound.centroid = 0.5f;
    effects_render(&effects, &sound, pixels);

    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);

    // The last of the 3 LEDs the sound entered on, 30 further out
    CHECK(brightest(LEDS / 2, LEDS) >= LEDS / 2 + 30);
    CHECK(brightest(LEDS / 2, LEDS) <= LEDS / 2 + 32);

    effects_deinit(&effects);
}

static void test_tuned_ripple_speed(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_RIPPLES;
    tuning.ripple_speed = 4.f;
    effects_tune(&effects, &tuning);

    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);

    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);

    CHECK_NEAR(brightest(LEDS / 2, LEDS), LEDS / 2 + 10 * 4.f, 0.5);

    effects_deinit(&effects);
}

// Without a hold time the peak dot falls on the very next frame
static void test_tuned_peak_hold(void) {
    size_t length = LEDS / 4;

    for (int held = 0; held < 2; held++) {
        effects_t effects;
        effects_tuning_t tuning = effects_default_tuning();
        sound_t sound = quiet();

        CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
        tuning.mode = EFFECTS_MODE_VU;
        tuning.peak_hold_ms = held ? EFFECTS_PEAK_HOLD_MS : 0.f;
        effects_tune(&effects, &tuning);

        sound.loudness = 0.5f;
        effects_render(&effects, &sound, pixels);
        sound = quiet();
        effects_render(&effects, &sound, pixels);

        CHECK((brightness(pixels[length]) > 0) == (bool)held);

        effects_deinit(&effects);
    }
}

// Without warmth the colours do not depend on loudness, without drift not
// on time, and the drift clock stays at 0
static void test_tuned_warmth_and_drift(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();
    uint32_t quiet_pixels[LEDS];

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 1.f;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_SPECTRUM;
    tuning.warmth = 0.f;
    tuning.drift_period_s = 0.f;
    effects_tune(&effects, &tuning);

    sound.loudness = 0.f;
    effects_render(&effects, &sound, quiet_pixels);
    sound.loudness = 1.f;
    for (int frame = 0; frame < 100; frame++)
        effects_render(&effects, &sound, pixels);

    CHECK(memcmp(quiet_pixels, pixels, sizeof(pixels)) == 0);
    CHECK(effects.time_s == 0.f);

    // With the default warmth, loudness moves the colours
    tuning.warmth = EFFECTS_WARMTH;
    effects_tune(&effects, &tuning);
    effects_render(&effects, &sound, pixels);
    CHECK(memcmp(quiet_pixels, pixels, sizeof(pixels)) != 0);

    effects_deinit(&effects);
}

// No flash, no sparkles
static void test_tuned_flash_and_sparkles(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 7));
    tuning.mode = EFFECTS_MODE_GLOW;
    tuning.flash_level = 0.f;
    tuning.sparkle_rate = 0.f;
    effects_tune(&effects, &tuning);

    for (size_t band = BANDS * 3 / 4; band < BANDS; band++)
        bands[band] = 1.f;
    sound.beat = true;
    sound.beat_strength = 1.f;

    for (int frame = 0; frame < 20; frame++)
        effects_render(&effects, &sound, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

// At 0 everything is dark, the beat flash included
static void test_brightness_zero_is_dark(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 1.f;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_SPECTRUM;
    tuning.brightness = 0.f;
    effects_tune(&effects, &tuning);

    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

// At half brightness the pixels are gamma of half their level, and the
// flash half as bright to the eye too: scaled by 0.5 ^ gamma after gamma
static void test_half_brightness(void) {
    effects_t effects;
    effects_tuning_t tuning = effects_default_tuning();
    sound_t sound = quiet();
    uint8_t full = color_gamma(1.f), half = color_gamma(0.5f);
    long white = lroundf(EFFECTS_FLASH_LEVEL * 255.f * powf(0.5f, COLOR_GAMMA));

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_SPECTRUM;
    tuning.brightness = 0.5f;
    effects_tune(&effects, &tuning);

    // The flash alone, on silence
    sound.beat = true;
    sound.beat_strength = 1.f;
    effects_render(&effects, &sound, pixels);
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};

        CHECK(color.grba.r == white && color.grba.g == white &&
              color.grba.b == white);
    }
    effects_deinit(&effects);

    // Full levels, no flash: no channel above gamma of half
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    effects_tune(&effects, &tuning);
    sound = quiet();
    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 1.f;
    effects_render(&effects, &sound, pixels);
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};

        CHECK(color.grba.r <= half && color.grba.g <= half &&
              color.grba.b <= half);
    }
    CHECK(half < full);

    effects_deinit(&effects);

    // The VU peak dot added on the end of the bar goes past 1: capped
    // before the brightness, it is no brighter than full white either
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning.mode = EFFECTS_MODE_VU;
    effects_tune(&effects, &tuning);
    sound = quiet();
    sound.loudness = 0.505f;
    effects_render(&effects, &sound, pixels);
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};

        CHECK(color.grba.r <= half && color.grba.g <= half &&
              color.grba.b <= half);
    }

    effects_deinit(&effects);
}

int main(void) {
    test_rejects_invalid();
    check_silence_is_dark(EFFECTS_MODE_SPECTRUM);
    check_silence_is_dark(EFFECTS_MODE_SPECTRUM_MIRRORED);
    test_spectrum_band_position();
    test_mirrored_spectrum_band_position();
    test_beat_flash();
    test_beat_flash_saturates();
    check_silence_is_dark(EFFECTS_MODE_RIVER);
    test_river_flows_outward();
    check_silence_is_dark(EFFECTS_MODE_RIPPLES);
    test_ripple_travels();
    test_sparkles_deterministic();
    test_drift_clock_wraps();
    test_default_tuning_changes_nothing();
    test_tuning_ignores_invalid();
    test_tuned_river_speed();
    test_tuned_ripple_speed();
    test_tuned_peak_hold();
    test_tuned_warmth_and_drift();
    test_tuned_flash_and_sparkles();
    test_brightness_zero_is_dark();
    test_half_brightness();
    check_silence_is_dark(EFFECTS_MODE_VU);
    check_silence_is_dark(EFFECTS_MODE_GLOW);
    test_vu();
    test_glow_follows_bass();

    return CHECK_REPORT();
}
