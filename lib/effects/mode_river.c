#include "effects_internal.h"

#include <string.h>

// The colour of the sound enters at the centre and flows outward
void effects_mode_river(effects_t *this, const sound_t *sound) {
    size_t speed = EFFECTS_RIVER_SPEED < this->half_led_count
                       ? EFFECTS_RIVER_SPEED
                       : this->half_led_count;
    rgb_t fresh = color_rgb_scale(
        effects_color_at(this, sound, sound->centroid), sound->loudness);

    memmove(this->river.history + speed, this->river.history,
            (this->half_led_count - speed) * sizeof(rgb_t));

    for (size_t d = 0; d < speed; d++)
        this->river.history[d] = fresh;

    for (size_t d = 0; d < this->half_led_count; d++)
        effects_put_mirrored(this, d, this->river.history[d]);
}
