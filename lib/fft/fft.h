#ifndef FFT_H
#define FFT_H

/**
 * Radix-2 FFTs in single precision, for the audio analysis.
 *
 * All tables (bit-reversed indices, twiddle factors) are computed once by
 * the *_init functions, so the transforms themselves never allocate and
 * run in O(N log N). The spectrum is left in bit-reversed order: read it
 * through reversed_indices, or let the transforms fill frequency_bins.
 */

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // Bit reversal of each index 0..count - 1, maps natural to FFT order
    unsigned int *reversed_indices;
    // W_N^k = e^(-2*pi*i*k/N) for k < count / 2
    float complex *twiddles;
    // N, the transform size
    size_t count;
} fft_t;

/**
 * Complex FFT, radix-2 decimation in time (dit) or in frequency (dif)
 *
 * count must be a power of two >= 2. The transforms work in place: samples
 * ends up holding the spectrum in bit-reversed order, and frequency_bins
 * (may be NULL) receives |X[k]| / (N/2) for k < N/2.
 *
 * The *_deinit functions may only be called after a successful *_init.
 */
bool fft_init(fft_t *this, size_t count);
void fft_rad2_dit(fft_t *this, float complex *samples, float *frequency_bins);
void fft_rad2_dif(fft_t *this, float complex *samples, float *frequency_bins);
void fft_deinit(fft_t *this);

/**
 * Real input FFT
 *
 * An N-point FFT of real samples computed with an N/2-point complex FFT
 * (the "packing" method): the even samples go into the real parts and the
 * odd samples into the imaginary parts, and the two interleaved spectra are
 * separated afterwards. Roughly half the work of a complex FFT with zero
 * imaginary parts.
 *
 * Usage:
 *   fft_real_pack(real_samples, packed, N);   // packed holds N/2 entries
 *   fft_real(&fft, packed, frequency_bins);   // N/2 bins
 *
 * count must be a power of two >= 4. frequency_bins receives the same
 * |X[k]| / (N/2) for k < N/2 as the complex transforms. packed is used as
 * scratch: afterwards it holds the half size spectrum Z (bit-reversed), not
 * X, even when frequency_bins is NULL.
 */
typedef struct {
    // N/2-point complex FFT that does the heavy lifting
    fft_t half;
    // W_N^k for k < N/2, used to separate the even/odd spectra
    float complex *twiddles;
    // N, the number of real samples
    size_t count;
} fft_real_t;

// count: N, a power of two >= 4. False if invalid or out of memory
bool fft_real_init(fft_real_t *this, size_t count);
// Packs count real samples into count / 2 complex ones, even + i * odd
void fft_real_pack(const float *samples, float complex *packed, size_t count);
// Transforms packed in place, then writes the count / 2 magnitude bins
void fft_real(fft_real_t *this, float complex *packed, float *frequency_bins);
void fft_real_deinit(fft_real_t *this);

#endif
