#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

typedef enum {
    PALETTE_RAINBOW,
    PALETTE_SYNTHWAVE,
    PALETTE_FIRE,
    PALETTE_OCEAN,
    PALETTE_COUNT
} palette_id_t;

// Linear 0..1 colour, before gamma
typedef struct {
    float r;
    float g;
    float b;
} rgb_t;

/**
 * Colour at a position, 0..1 spanning the palette. Beyond that the rainbow
 * wraps around and the other palettes reflect at their ends
 */
rgb_t palette_color(palette_id_t palette, float position);

/**
 * Linear 0..1 to a gamma 2.2 corrected 0..255 channel, clamped
 */
uint8_t palette_gamma(float value);

#endif
