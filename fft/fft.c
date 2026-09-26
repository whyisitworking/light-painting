#include "fft.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Ultra fast log base-2 of only 2^n numbers. For others, the
 * result/behavior is invalid/undefined.
 *
 * Since 2^n numbers will have only one '1' bit, we just need to shift
 * right until we find it, and that's log2N
 *
 * @param N The input. *MUST BE A POWER OF 2*
 * @return The log base-2 result, -1 if not a power of two
 */
static inline int log2N(unsigned int N) {
    int value, n;

    // Keep shifting right until we find a '1' at the LSB
    // and that's when we know we hit the jackpot!
    for (value = 0, n = N; (n & 0b1) == 0; n >>= 1, value++)
        ;

    if (n == 1)
        return value;

    return -1;
}

/**
 * @brief O(n) order reverse bits.
 *
 * @param N Value to be bit-reversed
 * @param bit_depth Number of bits to be reversed
 * @return unsigned int Bit-reversed number
 */
static inline unsigned int reverse_bits(unsigned int N,
                                        unsigned int bit_depth) {
    unsigned int output, i;

    // Simple, left shift one, right shift the other, bleh!
    for (output = 0, i = 0; i < bit_depth; i++, N >>= 1)
        output = (output << 1) | (N & 0b1);

    return output;
}

static void fill_reversed_indices(unsigned int *reversed_indices,
                                  unsigned int N) {
    unsigned int bit_depth, i;

    // Number of bits required
    bit_depth = log2N(N);

    for (i = 0; i < N; i++)
        reversed_indices[i] = reverse_bits(i, bit_depth);
}

/**
 * @brief Fills the N/2 twiddle factors W_N^i = e^(-2*pi*i/N) needed by an
 * N-point radix-2 FFT.
 *
 * @param twiddles Output, must hold N/2 entries
 * @param N The FFT size (not the number of twiddles)
 */
static void fill_twiddles(float complex *twiddles, unsigned int N) {
    float angle_per_sample;
    unsigned int i;

    // -2pi/N, constant, reducing the calculations
    angle_per_sample = -2.0f * (float)M_PI / N;

    // Cache the twiddle factors
    // Compromise some space for HUGE performance gain
    // Cache locality baby!
    for (i = 0; i < N / 2; i++)
        twiddles[i] = cexpf(angle_per_sample * i * I);
}

/**
 * @brief Double precision variant of fill_twiddles.
 *
 * @param twiddles Output, must hold N/2 entries
 * @param N The FFT size (not the number of twiddles)
 */
static void fill_twiddles_d(double complex *twiddles, unsigned int N) {
    double angle_per_sample;
    unsigned int i;

    // -2pi/N, constant, reducing the calculations
    angle_per_sample = -2.0 * M_PI / N;

    // Cache the twiddle factors
    // Compromise some space for HUGE performance gain
    // Cache locality baby!
    for (i = 0; i < N / 2; i++)
        twiddles[i] = cexp(angle_per_sample * i * I);
}

int fft_init(fft_t *this, size_t count) {
    unsigned int *reversed_indices;
    float complex *twiddles;

    reversed_indices = (unsigned int *)malloc(count * sizeof(unsigned int));

    if (reversed_indices == NULL)
        return -1;

    twiddles = (float complex *)malloc((count / 2) * sizeof(float complex));

    if (twiddles == NULL) {
        free(reversed_indices);
        return -1;
    }

    fill_reversed_indices(reversed_indices, count);
    fill_twiddles(twiddles, count);

    this->count = count;
    this->reversed_indices = reversed_indices;
    this->twiddles = twiddles;

    return 1;
}

void fft_rad2_dit(fft_t *this, float complex *samples, float *frequency_bins) {
    unsigned int halfN, set_count, ops_per_set, set, start, butterfly,
        butterfly_top_idx, butterfly_bottom_idx;
    float complex twiddle, butterfly_top, butterfly_bottom;

    // Don't mess with me
    if (samples == NULL)
        return;

    // Mr. Clean
    halfN = this->count / 2;

    // Perform the stages
    // i is the number of sets to perform
    // eg For N = 8
    // i = 4,2,1
    for (set_count = halfN; set_count >= 1; set_count >>= 1) {
        // No of operations per set in this stage
        // ops_per_set = 1,2,4
        ops_per_set = halfN / set_count;

        // Loop over sets
        // j is the set #
        for (set = 0; set < set_count; set++) {
            // Start the butterflies
            start = set * ops_per_set * 2;

            // Loop over butterflies
            for (butterfly = 0; butterfly < ops_per_set; butterfly++) {
                butterfly_top_idx = this->reversed_indices[start + butterfly];
                butterfly_bottom_idx =
                    this->reversed_indices[start + butterfly + ops_per_set];

                // Determine the twiddle to pre-multiply the lower
                // half of the butterfly
                twiddle =
                    this->twiddles[butterfly * set_count]; // Cache hit baby!
                butterfly_top = samples[butterfly_top_idx];
                butterfly_bottom = twiddle * samples[butterfly_bottom_idx];

                // Finally the butterfly
                samples[butterfly_top_idx] = butterfly_top + butterfly_bottom;
                samples[butterfly_bottom_idx] =
                    butterfly_top - butterfly_bottom;
            }
        }
    }

    if (frequency_bins != NULL) {
        for (size_t i = 0; i < halfN; i++) {
            float complex sample = samples[i];
            frequency_bins[i] = cabsf(sample) / halfN;
        }
    }
}

void fft_rad2_dif(fft_t *this, float complex *samples, float *frequency_bins) {
    unsigned int halfN, set_count, ops_per_set, set, start, butterfly,
        butterfly_top_idx, butterfly_bottom_idx;
    float complex twiddle, butterfly_top, butterfly_bottom;

    // Don't mess with me
    if (samples == NULL)
        return;

    // Mr. Clean!
    halfN = this->count / 2;

    // Perform the stages
    // i is the number of sets to perform
    // eg For N = 8
    // i = 1,2,4
    for (set_count = 1; set_count <= halfN; set_count <<= 1) {
        // No of operations per set in this stage
        // ops_per_set = 4,2,1
        ops_per_set = halfN / set_count;

        // Loop over sets
        for (set = 0; set < set_count; set++) {
            // Start the butterflies
            start = set * ops_per_set * 2;

            // Loop over butterflies
            for (butterfly = 0; butterfly < ops_per_set; butterfly++) {
                butterfly_top_idx = start + butterfly;
                butterfly_bottom_idx = start + butterfly + ops_per_set;

                // Determine the twiddle to pre-multiply the lower
                // half of the butterfly
                // Cache hit baby!
                twiddle = this->twiddles[butterfly * set_count];
                butterfly_top = samples[butterfly_top_idx];
                butterfly_bottom = samples[butterfly_bottom_idx];

                // Finally the butterfly
                samples[butterfly_top_idx] = butterfly_top + butterfly_bottom;
                samples[butterfly_bottom_idx] =
                    twiddle * (butterfly_top - butterfly_bottom);
            }
        }
    }

    if (frequency_bins != NULL) {
        for (size_t i = 0; i < halfN; i++) {
            float complex sample = samples[this->reversed_indices[i]];
            frequency_bins[i] = cabsf(sample) / halfN;
        }
    }
}

void fft_deinit(fft_t *this) {
    free(this->twiddles);
    free(this->reversed_indices);
}

int fft_init_d(fft_d_t *this, size_t count) {
    unsigned int *reversed_indices;
    double complex *twiddles;

    reversed_indices = (unsigned int *)malloc(count * sizeof(unsigned int));

    if (reversed_indices == NULL)
        return -1;

    twiddles = (double complex *)malloc((count / 2) * sizeof(double complex));

    if (twiddles == NULL) {
        free(reversed_indices);
        return -1;
    }

    fill_reversed_indices(reversed_indices, count);
    fill_twiddles_d(twiddles, count);

    this->count = count;
    this->reversed_indices = reversed_indices;
    this->twiddles = twiddles;

    return 1;
}

void fft_rad2_dit_d(fft_d_t *this, double complex *samples,
                    double *frequency_bins) {
    unsigned int halfN, set_count, ops_per_set, set, start, butterfly,
        butterfly_top_idx, butterfly_bottom_idx;
    double complex twiddle, butterfly_top, butterfly_bottom;

    // Don't mess with me
    if (samples == NULL)
        return;

    // Mr. Clean
    halfN = this->count / 2;

    // Perform the stages
    // i is the number of sets to perform
    // eg For N = 8
    // i = 4,2,1
    for (set_count = halfN; set_count >= 1; set_count >>= 1) {
        // No of operations per set in this stage
        // ops_per_set = 1,2,4
        ops_per_set = halfN / set_count;

        // Loop over sets
        // j is the set #
        for (set = 0; set < set_count; set++) {
            // Start the butterflies
            start = set * ops_per_set * 2;

            // Loop over butterflies
            for (butterfly = 0; butterfly < ops_per_set; butterfly++) {
                butterfly_top_idx = this->reversed_indices[start + butterfly];
                butterfly_bottom_idx =
                    this->reversed_indices[start + butterfly + ops_per_set];

                // Determine the twiddle to pre-multiply the lower
                // half of the butterfly
                twiddle =
                    this->twiddles[butterfly * set_count]; // Cache hit baby!
                butterfly_top = samples[butterfly_top_idx];
                butterfly_bottom = twiddle * samples[butterfly_bottom_idx];

                // Finally the butterfly
                samples[butterfly_top_idx] = butterfly_top + butterfly_bottom;
                samples[butterfly_bottom_idx] =
                    butterfly_top - butterfly_bottom;
            }
        }
    }

    if (frequency_bins != NULL) {
        for (size_t i = 0; i < halfN; i++) {
            double complex sample = samples[i];
            frequency_bins[i] = cabs(sample) / halfN;
        }
    }
}

void fft_rad2_dif_d(fft_d_t *this, double complex *samples,
                    double *frequency_bins) {
    unsigned int halfN, set_count, ops_per_set, set, start, butterfly,
        butterfly_top_idx, butterfly_bottom_idx;
    double complex twiddle, butterfly_top, butterfly_bottom;

    // Don't mess with me
    if (samples == NULL)
        return;

    // Mr. Clean
    halfN = this->count / 2;

    // Perform the stages
    // i is the number of sets to perform
    // eg For N = 8
    // i = 1,2,4
    for (set_count = 1; set_count <= halfN; set_count <<= 1) {
        // No of operations per set in this stage
        // ops_per_set = 4,2,1
        ops_per_set = halfN / set_count;

        // Loop over sets
        for (set = 0; set < set_count; set++) {
            // Start the butterflies
            start = set * ops_per_set * 2;

            // Loop over butterflies
            for (butterfly = 0; butterfly < ops_per_set; butterfly++) {
                butterfly_top_idx = start + butterfly;
                butterfly_bottom_idx = start + butterfly + ops_per_set;

                // Determine the twiddle to pre-multiply the lower
                // half of the butterfly
                // Cache hit baby!
                twiddle = this->twiddles[butterfly * set_count];
                butterfly_top = samples[butterfly_top_idx];
                butterfly_bottom = samples[butterfly_bottom_idx];

                // Finally the butterfly
                samples[butterfly_top_idx] = butterfly_top + butterfly_bottom;
                samples[butterfly_bottom_idx] =
                    twiddle * (butterfly_top - butterfly_bottom);
            }
        }
    }

    if (frequency_bins != NULL) {
        for (size_t i = 0; i < halfN; i++) {
            double complex sample = samples[this->reversed_indices[i]];
            frequency_bins[i] = cabs(sample) / halfN;
        }
    }
}

void fft_deinit_d(fft_d_t *this) {
    free(this->twiddles);
    free(this->reversed_indices);
}