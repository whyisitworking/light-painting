#include "wav.h"

#include <stdlib.h>
#include <string.h>

constexpr uint16_t FORMAT_PCM = 1;
constexpr uint16_t FORMAT_FLOAT = 3;
constexpr uint16_t FORMAT_EXTENSIBLE = 0xFFFE;

static uint16_t u16(const uint8_t *at) { return (uint16_t)(at[0] | at[1] << 8); }

static uint32_t u32(const uint8_t *at) {
    return u16(at) | (uint32_t)u16(at + 2) << 16;
}

// One sample of the given format and width, little endian, as -1 to 1
static float sample_at(const uint8_t *at, uint16_t format, uint16_t bits) {
    if (format == FORMAT_FLOAT) {
        float value;
        uint32_t word = u32(at);

        memcpy(&value, &word, sizeof(value));
        return value;
    }

    switch (bits) {
    case 16:
        return (float)(int16_t)u16(at) / 32768.f;
    case 24:
        // Sign extended from the top byte
        return (float)((int32_t)((uint32_t)at[0] << 8 | (uint32_t)at[1] << 16 |
                                 (uint32_t)at[2] << 24) >>
                       8) /
               8388608.f;
    default:
        return (float)(int32_t)u32(at) / 2147483648.f;
    }
}

bool wav_read(wav_t *this, const uint8_t *bytes, size_t size,
              const char **error) {
    const uint8_t *data = nullptr;
    size_t at = 12, data_size = 0, frame_size;
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;

    *this = (wav_t){};

    if (size < 12 || memcmp(bytes, "RIFF", 4) != 0 ||
        memcmp(bytes + 8, "WAVE", 4) != 0) {
        *error = "not a WAV file";
        return false;
    }

    // Chunks: an id, a size, the bytes, padded to an even size
    while (at + 8 <= size) {
        const uint8_t *chunk = bytes + at;
        size_t chunk_size = u32(chunk + 4);

        if (chunk_size > size - at - 8)
            chunk_size = size - at - 8;

        if (memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
            format = u16(chunk + 8);
            channels = u16(chunk + 10);
            rate = u32(chunk + 12);
            bits = u16(chunk + 22);
            // The real format is the first two bytes of the sub-format
            if (format == FORMAT_EXTENSIBLE && chunk_size >= 26)
                format = u16(chunk + 32);
        } else if (memcmp(chunk, "data", 4) == 0) {
            data = chunk + 8;
            data_size = chunk_size;
        }

        at += 8 + chunk_size + (chunk_size & 1);
    }

    if (data == nullptr || channels == 0 || rate == 0) {
        *error = "no format or no data";
        return false;
    }
    if (!(format == FORMAT_PCM && (bits == 16 || bits == 24 || bits == 32)) &&
        !(format == FORMAT_FLOAT && bits == 32)) {
        *error = "not 16, 24 or 32 bit PCM, nor 32 bit float";
        return false;
    }

    frame_size = (size_t)channels * bits / 8;
    this->count = data_size / frame_size;
    this->sample_rate = (float)rate;
    this->samples = (float *)malloc((this->count > 0 ? this->count : 1) *
                                    sizeof(float));
    if (this->samples == nullptr) {
        *error = "out of memory";
        return false;
    }

    for (size_t n = 0; n < this->count; n++) {
        float sum = 0.f;

        for (uint16_t c = 0; c < channels; c++)
            sum += sample_at(data + n * frame_size + c * bits / 8, format, bits);
        this->samples[n] = sum / (float)channels;
    }

    return true;
}

void wav_free(wav_t *this) {
    free(this->samples);
    *this = (wav_t){};
}
