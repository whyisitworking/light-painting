/**
 * Golden snapshot of the whole pipeline: I2S words through audio, features
 * and effects into pixels, for every mode. Each mode's frames are hashed and
 * compared with hashes recorded from a known good build.
 *
 * A change of hash means the pixels changed. That is expected only when a
 * change is meant to alter the look (tuning, a new colour step...): then
 * check the other tests still pass and record the new hashes printed by
 *   test_golden --print
 * The hashes depend on the host's libm (sinf, expf, powf...), so another
 * platform may need its own recording too.
 */

#include "audio.h"
#include "check.h"
#include "effects.h"
#include "features.h"
#include "signals.h"

#include <stdio.h>
#include <string.h>

#define FFT_SIZE 512
#define HOP 256
#define FS 48828.125
#define LEDS 300
#define GAIN 1.5f
#define HOPS 400

// Recorded hashes, in effects_mode_t order
static const uint32_t expected[EFFECTS_MODE_COUNT] = {
    0x5048769cu, 0x26df914cu, 0x137d2edcu,
    0x03413ae8u, 0x6a6784f4u, 0x648a73d4u,
};

static uint32_t fnv1a(uint32_t hash, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

// Noise, a 1 kHz tone swelling and fading, and 120 BPM 55 Hz kicks
static void make_hop(int32_t *frames, size_t hop, uint32_t *noise) {
    for (size_t i = 0; i < HOP; i++) {
        double t = (double)(hop * HOP + i) / FS;
        double beat_t = fmod(t, 0.5);
        double swell = 0.5 + 0.5 * sin(2.0 * M_PI * 0.7 * t);
        double sample = 20000.0 * signal_noise(noise) +
                        150000.0 * swell * sin(2.0 * M_PI * 1000.0 * t) +
                        600000.0 * exp(-beat_t / 0.04) *
                            sin(2.0 * M_PI * 55.0 * beat_t);

        i2s_put_mono(frames, i, sample);
    }
}

static uint32_t run_mode(effects_mode_t mode) {
    static int32_t frames[2 * HOP];
    static uint32_t pixels[LEDS];
    float hop_seconds = (float)(HOP / FS);
    audio_t audio;
    features_state_t features;
    effects_t effects;
    uint32_t noise = 12345, hash = 2166136261u;

    CHECK(audio_init(&audio, FFT_SIZE, HOP));
    CHECK(features_init(&features, audio_get_frequency_bin_count(&audio),
                        (float)(FS / FFT_SIZE), hop_seconds));
    CHECK(effects_init(&effects, LEDS, FEATURES_BAND_COUNT, hop_seconds, 1));
    effects_set_mode(&effects, mode);
    effects_set_palette(&effects, (effects_palette_t)(mode % PALETTE_COUNT));

    for (size_t hop = 0; hop < HOPS; hop++) {
        const features_t *sound;

        make_hop(frames, hop, &noise);
        audio_feed_i2s(&audio, frames);
        audio_envelope(&audio);
        audio_gain(&audio, GAIN);
        audio_fft(&audio);
        sound = features_update(&features, audio_get_frequency_bins(&audio));
        effects_render(&effects, sound, pixels);

        hash = fnv1a(hash, pixels, sizeof(pixels));
        hash = fnv1a(hash, &sound->beat, sizeof(sound->beat));
    }

    effects_deinit(&effects);
    features_deinit(&features);
    audio_deinit(&audio);

    return hash;
}

int main(int argc, char **argv) {
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
        uint32_t hash = run_mode((effects_mode_t)mode);

        if (print)
            printf("    0x%08xu,\n", (unsigned)hash);
        else if (hash != expected[mode]) {
            check_failures++;
            printf("mode %d: hash 0x%08x, expected 0x%08x\n", mode,
                   (unsigned)hash, (unsigned)expected[mode]);
        }
    }

    return CHECK_REPORT();
}
