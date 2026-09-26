#include "effects.h"

#include "color.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static rgb_t scale(rgb_t color, float k) {
    return (rgb_t){color.r * k, color.g * k, color.b * k};
}

static void add(rgb_t *pixel, rgb_t color) {
    pixel->r += color.r;
    pixel->g += color.g;
    pixel->b += color.b;
}

// Palette colour with the drift and the loudness warmth applied
static rgb_t color_at(const effects_t *this, const features_t *features,
                      float position) {
    float drift = EFFECTS_DRIFT_PERIOD_S > 0.f
                      ? this->time_s / EFFECTS_DRIFT_PERIOD_S
                      : 0.f;

    return palette_color(this->palette, position + drift +
                                            features->loudness *
                                                EFFECTS_WARMTH);
}

// Adds a colour at a distance from the centre, on both sides
static void put_mirrored(effects_t *this, size_t distance, rgb_t color) {
    size_t right = this->led_count / 2 + distance;
    size_t centre_left = (this->led_count - 1) / 2;

    if (right < this->led_count)
        add(&this->frame[right], color);

    // With an odd count both sides share the centre LED
    if (distance <= centre_left && centre_left - distance != right)
        add(&this->frame[centre_left - distance], color);
}

// Band level at a fractional band position, interpolated
static float band_at(const effects_t *this, const features_t *features,
                     float position) {
    size_t band = (size_t)position;
    float fraction;

    if (band >= this->band_count - 1)
        return features->bands[this->band_count - 1];

    fraction = position - (float)band;

    return features->bands[band] +
           (features->bands[band + 1] - features->bands[band]) * fraction;
}

static void render_spectrum(effects_t *this, const features_t *features) {
    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);

        this->frame[i] = scale(
            color_at(this, features, x),
            band_at(this, features, x * (float)(this->band_count - 1)));
    }
}

// Bass in the centre, treble towards both ends
static void render_spectrum_mirrored(effects_t *this,
                                     const features_t *features) {
    for (size_t d = 0; d < this->half; d++) {
        float x = this->half > 1 ? (float)d / (float)(this->half - 1) : 0.f;

        put_mirrored(
            this, d,
            scale(color_at(this, features, x),
                  band_at(this, features, x * (float)(this->band_count - 1))));
    }
}

bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed) {
    size_t half = (led_count + 1) / 2;
    rgb_t *frame, *river;
    float *sparkle;

    if (led_count < 2 || band_count < 2 || !(hop_seconds > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    river = (rgb_t *)calloc(half, sizeof(rgb_t));
    sparkle = (float *)calloc(led_count, sizeof(float));

    if (frame == NULL || river == NULL || sparkle == NULL) {
        free(frame);
        free(river);
        free(sparkle);
        return false;
    }

    *this = (effects_t){
        .led_count = led_count,
        .band_count = band_count,
        .half = half,
        .hop_seconds = hop_seconds,
        .mode = EFFECTS_RIVER,
        .palette = PALETTE_SYNTHWAVE,
        .flash_k = expf(-hop_seconds / (EFFECTS_FLASH_MS / 1000.f)),
        .frame = frame,
        .river = river,
        .sparkle = sparkle,
        .random = seed != 0 ? seed : 1,
    };

    return true;
}

void effects_set_mode(effects_t *this, effects_mode_t mode) {
    if (mode < EFFECTS_MODE_COUNT)
        this->mode = mode;
}

void effects_set_palette(effects_t *this, effects_palette_t palette) {
    if (palette < PALETTE_COUNT)
        this->palette = palette;
}

void effects_render(effects_t *this, const features_t *features,
                    uint32_t *pixels) {
    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    switch (this->mode) {
    case EFFECTS_SPECTRUM:
        render_spectrum(this, features);
        break;
    case EFFECTS_SPECTRUM_MIRRORED:
        render_spectrum_mirrored(this, features);
        break;
    default:
        break;
    }

    if (features->beat)
        this->flash = fmaxf(this->flash,
                            features->beat_strength * EFFECTS_FLASH_LEVEL);

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = this->frame[i];
        float flash = this->flash;

        pixels[i] = color_neopixel_from_rgb(palette_gamma(color.r + flash),
                                            palette_gamma(color.g + flash),
                                            palette_gamma(color.b + flash))
                        .value;
    }

    this->flash *= this->flash_k;
    this->time_s += this->hop_seconds;
}

void effects_deinit(effects_t *this) {
    free(this->frame);
    free(this->river);
    free(this->sparkle);
}
