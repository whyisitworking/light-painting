#include "palette.h"

#include <math.h>
#include <stdbool.h>

#define GAMMA 2.2f

typedef struct {
    const rgb_t *stops;
    unsigned count;
    // Wraps around instead of reflecting at the ends
    bool cyclic;
} palette_t;

static const rgb_t rainbow[] = {{1.f, 0.f, 0.f}, {1.f, 1.f, 0.f},
                                {0.f, 1.f, 0.f}, {0.f, 1.f, 1.f},
                                {0.f, 0.f, 1.f}, {1.f, 0.f, 1.f}};

static const rgb_t synthwave[] = {{0.05f, 0.f, 0.25f}, {0.55f, 0.f, 0.8f},
                                  {1.f, 0.1f, 0.6f},   {1.f, 0.5f, 0.1f},
                                  {0.1f, 0.9f, 1.f}};

static const rgb_t fire[] = {{0.15f, 0.f, 0.f},
                             {0.8f, 0.05f, 0.f},
                             {1.f, 0.4f, 0.f},
                             {1.f, 0.8f, 0.1f},
                             {1.f, 1.f, 0.7f}};

static const rgb_t ocean[] = {{0.f, 0.05f, 0.2f},
                              {0.f, 0.3f, 0.7f},
                              {0.f, 0.7f, 0.8f},
                              {0.3f, 1.f, 0.8f},
                              {0.9f, 1.f, 1.f}};

static const palette_t palettes[PALETTE_COUNT] = {
    [PALETTE_RAINBOW] = {rainbow, 6, true},
    [PALETTE_SYNTHWAVE] = {synthwave, 5, false},
    [PALETTE_FIRE] = {fire, 5, false},
    [PALETTE_OCEAN] = {ocean, 5, false},
};

rgb_t palette_color(palette_id_t palette, float position) {
    const palette_t *p = &palettes[palette < PALETTE_COUNT ? palette : 0];
    unsigned from, to;
    float scaled, fraction;
    rgb_t a, b;

    if (p->cyclic) {
        // Wrap into 0..1, the last stop blends back into the first
        position -= floorf(position);
        scaled = position * (float)p->count;
        from = (unsigned)scaled % p->count;
        to = (from + 1) % p->count;
        fraction = scaled - floorf(scaled);
    } else {
        // Reflect: 0..1 forwards, 1..2 backwards, and so on
        position -= 2.f * floorf(position / 2.f);
        if (position > 1.f)
            position = 2.f - position;

        scaled = position * (float)(p->count - 1);
        from = (unsigned)scaled;
        if (from > p->count - 2)
            from = p->count - 2;
        to = from + 1;
        fraction = scaled - (float)from;
    }

    a = p->stops[from];
    b = p->stops[to];

    return (rgb_t){a.r + (b.r - a.r) * fraction, a.g + (b.g - a.g) * fraction,
                   a.b + (b.b - a.b) * fraction};
}

uint8_t palette_gamma(float value) {
    static uint8_t table[256];
    static bool ready = false;

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

    return table[(unsigned)floorf(value * 255.f)];
}
