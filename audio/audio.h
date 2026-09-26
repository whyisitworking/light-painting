#ifndef AUDIO_H
#define AUDIO_H

#include "fft.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // FFT size: the number of mono samples analyzed at once
    size_t audio_sample_count;
    // Mono samples being analyzed
    float *audio_sample_buffer;
    // Scratch space of the real FFT, audio_sample_count / 2 entries
    float complex *packed_buffer;
    float *frequency_bins;
#ifdef AUDIO_ENVELOPE
    float *envelope;
#endif
    fft_real_t fft;
} audio_t;

bool audio_init(audio_t *this, size_t audio_sample_count);
/**
 * Feeds audio_sample_count stereo frames straight from the I2S driver: pairs
 * of left and right words. Both microphones are summed to mono.
 */
void audio_feed_i2s(audio_t *context, const int32_t *frames);

#ifdef AUDIO_ENVELOPE
void audio_envelope(audio_t *this);
#endif

void audio_gain(audio_t *this, float gain);
void audio_fft(audio_t *this);
const float *audio_get_frequency_bins(audio_t *this);
size_t audio_get_frequency_bin_count(audio_t *this);
void audio_deinit(audio_t *this);

#endif
