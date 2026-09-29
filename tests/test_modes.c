#include "check.h"
#include "color.h"
#include "effects_internal.h"

#include <math.h>
#include <string.h>

constexpr size_t LEDS = 300;
constexpr size_t BANDS = FEATURES_BAND_COUNT;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

// effects_sin against the libm sine, over four turns either side of 0
static void test_sin(void) {
    effects_t effects;
    double worst = 0.0;

    // Fills the table
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));

    for (int i = -2000; i <= 2000; i++) {
        float turns = (float)i / 500.f;

        worst = fmax(worst, fabs((double)effects_sin(turns) -
                                 sin(2.0 * M_PI * (double)turns)));
    }

    // Linear interpolation over 256 steps is off by at most (2 pi / 256)^2 / 8
    CHECK(worst < 1e-4);
    CHECK_NEAR(effects_sin(0.25f), 1.0, 1e-4);
    CHECK_NEAR(effects_sin(-1e-9f), 0.0, 1e-4);
    CHECK(effects_sin(NAN) == 0.f);
    CHECK(effects_sin(INFINITY) == 0.f);

    effects_deinit(&effects);
}

// The first values of xorshift32 from seed 1, the stream the sparkles used
static void test_random_stream(void) {
    effects_t effects;
    float unit;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));
    CHECK(effects_random(&effects) == 270369u);
    CHECK(effects_random(&effects) == 67634689u);

    unit = effects_random_unit(&effects);
    CHECK(unit >= 0.f && unit < 1.f);

    effects_deinit(&effects);

    // Seed 0 is replaced, the state is never 0
    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 0));
    CHECK(effects_random(&effects) == 270369u);
    effects_deinit(&effects);
}

static void test_peak_band(void) {
    effects_t effects;
    float bands[BANDS] = {0};
    sound_t sound = {.bands = bands};

    CHECK(effects_init(&effects, LEDS, BANDS, HOP_PERIOD_S, 1));

    // Silence
    CHECK(effects_peak_band(&effects, &sound) == 0.f);

    // A single lit band is its own position
    bands[10] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 10.0 / (BANDS - 1), 1e-6);

    // Two equal neighbours: half way between them
    bands[11] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 10.5 / (BANDS - 1), 1e-6);

    // The ends
    memset(bands, 0, sizeof(bands));
    bands[0] = 1.f;
    CHECK(effects_peak_band(&effects, &sound) == 0.f);
    memset(bands, 0, sizeof(bands));
    bands[BANDS - 1] = 1.f;
    CHECK_NEAR(effects_peak_band(&effects, &sound), 1.0, 1e-6);

    effects_deinit(&effects);
}

int main(void) {
    test_sin();
    test_random_stream();
    test_peak_band();

    return CHECK_REPORT();
}
