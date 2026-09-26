#ifndef SIGNALS_H
#define SIGNALS_H

/**
 * Test signals as the I2S driver delivers them: stereo frames of 32-bit
 * words, left word first
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// The I2S word of a 24-bit sample: one delay bit, 24 data bits, 7 unused
static inline int32_t i2s_word(int32_t sample) {
    return (int32_t)((uint32_t)sample << 7);
}

// The same 24-bit sample on both microphones
static inline void i2s_put_mono(int32_t *frames, size_t frame, double sample) {
    int32_t word = i2s_word((int32_t)lround(sample));

    frames[2 * frame] = word;
    frames[2 * frame + 1] = word;
}

// xorshift32, -1 (exclusive) to 1 (inclusive)
static inline double signal_noise(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return (double)((*state >> 8) + 1) / 8388608.0 - 1.0;
}

#endif
