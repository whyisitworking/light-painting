#include "color.h"

#include <math.h>
#include <stdbool.h>

#define GAMMA 2.2f

uint8_t color_gamma(float value) {
    static uint8_t table[256];
    static bool ready = false;

    // Built on the first call: 256 powf once, a lookup afterwards
    if (!ready) {
        for (unsigned i = 0; i < 256; i++)
            table[i] = (uint8_t)lroundf(255.f * powf(i / 255.f, GAMMA));
        ready = true;
    }

    // Also catches NaN
    if (!(value > 0.f))
        return 0;

    if (value >= 1.f)
        return table[255];

    return table[lroundf(value * 255.f)];
}
