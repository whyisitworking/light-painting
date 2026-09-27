#ifndef EFFECTS_INTERNAL_H
#define EFFECTS_INTERNAL_H

/**
 * Shared by effects.c and the mode renderers, not part of the API.
 *
 * Adding a mode: a value in effects_mode_t, a mode_*.c file with its
 * renderer, declared below, and its entry in the table in effects.c. A
 * renderer draws into this->frame, which is black when it is called; the
 * beat flash, gamma and the drift clock are applied after it, for all modes.
 */

#include "effects.h"

#include <stddef.h>

typedef void effects_renderer_t(effects_t *this, const sound_t *sound);

effects_renderer_t effects_mode_spectrum;
effects_renderer_t effects_mode_spectrum_mirrored;
effects_renderer_t effects_mode_river;
effects_renderer_t effects_mode_ripples;
effects_renderer_t effects_mode_vu;
effects_renderer_t effects_mode_glow;

// White sparkles appearing with the treble, fading each frame
void effects_draw_sparkles(effects_t *this, const sound_t *sound);

// Palette colour with the drift and the loudness warmth applied
static inline rgb_t effects_color_at(const effects_t *this,
                                     const sound_t *sound, float position) {
    float drift = EFFECTS_DRIFT_PERIOD_S > 0.f
                      ? this->time_s / EFFECTS_DRIFT_PERIOD_S
                      : 0.f;

    return palette_color(this->palette,
                         position + drift + sound->loudness * EFFECTS_WARMTH);
}

// Adds a colour at a distance from the centre, on both sides
static inline void effects_put_mirrored(effects_t *this, size_t distance,
                                        rgb_t color) {
    size_t right = this->led_count / 2 + distance;
    size_t centre_left = (this->led_count - 1) / 2;

    if (right < this->led_count)
        color_rgb_add(&this->frame[right], color);

    // With an odd count both sides share the centre LED
    if (distance <= centre_left && centre_left - distance != right)
        color_rgb_add(&this->frame[centre_left - distance], color);
}

// Band level at a fractional band position, interpolated
static inline float effects_band_at(const effects_t *this, const sound_t *sound,
                                    float position) {
    size_t band = (size_t)position;
    float fraction;

    if (band >= this->band_count - 1)
        return sound->bands[this->band_count - 1];

    fraction = position - (float)band;

    return sound->bands[band] +
           (sound->bands[band + 1] - sound->bands[band]) * fraction;
}

static inline float effects_band_mean(const sound_t *sound, size_t from,
                                      size_t to) {
    float sum = 0.f;

    for (size_t b = from; b < to; b++)
        sum += sound->bands[b];

    return to > from ? sum / (float)(to - from) : 0.f;
}

#endif
