/**
 * Light Painting firmware: brings up the drivers and the visualizer, then
 * runs one analysis and one frame per hop of new audio, forever. See the
 * README for the architecture, app/config.h for the settings.
 */

#include "config.h"
#include "diagnostics.h"
#include "i2s.h"
#include "persist.h"
#include "settings.h"
#include "tuning_link.h"
#include "ui.h"
#include "visualizer.h"
#include "ws2812.h"

#ifdef TEST_SIGNAL
#include "song.h"
#endif

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

int main(void) {
    visualizer_t visualizer;
    visualizer_tuning_t tuning;
    settings_t settings;
#ifdef TEST_SIGNAL
    // The song's hop, in place of the microphones'
    static int32_t song_frames[AUDIO_HOP_SIZE * AUDIO_WORDS_PER_FRAME];
    song_t song;
#endif

    // First: its unused stack is filled, for the diagnostics to see how
    // much of it gets used
    diagnostics_fill_core0_stack();

    stdio_usb_init();

    if (!init_step(i2s_init(AUDIO_HOP_SIZE * AUDIO_WORDS_PER_FRAME, MIC_SCK_PIN,
                            MIC_WS_PIN, MIC_DATA_PIN),
                   "INMP441 i2s driver"))
        return EXIT_FAILURE;

    printf("Sample rate %.3f Hz\n", i2s_sample_rate());

#ifdef TEST_SIGNAL
    if (!init_step(song_init(&song, i2s_sample_rate(), 1), "Test signal"))
        return EXIT_FAILURE;
#endif

    if (!init_step(ws2812_init(LED_COUNT, LED_DATA_PIN), "WS2812 driver"))
        return EXIT_FAILURE;

    // After i2s_init(): the sample rate comes from the I2S clock
    if (!init_step(visualizer_init(&visualizer,
                                   &(visualizer_config_t){
                                       .sample_rate = i2s_sample_rate(),
                                       .fft_size = AUDIO_FFT_SIZE,
                                       .hop_size = AUDIO_HOP_SIZE,
                                       .led_count = LED_COUNT,
                                       .bend_count = LED_BEND_COUNT,
                                       .seed = VISUALIZER_SEED,
                                   }),
                   "Visualizer"))
        return EXIT_FAILURE;

    // The settings saved last, or the defaults, applied to the visualizer.
    // Without them the lights still run, on the defaults
    if (init_step(persist_init(), "Settings storage"))
        persist_load(&settings);
    else
        settings_reset(&settings);
    tuning = settings_tuning(&settings);
    visualizer_tune(&visualizer, &tuning);

    if (!init_step(tuning_link_init(), "Tuning link"))
        return EXIT_FAILURE;

    // Without them the lights still run
    init_step(diagnostics_init(), "Diagnostics");

    // The menu, on core 1. The lights do not wait for it
    ui_start(&settings);

    i2s_start_sampling();
    ws2812_start_transmission();

    printf("Started sampling\n");

    while (true) {
        const visualizer_tuning_t *newest;
        const int32_t *frames;
        const sound_t *sound;

        // Wait for audio we have not processed yet
        frames = i2s_wait_buffer();
#ifdef TEST_SIGNAL
        // Made before the work is timed: its cost is not counted
        song_fill(&song, song_frames, AUDIO_HOP_SIZE);
        frames = song_frames;
#endif
        diagnostics_start_work();

        // What the menu changed since the last hop, if anything
        if ((newest = tuning_link_take()) != nullptr)
            visualizer_tune(&visualizer, newest);

        visualizer_analyze(&visualizer, frames);
        sound = visualizer_render(&visualizer, ws2812_frame());
        ws2812_submit();

        diagnostics_end_hop(&visualizer, frames, sound);
    }

    return EXIT_SUCCESS;
}
