#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>
#include <stdlib.h>

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

static color_neopixel_t color_neopixel_from_hsv(uint8_t h, uint8_t s,
                                                uint8_t v) {
    uint8_t region, remainder, p, q, t;

    if (s == 0)
        return color_neopixel_from_rgb(v, v, v);

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        return color_neopixel_from_rgb(v, t, p);
    case 1:
        return color_neopixel_from_rgb(q, v, p);
    case 2:
        return color_neopixel_from_rgb(p, v, t);
    case 3:
        return color_neopixel_from_rgb(p, q, v);
    case 4:
        return color_neopixel_from_rgb(t, p, v);
    default:
        return color_neopixel_from_rgb(v, p, q);
    }
}

static color_neopixel_t color_neopixel_from_hsv_f(float h, float s, float v) {
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

    return color_neopixel_from_rgb((uint8_t)(r * 255), (uint8_t)(g * 255),
                                   (uint8_t)(b * 255));
}

static color_neopixel_t color_neopixel_add(color_neopixel_t left,
                                           color_neopixel_t right) {
    return (color_neopixel_t){
        .grba =
            {
                .r = left.grba.r + right.grba.r,
                .g = left.grba.g + right.grba.g,
                .b = left.grba.b + right.grba.b,
                .a = 0,
            },
    };
}

#endif
