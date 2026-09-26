#ifndef AUDIO_H
#define AUDIO_H

/**
 * Audio analysis: turns the I2S words of the two INMP441 microphones into a
 * magnitude spectrum, once per hop.
 *
 *   frames ─► mono, scaled ─► history (sliding window) ─► window ─► gain
 *          ─► real FFT ─► audio_sample_count / 2 bins, |X[k]| / (N / 2)
 *
 * Bin k is centred at k * fs / audio_sample_count Hz. Everything is
 * allocated by audio_init(): the analysis itself never allocates.
 */

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
    // Magnitudes of the last analysis, audio_sample_count / 2 entries
    float *frequency_bins;
    // Analysis window, a sine lobe of audio_sample_count entries
    float *envelope;
    // Real FFT of audio_sample_count points
    fft_real_t fft;
} audio_t;

/**
 * audio_sample_count: FFT size, a power of two >= 4
 * hop_count: new samples per feed, 1 to audio_sample_count
 *
 * False if the sizes are invalid or memory runs out. Silence until the
 * first audio_sample_count samples arrived
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
// Multiplies the samples being analyzed by the window, against leakage
void audio_envelope(audio_t *this);
// Multiplies the samples being analyzed by gain
void audio_gain(audio_t *this, float gain);
// Transforms the samples being analyzed into the frequency bins
void audio_fft(audio_t *this);

// The bins of the last audio_fft(), audio_sample_count / 2 of them
const float *audio_get_frequency_bins(audio_t *this);
size_t audio_get_frequency_bin_count(audio_t *this);
// Only after a successful audio_init()
void audio_deinit(audio_t *this);

#endif
