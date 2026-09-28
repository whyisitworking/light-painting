#include "check.h"
#include "effects_internal.h"

#include <math.h>
#include <string.h>

constexpr size_t LEDS = 300;
constexpr size_t BANDS = FEATURES_BAND_COUNT;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

static effects_t effects;
static uint32_t pixels[LEDS];
static float bands[BANDS];

static void start(size_t led_count) {
    CHECK(effects_init(&effects, led_count, BANDS, HOP_PERIOD_S, 1));
}

static void clear_frame(void) {
    memset(effects.frame, 0, effects.led_count * sizeof(rgb_t));
}

static void set_pixel(size_t i, float r, float g, float b) {
    effects.frame[i] = (rgb_t){r, g, b};
}

static void tune(effects_tuning_t tuning) { effects_tune(&effects, &tuning); }

static void set_trails(float ms) {
    effects_tuning_t tuning = effects.tuning;

    tuning.trails_ms = ms;
    tune(tuning);
}

static double energy(void) {
    double sum = 0.0;

    for (size_t i = 0; i < effects.led_count; i++)
        sum += (double)effects.frame[i].r;

    return sum;
}

// Every layer off: the frame is left as the mode drew it, and nothing is
// remembered
static void test_off_changes_nothing(void) {
    rgb_t before[LEDS];

    start(LEDS);
    for (size_t i = 0; i < LEDS; i++)
        set_pixel(i, (float)(i % 7) * 0.1f, (float)(i % 5) * 0.2f, 0.3f);
    memcpy(before, effects.frame, sizeof(before));

    effects_layers_apply(&effects);

    CHECK(memcmp(before, effects.frame, sizeof(before)) == 0);
    CHECK(effects.layers.offset == 0.f);
    effects_deinit(&effects);
}

// A bright pixel fades by exp(-hop / tau) per frame
static void test_trails_fade(void) {
    float k = expf(-HOP_PERIOD_S / 0.1f);

    start(LEDS);
    set_trails(100.f);

    clear_frame();
    set_pixel(10, 1.f, 0.5f, 0.25f);
    effects_layers_apply(&effects);
    CHECK_NEAR(effects.frame[10].r, 1.0, 1e-6);

    for (int frame = 1; frame <= 20; frame++) {
        clear_frame();
        effects_layers_apply(&effects);
    }

    CHECK_NEAR(effects.frame[10].r, powf(k, 20.f), 1e-5);
    CHECK_NEAR(effects.frame[10].g, 0.5 * powf(k, 20.f), 1e-5);
    CHECK_NEAR(effects.frame[10].b, 0.25 * powf(k, 20.f), 1e-5);
    effects_deinit(&effects);
}

// The brighter of the new and the faded old, never their sum
static void test_trails_take_the_maximum(void) {
    start(LEDS);
    set_trails(1000.f);

    for (int frame = 0; frame < 5; frame++) {
        clear_frame();
        set_pixel(10, 0.6f, 0.f, 0.f);
        effects_layers_apply(&effects);
        CHECK_NEAR(effects.frame[10].r, 0.6, 1e-6);
    }

    // A new brighter pixel wins at once
    clear_frame();
    set_pixel(10, 0.9f, 0.f, 0.f);
    effects_layers_apply(&effects);
    CHECK_NEAR(effects.frame[10].r, 0.9, 1e-6);
    effects_deinit(&effects);
}

// What is remembered is what the strip can show: 1 at most
static void test_trails_remember_at_most_one(void) {
    float k = expf(-HOP_PERIOD_S / 0.1f);

    start(LEDS);
    set_trails(100.f);

    clear_frame();
    set_pixel(10, 3.f, 0.f, 0.f);
    effects_layers_apply(&effects);
    CHECK_NEAR(effects.frame[10].r, 3.0, 1e-6);

    clear_frame();
    effects_layers_apply(&effects);
    CHECK_NEAR(effects.frame[10].r, k, 1e-6);
    effects_deinit(&effects);
}

// Fades to exactly black, not to ever smaller floats
static void test_trails_end_at_zero(void) {
    start(LEDS);
    set_trails(100.f);

    clear_frame();
    set_pixel(10, 1.f, 1.f, 1.f);
    effects_layers_apply(&effects);

    for (int frame = 0; frame < 1000; frame++) {
        clear_frame();
        effects_layers_apply(&effects);
    }

    CHECK(effects.frame[10].r == 0.f);
    CHECK(effects.frame[10].g == 0.f);
    CHECK(effects.frame[10].b == 0.f);
    effects_deinit(&effects);
}

// Switching off forgets: switching on again does not show an old frame
static void test_trails_off_forgets(void) {
    start(LEDS);
    set_trails(1000.f);

    clear_frame();
    set_pixel(10, 1.f, 1.f, 1.f);
    effects_layers_apply(&effects);

    set_trails(0.f);
    clear_frame();
    effects_layers_apply(&effects);
    CHECK(effects.frame[10].r == 0.f);

    set_trails(1000.f);
    clear_frame();
    effects_layers_apply(&effects);
    CHECK(effects.frame[10].r == 0.f);
    effects_deinit(&effects);
}

// Through the whole render: a lit band lingers with trails and not without
static uint32_t lit_after_silence(float trails_ms) {
    sound_t sound = {.bands = bands};
    effects_tuning_t tuning;
    uint32_t lit;

    memset(bands, 0, sizeof(bands));
    start(LEDS);
    tuning = effects.tuning;
    tuning.mode = EFFECTS_MODE_SPECTRUM;
    tuning.trails_ms = trails_ms;
    tune(tuning);

    // Band 20 lights for one frame, then ten silent ones
    bands[20] = 1.f;
    effects_render(&effects, &sound, pixels);
    bands[20] = 0.f;
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &sound, pixels);

    lit = pixels[20 * (LEDS - 1) / (BANDS - 1)];
    effects_deinit(&effects);

    return lit;
}

static void test_trails_through_render(void) {
    CHECK(lit_after_silence(0.f) == 0);
    CHECK(lit_after_silence(500.f) != 0);
}

// Values out of range, or NaN, keep the current layer settings
static void test_tuning_ignores_invalid(void) {
    effects_tuning_t tuning;

    start(LEDS);
    tuning = effects.tuning;
    tuning.trails_ms = 200.f;
    tuning.diffuse = 0.5f;
    tuning.symmetry = 3;
    tuning.chase_leds_per_s = -40.f;
    tune(tuning);

    tuning = effects_default_tuning();
    tuning.trails_ms = -1.f;
    tuning.diffuse = 1.5f;
    tuning.symmetry = 0;
    tuning.chase_leds_per_s = NAN;
    tune(tuning);

    CHECK(effects.tuning.trails_ms == 200.f);
    CHECK(effects.tuning.diffuse == 0.5f);
    CHECK(effects.tuning.symmetry == 3);
    CHECK(effects.tuning.chase_leds_per_s == -40.f);

    tuning.symmetry = EFFECTS_SYMMETRY_MAX + 1;
    tuning.trails_ms = NAN;
    tune(tuning);
    CHECK(effects.tuning.symmetry == 3);
    CHECK(effects.tuning.trails_ms == 200.f);
    effects_deinit(&effects);
}

// The defaults leave every layer off
static void test_defaults_are_off(void) {
    effects_tuning_t tuning = effects_default_tuning();

    CHECK(tuning.trails_ms == 0.f);
    CHECK(tuning.diffuse == 0.f);
    CHECK(tuning.symmetry == 1);
    CHECK(tuning.chase_leds_per_s == 0.f);
}

static void set_diffuse(float amount) {
    effects_tuning_t tuning = effects.tuning;

    tuning.diffuse = amount;
    tune(tuning);
}

// Full amount averages three LEDs
static void test_diffuse_impulse(void) {
    start(LEDS);
    set_diffuse(1.f);

    clear_frame();
    set_pixel(100, 1.f, 0.f, 0.f);
    effects_layers_apply(&effects);

    CHECK_NEAR(effects.frame[99].r, 1.0 / 3.0, 1e-6);
    CHECK_NEAR(effects.frame[100].r, 1.0 / 3.0, 1e-6);
    CHECK_NEAR(effects.frame[101].r, 1.0 / 3.0, 1e-6);
    CHECK(effects.frame[98].r == 0.f);
    CHECK(effects.frame[102].r == 0.f);

    // Half the amount: a = 1/6 either side, 2/3 stays
    clear_frame();
    set_diffuse(0.5f);
    set_pixel(100, 1.f, 0.f, 0.f);
    effects_layers_apply(&effects);

    CHECK_NEAR(effects.frame[99].r, 1.0 / 6.0, 1e-6);
    CHECK_NEAR(effects.frame[100].r, 2.0 / 3.0, 1e-6);
    effects_deinit(&effects);
}

// Nothing leaks out of the strip's ends, and nothing is made
static void test_diffuse_keeps_the_energy(void) {
    double before;

    start(LEDS);
    set_diffuse(0.8f);

    for (size_t i = 0; i < LEDS; i++)
        set_pixel(i, (float)((i * 37) % 11) / 10.f, 0.f, 0.f);
    before = energy();
    effects_layers_apply(&effects);
    CHECK_NEAR(energy(), before, 1e-3);

    // At the ends too, where a neighbour is missing
    clear_frame();
    set_pixel(0, 1.f, 0.f, 0.f);
    set_pixel(LEDS - 1, 1.f, 0.f, 0.f);
    effects_layers_apply(&effects);
    CHECK_NEAR(energy(), 2.0, 1e-5);
    CHECK_NEAR(effects.frame[0].r, 1.0 - 0.8 / 3.0, 1e-6);
    effects_deinit(&effects);
}

// Blurring again spreads further
static void test_diffuse_spreads_over_frames(void) {
    start(LEDS);
    set_diffuse(1.f);

    clear_frame();
    set_pixel(100, 1.f, 0.f, 0.f);
    effects_layers_apply(&effects);
    effects_layers_apply(&effects);

    CHECK(effects.frame[98].r > 0.f);
    CHECK(effects.frame[102].r > 0.f);
    CHECK(effects.frame[97].r == 0.f);
    effects_deinit(&effects);
}

static void set_symmetry(size_t segments) {
    effects_tuning_t tuning = effects.tuning;

    tuning.symmetry = segments;
    tune(tuning);
}

// A ramp from 0 to 1 along the strip, in the red channel
static void draw_ramp(void) {
    for (size_t i = 0; i < effects.led_count; i++)
        set_pixel(i, (float)i / (float)(effects.led_count - 1), 0.f, 0.f);
}

// Two segments: a mirror of the whole image, squeezed to half
static void test_symmetry_two(void) {
    start(LEDS);
    set_symmetry(2);

    draw_ramp();
    effects_layers_apply(&effects);

    for (size_t i = 0; i < LEDS; i++)
        CHECK(effects.frame[i].r == effects.frame[LEDS - 1 - i].r);

    // The first segment holds the whole ramp: its ends are the ramp's ends,
    // each the mean of the two LEDs it covers
    CHECK_NEAR(effects.frame[0].r, 0.5 / 299.0, 1e-6);
    CHECK_NEAR(effects.frame[149].r, 298.5 / 299.0, 1e-6);
    effects_deinit(&effects);
}

// 3 and 4 segments: each is the first, or its reverse when it is odd
static void check_segments(size_t segments) {
    size_t length = LEDS / segments;

    start(LEDS);
    set_symmetry(segments);

    draw_ramp();
    effects_layers_apply(&effects);

    for (size_t j = 1; j < segments; j++)
        for (size_t p = 0; p < length; p++)
            CHECK(effects.frame[j * length + p].r ==
                  effects.frame[j % 2 == 0 ? p : length - 1 - p].r);

    effects_deinit(&effects);
}

static void test_symmetry_three_and_four(void) {
    check_segments(3);
    check_segments(4);
}

// A count that does not divide: every LED is written, nothing goes missing
static void test_symmetry_odd_count(void) {
    start(301);
    set_symmetry(2);

    for (size_t i = 0; i < 301; i++)
        set_pixel(i, 1.f, 1.f, 1.f);
    effects_layers_apply(&effects);

    for (size_t i = 0; i < 301; i++)
        CHECK_NEAR(effects.frame[i].r, 1.0, 1e-6);
    effects_deinit(&effects);
}

// A strip too short for the segments is left as it is
static void test_symmetry_too_short(void) {
    rgb_t before[5];

    start(5);
    set_symmetry(4);

    draw_ramp();
    memcpy(before, effects.frame, sizeof(before));
    effects_layers_apply(&effects);

    CHECK(memcmp(before, effects.frame, sizeof(before)) == 0);
    effects_deinit(&effects);
}

static void set_chase(float leds_per_s) {
    effects_tuning_t tuning = effects.tuning;

    tuning.chase_leds_per_s = leds_per_s;
    tune(tuning);
}

// Where the light is: the red channel's centre of mass
static double centroid(void) {
    double sum = 0.0, weighted = 0.0;

    for (size_t i = 0; i < effects.led_count; i++) {
        sum += (double)effects.frame[i].r;
        weighted += (double)effects.frame[i].r * (double)i;
    }

    return weighted / sum;
}

// A fresh image every frame, as the modes draw it: the shift accumulates in
// the offset, not in the frame
static void chase_frames(size_t from, int frames) {
    for (int frame = 0; frame < frames; frame++) {
        clear_frame();
        set_pixel(from, 1.f, 0.f, 0.f);
        effects_layers_apply(&effects);
    }
}

// 100 LEDs a second is 0.52 LEDs a frame, between two LEDs by interpolation
static void test_chase_moves_the_image(void) {
    double moved = 100.0 * (double)HOP_PERIOD_S * 20.0;

    start(LEDS);
    set_chase(100.f);
    chase_frames(10, 20);

    CHECK_NEAR(centroid(), 10.0 + moved, 0.01);
    CHECK_NEAR(energy(), 1.0, 1e-5);
    effects_deinit(&effects);
}

static void test_chase_backwards_and_around(void) {
    double moved = 200.0 * (double)HOP_PERIOD_S * 40.0;

    start(LEDS);
    set_chase(-200.f);
    chase_frames(5, 40);

    // 5 - 41.9 wraps to just under the end of the strip
    CHECK_NEAR(centroid(), 5.0 - moved + 300.0, 0.01);
    CHECK_NEAR(energy(), 1.0, 1e-5);
    effects_deinit(&effects);
}

// A whole number of LEDs is an exact shift, no blur
static void test_chase_whole_leds_are_exact(void) {
    start(LEDS);
    // One LED per frame
    set_chase(1.f / HOP_PERIOD_S);
    chase_frames(10, 6);

    CHECK_NEAR(effects.frame[16].r, 1.0, 1e-3);
    CHECK_NEAR(effects.frame[15].r, 0.0, 1e-3);
    CHECK_NEAR(effects.frame[17].r, 0.0, 1e-3);
    effects_deinit(&effects);
}

// The offset stays on the strip however long it runs, either way
static int frames_off_the_strip(float leds_per_s) {
    int off = 0;

    set_chase(leds_per_s);
    for (int frame = 0; frame < 100000; frame++) {
        clear_frame();
        effects_layers_apply(&effects);
        off += !(effects.layers.offset >= 0.f &&
                 effects.layers.offset < (float)LEDS);
    }

    return off;
}

static void test_chase_offset_stays_bounded(void) {
    start(LEDS);

    CHECK(frames_off_the_strip(200.f) == 0);
    CHECK(frames_off_the_strip(-200.f) == 0);
    effects_deinit(&effects);
}

// Off is exactly the unshifted image
static void test_chase_off_resets(void) {
    start(LEDS);
    set_chase(100.f);
    chase_frames(10, 20);
    CHECK(effects.layers.offset > 0.f);

    set_chase(0.f);
    chase_frames(10, 1);

    CHECK(effects.layers.offset == 0.f);
    CHECK(effects.frame[10].r == 1.f);
    effects_deinit(&effects);
}

// All four on, in the busiest mode: it runs and stays finite and bounded
static void test_all_layers_through_render(void) {
    sound_t sound = {.bands = bands, .loudness = 0.8f, .centroid = 0.5f};
    effects_tuning_t tuning;

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 0.7f;

    start(LEDS);
    tuning = effects.tuning;
    tuning.mode = EFFECTS_MODE_RIPPLES;
    tuning.trails_ms = 300.f;
    tuning.diffuse = 0.6f;
    tuning.symmetry = 3;
    tuning.chase_leds_per_s = 60.f;
    tune(tuning);

    for (int frame = 0; frame < 500; frame++) {
        sound.beat = frame % 40 == 0;
        sound.beat_strength = 0.8f;
        effects_render(&effects, &sound, pixels);
    }

    for (size_t i = 0; i < LEDS; i++)
        CHECK(isfinite(effects.frame[i].r) && effects.frame[i].r >= 0.f);
    CHECK(effects.layers.offset >= 0.f &&
          effects.layers.offset < (float)LEDS);
    effects_deinit(&effects);
}

int main(void) {
    test_off_changes_nothing();
    test_trails_fade();
    test_trails_take_the_maximum();
    test_trails_remember_at_most_one();
    test_trails_end_at_zero();
    test_trails_off_forgets();
    test_trails_through_render();
    test_diffuse_impulse();
    test_diffuse_keeps_the_energy();
    test_diffuse_spreads_over_frames();
    test_symmetry_two();
    test_symmetry_three_and_four();
    test_symmetry_odd_count();
    test_symmetry_too_short();
    test_chase_moves_the_image();
    test_chase_backwards_and_around();
    test_chase_whole_leds_are_exact();
    test_chase_offset_stays_bounded();
    test_chase_off_resets();
    test_all_layers_through_render();
    test_tuning_ignores_invalid();
    test_defaults_are_off();

    return CHECK_REPORT();
}
