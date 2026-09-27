/**
 * Light Painting firmware: brings up the drivers and the visualizer, then
 * runs one analysis and one frame per hop of new audio, forever. See the
 * README for the architecture, app/config.h for the settings.
 */

#include "config.h"
#include "i2s.h"
#include "perf.h"
#include "visualizer.h"
#include "ws2812.h"

#include <pico/stdlib.h>
#include <stdio.h>
#include <stdlib.h>

// Reports a startup step over USB, returns whether it succeeded
static bool init_step(bool ok, const char *what) {
    if (ok)
        printf("%s init!\n", what);
    else
        printf("Could not initialize %s\n", what);

    return ok;
}

int main() {
    visualizer_t visualizer;

    stdio_usb_init();

    if (!init_step(i2s_init(AUDIO_FFT_HOP * AUDIO_WORDS_PER_FRAME,
                            MIC_SCK_PIN, MIC_WS_PIN, MIC_DATA_PIN),
                   "INMP441 i2s driver"))
        return EXIT_FAILURE;

    printf("Sample rate %.3f Hz\n", i2s_sample_rate());

    if (!init_step(ws2812_init(LED_COUNT, LED_DATA_PIN), "WS2812 driver"))
        return EXIT_FAILURE;

    // After i2s_init(): the sample rate comes from the I2S clock
    if (!init_step(visualizer_init(&visualizer,
                                   &(visualizer_config_t){
                                       .sample_rate = i2s_sample_rate(),
                                       .fft_size = AUDIO_FFT_SIZE,
                                       .hop = AUDIO_FFT_HOP,
                                       .led_count = LED_COUNT,
                                       .gain = VISUALIZER_GAIN,
                                       .mode = VISUALIZER_MODE,
                                       .palette = VISUALIZER_PALETTE,
                                       .seed = VISUALIZER_SEED,
                                   }),
                   "Visualizer"))
        return EXIT_FAILURE;

    i2s_start_sampling();
    ws2812_start_transmission();

    printf("Started sampling\n");

    perf_init();

    while (true) {
        const int32_t *frames;
        const sound_t *sound;

        perf_begin();

        // Wait for audio we have not processed yet
        frames = i2s_wait_buffer();
        perf_lap(PERF_WAIT);

        visualizer_analyze(&visualizer, frames);
        perf_lap(PERF_ANALYZE);

        sound = visualizer_render(&visualizer, ws2812_frame());
        ws2812_submit();
        perf_lap(PERF_RENDER);

        perf_end(&visualizer, sound);
    }

    return EXIT_SUCCESS;
}
