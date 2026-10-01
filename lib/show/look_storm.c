#include "show_internal.h"

#include <math.h>
#include <string.h>

void show_reset_storm(show_t *this) {
    memset(this->storm.glow, 0, this->led_count * sizeof(float));
    this->storm.sky = 0.f;
}

// Lights length LEDs from start: mostly bright, sometimes dim, so it
// crackles
static void strike(show_t *this, size_t start, size_t length) {
    for (size_t i = start; i < start + length && i < this->led_count; i++) {
        float chance = show_random_unit(this);
        float level = chance < 0.15f ? 0.35f : 1.f - 0.4f * chance;

        this->storm.glow[i] = fmaxf(this->storm.glow[i], level);
    }
}

// A bolt of length LEDs somewhere on the strip, with a few shorter branches
static void bolt(show_t *this, size_t length) {
    size_t start, branch;

    if (length > this->led_count)
        length = this->led_count;

    start = (size_t)(show_random_unit(this) *
                     (float)(this->led_count - length + 1));
    strike(this, start, length);

    branch = length / 3;
    for (size_t b = 0; b < SHOW_STORM_BRANCHES; b++) {
        size_t at = start + (size_t)(show_random_unit(this) * (float)length);
        // Half of them go the other way
        size_t from =
            show_random_unit(this) < 0.5f && at >= branch ? at - branch : at;

        strike(this, from, branch);
    }
}

// Rain: count faint sparks at random
static void rain(show_t *this, float count) {
    for (size_t k = 0; (float)k < count; k++)
        blocks_spark(&this->blocks, show_random(this) % this->led_count,
                     SHOW_STORM_RAIN_LEVEL * (0.5f + 0.5f * show_random_unit(this)));
}

/**
 * Storm: darkness and lightning. A near black field; high hits (and in a
 * build mid hits too) bring rain, thicker as the build goes on; in calm a
 * low hit is distant sheet lightning, a dim glow of the whole sky; in high a
 * strong low hit strikes a bolt, longer for a stronger hit, with a flash of
 * sky in the accent colour. A drop strikes across the whole strip
 */
void show_look_storm(show_t *this, const sound_t *sound) {
    const features_hit_t *low = &sound->hits[FEATURES_LOW];
    float *glow = this->storm.glow;
    rgb_t accent = show_color(this, SCENE_ACCENT),
          hit = show_color(this, SCENE_HIT);

    blocks_wash(&this->blocks, 0, this->led_count,
                color_rgb_scale(show_color(this, SCENE_FIELD),
                                SHOW_STORM_WASH * show_presence(sound)));

    if (sound->part == PARTS_BUILD &&
        (sound->hits[FEATURES_HIGH].fired || sound->hits[FEATURES_MID].fired))
        rain(this, SHOW_STORM_RAIN +
                       SHOW_STORM_BUILD_RAIN * sound->build_progress);
    else if (sound->hits[FEATURES_HIGH].fired)
        rain(this, sound->part == PARTS_HIGH ? SHOW_STORM_HIGH_RAIN
                                             : SHOW_STORM_RAIN);

    if (sound->event == PARTS_DROP) {
        strike(this, 0, this->led_count);
        this->storm.sky = fmaxf(this->storm.sky, SHOW_STORM_DROP_SKY);
    } else if (low->fired && sound->part == PARTS_CALM) {
        this->storm.sky =
            fmaxf(this->storm.sky, SHOW_STORM_SHEET * (0.5f + 0.5f * low->strength));
    } else if (low->fired && sound->part == PARTS_HIGH &&
               low->strength >= SHOW_STORM_STRIKE_MIN) {
        bolt(this, SHOW_STORM_MIN_LENGTH +
                       (size_t)(low->strength * (float)(SHOW_STORM_MAX_LENGTH -
                                                        SHOW_STORM_MIN_LENGTH)));
        this->storm.sky =
            fmaxf(this->storm.sky, SHOW_STORM_SKY_LEVEL * low->strength);
    }

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color;

        if (glow[i] == 0.f)
            continue;

        // The core of a bolt is the hit colour, its edge the accent
        color = color_rgb_scale(accent, glow[i]);
        color_rgb_add(&color, color_rgb_scale(hit, glow[i] * glow[i]));
        color_rgb_add(&this->blocks.frame[i], color);

        glow[i] *= this->storm.glow_keep;
        if (glow[i] < BLOCKS_DARK)
            glow[i] = 0.f;
    }

    if (this->storm.sky > 0.f) {
        blocks_wash(&this->blocks, 0, this->led_count,
                    color_rgb_scale(accent, this->storm.sky));
        this->storm.sky *= this->storm.sky_keep;
        if (this->storm.sky < BLOCKS_DARK)
            this->storm.sky = 0.f;
    }

    blocks_draw(&this->blocks, show_mix(show_color(this, SCENE_FIELD), accent, 0.5f),
                SHOW_SPARK_FADE_S);
}
