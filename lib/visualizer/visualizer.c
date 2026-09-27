#include "visualizer.h"

bool visualizer_init(visualizer_t *this, const visualizer_config_t *config) {
    float hop_seconds;

    if (!(config->sample_rate > 0.f) ||
        !spectrum_init(&this->spectrum, config->fft_size, config->hop))
        return false;

    hop_seconds = (float)config->hop / config->sample_rate;

    if (!features_init(&this->features, spectrum_bin_count(&this->spectrum),
                       config->sample_rate / (float)config->fft_size,
                       hop_seconds)) {
        spectrum_deinit(&this->spectrum);
        return false;
    }

    if (!effects_init(&this->effects, config->led_count, FEATURES_BAND_COUNT,
                      hop_seconds, config->seed)) {
        features_deinit(&this->features);
        spectrum_deinit(&this->spectrum);
        return false;
    }

    effects_set_mode(&this->effects, config->mode);
    effects_set_palette(&this->effects, config->palette);
    this->gain = config->gain;

    return true;
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
