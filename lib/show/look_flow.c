#include "show_internal.h"

#include <string.h>

void show_reset_flow(show_t *this) {
    memset(this->flow.history, 0, this->half_led_count * sizeof(rgb_t));
    this->flow.carry = 0.f;
    this->flow.boost = 0.f;
}

/**
 * Flow: the colour of the sound pours out from the centre towards both ends,
 * between the field (dark tone) and the accent (bright tone), as bright as
 * it is loud. Low hits inject the hit colour. Slow in calm, faster as a
 * build goes on, standing still in a gap; a drop throws it forward
 */
void show_look_flow(show_t *this, const sound_t *sound) {
    rgb_t *history = this->flow.history;
    size_t half = this->half_led_count, steps;
    float speed;
    rgb_t fresh = color_rgb_scale(
        show_mix(show_color(this, SCENE_FIELD), show_color(this, SCENE_ACCENT),
                 sound->centroid),
        SHOW_FLOW_LEVEL * show_presence(sound) +
            SHOW_FLOW_LOUD_LEVEL * sound->loudness);

    switch (sound->part) {
    case PARTS_BUILD:
        speed = SHOW_FLOW_CALM_SPEED +
                SHOW_FLOW_BUILD_SPEED * sound->build_progress;
        break;
    case PARTS_HIGH:
        speed = SHOW_FLOW_HIGH_SPEED + SHOW_FLOW_GROOVE_SPEED * sound->groove;
        break;
    case PARTS_GAP:
        speed = 0.f;
        break;
    default:
        speed = SHOW_FLOW_CALM_SPEED;
        break;
    }

    if (sound->event == PARTS_DROP) {
        this->flow.boost = SHOW_FLOW_DROP_BOOST;
        color_rgb_add(&fresh, show_color(this, SCENE_HIT));
    }
    if (sound->hits[FEATURES_LOW].fired)
        color_rgb_add(&fresh,
                      color_rgb_scale(show_color(this, SCENE_HIT),
                                      SHOW_FLOW_HIT +
                                          SHOW_FLOW_HIT_STRENGTH *
                                              sound->hits[FEATURES_LOW].strength));

    this->flow.carry += (speed + this->flow.boost) * this->hop_period_s;
    this->flow.boost *= this->flow.boost_keep;
    steps = (size_t)this->flow.carry;
    this->flow.carry -= (float)steps;
    if (steps > half)
        steps = half;

    memmove(history + steps, history, (half - steps) * sizeof(rgb_t));
    for (size_t d = 0; d < steps; d++)
        history[d] = fresh;
    // Between steps the centre follows the sound, so nothing waits a hop
    if (steps == 0 && sound->part != PARTS_GAP)
        history[0] = fresh;

    for (size_t d = 0; d < half; d++)
        show_put_mirrored(this, d, history[d]);

    blocks_draw(&this->blocks, show_color(this, SCENE_HIT), SHOW_SPARK_FADE_S);
}
