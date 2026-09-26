#ifndef AUDIO_H
#define AUDIO_H

#include "fft.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t audio_sample_count;
    float complex *audio_sample_buffer;
    float *frequency_bins;
#ifdef AUDIO_ENVELOPE
    float *envelope;
#endif
    fft_t fft;
} audio_t;

bool audio_init(audio_t *this, size_t audio_sample_count);
void audio_feed_i2s(audio_t *context, const int32_t *samples);

#ifdef AUDIO_ENVELOPE
void audio_envelope(audio_t *this);
#endif

void audio_gain(audio_t *this, float gain);
void audio_fft(audio_t *this);
const float *audio_get_frequency_bins(audio_t *this);
size_t audio_get_frequency_bin_count(audio_t *this);
void audio_deinit(audio_t *this);

#endif
