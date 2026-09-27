#include "effects_internal.h"

// The whole strip breathes with the bass, the treble sparkles
void effects_mode_glow(effects_t *this, const sound_t *sound) {
    size_t bass_bands = EFFECTS_BASS_BANDS < this->band_count
                            ? EFFECTS_BASS_BANDS
                            : this->band_count;
    rgb_t color =
        color_rgb_scale(effects_color_at(this, sound, sound->centroid),
                        effects_band_mean(sound, 0, bass_bands));

    for (size_t i = 0; i < this->led_count; i++)
        this->frame[i] = color;

    effects_draw_sparkles(this, sound);
}
