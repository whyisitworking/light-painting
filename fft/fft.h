#ifndef FFT_H
#define FFT_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned int *reversed_indices;
    float complex *twiddles;
    size_t count;
} fft_t;

typedef struct {
    unsigned int *reversed_indices;
    double complex *twiddles;
    size_t count;
} fft_d_t;

int fft_init(fft_t *this, size_t count);
void fft_rad2_dit(fft_t *this, float complex *samples, float *frequency_bins);
void fft_rad2_dif(fft_t *this, float complex *samples, float *frequency_bins);
void fft_deinit(fft_t *this);

int fft_init_d(fft_d_t *this, size_t count);
void fft_rad2_dit_d(fft_d_t *this, double complex *samples,
                    double *frequency_bins);
void fft_rad2_dif_d(fft_d_t *this, double complex *samples,
                    double *frequency_bins);
void fft_deinit_d(fft_d_t *this);

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
 */
typedef struct {
    // N/2-point complex FFT that does the heavy lifting
    fft_t half;
    // W_N^k for k < N/2, used to separate the even/odd spectra
    float complex *twiddles;
    // N, the number of real samples
    size_t count;
} fft_real_t;

typedef struct {
    fft_d_t half;
    double complex *twiddles;
    size_t count;
} fft_real_d_t;

int fft_real_init(fft_real_t *this, size_t count);
void fft_real_pack(const float *samples, float complex *packed, size_t count);
void fft_real(fft_real_t *this, float complex *packed, float *frequency_bins);
void fft_real_deinit(fft_real_t *this);

int fft_real_init_d(fft_real_d_t *this, size_t count);
void fft_real_pack_d(const double *samples, double complex *packed,
                     size_t count);
void fft_real_d(fft_real_d_t *this, double complex *packed,
                double *frequency_bins);
void fft_real_deinit_d(fft_real_d_t *this);

#endif