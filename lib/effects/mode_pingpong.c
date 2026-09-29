#include "effects_internal.h"

#include <math.h>
#include <string.h>

void effects_reset_pingpong(effects_t *this) {
    memset(&this->pingpong, 0, sizeof(this->pingpong));
}

// A comet slot: a free one, or else the comet that has gone furthest
static effects_comet_t *free_slot(effects_t *this, float last) {
    effects_comet_t *slot = nullptr;
    float slot_travelled = -1.f;

    for (size_t c = 0; c < EFFECTS_PINGPONG_MAX_COMETS; c++) {
        effects_comet_t *comet = &this->pingpong.comets[c];
        float travelled;

        if (!comet->active)
            return comet;

        travelled = comet->direction > 0.f ? comet->position
                                           : last - comet->position;
        if (travelled > slot_travelled) {
            slot = comet;
            slot_travelled = travelled;
        }
    }

    return slot;
}

/**
 * Beats launch comets alternately from the left and right end, each with a
 * fading tail; where two cross they flash; they leave at the far end
 */
void effects_mode_pingpong(effects_t *this, const sound_t *sound) {
    float last = (float)(this->led_count - 1);
    effects_comet_t *comets = this->pingpong.comets;

    if (sound->beat) {
        bool from_left = this->pingpong.launches % 2 == 0;

        *free_slot(this, last) = (effects_comet_t){
            .position = from_left ? 0.f : last,
            .direction = from_left ? 1.f : -1.f,
            .strength = sound->beat_strength,
            .color_position = (float)(this->pingpong.launches % 8) / 8.f,
            .active = true,
        };
        this->pingpong.launches++;
    }

    // Two heads going opposite ways, close: a white flash between them
    for (size_t a = 0; a < EFFECTS_PINGPONG_MAX_COMETS; a++)
        for (size_t b = a + 1; b < EFFECTS_PINGPONG_MAX_COMETS; b++) {
            float middle;

            if (!comets[a].active || !comets[b].active ||
                comets[a].direction == comets[b].direction ||
                fabsf(comets[a].position - comets[b].position) >
                    EFFECTS_PINGPONG_CROSS)
                continue;

            middle = 0.5f * (comets[a].position + comets[b].position);
            for (int k = -3; k <= 3; k++) {
                int at = (int)(middle + 0.5f) + k;

                if (at >= 0 && at < (int)this->led_count)
                    color_rgb_add(&this->frame[at],
                                  color_rgb_scale((rgb_t){1.f, 1.f, 1.f},
                                                  1.f - fabsf((float)k) / 4.f));
            }
        }

    for (size_t c = 0; c < EFFECTS_PINGPONG_MAX_COMETS; c++) {
        effects_comet_t *comet = &comets[c];
        rgb_t color;
        float brightness;

        if (!comet->active)
            continue;

        color = effects_color_at(this, sound, comet->color_position);
        brightness = 0.6f + 0.4f * comet->strength;

        for (size_t k = 0; (float)k < EFFECTS_PINGPONG_TAIL; k++) {
            float at = comet->position - comet->direction * (float)k;

            if (at < 0.f || at > last)
                continue;

            color_rgb_add(&this->frame[(size_t)(at + 0.5f)],
                          color_rgb_scale(color,
                                          brightness * (1.f - (float)k / EFFECTS_PINGPONG_TAIL)));
        }

        comet->position += comet->direction * EFFECTS_PINGPONG_SPEED;

        // Gone once the tail has left as well
        if (comet->direction > 0.f
                ? comet->position - EFFECTS_PINGPONG_TAIL >= last
                : comet->position + EFFECTS_PINGPONG_TAIL <= 0.f)
            comet->active = false;
    }
}
