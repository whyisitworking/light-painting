#ifndef COLOR_H
#define COLOR_H

/**
 * Colours: linear RGB as the effects compute it, gamma correction, and the
 * WS2812 word the LED driver shifts out
 */

#include <stdint.h>

// Linear 0..1 colour, before gamma
typedef struct {
    float r;
    float g;
    float b;
} rgb_t;

// The word the WS2812 driver shifts out, MSB first: G, R, B, then 8
// unused bits
typedef union {
    struct {
        // Little endian
        uint8_t a;
        uint8_t b;
        uint8_t r;
        uint8_t g;
    } grba;
    uint32_t value;
} color_ws2812_t;

// color * k, channel by channel
static inline rgb_t color_rgb_scale(rgb_t color, float k) {
    return (rgb_t){color.r * k, color.g * k, color.b * k};
}

// Adds color to *pixel, channel by channel, unclamped: color_gamma() clamps
static inline void color_rgb_add(rgb_t *pixel, rgb_t color) {
    pixel->r += color.r;
    pixel->g += color.g;
    pixel->b += color.b;
}

/**
 * Linear 0..1 to a gamma 2.2 corrected 0..255 channel, clamped
 */
uint8_t color_gamma(float value);

// 0..255 channels, after gamma
static inline color_ws2812_t color_ws2812_from_rgb(uint8_t r, uint8_t g,
                                                   uint8_t b) {
    return (color_ws2812_t){
        .grba =
            {
                .r = r,
                .g = g,
                .b = b,
                .a = 0,
            },
    };
}

// 8-bit HSV, hue 0..255 around the circle, to a WS2812 word without gamma
static inline color_ws2812_t color_ws2812_from_hsv(uint8_t h, uint8_t s,
                                                   uint8_t v) {
    uint8_t region, remainder, p, q, t;

    if (s == 0)
        return color_ws2812_from_rgb(v, v, v);

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        return color_ws2812_from_rgb(v, t, p);
    case 1:
        return color_ws2812_from_rgb(q, v, p);
    case 2:
        return color_ws2812_from_rgb(p, v, t);
    case 3:
        return color_ws2812_from_rgb(p, q, v);
    case 4:
        return color_ws2812_from_rgb(t, p, v);
    default:
        return color_ws2812_from_rgb(v, p, q);
    }
}

// Float HSV, hue 0..360 degrees, saturation and value 0..1
static inline color_ws2812_t color_ws2812_from_hsv_f(float h, float s,
                                                     float v) {
    float hh, p, q, t, ff, r, g, b;
    int i;

    if (s <= 0.f) { // < is bogus, just shuts up warnings
        r = v;
        g = v;
        b = v;
    } else {
        hh = h;
        if (hh >= 360.f)
            hh = 0.f;
        hh /= 60.f;

        i = (int)hh;
        ff = hh - i;

        p = v * (1.f - s);
        q = v * (1.f - (s * ff));
        t = v * (1.f - (s * (1.f - ff)));

        switch (i) {
        case 0:
            r = v;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = v;
            b = p;
            break;
        case 2:
            r = p;
            g = v;
            b = t;
            break;

        case 3:
            r = p;
            g = q;
            b = v;
            break;
        case 4:
            r = t;
            g = p;
            b = v;
            break;
        case 5:
        default:
            r = v;
            g = p;
            b = q;
            break;
        }
    }

    return color_ws2812_from_rgb((uint8_t)(r * 255), (uint8_t)(g * 255),
                                 (uint8_t)(b * 255));
}

// left + right, at most 255
static inline uint8_t color_saturating_add(uint8_t left, uint8_t right) {
    unsigned int sum = (unsigned int)left + right;

    return sum > UINT8_MAX ? UINT8_MAX : (uint8_t)sum;
}

// Channel by channel, saturating
static inline color_ws2812_t color_ws2812_add(color_ws2812_t left,
                                              color_ws2812_t right) {
    // Saturate, a plain uint8_t sum would wrap bright colors to dark ones
    return (color_ws2812_t){
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
