#include "effects_internal.h"

#include <math.h>

// Where dot j rests: spread over the strip
static float home(const effects_t *this, size_t dot) {
    return (float)dot / (float)(EFFECTS_SWARM_DOTS - 1) *
           (float)(this->led_count - 1);
}

void effects_reset_swarm(effects_t *this) {
    for (size_t j = 0; j < EFFECTS_SWARM_DOTS; j++) {
        this->swarm.position[j] = home(this, j);
        this->swarm.velocity[j] = 0.f;
    }
}

/**
 * Dots on springs, each chasing the loudest band of its own stretch (a few
 * bands either side of its place in the spectrum), so the group slides and
 * clusters where the music is. A dot shines as bright as the band it chases
 */
void effects_mode_swarm(effects_t *this, const sound_t *sound) {
    float dt = this->hop_period_s;
    float last_led = (float)(this->led_count - 1);
    float last_band = (float)(this->band_count - 1);

    for (size_t j = 0; j < EFFECTS_SWARM_DOTS; j++) {
        size_t centre = (size_t)((float)j / (float)(EFFECTS_SWARM_DOTS - 1) *
                                     last_band +
                                 0.5f);
        size_t from = centre > EFFECTS_SWARM_REACH ? centre - EFFECTS_SWARM_REACH : 0;
        size_t to = centre + EFFECTS_SWARM_REACH < this->band_count - 1
                        ? centre + EFFECTS_SWARM_REACH
                        : this->band_count - 1;
        size_t best = from;
        float level, target, *x = &this->swarm.position[j],
                             *v = &this->swarm.velocity[j];

        for (size_t b = from + 1; b <= to; b++)
            if (sound->bands[b] > sound->bands[best])
                best = b;

        level = sound->bands[best];
        target = level >= EFFECTS_SWARM_QUIET ? (float)best / last_band * last_led
                                              : home(this, j);

        *v += (EFFECTS_SWARM_SPRING * (target - *x) - EFFECTS_SWARM_FRICTION * *v) * dt;
        *x += *v * dt;
        *x = fmaxf(0.f, fminf(last_led, *x));

        if (level < EFFECTS_SWARM_QUIET)
            continue;

        for (int k = -2; k <= 2; k++) {
            int at = (int)floorf(*x) + k;
            float weight = 1.f - fabsf((float)at - *x) / 1.5f;

            if (at >= 0 && at < (int)this->led_count && weight > 0.f)
                color_rgb_add(&this->frame[at],
                              color_rgb_scale(effects_color_at(this, sound, (float)best / last_band),
                                              level * weight));
        }
    }
}
