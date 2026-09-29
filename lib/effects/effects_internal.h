#ifndef EFFECTS_INTERNAL_H
#define EFFECTS_INTERNAL_H

/**
 * Shared by effects.c and the mode renderers, not part of the API.
 *
 * Adding a mode: a value in effects_mode_t, a mode_*.c file with its
 * renderer, declared below, its entry in the table in effects.c and, for a
 * mode with state, a reset in the table below it. A
 * renderer draws into this->frame, which is black when it is called; the
 * layers (layers.c), the beat flash, gamma and the drift clock are applied
 * after it, for all modes.
 */

#include "effects.h"

#include <stddef.h>

typedef void effects_renderer_t(effects_t *this, const sound_t *sound);

// Clears a mode's state. Called when the mode is switched to, and once by
// effects_init(): a simulation must not start with the waves or flames of
// the last time it was shown
typedef void effects_reset_t(effects_t *this);

effects_renderer_t effects_mode_spectrum;
effects_renderer_t effects_mode_spectrum_mirrored;
effects_renderer_t effects_mode_river;
effects_renderer_t effects_mode_ripples;
effects_renderer_t effects_mode_vu;
effects_renderer_t effects_mode_glow;
effects_renderer_t effects_mode_pond;
effects_reset_t effects_reset_pond;
effects_renderer_t effects_mode_cymatics;

/**
 * The layers, on top of every mode: slides, folds, blurs and trails
 * this->frame, in that order. Each does nothing while its tuning says off.
 * Allocates nothing: the buffers are effects_init()'s
 */
void effects_layers_apply(effects_t *this);

// White sparkles appearing with the treble, fading each frame
void effects_draw_sparkles(effects_t *this, const sound_t *sound);

// Below this a remembered level is dark on the strip (1e-3 ^ 2.2 of full
// scale after gamma) and goes to exactly 0, so that a fade ends at black
// instead of wandering through ever smaller floats
constexpr float EFFECTS_DARK = 1e-3f;

// Sine of 2 pi x turns from a 256 step table, interpolated: the cost of
// Plasma and Cymatics (hundreds of sines a frame) has a ceiling. Off by at
// most 8e-5. The table is filled by effects_init(). NaN and infinity give 0
void effects_sin_init(void);
float effects_sin(float turns);

// The shared random stream (xorshift32, seeded by effects_init()): renders
// are deterministic for a seed. The unit form is 0 inclusive to 1 exclusive
uint32_t effects_random(effects_t *this);
float effects_random_unit(effects_t *this);

/**
 * The loudest band as a position 0 (bass) to 1 (treble), interpolated
 * between the bands around it; 0 in silence. Cycle 3's peak tracker replaces
 * it behind the same name
 */
float effects_peak_band(const effects_t *this, const sound_t *sound);

// Palette colour with the drift and the loudness warmth applied
static inline rgb_t effects_color_at(const effects_t *this,
                                     const sound_t *sound, float position) {
    const effects_tuning_t *tuning = &this->tuning;
    float drift =
        tuning->drift_period_s > 0.f ? this->time_s / tuning->drift_period_s
                                     : 0.f;

    // One expression, as before the tuning: a compiler may fuse its
    // multiply and add, and must do it the same way to keep the pixels
    return palette_color(tuning->palette,
                         position + drift + sound->loudness * tuning->warmth);
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

// Mean of the bands counted as bass, and of the top fraction (treble)
static inline float effects_bass(const effects_t *this, const sound_t *sound) {
    size_t bands = EFFECTS_BASS_BANDS < this->band_count ? EFFECTS_BASS_BANDS
                                                         : this->band_count;

    return effects_band_mean(sound, 0, bands);
}

static inline float effects_treble(const effects_t *this,
                                   const sound_t *sound) {
    size_t from =
        this->band_count -
        (size_t)((float)this->band_count * EFFECTS_TREBLE_FRACTION);

    return effects_band_mean(sound, from, this->band_count);
}

#endif
