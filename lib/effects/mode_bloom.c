#include "effects_internal.h"

#include <math.h>

void effects_reset_bloom(effects_t *this) {
    for (size_t i = 0; i < this->led_count; i++) {
        this->bloom.u[i] = 1.f;
        this->bloom.v[i] = 0.f;
    }
    this->bloom.phase = 0.f;
}

// A spot of the bloom, radius SEED_RADIUS around centre
static void seed(effects_t *this, size_t centre) {
    size_t last = this->led_count - 1;

    for (size_t i = centre > EFFECTS_BLOOM_SEED_RADIUS ? centre - EFFECTS_BLOOM_SEED_RADIUS : 0;
         i <= centre + EFFECTS_BLOOM_SEED_RADIUS && i <= last; i++) {
        this->bloom.u[i] = 0.5f;
        this->bloom.v[i] = 0.25f;
    }
}

/**
 * Spots split, grow and die: a 1D Gray-Scott reaction-diffusion. The kill
 * rate drifts slowly, beats seed a new bloom and wipe a patch of old ones
 * (bass can change the feed rate too, at 0 by default). Experimental: the
 * look decides whether it stays. The pattern persists in silence, but is only shown while
 * there is sound
 */
void effects_mode_bloom(effects_t *this, const sound_t *sound) {
    size_t last = this->led_count - 1;
    float feed = EFFECTS_BLOOM_FEED + EFFECTS_BLOOM_FEED_BASS * effects_bass(this, sound);
    float level = fminf(1.f, sound->loudness * EFFECTS_BLOOM_GAIN);
    float kill;

    this->bloom.phase += 2.f * (float)M_PI * this->hop_period_s / EFFECTS_BLOOM_DRIFT_S;
    if (this->bloom.phase > 2.f * (float)M_PI)
        this->bloom.phase -= 2.f * (float)M_PI;
    kill = EFFECTS_BLOOM_KILL + EFFECTS_BLOOM_KILL_DRIFT * sinf(this->bloom.phase);

    if (sound->beat) {
        size_t wipe = (size_t)(effects_random_unit(this) * (float)last);
        size_t centre = (size_t)(effects_random_unit(this) * (float)last);

        for (size_t i = wipe > EFFECTS_BLOOM_WIPE_RADIUS ? wipe - EFFECTS_BLOOM_WIPE_RADIUS : 0;
             i <= wipe + EFFECTS_BLOOM_WIPE_RADIUS && i <= last; i++) {
            this->bloom.u[i] = 1.f;
            this->bloom.v[i] = 0.f;
        }

        seed(this, centre);
    }

    for (size_t s = 0; s < EFFECTS_BLOOM_SUBSTEPS; s++) {
        float *swap;

        for (size_t i = 0; i < this->led_count; i++) {
            float u = this->bloom.u[i], v = this->bloom.v[i];
            size_t l = i > 0 ? i - 1 : 0, r = i < last ? i + 1 : last;
            float reaction = u * v * v;

            this->bloom.next_u[i] =
                u + EFFECTS_BLOOM_DIFFUSION_U *
                        (this->bloom.u[l] + this->bloom.u[r] - 2.f * u) -
                reaction + feed * (1.f - u);
            this->bloom.next_v[i] =
                v + EFFECTS_BLOOM_DIFFUSION_V *
                        (this->bloom.v[l] + this->bloom.v[r] - 2.f * v) +
                reaction - (feed + kill) * v;
        }

        swap = this->bloom.u;
        this->bloom.u = this->bloom.next_u;
        this->bloom.next_u = swap;
        swap = this->bloom.v;
        this->bloom.v = this->bloom.next_v;
        this->bloom.next_v = swap;
    }

    if (!(level > 0.f))
        return;

    // With sound and nothing alive (all died or were wiped) a new spot comes
    // up somewhere, so the mode never stays dark over music
    {
        float alive = 0.f;

        for (size_t i = 0; i < this->led_count; i++)
            alive = fmaxf(alive, this->bloom.v[i]);
        if (alive < EFFECTS_BLOOM_ALIVE)
            seed(this, (size_t)(effects_random_unit(this) * (float)last));
    }

    for (size_t i = 0; i < this->led_count; i++) {
        float bloom = fminf(1.f, this->bloom.v[i] * 3.5f);

        this->frame[i] = color_rgb_scale(
            effects_color_at(this, sound, 0.4f + this->bloom.v[i] * 2.f), bloom * level);
    }
}
