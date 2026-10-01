#include "show_internal.h"

#include <math.h>

void show_reset_sweep(show_t *this) { this->sweep.launches = 0; }

// A beam from the next end in turn, even launches from the left
static void launch(show_t *this, float speed, float tail, float level,
                   rgb_t color) {
    bool left = this->sweep.launches++ % 2 == 0;

    blocks_beam(&this->blocks, left ? 0.f : (float)(this->led_count - 1),
                left ? speed : -speed, tail, level, color);
}

// Beams whose heads pass each other during this hop flash where they meet
static void flash_crossings(show_t *this) {
    blocks_beam_t *beams = this->blocks.beams;
    float dt = this->hop_period_s;

    for (size_t a = 0; a < BLOCKS_MAX_BEAMS; a++)
        for (size_t b = a + 1; b < BLOCKS_MAX_BEAMS; b++) {
            float now, next;

            if (!beams[a].active || !beams[b].active ||
                beams[a].velocity * beams[b].velocity >= 0.f)
                continue;

            now = beams[a].head - beams[b].head;
            next = now + (beams[a].velocity - beams[b].velocity) * dt;
            if (now * next <= 0.f)
                blocks_burst(&this->blocks,
                             (beams[a].head + beams[b].head) / 2.f,
                             SHOW_SWEEP_CROSS_RADIUS, 1e6f, 1.f,
                             SHOW_SWEEP_CROSS_FADE_S,
                             show_color(this, SCENE_HIT));
        }
}

/**
 * Sweep: beams cross the strip like moving heads over a stage, launched by
 * low hits from alternate ends, flashing where two meet, and turning the
 * corner into the bends. A few slow beams in calm; in a build mid hits
 * launch too, faster and faster; in high the speed follows the groove and
 * mid hits send short beams in the hit colour; a drop fires a volley from
 * both ends
 */
void show_look_sweep(show_t *this, const sound_t *sound) {
    const features_hit_t *low = &sound->hits[FEATURES_LOW],
                         *mid = &sound->hits[FEATURES_MID];
    rgb_t accent = show_color(this, SCENE_ACCENT),
          hit = show_color(this, SCENE_HIT);
    float speed, tail, level;

    blocks_wash(&this->blocks, 0, this->led_count,
                color_rgb_scale(show_color(this, SCENE_FIELD),
                                SHOW_SWEEP_WASH * show_presence(sound)));

    switch (sound->part) {
    case PARTS_BUILD:
        speed = SHOW_SWEEP_CALM_SPEED +
                SHOW_SWEEP_BUILD_SPEED * sound->build_progress;
        tail = SHOW_SWEEP_TAIL;
        level = SHOW_SWEEP_LEVEL;
        break;
    case PARTS_HIGH:
        speed = SHOW_SWEEP_HIGH_SPEED + SHOW_SWEEP_GROOVE_SPEED * sound->groove;
        tail = SHOW_SWEEP_TAIL;
        level = SHOW_SWEEP_LEVEL;
        break;
    default:
        speed = SHOW_SWEEP_CALM_SPEED;
        tail = SHOW_SWEEP_CALM_TAIL;
        level = SHOW_SWEEP_CALM_LEVEL;
        break;
    }

    if (sound->part != PARTS_GAP) {
        if (low->fired ||
            (sound->part == PARTS_BUILD && mid->fired))
            launch(this, speed, tail, level * (0.7f + 0.3f * low->strength),
                   accent);
        if (sound->part == PARTS_HIGH && mid->fired)
            launch(this, speed, SHOW_SWEEP_SHORT_TAIL, level, hit);
    }

    if (sound->event == PARTS_DROP)
        for (size_t k = 0; k < SHOW_SWEEP_VOLLEY; k++) {
            float back = SHOW_SWEEP_VOLLEY_GAP * (float)k;

            blocks_beam(&this->blocks, -back, speed * SHOW_SWEEP_VOLLEY_SPEED,
                        tail, 1.f, hit);
            blocks_beam(&this->blocks, (float)(this->led_count - 1) + back,
                        -speed * SHOW_SWEEP_VOLLEY_SPEED, tail, 1.f, hit);
        }

    flash_crossings(this);
    blocks_draw(&this->blocks, hit, SHOW_SPARK_FADE_S);
}
