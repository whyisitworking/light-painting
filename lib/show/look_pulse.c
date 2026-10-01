#include "show_internal.h"

/**
 * Pulse: the whole strip hits as one. A field wash breathing with the
 * groove (and black in silence); low hits burst from the centre (narrow and soft in calm, wide in
 * high, narrower and quicker as a build goes on, while the wash pales
 * towards the hit colour); in a build and in high, mid hits burst at both
 * ends and high hits spark. A drop fills the strip in the hit colour
 */
void show_look_pulse(show_t *this, const sound_t *sound) {
    const features_hit_t *low = &sound->hits[FEATURES_LOW],
                         *mid = &sound->hits[FEATURES_MID],
                         *high = &sound->hits[FEATURES_HIGH];
    float half = (float)this->led_count / 2.f, centre = show_centre(this);
    float progress = sound->build_progress;
    bool busy = sound->part == PARTS_BUILD || sound->part == PARTS_HIGH;
    rgb_t field = show_color(this, SCENE_FIELD),
          accent = show_color(this, SCENE_ACCENT),
          hit = show_color(this, SCENE_HIT), wash;

    switch (sound->part) {
    case PARTS_BUILD:
        wash = color_rgb_scale(
            show_mix(field, hit, SHOW_PULSE_BUILD_PALE * progress),
            SHOW_PULSE_WASH + SHOW_PULSE_BUILD_WASH * progress);
        break;
    case PARTS_HIGH:
        wash = color_rgb_scale(field, SHOW_PULSE_WASH +
                                          SHOW_PULSE_GROOVE_WASH * sound->groove);
        break;
    default:
        wash = color_rgb_scale(field, SHOW_PULSE_WASH);
        break;
    }
    blocks_wash(&this->blocks, 0, this->led_count,
                color_rgb_scale(wash, show_presence(sound)));

    if (sound->event == PARTS_DROP)
        blocks_burst(&this->blocks, centre, half + 1.f, 1e6f, 1.f,
                     SHOW_PULSE_DROP_FADE_S, hit);

    if (low->fired) {
        float level = 0.5f + 0.5f * low->strength, radius, fade_s;

        if (sound->part == PARTS_HIGH) {
            radius = half * (SHOW_PULSE_HIGH_RADIUS +
                             SHOW_PULSE_HIGH_RADIUS_STRENGTH * low->strength);
            fade_s = SHOW_PULSE_HIGH_FADE_S;
        } else if (sound->part == PARTS_BUILD) {
            radius = half * (SHOW_PULSE_BUILD_RADIUS -
                             SHOW_PULSE_BUILD_SQUEEZE * progress);
            fade_s = SHOW_PULSE_BUILD_FADE_S;
        } else {
            radius = half * SHOW_PULSE_CALM_RADIUS;
            fade_s = SHOW_PULSE_CALM_FADE_S;
            level *= 0.6f;
        }

        blocks_burst(&this->blocks, centre, radius, radius / SHOW_PULSE_GROW_S,
                     level, fade_s, accent);
    }

    if (busy && mid->fired) {
        float radius = (float)this->led_count * SHOW_PULSE_END_RADIUS;
        float level = 0.5f + 0.5f * mid->strength;

        blocks_burst(&this->blocks, 0.f, radius, radius / SHOW_PULSE_GROW_S,
                     level, SHOW_PULSE_END_FADE_S, hit);
        blocks_burst(&this->blocks, (float)(this->led_count - 1), radius,
                     radius / SHOW_PULSE_GROW_S, level, SHOW_PULSE_END_FADE_S,
                     hit);
    }

    if (busy && high->fired) {
        size_t count = (size_t)(SHOW_PULSE_SPARKS +
                                SHOW_PULSE_SPARKS_STRENGTH * high->strength);

        for (size_t k = 0; k < count; k++)
            blocks_spark(&this->blocks,
                         show_random(this) % this->led_count, 1.f);
    }

    blocks_draw(&this->blocks, hit, SHOW_SPARK_FADE_S);
}
