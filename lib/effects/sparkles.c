#include "effects_internal.h"

void effects_draw_sparkles(effects_t *this, const sound_t *sound) {
    float treble = effects_treble(this, sound);

    for (size_t i = 0; i < this->led_count; i++) {
        this->sparkles.levels[i] *= EFFECTS_SPARKLE_DECAY;

        if (effects_random_unit(this) < treble * this->tuning.sparkle_rate)
            this->sparkles.levels[i] = 1.f;

        color_rgb_add(&this->frame[i],
                      color_rgb_scale((rgb_t){1.f, 1.f, 1.f},
                                      0.8f * this->sparkles.levels[i]));
    }
}
