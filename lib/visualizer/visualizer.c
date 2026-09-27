#include "visualizer.h"

#include <math.h>

bool visualizer_init(visualizer_t *this, const visualizer_config_t *config) {
    float hop_period_s;

    if (!(config->sample_rate > 0.f) ||
        !spectrum_init(&this->spectrum, config->fft_size, config->hop_size))
        return false;

    hop_period_s = (float)config->hop_size / config->sample_rate;

    if (!features_init(&this->features, spectrum_bin_count(&this->spectrum),
                       config->sample_rate / (float)config->fft_size,
                       hop_period_s)) {
        spectrum_deinit(&this->spectrum);
        return false;
    }

    if (!effects_init(&this->effects, config->led_count, FEATURES_BAND_COUNT,
                      hop_period_s, config->seed)) {
        features_deinit(&this->features);
        spectrum_deinit(&this->spectrum);
        return false;
    }

    effects_set_mode(&this->effects, config->mode);
    effects_set_palette(&this->effects, config->palette);
    this->gain = config->gain;

    return true;
}

visualizer_tuning_t visualizer_default_tuning(void) {
    return (visualizer_tuning_t){
        .mode = VISUALIZER_MODE,
        .palette = VISUALIZER_PALETTE,
        .gain = VISUALIZER_GAIN,
        .features = features_default_tuning(),
        .effects = effects_default_tuning(),
    };
}

void visualizer_tune(visualizer_t *this, const visualizer_tuning_t *tuning) {
    effects_set_mode(&this->effects, tuning->mode);
    effects_set_palette(&this->effects, tuning->palette);

    if (tuning->gain > 0.f && isfinite(tuning->gain))
        this->gain = tuning->gain;

    features_tune(&this->features, &tuning->features);
    effects_tune(&this->effects, &tuning->effects);
}

void visualizer_analyze(visualizer_t *this, const int32_t *frames) {
    spectrum_analyze(&this->spectrum, frames, this->gain);
}

const sound_t *visualizer_render(visualizer_t *this, uint32_t *pixels) {
    const sound_t *sound =
        features_update(&this->features, spectrum_bins(&this->spectrum));

    effects_render(&this->effects, sound, pixels);

    return sound;
}

void visualizer_deinit(visualizer_t *this) {
    effects_deinit(&this->effects);
    features_deinit(&this->features);
    spectrum_deinit(&this->spectrum);
}
