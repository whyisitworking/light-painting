#include "async.h"
#include "audio.h"
#include "effects.h"
#include "features.h"
#include "i2s.h"
#include "neopixel.h"
#include "swapchain.h"

#include <pico/stdlib.h>
#include <pico/types.h>
#include <stdio.h>
#include <stdlib.h>

// Each analysis covers the last AUDIO_FFT_SIZE mono samples and runs every
// AUDIO_FFT_HOP new ones. At fs = 48828 Hz, 512 / 256:
//   window 10.5 ms, bin width fs / size = 95 Hz, a new analysis every 5.2 ms
// Larger sizes resolve lower frequencies, smaller ones react faster
#define AUDIO_FFT_SIZE 512
#define AUDIO_FFT_HOP 256

// One mono sample per stereo frame: a left and a right word
#define AUDIO_WORDS_PER_FRAME 2

_Static_assert(AUDIO_FFT_SIZE >= 4 &&
                   (AUDIO_FFT_SIZE & (AUDIO_FFT_SIZE - 1)) == 0,
               "AUDIO_FFT_SIZE must be a power of two >= 4");
_Static_assert(AUDIO_FFT_HOP >= 1 && AUDIO_FFT_HOP <= AUDIO_FFT_SIZE,
               "AUDIO_FFT_HOP must be between 1 and AUDIO_FFT_SIZE");
// The audio DMA streams into a hardware ring of two hops: a power of two, at
// most 32 KB
_Static_assert((AUDIO_FFT_HOP & (AUDIO_FFT_HOP - 1)) == 0 &&
                   AUDIO_FFT_HOP <= 2048,
               "AUDIO_FFT_HOP must be a power of two, at most 2048");
#define LED_COUNT 300

// Pico 2 header pins, SCK and WS must be consecutive
#define MIC_SCK_PIN 26
#define MIC_WS_PIN 27
#define MIC_DATA_PIN 28

#define LED_DATA_PIN 8

// Visualizer look, see effects/effects.h and effects/palette.h
#define VISUALIZER_MODE EFFECTS_RIVER
#define VISUALIZER_PALETTE PALETTE_SYNTHWAVE

#ifdef PERF_STATS
// Timing of one main loop stage, reported and reset about once per second
typedef struct {
    uint32_t count;
    uint32_t total_us;
    uint32_t max_us;
} perf_stat_t;

static void perf_add(perf_stat_t *stat, uint32_t us) {
    stat->count++;
    stat->total_us += us;

    if (us > stat->max_us)
        stat->max_us = us;
}

static void perf_print(const char *name, perf_stat_t *stat) {
    printf("%-7s avg %5lu us, max %5lu us\n", name,
           stat->count ? stat->total_us / stat->count : 0, stat->max_us);
    *stat = (perf_stat_t){0};
}
#endif

int main() {
    audio_t audio;
    features_state_t features;
    effects_t effects;
    float hop_seconds;
    swapchain_t audio_swapchain;
    swapchain_t led_swapchain;

    stdio_usb_init();

    if (!swapchain_init(&audio_swapchain,
                        i2s_required_buffer_size(AUDIO_FFT_HOP *
                                                 AUDIO_WORDS_PER_FRAME))) {
        printf("Could not initialize audio swapchain\n");
        return EXIT_FAILURE;
    }

    printf("Audio swapchain init!\n");

    if (!swapchain_init(&led_swapchain,
                        neopixel_required_buffer_size(LED_COUNT))) {
        printf("Could not initialize LED swapchain\n");
        return EXIT_FAILURE;
    }

    printf("LED swapchain init!\n");

    if (!i2s_init(&audio_swapchain, AUDIO_FFT_HOP * AUDIO_WORDS_PER_FRAME,
                  MIC_SCK_PIN, MIC_WS_PIN, MIC_DATA_PIN)) {
        printf("Could not initialize i2s driver\n");
        return EXIT_FAILURE;
    }

    printf("INMP init! Sample rate %.3f Hz\n", i2s_sample_rate());

    if (!neopixel_init(&led_swapchain, LED_COUNT, LED_DATA_PIN)) {
        printf("Could not initialize WS2812 driver\n");
        return EXIT_FAILURE;
    }

    printf("WS2812 init!\n");

    if (!audio_init(&audio, AUDIO_FFT_SIZE, AUDIO_FFT_HOP)) {
        printf("Could not initialize audio\n");
        return EXIT_FAILURE;
    }

    printf("Audio init!\n");

    hop_seconds = AUDIO_FFT_HOP / i2s_sample_rate();

    if (!features_init(&features, audio_get_frequency_bin_count(&audio),
                       i2s_sample_rate() / AUDIO_FFT_SIZE, hop_seconds)) {
        printf("Could not initialize features\n");
        return EXIT_FAILURE;
    }

    if (!effects_init(&effects, LED_COUNT, FEATURES_BAND_COUNT, hop_seconds,
                      1)) {
        printf("Could not initialize effects\n");
        return EXIT_FAILURE;
    }

    effects_set_mode(&effects, VISUALIZER_MODE);
    effects_set_palette(&effects, VISUALIZER_PALETTE);

    printf("Visualizer init!\n");

    i2s_start_sampling();
    neopixel_start_transmission();

    printf("Started sampling\n");

#ifdef PERF_STATS
    perf_stat_t perf_wait = {0}, perf_feed = {0}, perf_fft = {0},
                perf_render = {0};
    size_t audio_dropped = 0, led_dropped = 0;
    uint32_t perf_beats = 0;
    uint32_t perf_report_us = time_us_32();
    uint32_t t0, t1, t2, t3, t4;
#endif

    while (true) {
        const features_t *sound;
        bool fresh_audio;

#ifdef PERF_STATS
        t0 = time_us_32();
#endif

        // Wait for audio we have not processed yet
        do {
            synchronized(fresh_audio =
                             swapchain_consumer_swap(&audio_swapchain));
        } while (!fresh_audio);

#ifdef PERF_STATS
        t1 = time_us_32();
#endif

        audio_feed_i2s(&audio, swapchain_consumer_buffer(&audio_swapchain));
        audio_envelope(&audio);
        audio_gain(&audio, 1.5f);

#ifdef PERF_STATS
        t2 = time_us_32();
#endif

        audio_fft(&audio);

#ifdef PERF_STATS
        t3 = time_us_32();
#endif

        sound = features_update(&features, audio_get_frequency_bins(&audio));
        effects_render(&effects, sound,
                       swapchain_producer_buffer(&led_swapchain));

        synchronized(swapchain_producer_swap(&led_swapchain));
        neopixel_frame_ready();

#ifdef PERF_STATS
        t4 = time_us_32();

        perf_add(&perf_wait, t1 - t0);
        perf_add(&perf_feed, t2 - t1);
        perf_add(&perf_fft, t3 - t2);
        perf_add(&perf_render, t4 - t3);

        if (sound->beat)
            perf_beats++;

        // Report outside the measured stages, printing takes a while
        if (t4 - perf_report_us >= 1000000) {
            perf_report_us = t4;

            perf_print("wait", &perf_wait);
            perf_print("feed", &perf_feed);
            perf_print("fft", &perf_fft);
            perf_print("render", &perf_render);
            printf("Audio buffers dropped %zu, LED frames dropped %zu\n",
                   audio_swapchain.dropped - audio_dropped,
                   led_swapchain.dropped - led_dropped);
            audio_dropped = audio_swapchain.dropped;
            led_dropped = led_swapchain.dropped;
            // To tune FEATURES_MIN_CEILING_DB: a quiet room should read
            // loudness ~0 and no beats
            printf("Ceiling %.1f dB, loudness %.3f, beats %lu\n",
                   (double)features.ceiling_db, (double)sound->loudness,
                   (unsigned long)perf_beats);
            perf_beats = 0;
            i2s_print_irq_hits();
            neopixel_print_irq_hits();
        }
#endif
    }

    return EXIT_SUCCESS;
}
