#include "effects_internal.h"

#include <string.h>

// The colour of the sound enters at the centre and flows outward
void effects_mode_river(effects_t *this, const features_t *features) {
    size_t speed =
        EFFECTS_RIVER_SPEED < this->half ? EFFECTS_RIVER_SPEED : this->half;
    rgb_t fresh = scale(color_at(this, features, features->centroid),
                        features->loudness);

    memmove(this->river.history + speed, this->river.history,
            (this->half - speed) * sizeof(rgb_t));

    for (size_t d = 0; d < speed; d++)
        this->river.history[d] = fresh;

    for (size_t d = 0; d < this->half; d++)
        put_mirrored(this, d, this->river.history[d]);
}
