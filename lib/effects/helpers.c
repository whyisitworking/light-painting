#include "effects_internal.h"

#include <math.h>

constexpr size_t SIN_STEPS = 256;
constexpr float TAU = 6.28318530718f;

// One extra entry so that the last step can interpolate
static float sin_table[SIN_STEPS + 1];
static bool sin_ready;

void effects_sin_init(void) {
    if (sin_ready)
        return;

    for (size_t i = 0; i <= SIN_STEPS; i++)
        sin_table[i] = sinf(TAU * (float)i / (float)SIN_STEPS);

    sin_ready = true;
}

float effects_sin(float turns) {
    float x = turns - floorf(turns);
    float position;
    size_t step;

    // NaN, infinity, or a tiny negative that rounded up to 1
    if (!(x >= 0.f && x < 1.f))
        return 0.f;

    position = x * (float)SIN_STEPS;
    step = (size_t)position;

    return sin_table[step] +
           (sin_table[step + 1] - sin_table[step]) * (position - (float)step);
}

uint32_t effects_random(effects_t *this) {
    uint32_t x = this->random;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return this->random = x;
}

float effects_random_unit(effects_t *this) {
    return (float)(effects_random(this) >> 8) / 16777216.f;
}

float effects_peak_band(const effects_t *this, const sound_t *sound) {
    size_t best = 0;
    float position;

    for (size_t b = 1; b < this->band_count; b++)
        if (sound->bands[b] > sound->bands[best])
            best = b;

    if (!(sound->bands[best] > 0.f))
        return 0.f;

    position = (float)best;

    // The top of the parabola through the band and its two neighbours
    if (best > 0 && best + 1 < this->band_count) {
        float left = sound->bands[best - 1], right = sound->bands[best + 1];
        float curve = left - 2.f * sound->bands[best] + right;

        if (curve < 0.f)
            position += 0.5f * (left - right) / curve;
    }

    return position / (float)(this->band_count - 1);
}
