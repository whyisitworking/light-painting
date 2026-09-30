/**
 * Golden snapshot of the whole pipeline: I2S words through the visualizer
 * (spectrum, features and effects) into pixels, for every mode. Each mode's
 * frames are hashed and compared with hashes recorded from a known good
 * build.
 *
 * A change of hash means the pixels changed. That is expected only when a
 * change is meant to alter the look (tuning, a new colour step...): then
 * check the other tests still pass and record the new hashes printed by
 *   test_golden --print
 * The hashes depend on the host's libm (sinf, expf, powf...), so another
 * platform may need its own recording too.
 */

#include "check.h"
#include "signals.h"
#include "visualizer.h"

#include <stdio.h>
#include <string.h>

constexpr size_t FFT_SIZE = 512;
constexpr size_t HOP_SIZE = 256;
constexpr double FS = 48828.125;
constexpr size_t LEDS = 300;
constexpr float GAIN = 1.5f;
constexpr size_t HOPS = 400;

// Recorded hashes, in effects_mode_t order
// Hashes are recorded after the tuning pass with the user: the modes from
// EFFECTS_MODE_POND on are run (they must not crash) but not compared yet
constexpr int RECORDED = 6;

static const uint32_t expected[RECORDED] = {
    0xbf4999feu, 0xd6fb6394u, 0xa1887bdcu,
    0x1144afbbu, 0x0be60e20u, 0x5ac33004u,
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
    for (size_t i = 0; i < HOP_SIZE; i++) {
        double t = (double)(hop * HOP_SIZE + i) / FS;
        double beat_t = fmod(t, 0.5);
        double swell = 0.5 + 0.5 * sin(2.0 * M_PI * 0.7 * t);
        double sample = 20000.0 * signal_noise(noise) +
                        150000.0 * swell * sin(2.0 * M_PI * 1000.0 * t) +
                        600000.0 * exp(-beat_t / 0.04) *
                            sin(2.0 * M_PI * 55.0 * beat_t);

        signal_put_mono(frames, i, sample);
    }
}

static uint32_t run_mode(effects_mode_t mode) {
    static int32_t frames[2 * HOP_SIZE];
    static uint32_t pixels[LEDS];
    visualizer_t visualizer;
    visualizer_tuning_t tuning = visualizer_default_tuning();
    uint32_t noise = 12345, hash = 2166136261u;

    CHECK(visualizer_init(&visualizer,
                          &(visualizer_config_t){
                              .sample_rate = (float)FS,
                              .fft_size = FFT_SIZE,
                              .hop_size = HOP_SIZE,
                              .led_count = LEDS,
                              .seed = 1,
                          }));
    tuning.gain = GAIN;
    tuning.effects.mode = mode;
    tuning.effects.palette = (palette_t)(mode % PALETTE_COUNT);
    visualizer_tune(&visualizer, &tuning);

    for (size_t hop = 0; hop < HOPS; hop++) {
        const sound_t *sound;

        make_hop(frames, hop, &noise);
        visualizer_analyze(&visualizer, frames);
        sound = visualizer_render(&visualizer, pixels);

        hash = fnv1a(hash, pixels, sizeof(pixels));
        hash = fnv1a(hash, &sound->beat, sizeof(sound->beat));
    }

    visualizer_deinit(&visualizer);

    return hash;
}

int main(int argc, char **argv) {
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
        uint32_t hash = run_mode((effects_mode_t)mode);

        if (print)
            printf("    0x%08xu,\n", (unsigned)hash);
        else if (mode < RECORDED && hash != expected[mode]) {
            check_failures++;
            printf("mode %d: hash 0x%08x, expected 0x%08x\n", mode,
                   (unsigned)hash, (unsigned)expected[mode]);
        }
    }

    return CHECK_REPORT();
}
