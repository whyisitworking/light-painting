#ifndef SCENE_H
#define SCENE_H

/**
 * Scenes: three colours, each with a role. The looks ask for a role, never a
 * colour, so every look works with every scene and at most three colours are
 * on the strip at once (concert designers keep to two or three).
 *
 *   field   most of the strip, the background
 *   accent  what draws the eye
 *   hit     the peaks
 *
 * Linear 0..1 RGB before gamma, first guesses to be tuned by eye
 */

#include "color.h"

typedef enum {
    SCENE_NEON_NOIR,
    SCENE_EMBER,
    SCENE_DUSK,
    SCENE_ACID,
    SCENE_ICE,
    SCENE_COUNT
} scene_t;

typedef enum {
    SCENE_FIELD,
    SCENE_ACCENT,
    SCENE_HIT,
    SCENE_ROLE_COUNT
} scene_role_t;

// The colour of a role in a scene. Out of range: the first scene, the field
rgb_t scene_color(scene_t scene, scene_role_t role);

#endif
