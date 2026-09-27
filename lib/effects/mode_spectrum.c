#include "effects_internal.h"

// Band levels along the strip, bass at the start
void effects_mode_spectrum(effects_t *this, const sound_t *sound) {
    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);

        this->frame[i] =
            scale(color_at(this, sound, x),
                  band_at(this, sound, x * (float)(this->band_count - 1)));
    }
}

// Bass in the centre, treble towards both ends
void effects_mode_spectrum_mirrored(effects_t *this, const sound_t *sound) {
    for (size_t d = 0; d < this->half; d++) {
        float x = this->half > 1 ? (float)d / (float)(this->half - 1) : 0.f;

        put_mirrored(
            this, d,
            scale(color_at(this, sound, x),
                  band_at(this, sound, x * (float)(this->band_count - 1))));
    }
}
