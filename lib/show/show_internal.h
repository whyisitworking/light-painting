#ifndef SHOW_INTERNAL_H
#define SHOW_INTERNAL_H

/**
 * Shared by show.c and the looks, not part of the API.
 *
 * Adding a look: a value in show_look_t, a look_*.c file with its draw
 * function (and a reset, for a look with state of its own), declared below,
 * and their entries in the tables in show.c. A look draws into
 * this->blocks.frame, which is black when it is called, and calls
 * blocks_draw() once; the gap fade, the rules, the brightness and the gamma
 * follow, for every look.
 */

#include "show.h"

typedef void show_look_fn(show_t *this, const sound_t *sound);
typedef void show_reset_fn(show_t *this);

show_look_fn show_look_pulse;

// A role's colour in the scene, field and accent swapped after a drop or a
// lift
rgb_t show_color(const show_t *this, scene_role_t role);

// The shared random stream: 32 bits, and 0 inclusive to 1 exclusive
uint32_t show_random(show_t *this);
float show_random_unit(show_t *this);

// 0 in silence, 1 from SHOW_PRESENCE_GROOVE up: what the washes scale by
static inline float show_presence(const sound_t *sound) {
    float presence = sound->groove / SHOW_PRESENCE_GROOVE;

    return presence < 1.f ? presence : 1.f;
}

// From a to b, t 0..1
static inline rgb_t show_mix(rgb_t a, rgb_t b, float t) {
    return (rgb_t){a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                   a.b + (b.b - a.b) * t};
}

// The centre of the strip, an LED index (between two for an even count)
static inline float show_centre(const show_t *this) {
    return (float)(this->led_count - 1) / 2.f;
}

// The LED at a distance from the centre, right side (true) or left
static inline size_t show_mirror_index(const show_t *this, size_t distance,
                                       bool right) {
    return right ? this->led_count / 2 + distance
                 : (this->led_count - 1) / 2 - distance;
}

// Adds a colour at a distance from the centre, on both sides
static inline void show_put_mirrored(show_t *this, size_t distance,
                                     rgb_t color) {
    size_t right = show_mirror_index(this, distance, true);
    size_t centre_left = (this->led_count - 1) / 2;

    if (right < this->led_count)
        color_rgb_add(&this->blocks.frame[right], color);

    // With an odd count both sides share the centre LED
    if (distance <= centre_left && centre_left - distance != right)
        color_rgb_add(&this->blocks.frame[centre_left - distance], color);
}

// Band level at a fractional band position, interpolated
static inline float show_band_at(const show_t *this, const sound_t *sound,
                                 float position) {
    size_t band = (size_t)position;
    float fraction;

    if (band >= this->band_count - 1)
        return sound->bands[this->band_count - 1];

    fraction = position - (float)band;

    return sound->bands[band] +
           (sound->bands[band + 1] - sound->bands[band]) * fraction;
}

#endif
