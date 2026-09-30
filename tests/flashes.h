#ifndef TESTS_FLASHES_H
#define TESTS_FLASHES_H

/**
 * Counting flashes the way WCAG 2.3.1 defines them, for the tests of the
 * flash guard: a flash is a pair of opposite changes of light of more than
 * rise. One starts where the light climbs more than rise above its lowest
 * since the last flash ended, and ends where the light falls more than rise
 * from its peak
 */

#include <math.h>
#include <stddef.h>

/**
 * The most flashes starting in any per_s hops of count hops of light, 0..1
 * of full, the strip dark before the first
 */
static inline size_t flashes_most(const float *light, size_t count,
                                  size_t per_s, float rise) {
    size_t most = 0, in_window = 0;
    float low = 0.f, peak = 0.f;
    bool armed = true;
    static bool starts[1 << 14];

    for (size_t i = 0; i < count && i < sizeof(starts); i++) {
        starts[i] = false;
        if (!armed) {
            peak = fmaxf(peak, light[i]);
            if (light[i] < peak - rise) {
                armed = true;
                low = light[i];
            }
        } else if (light[i] - low > rise) {
            starts[i] = true;
            armed = false;
            peak = light[i];
        } else {
            low = fminf(low, light[i]);
        }

        // Over the window of the last per_s hops
        in_window += starts[i];
        if (i >= per_s)
            in_window -= starts[i - per_s];
        most = in_window > most ? in_window : most;
    }

    return most;
}

#endif
