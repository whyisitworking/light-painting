#include "spectrum.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// INMP441 samples are 24-bit two's complement, full scale 2^23 - 1
#define SPECTRUM_FULL_SCALE ((float)(1 << 23))

// The INMP441 sensitivity is -26 dBFS at 94 dB SPL: loud sound peaks around
// 0.05 of full scale. Boost it 8x, i.e. scale samples by 2^20 instead of 2^23
#define SPECTRUM_INPUT_GAIN 8.f

// Sine window: sin(pi * i / count), 0 at the start, 1 in the middle. The
// ends of the analysis fade out, so a tone that does not fit a whole number
// of periods does not smear over the whole spectrum
static inline void fill_sine_window(float *window, size_t count) {
    float step = (float)M_PI / count;

    for (size_t i = 0; i < count; i++)
        window[i] = sinf(i * step);
}

bool spectrum_init(spectrum_t *this, size_t fft_size, size_t hop_size) {
    float *history, *samples, *bins, *window;
    float complex *packed;

    if (hop_size < 1 || hop_size > fft_size)
        return false;

    // Silence until enough audio arrived
    history = (float *)calloc(fft_size, sizeof(float));
    samples = (float *)malloc(fft_size * sizeof(float));
    packed = (float complex *)malloc((fft_size / 2) * sizeof(float complex));
    bins = (float *)malloc((fft_size / 2) * sizeof(float));
    window = (float *)malloc(fft_size * sizeof(float));

    if (history == NULL || samples == NULL || packed == NULL || bins == NULL ||
        window == NULL || !fft_real_init(&this->fft, fft_size)) {
        free(history);
        free(samples);
        free(packed);
        free(bins);
        free(window);
        return false;
    }

    fill_sine_window(window, fft_size);

    this->fft_size = fft_size;
    this->hop_size = hop_size;
    this->history = history;
    this->samples = samples;
    this->packed = packed;
    this->bins = bins;
    this->window = window;

    return true;
}

const float *spectrum_analyze(spectrum_t *this, const int32_t *frames,
                              float gain) {
    spectrum_feed_i2s(this, frames);
    spectrum_apply_window(this);
    spectrum_apply_gain(this, gain);
    spectrum_transform(this);

    return this->bins;
}

void spectrum_feed_i2s(spectrum_t *this, const int32_t *frames) {
    size_t kept = this->fft_size - this->hop_size;
    float *new_samples = this->history + kept;

    // Drop the oldest hop_size samples
    memmove(this->history, this->history + this->hop_size,
            kept * sizeof(float));

    for (size_t i = 0; i < this->hop_size; i++) {
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
                         (SPECTRUM_INPUT_GAIN / SPECTRUM_FULL_SCALE);
    }

    // Analyze a copy, the history must stay unwindowed
    memcpy(this->samples, this->history, this->fft_size * sizeof(float));
}

void spectrum_apply_window(spectrum_t *this) {
    for (size_t i = 0; i < this->fft_size; i++)
        this->samples[i] *= this->window[i];
}

void spectrum_apply_gain(spectrum_t *this, float gain) {
    for (size_t i = 0; i < this->fft_size; i++)
        this->samples[i] *= gain;
}

void spectrum_transform(spectrum_t *this) {
    fft_real_pack(this->samples, this->packed, this->fft_size);
    fft_real(&this->fft, this->packed, this->bins);
}

const float *spectrum_bins(spectrum_t *this) { return this->bins; }

size_t spectrum_bin_count(spectrum_t *this) { return this->fft_size / 2; }

void spectrum_deinit(spectrum_t *this) {
    free(this->history);
    free(this->samples);
    free(this->packed);
    free(this->bins);
    free(this->window);
    fft_real_deinit(&this->fft);
}
