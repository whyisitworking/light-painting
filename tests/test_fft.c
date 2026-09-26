#include "check.h"
#include "fft.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#define MAX_N 1024

// Reference: O(N^2) DFT straight from the definition, in double precision
static void naive_dft(const double complex *input, double complex *output,
                      size_t n) {
    for (size_t k = 0; k < n; k++) {
        double complex sum = 0;

        for (size_t t = 0; t < n; t++)
            sum += input[t] * cexp(-2.0 * M_PI * I * (double)(k * t) / n);

        output[k] = sum;
    }
}

static void fill_random(double complex *signal, size_t n) {
    for (size_t i = 0; i < n; i++)
        signal[i] = (double)rand() / RAND_MAX - 0.5;
}

static void test_init(void) {
    fft_t fft;

    CHECK(fft_init(&fft, 64) == 1);
    CHECK(fft.count == 64);

    fft_deinit(&fft);
}

static void test_dif_matches_dft(size_t n) {
    static double complex signal[MAX_N], expected[MAX_N];
    static float complex samples[MAX_N];
    static float bins[MAX_N / 2];
    fft_t fft;

    fill_random(signal, n);
    naive_dft(signal, expected, n);

    for (size_t i = 0; i < n; i++)
        samples[i] = (float complex)signal[i];

    CHECK(fft_init(&fft, n) == 1);
    fft_rad2_dif(&fft, samples, bins);

    // DIF leaves the spectrum in bit-reversed order
    for (size_t k = 0; k < n; k++) {
        float complex actual = samples[fft.reversed_indices[k]];
        CHECK_NEAR(crealf(actual), creal(expected[k]), 1e-3 * n);
        CHECK_NEAR(cimagf(actual), cimag(expected[k]), 1e-3 * n);
    }

    // Bins are the magnitudes of the first half, normalized by N/2
    for (size_t k = 0; k < n / 2; k++)
        CHECK_NEAR(bins[k], cabs(expected[k]) / (n / 2.0), 1e-3);

    fft_deinit(&fft);
}

static void test_dif_d_matches_dft(size_t n) {
    static double complex signal[MAX_N], expected[MAX_N], samples[MAX_N];
    static double bins[MAX_N / 2];
    fft_d_t fft;

    fill_random(signal, n);
    naive_dft(signal, expected, n);

    for (size_t i = 0; i < n; i++)
        samples[i] = signal[i];

    CHECK(fft_init_d(&fft, n) == 1);
    fft_rad2_dif_d(&fft, samples, bins);

    for (size_t k = 0; k < n; k++) {
        double complex actual = samples[fft.reversed_indices[k]];
        CHECK_NEAR(creal(actual), creal(expected[k]), 1e-9 * n);
        CHECK_NEAR(cimag(actual), cimag(expected[k]), 1e-9 * n);
    }

    for (size_t k = 0; k < n / 2; k++)
        CHECK_NEAR(bins[k], cabs(expected[k]) / (n / 2.0), 1e-9);

    fft_deinit_d(&fft);
}

// A pure tone of amplitude A at bin k0 must show up as a single bin of
// height A, everything else ~0
static void test_dif_tone(size_t n, size_t k0, float amplitude) {
    static float complex samples[MAX_N];
    static float bins[MAX_N / 2];
    fft_t fft;

    for (size_t i = 0; i < n; i++)
        samples[i] = amplitude * cosf(2.f * (float)M_PI * k0 * i / n);

    CHECK(fft_init(&fft, n) == 1);
    fft_rad2_dif(&fft, samples, bins);

    for (size_t k = 0; k < n / 2; k++)
        CHECK_NEAR(bins[k], k == k0 ? amplitude : 0.f, 1e-3);

    fft_deinit(&fft);
}

int main(void) {
    srand(1);

    test_init();

    for (size_t n = 2; n <= MAX_N; n <<= 1) {
        test_dif_matches_dft(n);
        test_dif_d_matches_dft(n);
    }

    test_dif_tone(64, 5, 1.f);
    test_dif_tone(512, 37, 0.25f);

    return CHECK_REPORT();
}
