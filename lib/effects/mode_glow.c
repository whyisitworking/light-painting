#include "effects_internal.h"

// The whole strip breathes with the bass, the treble sparkles
void effects_mode_glow(effects_t *this, const features_t *features) {
    size_t bass_bands = EFFECTS_BASS_BANDS < this->band_count
                            ? EFFECTS_BASS_BANDS
                            : this->band_count;
    rgb_t color = scale(color_at(this, features, features->centroid),
                        band_mean(features, 0, bass_bands));

    for (size_t i = 0; i < this->led_count; i++)
        this->frame[i] = color;

    effects_sparkles(this, features);
}
