#ifndef PALETTE_H
#define PALETTE_H

/**
 * Palettes: lists of colour stops, interpolated linearly. The effects pick
 * colours by position, 0 at the first stop and 1 at the last.
 */

#include "color.h"

// Rainbow wraps around, the others go dark to bright
typedef enum {
    PALETTE_RAINBOW,
    PALETTE_SYNTHWAVE,
    PALETTE_FIRE,
    PALETTE_OCEAN,
    PALETTE_COUNT
} palette_t;

/**
 * Colour at a position, 0..1 spanning the palette. Beyond that the rainbow
 * wraps around and the other palettes reflect at their ends
 */
rgb_t palette_color(palette_t palette, float position);

#endif
