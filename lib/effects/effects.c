#include "effects.h"

#include "color.h"
#include "effects_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// The renderer of each mode, see effects_internal.h
static effects_renderer_t *const renderers[EFFECTS_MODE_COUNT] = {
    [EFFECTS_MODE_SPECTRUM] = effects_mode_spectrum,
    [EFFECTS_MODE_SPECTRUM_MIRRORED] = effects_mode_spectrum_mirrored,
    [EFFECTS_MODE_RIVER] = effects_mode_river,
    [EFFECTS_MODE_RIPPLES] = effects_mode_ripples,
    [EFFECTS_MODE_VU] = effects_mode_vu,
    [EFFECTS_MODE_GLOW] = effects_mode_glow,
};

bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_period_s, uint32_t seed) {
    size_t half_led_count = (led_count + 1) / 2;
    rgb_t *frame, *river;
    float *sparkles;

    if (led_count < 2 || band_count < 2 || !(hop_period_s > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    river = (rgb_t *)calloc(half_led_count, sizeof(rgb_t));
    sparkles = (float *)calloc(led_count, sizeof(float));

    if (frame == nullptr || river == nullptr || sparkles == nullptr) {
        free(frame);
        free(river);
        free(sparkles);
        return false;
    }

    *this = (effects_t){
        .tuning = effects_default_tuning(),
        .led_count = led_count,
        .band_count = band_count,
        .half_led_count = half_led_count,
        .hop_period_s = hop_period_s,
        .mode = EFFECTS_MODE_RIVER,
        .palette = PALETTE_SYNTHWAVE,
        .flash_k = expf(-hop_period_s / (EFFECTS_FLASH_MS / 1000.f)),
        .frame = frame,
        .river.history = river,
        .sparkles.levels = sparkles,
        .sparkles.random = seed != 0 ? seed : 1,
    };
    effects_tune(this, &this->tuning);

    return true;
}

void effects_set_mode(effects_t *this, effects_mode_t mode) {
    if (mode < EFFECTS_MODE_COUNT)
        this->mode = mode;
}

void effects_set_palette(effects_t *this, palette_t palette) {
    if (palette < PALETTE_COUNT)
        this->palette = palette;
}

effects_tuning_t effects_default_tuning(void) {
    return (effects_tuning_t){
        .river_speed = EFFECTS_RIVER_SPEED,
        .ripple_speed = EFFECTS_RIPPLE_SPEED,
        .peak_hold_ms = EFFECTS_PEAK_HOLD_MS,
        .drift_period_s = EFFECTS_DRIFT_PERIOD_S,
        .warmth = EFFECTS_WARMTH,
        .flash_level = EFFECTS_FLASH_LEVEL,
        .sparkle_rate = EFFECTS_SPARKLE_RATE,
    };
}

static bool is_at_least(float value, float least) {
    return value >= least && isfinite(value);
}

static bool is_fraction(float value) { return value >= 0.f && value <= 1.f; }

void effects_tune(effects_t *this, const effects_tuning_t *tuning) {
    if (tuning->river_speed >= 1)
        this->tuning.river_speed = tuning->river_speed;
    if (tuning->ripple_speed > 0.f && isfinite(tuning->ripple_speed))
        this->tuning.ripple_speed = tuning->ripple_speed;
    if (is_at_least(tuning->peak_hold_ms, 0.f))
        this->tuning.peak_hold_ms = tuning->peak_hold_ms;
    if (is_at_least(tuning->drift_period_s, 0.f))
        this->tuning.drift_period_s = tuning->drift_period_s;
    if (is_at_least(tuning->warmth, 0.f))
        this->tuning.warmth = tuning->warmth;
    if (is_fraction(tuning->flash_level))
        this->tuning.flash_level = tuning->flash_level;
    if (is_fraction(tuning->sparkle_rate))
        this->tuning.sparkle_rate = tuning->sparkle_rate;

    this->peak_hold_s = this->tuning.peak_hold_ms / 1000.f;
}

void effects_render(effects_t *this, const sound_t *sound, uint32_t *pixels) {
    color_ws2812_t flash;
    uint8_t white;

    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    // A mode without a renderer stays dark
    if (renderers[this->mode] != nullptr)
        renderers[this->mode](this, sound);

    if (sound->beat)
        this->flash =
            fmaxf(this->flash, sound->beat_strength * this->tuning.flash_level);

    // The flash goes on after gamma, as shown: added before it, 0.35 would
    // come out as 25 / 255. Saturating, so bright pixels do not wrap dark
    white = (uint8_t)lroundf(this->flash * 255.f);
    flash = color_ws2812_from_rgb(white, white, white);

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = this->frame[i];

        pixels[i] =
            color_ws2812_add(color_ws2812_from_rgb(color_gamma(color.r),
                                                   color_gamma(color.g),
                                                   color_gamma(color.b)),
                             flash)
                .value;
    }

    this->flash *= this->flash_k;

    // Wrapped where the drift repeats (two periods, for reflecting palettes
    // too): a float growing forever loses precision and freezes after ~36 h
    if (this->tuning.drift_period_s > 0.f)
        this->time_s = fmodf(this->time_s + this->hop_period_s,
                             2.f * this->tuning.drift_period_s);
    else
        this->time_s = 0.f;
}

void effects_deinit(effects_t *this) {
    free(this->frame);
    free(this->river.history);
    free(this->sparkles.levels);
}
