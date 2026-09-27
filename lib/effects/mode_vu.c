#include "effects_internal.h"

#include <math.h>

// Twin meters filling from both ends, with peak dots that hold then fall
void effects_mode_vu(effects_t *this, const sound_t *sound) {
    float length = sound->loudness * (float)this->half;

    if (length >= this->vu.peak) {
        this->vu.peak = length;
        this->vu.hold_s = EFFECTS_PEAK_HOLD_MS / 1000.f;
    } else if (this->vu.hold_s > 0.f) {
        this->vu.hold_s -= this->hop_seconds;
    } else {
        this->vu.peak = fmaxf(length, this->vu.peak - EFFECTS_PEAK_FALL);
    }

    for (size_t d = 0; d < this->half && (float)d < length; d++) {
        rgb_t color = color_at(this, sound, (float)d / (float)this->half);

        add(&this->frame[d], color);
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], color);
    }

    if (this->vu.peak >= 1.f) {
        size_t d = (size_t)this->vu.peak;

        if (d >= this->half)
            d = this->half - 1;

        add(&this->frame[d], (rgb_t){1.f, 1.f, 1.f});
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], (rgb_t){1.f, 1.f, 1.f});
    }
}
