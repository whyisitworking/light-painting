#include "effects_internal.h"

#include <math.h>

/**
 * Layered sines scrolling along the strip. Bass speeds the scroll, beats jerk
 * it forward, the spectral balance shifts the colour. Dark in silence
 */
void effects_mode_plasma(effects_t *this, const sound_t *sound) {
    float level = fminf(1.f, sound->loudness * EFFECTS_PLASMA_GAIN);
    float phase = this->plasma.phase;

    phase += this->hop_period_s *
             (EFFECTS_PLASMA_SPEED + effects_bass(this, sound) * EFFECTS_PLASMA_BASS_SPEED);
    if (sound->beat)
        phase += EFFECTS_PLASMA_JERK * sound->beat_strength;
    this->plasma.phase = phase - floorf(phase);

    if (!(level > 0.f))
        return;

    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);
        // Whole multiples of the phase, so it wraps without a jump
        float sum = effects_sin(1.5f * x + this->plasma.phase) +
                    effects_sin(4.f * x - 2.f * this->plasma.phase) +
                    effects_sin(9.f * x + 3.f * this->plasma.phase);
        float wave = 0.5f + sum / 6.f;

        this->frame[i] = color_rgb_scale(
            effects_color_at(this, sound, 0.5f * wave + 0.5f * sound->centroid),
            level * (0.25f + 0.75f * wave));
    }
}
