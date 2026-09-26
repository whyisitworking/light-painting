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

// The colour of the sound enters at the centre and flows outward
static void render_river(effects_t *this, const features_t *features) {
    size_t speed =
        EFFECTS_RIVER_SPEED < this->half ? EFFECTS_RIVER_SPEED : this->half;
    rgb_t fresh = scale(color_at(this, features, features->centroid),
                        features->loudness);

    memmove(this->river + speed, this->river,
            (this->half - speed) * sizeof(rgb_t));

    for (size_t d = 0; d < speed; d++)
        this->river[d] = fresh;

    for (size_t d = 0; d < this->half; d++)
        put_mirrored(this, d, this->river[d]);
}

static uint32_t next_random(effects_t *this) {
    uint32_t x = this->random;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return this->random = x;
}

// 0 (inclusive) to 1 (exclusive)
static float random_unit(effects_t *this) {
    return (float)(next_random(this) >> 8) / 16777216.f;
}

static float band_mean(const features_t *features, size_t from, size_t to) {
    float sum = 0.f;

    for (size_t b = from; b < to; b++)
        sum += features->bands[b];

    return to > from ? sum / (float)(to - from) : 0.f;
}

// White sparkles appearing with the treble, fading each frame
static void render_sparkles(effects_t *this, const features_t *features) {
    size_t from =
        this->band_count -
        (size_t)((float)this->band_count * EFFECTS_TREBLE_FRACTION);
    float treble = band_mean(features, from, this->band_count);

    for (size_t i = 0; i < this->led_count; i++) {
        this->sparkle[i] *= EFFECTS_SPARKLE_DECAY;

        if (random_unit(this) < treble * EFFECTS_SPARKLE_RATE)
            this->sparkle[i] = 1.f;

        add(&this->frame[i],
            scale((rgb_t){1.f, 1.f, 1.f}, 0.8f * this->sparkle[i]));
    }
}

// Beats launch pulses from the centre, the treble sparkles
static void render_ripples(effects_t *this, const features_t *features) {
    if (features->beat) {
        effects_ripple_t *slot = NULL;

        // A free slot, or else the pulse furthest out
        for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
            effects_ripple_t *ripple = &this->ripples[r];

            if (!ripple->active) {
                slot = ripple;
                break;
            }

            if (slot == NULL || ripple->position > slot->position)
                slot = ripple;
        }

        *slot = (effects_ripple_t){
            .position = 0.f,
            .strength = features->beat_strength,
            .color_position = (float)(this->beat_count++ % 8) / 8.f,
            .active = true,
        };
    }

    for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
        effects_ripple_t *ripple = &this->ripples[r];
        float width, fade, brightness;
        rgb_t color;

        if (!ripple->active)
            continue;

        width = 3.f + 6.f * ripple->strength;
        fade = fmaxf(0.f, 1.f - ripple->position / (float)this->half);
        brightness = (0.5f + 0.5f * ripple->strength) * fade;
        color = color_at(this, features, ripple->color_position);

        // Brightest at the leading edge, fading behind it
        for (size_t k = 0; (float)k < width; k++) {
            float distance = ripple->position - (float)k;

            if (distance < 0.f)
                break;

            put_mirrored(this, (size_t)distance,
                         scale(color, brightness * (1.f - (float)k / width)));
        }

        ripple->position += EFFECTS_RIPPLE_SPEED;

        if (ripple->position - width >= (float)this->half)
            ripple->active = false;
    }

    render_sparkles(this, features);
}

// Twin meters filling from both ends, with peak dots that hold then fall
static void render_vu(effects_t *this, const features_t *features) {
    float length = features->loudness * (float)this->half;

    if (length >= this->peak) {
        this->peak = length;
        this->peak_hold_s = EFFECTS_PEAK_HOLD_MS / 1000.f;
    } else if (this->peak_hold_s > 0.f) {
        this->peak_hold_s -= this->hop_seconds;
    } else {
        this->peak = fmaxf(length, this->peak - EFFECTS_PEAK_FALL);
    }

    for (size_t d = 0; d < this->half && (float)d < length; d++) {
        rgb_t color = color_at(this, features, (float)d / (float)this->half);

        add(&this->frame[d], color);
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], color);
    }

    if (this->peak >= 1.f) {
        size_t d = (size_t)this->peak;

        if (d >= this->half)
            d = this->half - 1;

        add(&this->frame[d], (rgb_t){1.f, 1.f, 1.f});
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], (rgb_t){1.f, 1.f, 1.f});
    }
}

// The whole strip breathes with the bass, the treble sparkles
static void render_glow(effects_t *this, const features_t *features) {
    size_t bass_bands = EFFECTS_BASS_BANDS < this->band_count
                            ? EFFECTS_BASS_BANDS
                            : this->band_count;
    rgb_t color = scale(color_at(this, features, features->centroid),
                        band_mean(features, 0, bass_bands));

    for (size_t i = 0; i < this->led_count; i++)
        this->frame[i] = color;

    render_sparkles(this, features);
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
    color_neopixel_t flash;
    uint8_t white;

    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    switch (this->mode) {
    case EFFECTS_SPECTRUM:
        render_spectrum(this, features);
        break;
    case EFFECTS_SPECTRUM_MIRRORED:
        render_spectrum_mirrored(this, features);
        break;
    case EFFECTS_RIVER:
        render_river(this, features);
        break;
    case EFFECTS_RIPPLES:
        render_ripples(this, features);
        break;
    case EFFECTS_VU:
        render_vu(this, features);
        break;
    case EFFECTS_GLOW:
        render_glow(this, features);
        break;
    default:
        break;
    }

    if (features->beat)
        this->flash = fmaxf(this->flash,
                            features->beat_strength * EFFECTS_FLASH_LEVEL);

    // The flash goes on after gamma, as shown: added before it, 0.35 would
    // come out as 25 / 255. Saturating, so bright pixels do not wrap dark
    white = (uint8_t)lroundf(this->flash * 255.f);
    flash = color_neopixel_from_rgb(white, white, white);

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = this->frame[i];

        pixels[i] = color_neopixel_add(
                        color_neopixel_from_rgb(palette_gamma(color.r),
                                                palette_gamma(color.g),
                                                palette_gamma(color.b)),
                        flash)
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
