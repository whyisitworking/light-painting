#include "audio.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#define AMPLITUDE_24BIT ((uint32_t)0x00FFFFFF)

#ifdef AUDIO_ENVELOPE
static inline void generate_envelope(float *samples, size_t count) {
    float aDelta = (float)M_PI / count;

    for (size_t i = 0; i < count; i++)
        samples[i] = sinf(i * aDelta);
}
#endif

bool audio_init(audio_t *this, size_t audio_sample_count) {
    float complex *audio_sample_buffer;
    float *frequency_bins;
#ifdef AUDIO_ENVELOPE
    float *envelope = NULL;
#endif

    audio_sample_buffer =
        (float complex *)malloc(audio_sample_count * sizeof(float complex));

    if (audio_sample_buffer == NULL)
        return false;

    frequency_bins = (float *)malloc((audio_sample_count / 2) * sizeof(float));

    if (frequency_bins == NULL) {
        free(audio_sample_buffer);
        return false;
    }

#ifdef AUDIO_ENVELOPE
    envelope = (float *)malloc(audio_sample_count * sizeof(float));

    if (envelope == NULL) {
        free(audio_sample_buffer);
        free(frequency_bins);
        return false;
    }

    generate_envelope(envelope, audio_sample_count);
#endif

    if (!fft_init(&this->fft, audio_sample_count)) {
        free(audio_sample_buffer);
        free(frequency_bins);
#ifdef AUDIO_ENVELOPE
        free(envelope);
#endif
        return false;
    }

    this->audio_sample_count = audio_sample_count;
    this->audio_sample_buffer = audio_sample_buffer;
    this->frequency_bins = frequency_bins;
#ifdef AUDIO_ENVELOPE
    this->envelope = envelope;
#endif

    return true;
}

void audio_feed_i2s(audio_t *this, const int32_t *samples) {
    for (size_t i = 0; i < this->audio_sample_count; i++) {
        // Extract the sample
        int32_t sample = samples[i];

        // Signed 24-bit align
        sample = (sample << 1) >> 8;

        // Scale and put
        this->audio_sample_buffer[i] = (float)sample / (float)0x000FFFFF;
    }
}

#ifdef AUDIO_ENVELOPE
void audio_envelope(audio_t *this) {
    for (size_t i = 0; i < this->audio_sample_count; i++)
        this->audio_sample_buffer[i] *= this->envelope[i];
}
#endif

void audio_gain(audio_t *this, float gain) {
    for (size_t i = 0; i < this->audio_sample_count; i++)
        this->audio_sample_buffer[i] *= gain;
}

void audio_fft(audio_t *this) {
    fft_rad2_dif(&this->fft, this->audio_sample_buffer, this->frequency_bins);
}

const float *audio_get_frequency_bins(audio_t *this) {
    return this->frequency_bins;
}

size_t audio_get_frequency_bin_count(audio_t *this) {
    return this->audio_sample_count / 2;
}

void audio_deinit(audio_t *this) {
    free(this->audio_sample_buffer);
    free(this->frequency_bins);
    fft_deinit(&this->fft);
}
