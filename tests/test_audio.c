#include "audio.h"
#include "check.h"

#include <math.h>

#define N 64

// The I2S word of a 24-bit sample: one delay bit, 24 data bits, 7 unused
static int32_t i2s_word(int32_t sample) {
    return (int32_t)((uint32_t)sample << 7);
}

// Stereo frames with a tone of the given 24-bit amplitudes at bin k0
static void make_frames(int32_t *frames, size_t k0, double left_amplitude,
                        double right_amplitude) {
    for (size_t i = 0; i < N; i++) {
        double phase = cos(2.0 * M_PI * k0 * i / N);

        frames[2 * i] = i2s_word((int32_t)lround(left_amplitude * phase));
        frames[2 * i + 1] = i2s_word((int32_t)lround(right_amplitude * phase));
    }
}

static size_t peak_bin(const float *bins, size_t count) {
    size_t peak = 0;

    for (size_t k = 1; k < count; k++)
        if (bins[k] > bins[peak])
            peak = k;

    return peak;
}

// 2^23 / 16 scaled by the 8x input gain is 0.5
static void test_feed_scaling_and_sum(void) {
    int32_t frames[2 * N];
    audio_t audio;

    CHECK(audio_init(&audio, N));

    make_frames(frames, 0, (1 << 23) / 16.0, (1 << 23) / 16.0);
    audio_feed_i2s(&audio, frames);
    CHECK_NEAR(audio.audio_sample_buffer[0], 0.5, 1e-6);

    // Only one microphone: half the level
    make_frames(frames, 0, (1 << 23) / 16.0, 0);
    audio_feed_i2s(&audio, frames);
    CHECK_NEAR(audio.audio_sample_buffer[0], 0.25, 1e-6);

    // Negative samples keep their sign through the 24-bit alignment
    make_frames(frames, 0, -(1 << 23) / 16.0, -(1 << 23) / 16.0);
    audio_feed_i2s(&audio, frames);
    CHECK_NEAR(audio.audio_sample_buffer[0], -0.5, 1e-6);

    audio_deinit(&audio);
}

static void test_tone_lands_in_its_bin(size_t k0) {
    int32_t frames[2 * N];
    audio_t audio;

    CHECK(audio_init(&audio, N));
    CHECK(audio_get_frequency_bin_count(&audio) == N / 2);

    make_frames(frames, k0, (1 << 23) / 16.0, (1 << 23) / 16.0);
    audio_feed_i2s(&audio, frames);
    audio_envelope(&audio);
    audio_fft(&audio);

    CHECK(peak_bin(audio_get_frequency_bins(&audio), N / 2) == k0);

    audio_deinit(&audio);
}

// Opposite microphones cancel: proves the channels are summed
static void test_opposite_channels_cancel(void) {
    int32_t frames[2 * N];
    const float *bins;
    audio_t audio;

    CHECK(audio_init(&audio, N));

    make_frames(frames, 5, (1 << 23) / 16.0, -(1 << 23) / 16.0);
    audio_feed_i2s(&audio, frames);
    audio_envelope(&audio);
    audio_fft(&audio);

    bins = audio_get_frequency_bins(&audio);
    for (size_t k = 0; k < N / 2; k++)
        CHECK_NEAR(bins[k], 0, 1e-6);

    audio_deinit(&audio);
}

int main(void) {
    test_feed_scaling_and_sum();
    test_tone_lands_in_its_bin(3);
    test_tone_lands_in_its_bin(17);
    test_opposite_channels_cancel();

    return CHECK_REPORT();
}
