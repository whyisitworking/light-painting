#include "check.h"
#include "wav.h"

#include <string.h>

static uint8_t file[256];

static void put16(size_t at, uint16_t value) {
    file[at] = (uint8_t)value;
    file[at + 1] = (uint8_t)(value >> 8);
}

static void put32(size_t at, uint32_t value) {
    put16(at, (uint16_t)value);
    put16(at + 2, (uint16_t)(value >> 16));
}

// A WAV header of this format, a "LIST" chunk to skip, and the data chunk's
// header; returns where the data starts
static size_t header(uint16_t format, uint16_t channels, uint32_t rate,
                     uint16_t bits, size_t data_size) {
    memset(file, 0, sizeof(file));
    memcpy(file, "RIFF", 4);
    put32(4, (uint32_t)(4 + 24 + 12 + 8 + data_size));
    memcpy(file + 8, "WAVE", 4);
    memcpy(file + 12, "fmt ", 4);
    put32(16, 16);
    put16(20, format);
    put16(22, channels);
    put32(24, rate);
    put32(28, rate * channels * bits / 8);
    put16(32, (uint16_t)(channels * bits / 8));
    put16(34, bits);
    // An odd sized chunk: padded to an even size
    memcpy(file + 36, "LIST", 4);
    put32(40, 3);
    memcpy(file + 48, "data", 4);
    put32(52, (uint32_t)data_size);

    return 56;
}

// 16 bit stereo: the channels mixed to mono
static void test_pcm16_stereo(void) {
    wav_t wav;
    const char *error = "";
    size_t at = header(1, 2, 44100, 16, 8);

    put16(at, 16384);
    put16(at + 2, 0);
    put16(at + 4, (uint16_t)-32768);
    put16(at + 6, (uint16_t)-32768);

    CHECK(wav_read(&wav, file, at + 8, &error));
    CHECK(wav.sample_rate == 44100.f);
    CHECK(wav.count == 2);
    CHECK_NEAR(wav.samples[0], 0.25, 1e-6);
    CHECK_NEAR(wav.samples[1], -1.0, 1e-6);
    wav_free(&wav);
}

// 24 bit mono, negative values sign extended
static void test_pcm24(void) {
    wav_t wav;
    const char *error = "";
    size_t at = header(1, 1, 48000, 24, 6);

    // 0x400000 is half of full scale; 0xC00000 minus half
    file[at] = 0x00, file[at + 1] = 0x00, file[at + 2] = 0x40;
    file[at + 3] = 0x00, file[at + 4] = 0x00, file[at + 5] = 0xC0;

    CHECK(wav_read(&wav, file, at + 6, &error));
    CHECK(wav.count == 2);
    CHECK_NEAR(wav.samples[0], 0.5, 1e-6);
    CHECK_NEAR(wav.samples[1], -0.5, 1e-6);
    wav_free(&wav);
}

static void test_float(void) {
    wav_t wav;
    const char *error = "";
    size_t at = header(3, 1, 48000, 32, 4);
    float value = -0.125f;
    uint32_t word;

    memcpy(&word, &value, sizeof(word));
    put32(at, word);

    CHECK(wav_read(&wav, file, at + 4, &error));
    CHECK(wav.count == 1);
    CHECK(wav.samples[0] == -0.125f);
    wav_free(&wav);
}

// Not a WAV, or a format it does not read: refused with a reason
static void test_rejects(void) {
    wav_t wav;
    const char *error = "";
    size_t at;

    CHECK(!wav_read(&wav, (const uint8_t *)"hello", 5, &error));
    CHECK(strlen(error) > 0);

    at = header(1, 1, 48000, 8, 4);
    CHECK(!wav_read(&wav, file, at + 4, &error));

    at = header(1, 1, 48000, 16, 4);
    memcpy(file + 48, "junk", 4);
    CHECK(!wav_read(&wav, file, at + 4, &error));
}

int main(void) {
    test_pcm16_stereo();
    test_pcm24();
    test_float();
    test_rejects();

    return CHECK_REPORT();
}
