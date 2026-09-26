#include "audio.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// INMP441 samples are 24-bit two's complement, full scale 2^23 - 1
#define AUDIO_FULL_SCALE ((float)(1 << 23))

// The INMP441 sensitivity is -26 dBFS at 94 dB SPL: loud sound peaks around
// 0.05 of full scale. Boost it 8x, i.e. scale samples by 2^20 instead of 2^23
#define AUDIO_INPUT_GAIN 8.f

static inline void generate_envelope(float *samples, size_t count) {
    float aDelta = (float)M_PI / count;

    for (size_t i = 0; i < count; i++)
        samples[i] = sinf(i * aDelta);
}

bool audio_init(audio_t *this, size_t audio_sample_count, size_t hop_count) {
    float *history, *audio_sample_buffer, *frequency_bins, *envelope;
    float complex *packed_buffer;

    if (hop_count < 1 || hop_count > audio_sample_count)
        return false;

    // Silence until enough audio arrived
    history = (float *)calloc(audio_sample_count, sizeof(float));
    audio_sample_buffer = (float *)malloc(audio_sample_count * sizeof(float));
    packed_buffer = (float complex *)malloc((audio_sample_count / 2) *
                                            sizeof(float complex));
    frequency_bins = (float *)malloc((audio_sample_count / 2) * sizeof(float));
    envelope = (float *)malloc(audio_sample_count * sizeof(float));

    if (history == NULL || audio_sample_buffer == NULL ||
        packed_buffer == NULL || frequency_bins == NULL || envelope == NULL ||
        !fft_real_init(&this->fft, audio_sample_count)) {
        free(history);
        free(audio_sample_buffer);
        free(packed_buffer);
        free(frequency_bins);
        free(envelope);
        return false;
    }

    generate_envelope(envelope, audio_sample_count);

    this->audio_sample_count = audio_sample_count;
    this->hop_count = hop_count;
    this->history = history;
    this->audio_sample_buffer = audio_sample_buffer;
    this->packed_buffer = packed_buffer;
    this->frequency_bins = frequency_bins;
    this->envelope = envelope;

    return true;
}

const float *audio_analyze(audio_t *this, const int32_t *frames, float gain) {
    audio_feed_i2s(this, frames);
    audio_envelope(this);
    audio_gain(this, gain);
    audio_fft(this);

    return this->frequency_bins;
}

void audio_feed_i2s(audio_t *this, const int32_t *frames) {
    size_t kept = this->audio_sample_count - this->hop_count;
    float *new_samples = this->history + kept;

    // Drop the oldest hop_count samples
    memmove(this->history, this->history + this->hop_count,
            kept * sizeof(float));

    for (size_t i = 0; i < this->hop_count; i++) {
        // Extract the samples, left and right words alternate
        int32_t left = frames[2 * i];
        int32_t right = frames[2 * i + 1];

        // Signed 24-bit align, shift left as unsigned: shifting a negative
        // signed value left is undefined in C
        left = (int32_t)((uint32_t)left << 1) >> 8;
        right = (int32_t)((uint32_t)right << 1) >> 8;

        // Both microphones hear the same sound (2 cm apart), sum to mono,
        // scale and put
        new_samples[i] = ((float)left + (float)right) * 0.5f *
                         (AUDIO_INPUT_GAIN / AUDIO_FULL_SCALE);
    }

    // Analyze a copy, the history must stay unwindowed
    memcpy(this->audio_sample_buffer, this->history,
           this->audio_sample_count * sizeof(float));
}

void audio_envelope(audio_t *this) {
    for (size_t i = 0; i < this->audio_sample_count; i++)
        this->audio_sample_buffer[i] *= this->envelope[i];
}

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
    free(this->history);
    free(this->audio_sample_buffer);
    free(this->packed_buffer);
    free(this->frequency_bins);
    free(this->envelope);
    fft_real_deinit(&this->fft);
}
