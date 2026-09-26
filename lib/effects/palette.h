#ifndef PALETTE_H
#define PALETTE_H

#include "color.h"

typedef enum {
    PALETTE_RAINBOW,
    PALETTE_SYNTHWAVE,
    PALETTE_FIRE,
    PALETTE_OCEAN,
    PALETTE_COUNT
} palette_id_t;

/**
 * Colour at a position, 0..1 spanning the palette. Beyond that the rainbow
 * wraps around and the other palettes reflect at their ends
 */
rgb_t palette_color(palette_id_t palette, float position);

#endif
