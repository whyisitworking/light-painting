#include "check.h"
#include "flashes.h"
#include "rules.h"

#include <math.h>

constexpr size_t LEDS = 300;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

static rgb_t frame[LEDS];

static void fill(rgb_t color) {
    for (size_t i = 0; i < LEDS; i++)
        frame[i] = color;
}

// The light the strip gives, as the rules measure it: every channel through
// the gamma curve, averaged
static float light(void) {
    float sum = 0.f;

    for (size_t i = 0; i < LEDS; i++)
        sum += (float)(color_gamma(frame[i].r) + color_gamma(frame[i].g) +
                       color_gamma(frame[i].b));

    return sum / (3.f * 255.f * (float)LEDS);
}

// Over full, an LED is scaled as a whole: the ratios of its colour stay
static void test_hue_safe(void) {
    rules_t rules;

    rules_init(&rules, HOP_PERIOD_S);
    fill((rgb_t){0.f, 0.f, 0.f});
    frame[5] = (rgb_t){2.f, 1.f, 0.5f};
    rules_apply(&rules, frame, LEDS);

    CHECK(frame[5].r == 1.f);
    CHECK_NEAR(frame[5].g, 0.5, 1e-6);
    CHECK_NEAR(frame[5].b, 0.25, 1e-6);
}

/**
 * Draws a strip switching between two frames 10 times a second for 5 s
 * through the rules, and returns the most flashes in any one second of the
 * light that came out (flashes.h)
 */
static size_t most_flashes(void (*draw)(bool on)) {
    rules_t rules;
    const size_t hops = (size_t)(5.f / HOP_PERIOD_S),
                 per_s = (size_t)lroundf(1.f / HOP_PERIOD_S);
    static float lights[2000];

    rules_init(&rules, HOP_PERIOD_S);
    for (size_t hop = 0; hop < hops; hop++) {
        draw((hop * 10 / per_s) % 2 == 0);
        CHECK_NEAR(rules_apply(&rules, frame, LEDS), light(), 1e-5);
        lights[hop] = light();
    }

    return flashes_most(lights, hops, per_s, RULES_FLASH_RISE);
}

static void white_or_black(bool on) {
    fill(on ? (rgb_t){1.f, 1.f, 1.f} : (rgb_t){0.f, 0.f, 0.f});
}

// 0.9 and 0.8: a small step before gamma, 18 % of the most light after it
static void bright_flicker(bool on) {
    float level = on ? 0.9f : 0.8f;

    fill((rgb_t){level, level, level});
}

// White on 45 LEDs, a burst, over black
static void white_burst(bool on) {
    fill((rgb_t){0.f, 0.f, 0.f});
    for (size_t i = 100; on && i < 145; i++)
        frame[i] = (rgb_t){1.f, 1.f, 1.f};
}

// The counting itself, on light worked out by hand: two rises and falls of
// 0.2, then from a dip of 0.05 a rise of 0.07 (none) and one to 0.3: three
// flashes. A staircase rising 0.15 at a time without falling is one
static void test_counting(void) {
    static const float pairs[] = {0.f, 0.2f, 0.f, 0.2f, 0.05f, 0.12f, 0.3f};
    static const float stairs[] = {0.f, 0.15f, 0.3f, 0.45f, 0.6f};

    CHECK(flashes_most(pairs, 7, 7, RULES_FLASH_RISE) == 3);
    CHECK(flashes_most(stairs, 5, 5, RULES_FLASH_RISE) == 1);
}

// Flashing 10 times a second: in any second at most three flashes get
// through, measured in light, whether the whole strip goes from black to
// white, flickers near full, or a white burst lights 45 LEDs
static void test_flash_guard(void) {
    size_t whole = most_flashes(white_or_black),
           flicker = most_flashes(bright_flicker),
           burst = most_flashes(white_burst);

    printf("flash guard: at most %zu, %zu and %zu flashes in any second\n",
           whole, flicker, burst);
    CHECK(whole <= RULES_FLASHES_PER_S && whole > 0);
    CHECK(flicker <= RULES_FLASHES_PER_S && flicker > 0);
    CHECK(burst <= RULES_FLASHES_PER_S && burst > 0);
}

// A slow brightening is no flash: nothing is held back
static void test_slow_rise_passes(void) {
    rules_t rules;

    rules_init(&rules, HOP_PERIOD_S);
    for (int hop = 0; hop < 400; hop++) {
        float level = (float)hop / 400.f;

        fill((rgb_t){level, level, level});
        rules_apply(&rules, frame, LEDS);
        CHECK(frame[0].r == level && frame[LEDS - 1].b == level);
    }
}

int main(void) {
    test_hue_safe();
    test_counting();
    test_flash_guard();
    test_slow_rise_passes();

    return CHECK_REPORT();
}
