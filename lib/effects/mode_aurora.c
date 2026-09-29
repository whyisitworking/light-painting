#include "effects_internal.h"

#include <math.h>

// A random 0..1 for a point of the noise lattice
static float lattice(uint32_t ix, uint32_t iy) {
    uint32_t h = ix * 374761393u + iy * 668265263u;

    h = (h ^ (h >> 13)) * 1274126177u;

    return (float)((h ^ (h >> 16)) >> 8) / 16777216.f;
}

static float smooth(float t) { return t * t * (3.f - 2.f * t); }

// Value noise at x along the strip and (cell + fraction) in time, 0..1
static float noise(float x, uint32_t cell, float fraction) {
    float whole = floorf(x);
    uint32_t ix = (uint32_t)(int32_t)whole;
    float fx = smooth(x - whole), fy = smooth(fraction);
    float low = lattice(ix, cell) +
                (lattice(ix + 1, cell) - lattice(ix, cell)) * fx;
    float high = lattice(ix, cell + 1) +
                 (lattice(ix + 1, cell + 1) - lattice(ix, cell + 1)) * fx;

    return low + (high - low) * fy;
}

static void advance(uint32_t *cell, float *fraction, float cells) {
    *fraction += cells;

    while (*fraction >= 1.f) {
        *fraction -= 1.f;
        (*cell)++;
    }
}

/**
 * Slow drifting curtains from smooth noise, with fine rays that deepen with
 * the treble. A calm mode: dark when silent
 */
void effects_mode_aurora(effects_t *this, const sound_t *sound) {
    float level = fminf(1.f, sound->loudness * EFFECTS_AURORA_GAIN);
    float treble = effects_treble(this, sound);

    advance(&this->aurora.cell[0], &this->aurora.fraction[0],
            this->hop_period_s * EFFECTS_AURORA_CURTAIN_SPEED);
    advance(&this->aurora.cell[1], &this->aurora.fraction[1],
            this->hop_period_s * EFFECTS_AURORA_RAY_SPEED);

    if (!(level > 0.f))
        return;

    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);
        float curtain = noise(x * EFFECTS_AURORA_CURTAIN_CELLS,
                              this->aurora.cell[0], this->aurora.fraction[0]);
        float rays = noise(x * EFFECTS_AURORA_RAY_CELLS, this->aurora.cell[1],
                           this->aurora.fraction[1]);
        // Broad curtains: value noise rarely leaves the middle, so a
        // smooth ramp from EDGE to EDGE+SPAN lights the good parts fully
        float ramp = fminf(1.f, fmaxf(0.f, (curtain - EFFECTS_AURORA_EDGE) / EFFECTS_AURORA_SPAN));
        float shape = smooth(ramp);

        this->frame[i] = color_rgb_scale(
            effects_color_at(this, sound, 0.45f + 0.4f * curtain),
            level * shape * (1.f - 0.5f * treble * (1.f - rays)));
    }
}
