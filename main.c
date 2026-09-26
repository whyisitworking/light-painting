#include "async.h"
#include "audio.h"
#include "color.h"
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
#define LED_COUNT 300

// Pico 2 header pins, SCK and WS must be consecutive
#define MIC_SCK_PIN 26
#define MIC_WS_PIN 27
#define MIC_DATA_PIN 28

#define LED_DATA_PIN 8

static color_neopixel_t magnitude_to_color(float magnitude) {
    if (magnitude < 0.f) {
        magnitude = 0.f;
    }

    if (magnitude > 1.f) {
        magnitude = 1.f;
    }

    return color_neopixel_from_hsv_f(magnitude * 360.f, 1.f, 1.f);
}

static void visualizer_map_frequency_bins_to_pixels(const float *frequency_bins,
                                                    size_t frequency_bin_count,
                                                    uint32_t *pixel_buffer,
                                                    size_t pixel_count) {
    if (frequency_bin_count > pixel_count) {
        float pitch = (float)frequency_bin_count / pixel_count;

        for (size_t pixel = 0; pixel < pixel_count; pixel++) {
            float index = pixel * pitch;
            size_t index_i = (size_t)index;
            float index_f = index - index_i;

            pixel_buffer[pixel] =
                color_neopixel_add(
                    magnitude_to_color((1.f - index_f) *
                                       frequency_bins[index_i]),
                    magnitude_to_color(index_f * frequency_bins[index_i + 1]))
                    .value;
        }
    } else if (frequency_bin_count < pixel_count) {
        float pitch = (float)frequency_bin_count / pixel_count;

        for (size_t i = 0; i < pixel_count; i++) {
            size_t bin = i * pitch;
            pixel_buffer[i] = magnitude_to_color(frequency_bins[bin]).value;
        }
    } else {
        for (size_t i = 0; i < pixel_count; i++)
            pixel_buffer[i] = magnitude_to_color(frequency_bins[i]).value;
    }
}

int main() {
    audio_t audio;
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
        printf("Could not initialize i2s driver");
        return EXIT_FAILURE;
    }

    printf("INMP init!\n");

    if (!neopixel_init(&led_swapchain, LED_COUNT, LED_DATA_PIN)) {
        printf("Could not initialize WS2812 driver");
        return EXIT_FAILURE;
    }

    printf("WS2812 init!\n");

    if (!audio_init(&audio, AUDIO_FFT_SIZE, AUDIO_FFT_HOP)) {
        printf("Could not initialize audio");
        return EXIT_FAILURE;
    }

    printf("Audio init!\n");

    i2s_start_sampling();
    neopixel_start_transmission();

    printf("Started sampling\n");

    // Scratch buffers
    int32_t scratch_int[8];
    float scratch_float[8];

    uint32_t max = 0;

    while (true) {
        bool fresh_audio;

        // Wait for audio we have not processed yet
        do {
            synchronized(fresh_audio =
                             swapchain_consumer_swap(&audio_swapchain));
        } while (!fresh_audio);

        audio_feed_i2s(&audio, swapchain_consumer_buffer(&audio_swapchain));
        audio_envelope(&audio);
        audio_gain(&audio, 1.5f);
        audio_fft(&audio);

        visualizer_map_frequency_bins_to_pixels(
            audio_get_frequency_bins(&audio),
            audio_get_frequency_bin_count(&audio),
            swapchain_producer_buffer(&led_swapchain),
            neopixel_get_pixel_count());

        synchronized(swapchain_producer_swap(&led_swapchain));
        neopixel_frame_ready();

        // const int32_t *source =
        //     (const int32_t *)swapchain_consumer_buffer(&audio_swapchain);
        // uint32_t *dest = swapchain_producer_buffer(&led_swapchain);

        // for (size_t i = 0; i < 8; i++) {
        //     int32_t int_value = ((source[2 * i + 1]) << 1) >> 8;
        //     uint32_t abs_value = (uint32_t)abs(int_value);
        //     if (abs_value > max) {
        //         max = abs_value;
        //         printf("Highest %u\n", max);
        //     }
        //     // scratch_float[i] = (float)int_value;
        // }

        // printf("%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n", scratch_int[0],
        //        scratch_int[1], scratch_int[2], scratch_int[3],
        //        scratch_int[4], scratch_int[5], scratch_int[6],
        //        scratch_int[7]);

        // printf("%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n",
        //        scratch_float[0], scratch_float[1], scratch_float[2],
        //        scratch_float[3], scratch_float[4], scratch_float[5],
        //        scratch_float[6], scratch_float[7]);

        // sleep_ms(500);
    }

    return EXIT_SUCCESS;
}
