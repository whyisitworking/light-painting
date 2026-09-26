#ifndef AUDIO_H
#define AUDIO_H

#include "fft.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // FFT size: the number of mono samples analyzed at once
    size_t audio_sample_count;
    // New mono samples per feed, analyses overlap by the rest
    size_t hop_count;
    // The last audio_sample_count mono samples, oldest first
    float *history;
    // Mono samples being analyzed, a copy of the history
    float *audio_sample_buffer;
    // Scratch space of the real FFT, audio_sample_count / 2 entries
    float complex *packed_buffer;
    float *frequency_bins;
    // Analysis window, a sine lobe of audio_sample_count entries
    float *envelope;
    fft_real_t fft;
} audio_t;

/**
 * audio_sample_count: FFT size, a power of two >= 4
 * hop_count: new samples per feed, 1 to audio_sample_count
 */
bool audio_init(audio_t *this, size_t audio_sample_count, size_t hop_count);

/**
 * One analysis: feeds hop_count stereo frames, windows, applies the gain and
 * transforms. Returns the audio_sample_count / 2 magnitude bins, valid until
 * the next call. The same as calling the stages below in order.
 */
const float *audio_analyze(audio_t *this, const int32_t *frames, float gain);

/**
 * Feeds hop_count stereo frames straight from the I2S driver: pairs of left
 * and right words. Both microphones are summed to mono and appended to the
 * history, which is then ready to be analyzed.
 */
void audio_feed_i2s(audio_t *this, const int32_t *frames);
void audio_envelope(audio_t *this);
void audio_gain(audio_t *this, float gain);
void audio_fft(audio_t *this);

const float *audio_get_frequency_bins(audio_t *this);
size_t audio_get_frequency_bin_count(audio_t *this);
void audio_deinit(audio_t *this);

#endif
