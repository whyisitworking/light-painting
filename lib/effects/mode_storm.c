#include "effects_internal.h"

#include <math.h>
#include <string.h>

void effects_reset_storm(effects_t *this) {
    memset(this->storm.afterglow, 0, this->led_count * sizeof(float));
    this->storm.sky = 0.f;
}

// Lights length LEDs from start: mostly bright, sometimes dim, so it
// crackles
static void strike(effects_t *this, size_t start, size_t length) {
    for (size_t i = start; i < start + length && i < this->led_count; i++) {
        float chance = effects_random_unit(this);
        float level = chance < 0.15f ? 0.35f : 1.f - 0.4f * chance;

        this->storm.afterglow[i] = fmaxf(this->storm.afterglow[i], level);
    }
}

/**
 * A strong beat strikes a bolt (longer for a stronger beat) with a few
 * shorter side branches, then a fast afterglow and a dim flash of the sky.
 * Rain sparkles between strikes
 */
void effects_mode_storm(effects_t *this, const sound_t *sound) {
    float *glow = this->storm.afterglow;

    if (sound->beat) {
        float strength = EFFECTS_STORM_STRENGTH_FLOOR +
                         (1.f - EFFECTS_STORM_STRENGTH_FLOOR) * sound->beat_strength;
        size_t length =
            EFFECTS_STORM_MIN_LENGTH +
            (size_t)(strength *
                     (float)(EFFECTS_STORM_MAX_LENGTH - EFFECTS_STORM_MIN_LENGTH));
        size_t start, branch;

        if (length > this->led_count)
            length = this->led_count;

        start = (size_t)(effects_random_unit(this) *
                         (float)(this->led_count - length + 1));
        strike(this, start, length);

        branch = length / 3;
        for (size_t b = 0; b < EFFECTS_STORM_BRANCHES; b++) {
            size_t at = start + (size_t)(effects_random_unit(this) * (float)length);
            // Half of them go the other way
            size_t from = effects_random_unit(this) < 0.5f && at >= branch
                              ? at - branch
                              : at;

            strike(this, from, branch);
        }

        this->storm.sky = fmaxf(this->storm.sky,
                                EFFECTS_STORM_SKY_LEVEL * strength);
    }

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color;
        float level;

        glow[i] *= this->storm.glow_k;
        if (glow[i] < EFFECTS_DARK) {
            glow[i] = 0.f;
            continue;
        }

        level = glow[i];
        color = color_rgb_scale(
            effects_color_at(this, sound, (float)i / (float)(this->led_count - 1)),
            level);
        // The core of a bolt is white
        color_rgb_add(&color,
                      color_rgb_scale((rgb_t){1.f, 1.f, 1.f}, level * level));
        color_rgb_add(&this->frame[i], color);
    }

    this->storm.sky *= this->storm.sky_k;
    if (this->storm.sky < EFFECTS_DARK)
        this->storm.sky = 0.f;
    else
        for (size_t i = 0; i < this->led_count; i++)
            color_rgb_add(&this->frame[i],
                          color_rgb_scale(effects_color_at(this, sound, 0.1f),
                                          this->storm.sky));

    effects_draw_sparkles(this, sound);
}
