#include "effects.h"

#include "color.h"
#include "effects_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// The renderer of each mode, see effects_internal.h
static effects_renderer_t *const renderers[EFFECTS_MODE_COUNT] = {
    [EFFECTS_SPECTRUM] = effects_mode_spectrum,
    [EFFECTS_SPECTRUM_MIRRORED] = effects_mode_spectrum_mirrored,
    [EFFECTS_RIVER] = effects_mode_river,
    [EFFECTS_RIPPLES] = effects_mode_ripples,
    [EFFECTS_VU] = effects_mode_vu,
    [EFFECTS_GLOW] = effects_mode_glow,
};

bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed) {
    size_t half = (led_count + 1) / 2;
    rgb_t *frame, *river;
    float *sparkles;

    if (led_count < 2 || band_count < 2 || !(hop_seconds > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    river = (rgb_t *)calloc(half, sizeof(rgb_t));
    sparkles = (float *)calloc(led_count, sizeof(float));

    if (frame == NULL || river == NULL || sparkles == NULL) {
        free(frame);
        free(river);
        free(sparkles);
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
        .river.history = river,
        .sparkles.levels = sparkles,
        .sparkles.random = seed != 0 ? seed : 1,
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

void effects_render(effects_t *this, const sound_t *sound, uint32_t *pixels) {
    color_ws2812_t flash;
    uint8_t white;

    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    // A mode without a renderer stays dark
    if (renderers[this->mode] != NULL)
        renderers[this->mode](this, sound);

    if (sound->beat)
        this->flash =
            fmaxf(this->flash, sound->beat_strength * EFFECTS_FLASH_LEVEL);

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
    this->time_s += this->hop_seconds;
    if (EFFECTS_DRIFT_PERIOD_S > 0.f)
        this->time_s = fmodf(this->time_s, 2.f * EFFECTS_DRIFT_PERIOD_S);
}

void effects_deinit(effects_t *this) {
    free(this->frame);
    free(this->river.history);
    free(this->sparkles.levels);
}
