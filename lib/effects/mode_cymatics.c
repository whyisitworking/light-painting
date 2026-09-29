#include "effects_internal.h"

#include <math.h>

/**
 * A standing wave: brightness follows sin(n pi x) squared along the strip,
 * with n set by the loudest band and slewed so the nodes slide instead of
 * jumping, breathing with the loudness. Dark in silence
 */
void effects_mode_cymatics(effects_t *this, const sound_t *sound) {
    float target = 1.f + effects_peak_band(this, sound) *
                             (EFFECTS_CYMATICS_MAX_NODES - 1.f);
    float slew = 1.f - expf(-this->hop_period_s / EFFECTS_CYMATICS_SLEW_S);
    float level = fminf(1.f, sound->loudness * EFFECTS_CYMATICS_GAIN);
    float breath;

    this->cymatics.nodes += (target - this->cymatics.nodes) * slew;
    this->cymatics.phase = fmodf(
        this->cymatics.phase + this->hop_period_s * EFFECTS_CYMATICS_BREATH_HZ,
        1.f);

    if (!(level > 0.f))
        return;

    breath = 0.8f + 0.2f * effects_sin(this->cymatics.phase);

    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);
        // sin(pi n x) is n / 2 turns
        float wave = effects_sin(0.5f * this->cymatics.nodes * x);

        this->frame[i] = color_rgb_scale(effects_color_at(this, sound, x),
                                         wave * wave * level * breath);
    }
}
