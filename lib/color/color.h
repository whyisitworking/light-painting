#ifndef COLOR_H
#define COLOR_H

/**
 * Colours: linear RGB as the effects compute it, gamma correction, and the
 * WS2812 word the neopixel driver shifts out
 */

#include <stdint.h>

// Linear 0..1 colour, before gamma
typedef struct {
    float r;
    float g;
    float b;
} rgb_t;

typedef union {
    struct {
        // Little endian
        uint8_t a;
        uint8_t b;
        uint8_t r;
        uint8_t g;
    } grba;
    uint32_t value;
} color_neopixel_t;

/**
 * Linear 0..1 to a gamma 2.2 corrected 0..255 channel, clamped
 */
uint8_t color_gamma(float value);

static inline color_neopixel_t color_neopixel_from_rgb(uint8_t r, uint8_t g,
                                                       uint8_t b) {
    return (color_neopixel_t){
        .grba =
            {
                .r = r,
                .g = g,
                .b = b,
                .a = 0,
            },
    };
}

static inline uint8_t color_saturating_add(uint8_t left, uint8_t right) {
    unsigned int sum = (unsigned int)left + right;

    return sum > UINT8_MAX ? UINT8_MAX : (uint8_t)sum;
}

static inline color_neopixel_t color_neopixel_add(color_neopixel_t left,
                                                  color_neopixel_t right) {
    // Saturate, a plain uint8_t sum would wrap bright colors to dark ones
    return (color_neopixel_t){
        .grba =
            {
                .r = color_saturating_add(left.grba.r, right.grba.r),
                .g = color_saturating_add(left.grba.g, right.grba.g),
                .b = color_saturating_add(left.grba.b, right.grba.b),
                .a = 0,
            },
    };
}

#endif
