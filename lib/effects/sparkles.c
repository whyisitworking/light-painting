#include "effects_internal.h"

static uint32_t next_random(effects_t *this) {
    uint32_t x = this->sparkles.random;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return this->sparkles.random = x;
}

// 0 (inclusive) to 1 (exclusive)
static float random_unit(effects_t *this) {
    return (float)(next_random(this) >> 8) / 16777216.f;
}

void effects_draw_sparkles(effects_t *this, const sound_t *sound) {
    size_t from =
        this->band_count -
        (size_t)((float)this->band_count * EFFECTS_TREBLE_FRACTION);
    float treble = effects_band_mean(sound, from, this->band_count);

    for (size_t i = 0; i < this->led_count; i++) {
        this->sparkles.levels[i] *= EFFECTS_SPARKLE_DECAY;

        if (random_unit(this) < treble * this->tuning.sparkle_rate)
            this->sparkles.levels[i] = 1.f;

        color_rgb_add(&this->frame[i],
                      color_rgb_scale((rgb_t){1.f, 1.f, 1.f},
                                      0.8f * this->sparkles.levels[i]));
    }
}
