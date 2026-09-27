#include "effects_internal.h"

#include <math.h>

// Beats launch pulses from the centre, the treble sparkles
void effects_mode_ripples(effects_t *this, const sound_t *sound) {
    if (sound->beat) {
        effects_ripple_t *slot = NULL;

        // A free slot, or else the pulse furthest out
        for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
            effects_ripple_t *ripple = &this->ripples.pulses[r];

            if (!ripple->active) {
                slot = ripple;
                break;
            }

            if (slot == NULL || ripple->position > slot->position)
                slot = ripple;
        }

        *slot = (effects_ripple_t){
            .position = 0.f,
            .strength = sound->beat_strength,
            .color_position = (float)(this->ripples.beat_count++ % 8) / 8.f,
            .active = true,
        };
    }

    for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
        effects_ripple_t *ripple = &this->ripples.pulses[r];
        float width, fade, brightness;
        rgb_t color;

        if (!ripple->active)
            continue;

        width = 3.f + 6.f * ripple->strength;
        fade = fmaxf(0.f, 1.f - ripple->position / (float)this->half);
        brightness = (0.5f + 0.5f * ripple->strength) * fade;
        color = color_at(this, sound, ripple->color_position);

        // Brightest at the leading edge, fading behind it
        for (size_t k = 0; (float)k < width; k++) {
            float distance = ripple->position - (float)k;

            if (distance < 0.f)
                break;

            put_mirrored(this, (size_t)distance,
                         scale(color, brightness * (1.f - (float)k / width)));
        }

        ripple->position += EFFECTS_RIPPLE_SPEED;

        if (ripple->position - width >= (float)this->half)
            ripple->active = false;
    }

    effects_sparkles(this, sound);
}
