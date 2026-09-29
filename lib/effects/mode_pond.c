#include "effects_internal.h"

#include <math.h>
#include <string.h>

void effects_reset_pond(effects_t *this) {
    memset(this->pond.height, 0, this->led_count * sizeof(float));
    memset(this->pond.previous, 0, this->led_count * sizeof(float));
}

// A bump in both rows: it starts at rest and splits into two waves
static void drop(effects_t *this, float centre, float amount) {
    for (size_t i = 0; i < this->led_count; i++) {
        float d = ((float)i - centre) / EFFECTS_POND_STONE_WIDTH;

        if (fabsf(d) > 3.f)
            continue;

        this->pond.height[i] += amount * expf(-d * d);
        this->pond.previous[i] += amount * expf(-d * d);
    }
}

/**
 * A 1D wave simulation over the whole strip: each beat drops a stone at the
 * centroid, treble adds small drizzle drops. The ends reflect. Crests take
 * the upper half of the palette, troughs the lower
 */
void effects_mode_pond(effects_t *this, const sound_t *sound) {
    size_t last = this->led_count - 1;
    float speed2 = EFFECTS_POND_WAVE_SPEED * EFFECTS_POND_WAVE_SPEED;
    float treble = effects_treble(this, sound);
    float *height = this->pond.height, *previous = this->pond.previous;

    if (sound->beat)
        drop(this, sound->centroid * (float)last, sound->beat_strength);

    if (effects_random_unit(this) < treble * EFFECTS_POND_DRIZZLE_RATE)
        drop(this, effects_random_unit(this) * (float)last,
             EFFECTS_POND_DRIZZLE_HEIGHT);

    height = this->pond.height;
    previous = this->pond.previous;

    for (size_t s = 0; s < EFFECTS_POND_SUBSTEPS; s++) {
        float *swap;

        // The next row goes over the oldest, then they change places
        for (size_t i = 0; i < this->led_count; i++) {
            float left = height[i > 0 ? i - 1 : 0];
            float right = height[i < last ? i + 1 : last];
            float velocity = (height[i] - previous[i]) * this->pond.velocity_k;

            previous[i] =
                (height[i] + velocity +
                 speed2 * (left + right - 2.f * height[i])) *
                this->pond.leak_k;
        }

        swap = height;
        height = previous;
        previous = swap;
    }

    this->pond.height = height;
    this->pond.previous = previous;

    for (size_t i = 0; i < this->led_count; i++) {
        float level, position;

        if (fabsf(height[i]) < EFFECTS_DARK &&
            fabsf(height[i] - previous[i]) < EFFECTS_DARK) {
            height[i] = 0.f;
            previous[i] = 0.f;
            continue;
        }

        level = fminf(1.f, fabsf(height[i]) * EFFECTS_POND_GAIN);
        position = 0.5f + 0.5f * fmaxf(-1.f, fminf(1.f, height[i]));
        this->frame[i] =
            color_rgb_scale(effects_color_at(this, sound, position), level);
    }
}
