#include "check.h"
#include "preview_api.h"
#include "signals.h"
#include "visualizer.h"

#include <math.h>
#include <string.h>

constexpr float FS = 48828.125f;
constexpr size_t HOP = PREVIEW_HOP_SIZE;
constexpr size_t LEDS = PREVIEW_LED_COUNT;
constexpr size_t HOPS = 400;

// The golden test's signal as 24-bit integers: noise, a swelling 1 kHz tone
// and 120 BPM kicks
static int32_t sample_at(size_t n, uint32_t *noise) {
    double t = (double)n / (double)FS, beat_t = fmod(t, 0.5);
    double swell = 0.5 + 0.5 * sin(2.0 * M_PI * 0.7 * t);
    double sample = 20000.0 * signal_noise(noise) +
                    150000.0 * swell * sin(2.0 * M_PI * 1000.0 * t) +
                    600000.0 * exp(-beat_t / 0.04) *
                        sin(2.0 * M_PI * 55.0 * beat_t);

    return (int32_t)lround(sample);
}

// A hop of the signal into the engine's input, and as I2S frames for a
// visualizer. The float is the sample over 2^23 - 1, which the engine scales
// back to the same integer
static void make_hop(size_t hop, uint32_t *noise, int32_t *frames) {
    float *input = preview_input();

    for (size_t i = 0; i < HOP; i++) {
        int32_t sample = sample_at(hop * HOP + i, noise);

        input[i] = (float)sample / 8388607.f;
        frames[2 * i] = signal_i2s_word(sample);
        frames[2 * i + 1] = signal_i2s_word(sample);
    }
}

static void start_visualizer(visualizer_t *visualizer) {
    CHECK(visualizer_init(visualizer,
                          &(visualizer_config_t){
                              .sample_rate = FS,
                              .fft_size = PREVIEW_FFT_SIZE,
                              .hop_size = HOP,
                              .led_count = LEDS,
                              .seed = 1,
                          }));
}

// With the default settings and the gallery on, each mode's pixels are what
// a visualizer on that mode renders, hop by hop
static void test_matches_the_visualizer(void) {
    static visualizer_t visualizers[EFFECTS_MODE_COUNT];
    static int32_t frames[2 * HOP];
    static uint32_t pixels[LEDS];
    uint32_t noise = 12345;
    size_t differing = 0;

    CHECK(preview_init(FS));
    preview_set_gallery(1);

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
        visualizer_tuning_t tuning = visualizer_default_tuning();

        start_visualizer(&visualizers[mode]);
        tuning.effects.mode = (effects_mode_t)mode;
        visualizer_tune(&visualizers[mode], &tuning);
    }

    for (size_t hop = 0; hop < HOPS; hop++) {
        make_hop(hop, &noise, frames);
        CHECK(preview_push((int)HOP) == 1);

        for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
            visualizer_analyze(&visualizers[mode], frames);
            visualizer_render(&visualizers[mode], pixels);
            differing += memcmp(pixels, preview_pixels(mode), sizeof(pixels)) != 0;
        }
    }

    CHECK(differing == 0);
    CHECK(preview_hops() == (int)HOPS);
    CHECK(preview_beats() > 0);

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++)
        visualizer_deinit(&visualizers[mode]);
    preview_deinit();
}

// A menu state set through the engine looks as it does through the firmware's
// own settings: several settings at once, the layers included
static void test_settings_look_as_on_the_board(void) {
    static const struct {
        settings_id_t id;
        int value;
    } changes[] = {
        {SETTINGS_MODE, EFFECTS_MODE_RIPPLES},
        {SETTINGS_PALETTE, PALETTE_FIRE},
        {SETTINGS_BRIGHTNESS, 50},
        {SETTINGS_GAIN, 25},
        {SETTINGS_BEAT_THRESHOLD, 20},
        {SETTINGS_TRAILS, 300},
        {SETTINGS_DIFFUSE, 40},
        {SETTINGS_SYMMETRY, 3},
    };
    static int32_t frames[2 * HOP];
    static uint32_t pixels[LEDS];
    settings_t settings;
    visualizer_t visualizer;
    visualizer_tuning_t tuning;
    uint32_t noise = 12345;
    size_t differing = 0;

    CHECK(preview_init(FS));
    settings_reset(&settings);

    for (size_t i = 0; i < sizeof(changes) / sizeof(changes[0]); i++) {
        settings_set(&settings, changes[i].id, changes[i].value);
        CHECK(preview_set(changes[i].id, changes[i].value) ==
              settings_get(&settings, changes[i].id));
    }

    start_visualizer(&visualizer);
    tuning = settings_tuning(&settings);
    visualizer_tune(&visualizer, &tuning);

    for (size_t hop = 0; hop < HOPS; hop++) {
        make_hop(hop, &noise, frames);
        CHECK(preview_push((int)HOP) == 1);

        visualizer_analyze(&visualizer, frames);
        visualizer_render(&visualizer, pixels);
        differing += memcmp(pixels, preview_pixels(EFFECTS_MODE_RIPPLES),
                            sizeof(pixels)) != 0;
    }

    CHECK(differing == 0);
    visualizer_deinit(&visualizer);
    preview_deinit();
}

// Without the gallery only the selected mode renders; selecting another one
// renders that one from then on
static void test_gallery_off_renders_the_selected_mode(void) {
    static int32_t frames[2 * HOP];
    uint32_t noise = 12345;
    bool others_dark = true, selected_lit = false, switched_lit = false;

    CHECK(preview_init(FS));
    preview_set(SETTINGS_MODE, EFFECTS_MODE_SPECTRUM);

    for (size_t hop = 0; hop < 200; hop++) {
        make_hop(hop, &noise, frames);
        preview_push((int)HOP);
    }

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++)
        for (size_t i = 0; i < LEDS; i++) {
            if (mode == EFFECTS_MODE_SPECTRUM)
                selected_lit = selected_lit || preview_pixels(mode)[i] != 0;
            else
                others_dark = others_dark && preview_pixels(mode)[i] == 0;
        }

    preview_set(SETTINGS_MODE, EFFECTS_MODE_GLOW);
    for (size_t hop = 200; hop < 400; hop++) {
        make_hop(hop, &noise, frames);
        preview_push((int)HOP);
    }
    for (size_t i = 0; i < LEDS; i++)
        switched_lit = switched_lit || preview_pixels(EFFECTS_MODE_GLOW)[i] != 0;

    CHECK(others_dark);
    CHECK(selected_lit);
    CHECK(switched_lit);
    preview_deinit();
}

static void test_push_counts_hops(void) {
    float *input;

    CHECK(preview_init(FS));
    input = preview_input();
    memset(input, 0, PREVIEW_INPUT_CAPACITY * sizeof(float));

    CHECK(preview_push(100) == 0);
    CHECK(preview_push(156) == 1);
    CHECK(preview_push(0) == 0);
    CHECK(preview_push(-5) == 0);
    // 1000 more: 3 hops, and 232 samples of the fourth waiting
    CHECK(preview_push(1000) == 3);
    // More than the buffer holds: the buffer's worth, (232 + 8192) / 256
    CHECK(preview_push((int)PREVIEW_INPUT_CAPACITY + 500) == 32);
    preview_deinit();
}

// A trim far below any level is silence
static void test_input_trim(void) {
    static int32_t frames[2 * HOP];
    uint32_t noise = 12345;
    bool dark = true;

    CHECK(preview_init(FS));
    preview_set_input_trim_db(-200.f);

    for (size_t hop = 0; hop < 200; hop++) {
        make_hop(hop, &noise, frames);
        preview_push((int)HOP);
    }

    for (size_t i = 0; i < LEDS; i++)
        dark = dark && preview_pixels(preview_get(SETTINGS_MODE))[i] == 0;
    CHECK(dark);
    CHECK(preview_loudness() == 0.f);
    preview_deinit();
}

static void test_settings_and_names(void) {
    const int16_t *range;

    CHECK(preview_init(FS));
    CHECK(preview_setting_count() == SETTINGS_ID_COUNT);

    range = preview_setting_range(SETTINGS_BRIGHTNESS);
    CHECK(range != nullptr);
    CHECK(range[0] == 10 && range[1] == 100 && range[2] == 5 &&
          range[3] == 100 && range[4] == 100 && range[5] == 0);
    CHECK(preview_setting_range(-1) == nullptr);
    CHECK(preview_setting_range(SETTINGS_ID_COUNT) == nullptr);

    // Snaps to the grid like the menu, and resets to the defaults
    CHECK(preview_set(SETTINGS_BRIGHTNESS, 47) == 45);
    CHECK(preview_get(SETTINGS_BRIGHTNESS) == 45);
    CHECK(preview_set(-1, 5) == 0);
    preview_reset();
    CHECK(preview_get(SETTINGS_BRIGHTNESS) == 100);

    CHECK(preview_mode_count() == EFFECTS_MODE_COUNT);
    CHECK(strcmp(preview_mode_name(EFFECTS_MODE_RIVER), "River") == 0);
    CHECK(preview_palette_count() == PALETTE_COUNT);
    CHECK(strcmp(preview_palette_name(PALETTE_SYNTHWAVE), "Synthwave") == 0);
    CHECK(preview_pixels(-1) == nullptr);
    CHECK(preview_pixels(EFFECTS_MODE_COUNT) == nullptr);
    CHECK(preview_bands() != nullptr);
    preview_deinit();
}

// Starting again starts over, at another sample rate too
static void test_reinit(void) {
    static int32_t frames[2 * HOP];
    uint32_t noise = 12345;

    CHECK(preview_init(FS));
    make_hop(0, &noise, frames);
    preview_push((int)HOP);
    CHECK(preview_hops() == 1);

    CHECK(preview_init(44100.f));
    CHECK(preview_hops() == 0);
    CHECK(preview_beats() == 0);
    CHECK(!preview_init(0.f));
    preview_deinit();
}

int main(void) {
    test_matches_the_visualizer();
    test_settings_look_as_on_the_board();
    test_gallery_off_renders_the_selected_mode();
    test_push_counts_hops();
    test_input_trim();
    test_settings_and_names();
    test_reinit();

    return CHECK_REPORT();
}
