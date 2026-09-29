#include "check.h"
#include "color.h"
#include "effects_internal.h"

#include <math.h>
#include <string.h>

constexpr size_t LEDS = 300;
constexpr size_t BANDS = FEATURES_BAND_COUNT;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

// After the music stops the strip must be exactly dark within 10 s
constexpr int TRIPWIRE_HOPS = (int)(10.f / HOP_PERIOD_S) + 1;

static effects_t effects;
static float bands[BANDS];
static uint32_t pixels[LEDS];

static sound_t quiet(void) {
    memset(bands, 0, sizeof(bands));
    return (sound_t){.bands = bands};
}

// Something for every mode to draw: a spread of bands that wanders, and a
// beat every half second at a moving centroid
static sound_t music(int hop) {
    float sum = 0.f;

    for (size_t b = 0; b < BANDS; b++) {
        bands[b] = 0.25f + 0.5f * (float)((b * 7 + (size_t)hop / 6) % 11) / 10.f;
        sum += bands[b];
    }

    return (sound_t){
        .bands = bands,
        .loudness = sum / (float)BANDS,
        .centroid = 0.15f + 0.07f * (float)((hop / 96) % 10),
        .beat = hop % 96 == 0,
        .beat_strength = 0.8f,
    };
}

static unsigned lum(uint32_t pixel) {
    color_ws2812_t color = {.value = pixel};

    return color.grba.r + color.grba.g + color.grba.b;
}

static bool all_dark(void) {
    for (size_t i = 0; i < LEDS; i++)
        if (pixels[i] != 0)
            return false;
    return true;
}

// The beat flash and the sparkles off: the tests measure the mode
static void start(effects_mode_t mode) {
    effects_tuning_t tuning;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    tuning = effects.tuning;
    tuning.mode = mode;
    tuning.flash_level = 0.f;
    tuning.sparkle_rate = 0.f;
    effects_tune(&effects, &tuning);
}

static void stop(void) { effects_deinit(&effects); }

static void render(const sound_t *sound) {
    effects_render(&effects, sound, pixels);
}

// Switches mode, the rest of the tuning as it is
static void set_mode(effects_mode_t mode) {
    effects_tuning_t tuning = effects.tuning;

    tuning.mode = mode;
    effects_tune(&effects, &tuning);
}

static uint32_t run_hash(effects_mode_t mode, int hops) {
    uint32_t hash = 2166136261u;

    start(mode);
    for (int hop = 0; hop < hops; hop++) {
        sound_t sound = music(hop);

        render(&sound);
        for (size_t i = 0; i < LEDS; i++)
            hash = (hash ^ pixels[i]) * 16777619u;
    }
    stop();

    return hash;
}

// What every mode must do, see the spec's Tests
static void check_all(effects_mode_t mode) {
    sound_t sound = quiet();
    int hop;

    // Silence is dark from a fresh start
    start(mode);
    for (hop = 0; hop < 50; hop++)
        render(&sound);
    CHECK(all_dark());
    stop();

    // Activity, then silence: dark within 10 s
    start(mode);
    for (hop = 0; hop < 300; hop++) {
        sound = music(hop);
        render(&sound);
    }
    sound = quiet();
    for (hop = 0; hop < TRIPWIRE_HOPS; hop++)
        render(&sound);
    CHECK(all_dark());
    stop();

    // Same seed, same pixels
    CHECK(run_hash(mode, 400) == run_hash(mode, 400));
}

// effects_sin against the libm sine, over four turns either side of 0
static void test_sin(void) {
    effects_t effects;
    double worst = 0.0;

    // Fills the table
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));

    for (int i = -2000; i <= 2000; i++) {
        float turns = (float)i / 500.f;

        worst = fmax(worst, fabs((double)effects_sin(turns) -
                                 sin(2.0 * M_PI * (double)turns)));
    }

    // Linear interpolation over 256 steps is off by at most (2 pi / 256)^2 / 8
    CHECK(worst < 1e-4);
    CHECK_NEAR(effects_sin(0.25f), 1.0, 1e-4);
    CHECK_NEAR(effects_sin(-1e-9f), 0.0, 1e-4);
    CHECK(effects_sin(NAN) == 0.f);
    CHECK(effects_sin(INFINITY) == 0.f);

    effects_deinit(&effects);
}

// The first values of xorshift32 from seed 1, the stream the sparkles used
static void test_random_stream(void) {
    effects_t effects;
    float unit;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    CHECK(effects_random(&effects) == 270369u);
    CHECK(effects_random(&effects) == 67634689u);

    unit = effects_random_unit(&effects);
    CHECK(unit >= 0.f && unit < 1.f);

    effects_deinit(&effects);

    // Seed 0 is replaced, the state is never 0
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 0));
    CHECK(effects_random(&effects) == 270369u);
    effects_deinit(&effects);
}

static void test_peak_band(void) {
    effects_t effects;
    float bands[BANDS] = {0};
    sound_t sound = {.bands = bands};

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));

    // Silence
    CHECK(effects_peak_band(&effects, &sound) == 0.f);

    // A single lit band is its own position
    bands[10] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 10.0 / (BANDS - 1), 1e-6);

    // Two equal neighbours: half way between them
    bands[11] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 10.5 / (BANDS - 1), 1e-6);

    // The ends
    memset(bands, 0, sizeof(bands));
    bands[0] = 1.f;
    CHECK(effects_peak_band(&effects, &sound) == 0.f);
    memset(bands, 0, sizeof(bands));
    bands[BANDS - 1] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 1.0, 1e-6);

    effects_deinit(&effects);
}

static float peak_height(size_t from, size_t to) {
    float peak = 0.f;

    for (size_t i = from; i < to; i++)
        peak = fmaxf(peak, fabsf(effects.pond.height[i]));

    return peak;
}

// A stone at the centroid: two waves run away from it, the one going left
// reflects at the end and comes back (1.5 LEDs a frame)
static void test_pond_wave_travels_and_reflects(void) {
    sound_t sound = quiet();

    start(EFFECTS_MODE_POND);
    sound.beat = true;
    sound.beat_strength = 1.f;
    sound.centroid = 0.1f;
    render(&sound);
    sound.beat = false;

    CHECK(peak_height(28, 33) > 0.3f);
    CHECK(peak_height(50, 80) < 0.01f);

    for (int frame = 0; frame < 20; frame++)
        render(&sound);
    CHECK(peak_height(50, 75) > 0.05f);

    for (int frame = 0; frame < 10; frame++)
        render(&sound);
    CHECK(peak_height(8, 26) > 0.05f);
    CHECK(peak_height(28, 33) < 0.1f);

    stop();
}

static void test_pond_reset_on_entry(void) {
    start(EFFECTS_MODE_POND);
    for (int hop = 0; hop < 200; hop++) {
        sound_t sound = music(hop);

        render(&sound);
    }
    CHECK(peak_height(0, LEDS) > 0.f);

    set_mode(EFFECTS_MODE_RIVER);
    set_mode(EFFECTS_MODE_POND);
    CHECK(peak_height(0, LEDS) == 0.f);

    // Tuning the same mode again does not reset
    for (int hop = 0; hop < 200; hop++) {
        sound_t sound = music(hop);

        render(&sound);
    }
    set_mode(EFFECTS_MODE_POND);
    CHECK(peak_height(0, LEDS) > 0.f);

    stop();
}

// Regions of the strip brighter than half of the brightest pixel
static int lobes(void) {
    unsigned top = 0;
    int count = 0;
    bool inside = false;

    for (size_t i = 0; i < LEDS; i++)
        if (lum(pixels[i]) > top)
            top = lum(pixels[i]);

    for (size_t i = 0; i < LEDS; i++) {
        bool above = top > 0 && lum(pixels[i]) * 2 > top;

        count += above && !inside;
        inside = above;
    }

    return count;
}

// A higher loudest band gives more nodes, and they slide there
static void test_cymatics_nodes_follow_the_peak_band(void) {
    sound_t sound = quiet();
    int low, high;

    start(EFFECTS_MODE_CYMATICS);
    sound.loudness = 0.5f;

    bands[5] = 1.f;
    for (int frame = 0; frame < 150; frame++)
        render(&sound);
    low = lobes();

    quiet();
    bands[25] = 1.f;
    // Not there yet a moment later: the nodes slide, they do not jump
    render(&sound);
    CHECK(lobes() <= low + 2);
    for (int frame = 0; frame < 150; frame++)
        render(&sound);
    high = lobes();

    CHECK(low >= 3 && low <= 7);
    CHECK(high >= 15);
    stop();
}

static size_t hottest(void) {
    size_t best = 0;

    for (size_t i = 0; i < effects.half_led_count; i++)
        if (effects.fire.heat[i] > effects.fire.heat[best])
            best = i;

    return best;
}

// Bass makes heat at the centre, and it moves outward as it cools
static void test_fire_burns_outward(void) {
    sound_t sound = quiet();
    size_t early, late;

    start(EFFECTS_MODE_FIRE);
    for (size_t b = 0; b < EFFECTS_BASS_BANDS; b++)
        bands[b] = 1.f;
    sound.loudness = 0.2f;

    for (int frame = 0; frame < 10; frame++)
        render(&sound);
    CHECK(effects.fire.heat[0] > 0.5f);

    sound = quiet();
    for (int frame = 0; frame < 10; frame++)
        render(&sound);
    early = hottest();
    for (int frame = 0; frame < 20; frame++)
        render(&sound);
    late = hottest();

    CHECK(late > early + 5);
    CHECK(effects.fire.heat[0] < 0.2f);
    stop();
}

static void test_fire_reset_on_entry(void) {
    start(EFFECTS_MODE_FIRE);
    for (int hop = 0; hop < 200; hop++) {
        sound_t sound = music(hop);

        render(&sound);
    }
    CHECK(effects.fire.heat[hottest()] > 0.f);

    set_mode(EFFECTS_MODE_RIVER);
    set_mode(EFFECTS_MODE_FIRE);
    CHECK(effects.fire.heat[hottest()] == 0.f);
    stop();
}

static int bright_count(unsigned above) {
    int count = 0;

    for (size_t i = 0; i < LEDS; i++)
        count += lum(pixels[i]) > above;

    return count;
}

static int strike_size(float strength) {
    sound_t sound = quiet();
    int size;

    start(EFFECTS_MODE_STORM);
    sound.beat = true;
    sound.beat_strength = strength;
    render(&sound);
    size = bright_count(150);
    stop();

    return size;
}

// A stronger beat strikes a longer bolt, and it fades fast
static void test_storm_strike_length_and_fade(void) {
    sound_t sound = quiet();
    int weak = strike_size(0.3f), strong = strike_size(1.f);

    CHECK(weak >= 30);
    CHECK(strong * 2 > weak * 3);

    start(EFFECTS_MODE_STORM);
    sound.beat = true;
    sound.beat_strength = 1.f;
    render(&sound);
    sound.beat = false;
    for (int frame = 0; frame < 40; frame++)
        render(&sound);
    CHECK(bright_count(150) == 0);
    stop();
}

static void test_storm_reset_on_entry(void) {
    sound_t sound = quiet();

    start(EFFECTS_MODE_STORM);
    sound.beat = true;
    sound.beat_strength = 1.f;
    render(&sound);
    CHECK(effects.storm.sky > 0.f);

    set_mode(EFFECTS_MODE_RIVER);
    set_mode(EFFECTS_MODE_STORM);
    CHECK(effects.storm.sky == 0.f);
    for (size_t i = 0; i < LEDS; i++)
        CHECK(effects.storm.afterglow[i] == 0.f);
    stop();
}

static bool has_white_pixel(void) {
    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};

        if (color.grba.r == 255 && color.grba.g == 255 && color.grba.b == 255)
            return true;
    }

    return false;
}

// Beats launch from alternate ends; where two cross they flash
static void test_pingpong_alternates_and_flashes_on_crossing(void) {
    sound_t sound = quiet();
    int first_flash = -1;
    int active = 0;
    float up = 0.f, down = 0.f;

    start(EFFECTS_MODE_PINGPONG);
    sound.beat = true;
    sound.beat_strength = 0.8f;
    render(&sound);
    CHECK(lum(pixels[0]) > 0);
    CHECK(lum(pixels[LEDS - 1]) == 0);
    render(&sound);
    sound.beat = false;

    for (size_t c = 0; c < EFFECTS_PINGPONG_MAX_COMETS; c++)
        if (effects.pingpong.comets[c].active) {
            active++;
            if (effects.pingpong.comets[c].direction > 0.f)
                up += 1.f;
            else
                down += 1.f;
        }
    CHECK(active == 2 && up == 1.f && down == 1.f);

    for (int frame = 2; frame < 90; frame++) {
        render(&sound);
        if (has_white_pixel() && first_flash < 0)
            first_flash = frame;
    }
    // They meet after about 50 frames (3 LEDs a frame each, 300 apart)
    CHECK(first_flash >= 40 && first_flash <= 60);
    stop();
}

// Nine beats in a row: at most eight comets, the most advanced makes room
static void test_pingpong_slots(void) {
    sound_t sound = quiet();

    start(EFFECTS_MODE_PINGPONG);
    sound.beat = true;
    sound.beat_strength = 0.5f;
    for (int beat = 0; beat < 9; beat++)
        render(&sound);
    for (size_t c = 0; c < EFFECTS_PINGPONG_MAX_COMETS; c++)
        CHECK(effects.pingpong.comets[c].active);
    stop();
}

static void test_pingpong_reset_on_entry(void) {
    sound_t sound = quiet();

    start(EFFECTS_MODE_PINGPONG);
    sound.beat = true;
    sound.beat_strength = 0.5f;
    render(&sound);
    set_mode(EFFECTS_MODE_RIVER);
    set_mode(EFFECTS_MODE_PINGPONG);
    for (size_t c = 0; c < EFFECTS_PINGPONG_MAX_COMETS; c++)
        CHECK(!effects.pingpong.comets[c].active);
    CHECK(effects.pingpong.launches == 0);
    stop();
}

// A dot follows the band that lights up in its stretch, and the others of
// that stretch gather there
static void test_swarm_chases_the_lit_band(void) {
    sound_t sound = quiet();
    // Dot 17 looks at bands 20 to 26 (its centre is band 22.9)
    float home, target;

    start(EFFECTS_MODE_SWARM);
    home = effects.swarm.position[17];
    target = 20.f / (float)(BANDS - 1) * (float)(LEDS - 1);
    CHECK(fabsf(home - target) > 20.f);

    bands[20] = 1.f;
    sound.loudness = 0.05f;
    for (int frame = 0; frame < 150; frame++)
        render(&sound);

    CHECK_NEAR(effects.swarm.position[17], target, 3.0);
    // A dot with the lit band out of reach stays home
    CHECK_NEAR(effects.swarm.position[0], 0.0, 1.0);
    stop();
}

static void test_swarm_reset_on_entry(void) {
    sound_t sound = quiet();
    float home;

    start(EFFECTS_MODE_SWARM);
    home = effects.swarm.position[17];
    bands[20] = 1.f;
    for (int frame = 0; frame < 100; frame++)
        render(&sound);
    CHECK(fabsf(effects.swarm.position[17] - home) > 10.f);

    set_mode(EFFECTS_MODE_RIVER);
    set_mode(EFFECTS_MODE_SWARM);
    CHECK(effects.swarm.position[17] == home);
    CHECK(effects.swarm.velocity[17] == 0.f);
    stop();
}

static uint32_t frame_hash(void) {
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < LEDS; i++)
        hash = (hash ^ pixels[i]) * 16777619u;

    return hash;
}

// The pattern moves while there is sound, and stays dark without
static void test_plasma_moves_and_is_gated(void) {
    sound_t sound = music(1);
    uint32_t first, later;

    start(EFFECTS_MODE_PLASMA);
    render(&sound);
    first = frame_hash();
    CHECK(!all_dark());

    for (int frame = 0; frame < 40; frame++)
        render(&sound);
    later = frame_hash();
    CHECK(first != later);

    // The phase keeps moving in silence, but nothing shows
    sound = quiet();
    render(&sound);
    CHECK(all_dark());
    stop();
}

int main(void) {
    test_sin();
    test_random_stream();
    test_peak_band();
    check_all(EFFECTS_MODE_POND);
    test_pond_wave_travels_and_reflects();
    test_pond_reset_on_entry();
    check_all(EFFECTS_MODE_CYMATICS);
    test_cymatics_nodes_follow_the_peak_band();
    check_all(EFFECTS_MODE_FIRE);
    test_fire_burns_outward();
    test_fire_reset_on_entry();
    check_all(EFFECTS_MODE_STORM);
    test_storm_strike_length_and_fade();
    test_storm_reset_on_entry();
    check_all(EFFECTS_MODE_PINGPONG);
    test_pingpong_alternates_and_flashes_on_crossing();
    test_pingpong_slots();
    test_pingpong_reset_on_entry();
    check_all(EFFECTS_MODE_SWARM);
    test_swarm_chases_the_lit_band();
    test_swarm_reset_on_entry();
    check_all(EFFECTS_MODE_PLASMA);
    test_plasma_moves_and_is_gated();

    return CHECK_REPORT();
}
