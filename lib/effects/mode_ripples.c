#include "effects_internal.h"

#include <math.h>

// Beats launch pulses from the centre, the treble sparkles
void effects_mode_ripples(effects_t *this, const sound_t *sound) {
    if (sound->beat) {
        effects_pulse_t *slot = nullptr;

        // A free slot, or else the pulse furthest out
        for (size_t r = 0; r < EFFECTS_RIPPLE_MAX_PULSES; r++) {
            effects_pulse_t *pulse = &this->ripples.pulses[r];

            if (!pulse->active) {
                slot = pulse;
                break;
            }

            if (slot == nullptr || pulse->position > slot->position)
                slot = pulse;
        }

        *slot = (effects_pulse_t){
            .position = 0.f,
            .strength = sound->beat_strength,
            .color_position = (float)(this->ripples.beat_count++ % 8) / 8.f,
            .active = true,
        };
    }

    for (size_t r = 0; r < EFFECTS_RIPPLE_MAX_PULSES; r++) {
        effects_pulse_t *pulse = &this->ripples.pulses[r];
        float width, fade, brightness;
        rgb_t color;

        if (!pulse->active)
            continue;

        width = 3.f + 6.f * pulse->strength;
        fade = fmaxf(0.f, 1.f - pulse->position / (float)this->half_led_count);
        brightness = (0.5f + 0.5f * pulse->strength) * fade;
        color = effects_color_at(this, sound, pulse->color_position);

        // Brightest at the leading edge, fading behind it
        for (size_t k = 0; (float)k < width; k++) {
            float distance = pulse->position - (float)k;

            if (distance < 0.f)
                break;

            effects_put_mirrored(
                this, (size_t)distance,
                color_rgb_scale(color, brightness * (1.f - (float)k / width)));
        }

        pulse->position += this->tuning.ripple_speed;

        if (pulse->position - width >= (float)this->half_led_count)
            pulse->active = false;
    }

    effects_draw_sparkles(this, sound);
}
