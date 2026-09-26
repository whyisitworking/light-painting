#include "visualizer.h"

bool visualizer_init(visualizer_t *this, const visualizer_config_t *config) {
    float hop_seconds;

    if (!(config->sample_rate > 0.f) ||
        !audio_init(&this->audio, config->fft_size, config->hop))
        return false;

    hop_seconds = (float)config->hop / config->sample_rate;

    if (!features_init(&this->features,
                       audio_get_frequency_bin_count(&this->audio),
                       config->sample_rate / (float)config->fft_size,
                       hop_seconds)) {
        audio_deinit(&this->audio);
        return false;
    }

    if (!effects_init(&this->effects, config->led_count, FEATURES_BAND_COUNT,
                      hop_seconds, config->seed)) {
        features_deinit(&this->features);
        audio_deinit(&this->audio);
        return false;
    }

    effects_set_mode(&this->effects, config->mode);
    effects_set_palette(&this->effects, config->palette);
    this->gain = config->gain;

    return true;
}

void visualizer_analyze(visualizer_t *this, const int32_t *frames) {
    audio_analyze(&this->audio, frames, this->gain);
}

const features_t *visualizer_render(visualizer_t *this, uint32_t *pixels) {
    const features_t *features = features_update(
        &this->features, audio_get_frequency_bins(&this->audio));

    effects_render(&this->effects, features, pixels);

    return features;
}

void visualizer_deinit(visualizer_t *this) {
    effects_deinit(&this->effects);
    features_deinit(&this->features);
    audio_deinit(&this->audio);
}
