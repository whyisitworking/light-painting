#include "check.h"
#include "palette.h"

#include <math.h>

#define TOLERANCE 1e-5

static void check_rgb(rgb_t actual, float r, float g, float b) {
    CHECK_NEAR(actual.r, r, TOLERANCE);
    CHECK_NEAR(actual.g, g, TOLERANCE);
    CHECK_NEAR(actual.b, b, TOLERANCE);
}

static void test_endpoints(void) {
    check_rgb(palette_color(PALETTE_FIRE, 0.f), 0.15f, 0.f, 0.f);
    check_rgb(palette_color(PALETTE_FIRE, 1.f), 1.f, 1.f, 0.7f);
    check_rgb(palette_color(PALETTE_OCEAN, 0.f), 0.f, 0.05f, 0.2f);
    check_rgb(palette_color(PALETTE_RAINBOW, 0.f), 1.f, 0.f, 0.f);
}

// Rainbow wraps around, the others reflect at their ends
static void test_wrap_and_reflect(void) {
    rgb_t a, b;

    check_rgb(palette_color(PALETTE_RAINBOW, 1.f), 1.f, 0.f, 0.f);
    check_rgb(palette_color(PALETTE_RAINBOW, 0.5f), 0.f, 1.f, 1.f);

    a = palette_color(PALETTE_RAINBOW, 0.3f);
    b = palette_color(PALETTE_RAINBOW, 2.3f);
    check_rgb(b, a.r, a.g, a.b);

    a = palette_color(PALETTE_FIRE, 0.75f);
    b = palette_color(PALETTE_FIRE, 1.25f);
    check_rgb(b, a.r, a.g, a.b);

    a = palette_color(PALETTE_SYNTHWAVE, 0.2f);
    b = palette_color(PALETTE_SYNTHWAVE, -0.2f);
    check_rgb(b, a.r, a.g, a.b);
}

static void test_interpolates_between_stops(void) {
    // Fire stops 0 and 1 are (0.15, 0, 0) and (0.8, 0.05, 0), 4 segments
    check_rgb(palette_color(PALETTE_FIRE, 0.125f), 0.475f, 0.025f, 0.f);
}

static void test_gamma(void) {
    CHECK(palette_gamma(0.f) == 0);
    CHECK(palette_gamma(1.f) == 255);
    CHECK(palette_gamma(0.5f) == 55);
    CHECK(palette_gamma(-1.f) == 0);
    CHECK(palette_gamma(2.f) == 255);
    CHECK(palette_gamma(NAN) == 0);
}

int main(void) {
    test_endpoints();
    test_wrap_and_reflect();
    test_interpolates_between_stops();
    test_gamma();

    return CHECK_REPORT();
}
