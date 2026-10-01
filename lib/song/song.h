#ifndef SONG_H
#define SONG_H

/**
 * A synthetic song, looping: the same signal for the host tests of the song
 * parts and for the board's timing build (-DTEST_SIGNAL=ON), where it stands
 * in for the microphones. Cheap to make: oscillators are rotating phasors and
 * envelopes are multiplications, no sinf or expf per sample.
 *
 *   0 s   calm    a soft pad and a soft kick every second
 *   12 s  build   a snare roll speeding up, rising noise, the pad
 *   24 s  gap     silence
 *   25 s  drop    loud kicks at 120 BPM, a bass, a snare, hi-hats, the pad
 *   45 s  calm    as at the start, until the loop starts over at 48 s
 *
 * Levels are 24-bit sample amplitudes, as the INMP441 delivers them: the
 * same scale as tests/signals.h, where a kick of 600000 is loud
 */

#include <stddef.h>
#include <stdint.h>

// Where each section ends, in seconds, and the loop's length
constexpr float SONG_CALM_END_S = 12.f;
constexpr float SONG_BUILD_END_S = 24.f;
constexpr float SONG_GAP_END_S = 25.f;
constexpr float SONG_DROP_END_S = 45.f;
constexpr float SONG_LENGTH_S = 48.f;

// A sine as a rotating phasor: (c, s) turns by the step each sample
typedef struct {
    float c;
    float s;
    float step_c;
    float step_s;
} song_osc_t;

// A decaying sound restarted by each hit: its level and what a sample keeps
typedef struct {
    float level;
    float keep;
} song_env_t;

typedef struct {
    float sample_rate;
    // Samples since the start of the loop
    uint32_t sample;
    uint32_t loop_samples;
    // xorshift32, never 0
    uint32_t noise;

    song_osc_t pad_low;
    song_osc_t pad_high;
    song_osc_t bass;
    song_osc_t kick;
    song_osc_t snare_body;

    song_env_t kick_env;
    song_env_t snare_env;
    song_env_t hat_env;
} song_t;

// False if the sample rate is not positive. The seed drives the noise
[[nodiscard]] bool song_init(song_t *this, float sample_rate, uint32_t seed);

/**
 * The next count samples as stereo I2S frames (left and right word, the same
 * sample on both), as i2s_wait_buffer() delivers them: 2 * count words
 */
void song_fill(song_t *this, int32_t *frames, size_t count);

#endif
