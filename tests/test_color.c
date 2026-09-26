#include "check.h"
#include "color.h"

static void test_layout(void) {
    // The neopixel driver shifts out the top 24 bits MSB first: G, R, B
    color_neopixel_t color = color_neopixel_from_rgb(0x11, 0x22, 0x33);

    CHECK(color.value == 0x22113300u);
}

static void test_add(void) {
    color_neopixel_t sum = color_neopixel_add(
        color_neopixel_from_rgb(10, 20, 30), color_neopixel_from_rgb(1, 2, 3));

    CHECK(sum.grba.r == 11);
    CHECK(sum.grba.g == 22);
    CHECK(sum.grba.b == 33);
}

static void test_add_saturates(void) {
    color_neopixel_t sum =
        color_neopixel_add(color_neopixel_from_rgb(200, 255, 128),
                           color_neopixel_from_rgb(100, 1, 127));

    CHECK(sum.grba.r == 255);
    CHECK(sum.grba.g == 255);
    CHECK(sum.grba.b == 255);
}

int main(void) {
    test_layout();
    test_add();
    test_add_saturates();

    return CHECK_REPORT();
}
