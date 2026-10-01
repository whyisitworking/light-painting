#ifndef WAV_H
#define WAV_H

/**
 * Reads a WAV file's samples from memory: PCM of 16, 24 or 32 bits, or 32
 * bit float, any sample rate, any number of channels (mixed to mono). Enough
 * for what afconvert or ffmpeg write, e.g.
 *   afconvert -f WAVE -d LEI16 song.mp3 song.wav
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    float sample_rate;
    // Mono, -1 to 1, count of them. Allocated by wav_read(), see wav_free()
    float *samples;
    size_t count;
} wav_t;

/**
 * Parses size bytes of a WAV file. False, with a reason in *error, if it is
 * not one it can read or memory runs out
 */
[[nodiscard]] bool wav_read(wav_t *this, const uint8_t *bytes, size_t size,
                            const char **error);

void wav_free(wav_t *this);

#endif
