#include "check.h"
#include "color.h"

#include <math.h>

static void test_layout(void) {
    // The WS2812 driver shifts out the top 24 bits MSB first: G, R, B
    color_ws2812_t color = color_ws2812_from_rgb(0x11, 0x22, 0x33);

    CHECK(color.value == 0x22113300u);
}

static void test_add(void) {
    color_ws2812_t sum = color_ws2812_add(color_ws2812_from_rgb(10, 20, 30),
                                          color_ws2812_from_rgb(1, 2, 3));

    CHECK(sum.grba.r == 11);
    CHECK(sum.grba.g == 22);
    CHECK(sum.grba.b == 33);
}

static void test_add_saturates(void) {
    color_ws2812_t sum = color_ws2812_add(color_ws2812_from_rgb(200, 255, 128),
                                          color_ws2812_from_rgb(100, 1, 127));

    CHECK(sum.grba.r == 255);
    CHECK(sum.grba.g == 255);
    CHECK(sum.grba.b == 255);
}

static void test_gamma(void) {
    CHECK(color_gamma(0.f) == 0);
    CHECK(color_gamma(1.f) == 255);
    CHECK(color_gamma(0.5f) == 56);  // 127.5 rounds to entry 128
    CHECK(color_gamma(-1.f) == 0);
    CHECK(color_gamma(2.f) == 255);
    CHECK(color_gamma(NAN) == 0);
}

// The primaries, and grey without saturation
static void test_hsv(void) {
    color_ws2812_t red = color_ws2812_from_hsv(0, 255, 255);
    color_ws2812_t green = color_ws2812_from_hsv_f(120.f, 1.f, 1.f);
    color_ws2812_t grey = color_ws2812_from_hsv(100, 0, 128);

    CHECK(red.grba.r == 255 && red.grba.g == 0 && red.grba.b == 0);
    CHECK(green.grba.r == 0 && green.grba.g == 255 && green.grba.b == 0);
    CHECK(grey.grba.r == 128 && grey.grba.g == 128 && grey.grba.b == 128);
}

int main(void) {
    test_layout();
    test_add();
    test_add_saturates();
    test_gamma();
    test_hsv();

    return CHECK_REPORT();
}
