#include "check.h"
#include "color.h"
#include "effects.h"

#include <string.h>

#define LEDS 300
#define BANDS FEATURES_BAND_COUNT
#define HOP (256.f / 48828.125f)

static float bands[BANDS];
static uint32_t pixels[LEDS];

static features_t quiet(void) {
    memset(bands, 0, sizeof(bands));
    return (features_t){.bands = bands};
}

static unsigned brightness(uint32_t pixel) {
    color_neopixel_t color = {.value = pixel};
    return color.grba.r + color.grba.g + color.grba.b;
}

static size_t brightest(size_t from, size_t to) {
    size_t best = from;

    for (size_t i = from; i < to; i++)
        if (brightness(pixels[i]) > brightness(pixels[best]))
            best = i;

    return best;
}

static bool all_dark(void) {
    for (size_t i = 0; i < LEDS; i++)
        if (pixels[i] != 0)
            return false;
    return true;
}

static void test_rejects_invalid(void) {
    effects_t effects;

    CHECK(!effects_init(&effects, 1, BANDS, HOP, 1));
    CHECK(!effects_init(&effects, LEDS, 1, HOP, 1));
    CHECK(!effects_init(&effects, LEDS, BANDS, 0.f, 1));
}

static void check_silence_is_dark(effects_mode_t mode) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, mode);

    for (int frame = 0; frame < 50; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

static void test_spectrum_band_position(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM);

    // Band 20 of 32 lands at 20 / 31 of the strip
    bands[20] = 1.f;
    effects_render(&effects, &features, pixels);
    CHECK_NEAR(brightest(0, LEDS), 20.0 * (LEDS - 1) / (BANDS - 1), 1.0);

    effects_deinit(&effects);
}

static void test_mirrored_spectrum_band_position(void) {
    effects_t effects;
    features_t features = quiet();
    size_t half = LEDS / 2;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM_MIRRORED);

    bands[20] = 1.f;
    effects_render(&effects, &features, pixels);

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
    features_t features = quiet();
    unsigned first;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM);

    features.beat = true;
    features.beat_strength = 1.f;
    effects_render(&effects, &features, pixels);
    first = brightness(pixels[0]);
    CHECK(first > 0);

    features = quiet();
    for (int frame = 0; frame < 5; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(brightness(pixels[0]) < first);

    for (int frame = 0; frame < 200; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

// A sound enters at the centre and flows outward one LED per frame
static void test_river_flows_outward(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_RIVER);

    features.loudness = 1.f;
    features.centroid = 0.5f;
    effects_render(&effects, &features, pixels);
    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2);

    features = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &features, pixels);

    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2 + 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightest(0, LEDS / 2) ==
          LEDS / 2 - 1 - 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightness(pixels[LEDS / 2]) == 0);

    effects_deinit(&effects);
}

// A beat launches a pulse from the centre that travels outward
static void test_ripple_travels(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_RIPPLES);

    features.beat = true;
    features.beat_strength = 1.f;
    effects_render(&effects, &features, pixels);

    features = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &features, pixels);

    CHECK_NEAR(brightest(LEDS / 2, LEDS), LEDS / 2 + 10 * EFFECTS_RIPPLE_SPEED,
               0.5);

    effects_deinit(&effects);
}

// Treble sparkles, reproducibly for a seed
static void test_sparkles_deterministic(void) {
    effects_t a, b;
    features_t features = quiet();
    uint32_t first[LEDS];
    bool lit = false;

    for (size_t band = BANDS * 3 / 4; band < BANDS; band++)
        bands[band] = 1.f;

    CHECK(effects_init(&a, LEDS, BANDS, HOP, 7));
    CHECK(effects_init(&b, LEDS, BANDS, HOP, 7));
    effects_set_mode(&a, EFFECTS_RIPPLES);
    effects_set_mode(&b, EFFECTS_RIPPLES);

    for (int frame = 0; frame < 20; frame++) {
        effects_render(&a, &features, first);
        effects_render(&b, &features, pixels);
        CHECK(memcmp(first, pixels, sizeof(pixels)) == 0);
    }

    for (size_t i = 0; i < LEDS; i++)
        lit = lit || pixels[i] != 0;
    CHECK(lit);

    effects_deinit(&a);
    effects_deinit(&b);
}

int main(void) {
    test_rejects_invalid();
    check_silence_is_dark(EFFECTS_SPECTRUM);
    check_silence_is_dark(EFFECTS_SPECTRUM_MIRRORED);
    test_spectrum_band_position();
    test_mirrored_spectrum_band_position();
    test_beat_flash();
    check_silence_is_dark(EFFECTS_RIVER);
    test_river_flows_outward();
    check_silence_is_dark(EFFECTS_RIPPLES);
    test_ripple_travels();
    test_sparkles_deterministic();

    return CHECK_REPORT();
}
