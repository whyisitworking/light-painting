#ifndef SPECTRUM_H
#define SPECTRUM_H

/**
 * Spectrum analysis: turns the I2S words of the two INMP441 microphones into a
 * magnitude spectrum, once per hop.
 *
 *   frames ─► mono, scaled ─► history (sliding window) ─► window ─► gain
 *          ─► real FFT ─► fft_size / 2 bins, |X[k]| / (N / 2)
 *
 * Bin k is centred at k * fs / fft_size Hz. Everything is allocated by
 * spectrum_init(): the analysis itself never allocates.
 */

#include "fft.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // FFT size: the number of mono samples analyzed at once
    size_t fft_size;
    // New mono samples per feed, analyses overlap by the rest
    size_t hop_size;
    // The last fft_size mono samples, oldest first
    float *history;
    // Mono samples being analyzed, a copy of the history
    float *samples;
    // Scratch space of the real FFT, fft_size / 2 entries
    float complex *packed;
    // Magnitudes of the last analysis, fft_size / 2 entries
    float *bins;
    // Analysis window, a sine lobe of fft_size entries
    float *window;
    // Real FFT of fft_size points
    fft_real_t fft;
} spectrum_t;

/**
 * fft_size: FFT size, a power of two >= 4
 * hop_size: new samples per feed, 1 to fft_size
 *
 * False if the sizes are invalid or memory runs out. Silence until the
 * first fft_size samples arrived
 */
[[nodiscard]] bool spectrum_init(spectrum_t *this, size_t fft_size,
                                 size_t hop_size);

/**
 * One analysis: feeds hop_size stereo frames, windows, applies the gain and
 * transforms. Returns the fft_size / 2 magnitude bins, valid until the next
 * call. The same as calling the stages below in order.
 */
const float *spectrum_analyze(spectrum_t *this, const int32_t *frames,
                              float gain);

/**
 * Feeds hop_size stereo frames straight from the I2S driver: pairs of left
 * and right words. Both microphones are summed to mono and appended to the
 * history, which is then ready to be analyzed.
 */
void spectrum_feed_i2s(spectrum_t *this, const int32_t *frames);
// Multiplies the samples being analyzed by the window, against leakage
void spectrum_apply_window(spectrum_t *this);
// Multiplies the samples being analyzed by gain
void spectrum_apply_gain(spectrum_t *this, float gain);
// Transforms the samples being analyzed into the frequency bins
void spectrum_transform(spectrum_t *this);

// The bins of the last spectrum_transform(), fft_size / 2 of them
const float *spectrum_bins(const spectrum_t *this);
size_t spectrum_bin_count(const spectrum_t *this);
// Only after a successful spectrum_init()
void spectrum_deinit(spectrum_t *this);

#endif
