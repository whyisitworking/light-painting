#include "effects_internal.h"

#include <math.h>
#include <string.h>

void effects_reset_fire(effects_t *this) {
    memset(this->fire.heat, 0, this->half_led_count * sizeof(float));
}

/**
 * Flames burn outward from the centre (half a strip, mirrored) and cool as
 * they travel. Bass and beats feed the centre, treble makes the tips flicker.
 * Heat picks the colour and the brightness
 */
void effects_mode_fire(effects_t *this, const sound_t *sound) {
    float *heat = this->fire.heat;
    size_t count = this->half_led_count;
    float bass = effects_bass(this, sound), treble = effects_treble(this, sound);
    float source = fminf(1.f, bass * 1.2f + (sound->beat ? sound->beat_strength : 0.f));

    // Moves outward from the far end in, so heat[i - 1] is still last frame's
    for (size_t i = count - 1; i > 0; i--)
        heat[i] = (heat[i] * (1.f - EFFECTS_FIRE_SPEED) +
                   heat[i - 1] * EFFECTS_FIRE_SPEED) *
                  this->fire.cool_k;
    heat[0] *= (1.f - EFFECTS_FIRE_SPEED) * this->fire.cool_k;

    heat[0] = fmaxf(heat[0], source);

    if (effects_random_unit(this) < bass) {
        size_t at = (size_t)(effects_random_unit(this) *
                             (float)EFFECTS_FIRE_SPARK_LEDS);

        if (at >= count)
            at = count - 1;
        heat[at] = fmaxf(heat[at], 0.5f + 0.5f * bass);
    }

    for (size_t i = 0; i < count; i++) {
        float flicker;

        if (heat[i] < EFFECTS_DARK) {
            heat[i] = 0.f;
            continue;
        }

        flicker = 1.f - EFFECTS_FIRE_FLICKER * treble *
                            effects_random_unit(this) * ((float)i / (float)count);
        effects_put_mirrored(
            this, i,
            color_rgb_scale(effects_color_at(this, sound, heat[i] * 0.6f),
                            heat[i] * flicker));
    }
}
