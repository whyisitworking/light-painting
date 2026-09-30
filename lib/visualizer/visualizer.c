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

    if (!show_init(&this->show, config->led_count, FEATURES_BAND_COUNT,
                   hop_period_s, config->bend_count, config->seed)) {
        features_deinit(&this->features);
        spectrum_deinit(&this->spectrum);
        return false;
    }

    this->gain = VISUALIZER_GAIN;
    features_set_gain(&this->features, this->gain);

    return true;
}

visualizer_tuning_t visualizer_default_tuning(void) {
    return (visualizer_tuning_t){
        .gain = VISUALIZER_GAIN,
        .features = features_default_tuning(),
        .show = show_default_tuning(),
    };
}

void visualizer_tune(visualizer_t *this, const visualizer_tuning_t *tuning) {
    if (tuning->gain > 0.f && isfinite(tuning->gain))
        this->gain = tuning->gain;
    features_set_gain(&this->features, this->gain);

    features_tune(&this->features, &tuning->features);
    show_tune(&this->show, &tuning->show);
}

void visualizer_analyze(visualizer_t *this, const int32_t *frames) {
    spectrum_analyze(&this->spectrum, frames, this->gain);
}

const sound_t *visualizer_render(visualizer_t *this, uint32_t *pixels) {
    const sound_t *sound =
        features_update(&this->features, spectrum_bins(&this->spectrum));

    show_render(&this->show, sound, pixels);

    return sound;
}

void visualizer_deinit(visualizer_t *this) {
    show_deinit(&this->show);
    features_deinit(&this->features);
    spectrum_deinit(&this->spectrum);
}
