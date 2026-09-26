#include "audio.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

// INMP441 samples are 24-bit two's complement, full scale 2^23 - 1
#define AUDIO_FULL_SCALE ((float)(1 << 23))

// The INMP441 sensitivity is -26 dBFS at 94 dB SPL: loud sound peaks around
// 0.05 of full scale. Boost it 8x, i.e. scale samples by 2^20 instead of 2^23
#define AUDIO_INPUT_GAIN 8.f

#ifdef AUDIO_ENVELOPE
static inline void generate_envelope(float *samples, size_t count) {
    float aDelta = (float)M_PI / count;

    for (size_t i = 0; i < count; i++)
        samples[i] = sinf(i * aDelta);
}
#endif

bool audio_init(audio_t *this, size_t audio_sample_count) {
    float *audio_sample_buffer;
    float complex *packed_buffer;
    float *frequency_bins;
#ifdef AUDIO_ENVELOPE
    float *envelope = NULL;
#endif

    audio_sample_buffer = (float *)malloc(audio_sample_count * sizeof(float));

    if (audio_sample_buffer == NULL)
        return false;

    packed_buffer = (float complex *)malloc((audio_sample_count / 2) *
                                            sizeof(float complex));

    if (packed_buffer == NULL) {
        free(audio_sample_buffer);
        return false;
    }

    frequency_bins = (float *)malloc((audio_sample_count / 2) * sizeof(float));

    if (frequency_bins == NULL) {
        free(audio_sample_buffer);
        free(packed_buffer);
        return false;
    }

#ifdef AUDIO_ENVELOPE
    envelope = (float *)malloc(audio_sample_count * sizeof(float));

    if (envelope == NULL) {
        free(audio_sample_buffer);
        free(packed_buffer);
        free(frequency_bins);
        return false;
    }

    generate_envelope(envelope, audio_sample_count);
#endif

    if (!fft_real_init(&this->fft, audio_sample_count)) {
        free(audio_sample_buffer);
        free(packed_buffer);
        free(frequency_bins);
#ifdef AUDIO_ENVELOPE
        free(envelope);
#endif
        return false;
    }

    this->audio_sample_count = audio_sample_count;
    this->audio_sample_buffer = audio_sample_buffer;
    this->packed_buffer = packed_buffer;
    this->frequency_bins = frequency_bins;
#ifdef AUDIO_ENVELOPE
    this->envelope = envelope;
#endif

    return true;
}

void audio_feed_i2s(audio_t *this, const int32_t *frames) {
    for (size_t i = 0; i < this->audio_sample_count; i++) {
        // Extract the samples, left and right words alternate
        int32_t left = frames[2 * i];
        int32_t right = frames[2 * i + 1];

        // Signed 24-bit align
        left = (left << 1) >> 8;
        right = (right << 1) >> 8;

        // Both microphones hear the same sound (2 cm apart), sum to mono,
        // scale and put
        this->audio_sample_buffer[i] = ((float)left + (float)right) * 0.5f *
                                       (AUDIO_INPUT_GAIN / AUDIO_FULL_SCALE);
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
    fft_real_pack(this->audio_sample_buffer, this->packed_buffer,
                  this->audio_sample_count);
    fft_real(&this->fft, this->packed_buffer, this->frequency_bins);
}

const float *audio_get_frequency_bins(audio_t *this) {
    return this->frequency_bins;
}

size_t audio_get_frequency_bin_count(audio_t *this) {
    return this->audio_sample_count / 2;
}

void audio_deinit(audio_t *this) {
    free(this->audio_sample_buffer);
    free(this->packed_buffer);
    free(this->frequency_bins);
    fft_real_deinit(&this->fft);
}
