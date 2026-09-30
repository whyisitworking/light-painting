#include "check.h"
#include "color.h"
#include "signals.h"
#include "visualizer.h"

#include <string.h>

constexpr size_t FFT_SIZE = 512;
constexpr size_t HOP_SIZE = 256;
constexpr double FS = 48828.125;
constexpr size_t LEDS = 300;

static int32_t frames[2 * HOP_SIZE];
static uint32_t pixels[LEDS];

static visualizer_config_t config(void) {
    return (visualizer_config_t){
        .sample_rate = (float)FS,
        .fft_size = FFT_SIZE,
        .hop_size = HOP_SIZE,
        .led_count = LEDS,
        .seed = 1,
    };
}

// The defaults in a look
static visualizer_tuning_t tuning_for(show_look_t look) {
    visualizer_tuning_t tuning = visualizer_default_tuning();

    tuning.show.look = look;

    return tuning;
}

static bool start(visualizer_t *visualizer, show_look_t look) {
    visualizer_config_t settings = config();
    visualizer_tuning_t tuning = tuning_for(look);

    if (!visualizer_init(visualizer, &settings))
        return false;

    visualizer_tune(visualizer, &tuning);

    return true;
}

static unsigned brightness(uint32_t pixel) {
    color_ws2812_t color = {.value = pixel};
    return color.grba.r + color.grba.g + color.grba.b;
}

// A tone of a 24-bit amplitude, 0 for silence, continuous across hops
static void tone_hop(size_t hop, double hz, double amplitude) {
    for (size_t i = 0; i < HOP_SIZE; i++)
        signal_put_mono(frames, i,
                        amplitude * sin(2.0 * M_PI * hz *
                                        (double)(hop * HOP_SIZE + i) / FS));
}

// 55 Hz kicks decaying over 40 ms, one per period
static void kick_hop(size_t hop, double period_s) {
    for (size_t i = 0; i < HOP_SIZE; i++) {
        double t = fmod((double)(hop * HOP_SIZE + i) / FS, period_s);

        signal_put_mono(frames, i,
                        600000.0 * exp(-t / 0.04) * sin(2.0 * M_PI * 55.0 * t));
    }
}

static void test_rejects_invalid(void) {
    visualizer_t visualizer;
    visualizer_config_t bad;

    bad = config();
    bad.sample_rate = 0.f;
    CHECK(!visualizer_init(&visualizer, &bad));

    bad = config();
    bad.fft_size = 100;
    CHECK(!visualizer_init(&visualizer, &bad));

    bad = config();
    bad.hop_size = FFT_SIZE + 1;
    CHECK(!visualizer_init(&visualizer, &bad));

    // Fails late, in the show, after the spectrum and features are set up
    bad = config();
    bad.led_count = 1;
    CHECK(!visualizer_init(&visualizer, &bad));
}

static void test_silence_is_dark(void) {
    visualizer_t visualizer;
    bool dark = true;

    CHECK(start(&visualizer, SHOW_LOOK_STAGE));

    for (size_t hop = 0; hop < 200; hop++) {
        tone_hop(hop, 1000.0, 0.0);
        visualizer_analyze(&visualizer, frames);
        visualizer_render(&visualizer, pixels);
    }

    for (size_t i = 0; i < LEDS; i++)
        dark = dark && pixels[i] == 0;
    CHECK(dark);

    visualizer_deinit(&visualizer);
}

// On Stage a 1 kHz tone lights the bars where its band is, on both sides of
// the centre, brighter than the bars of silent bands next to the centre
static void test_tone_lights_its_position(void) {
    visualizer_t visualizer;
    size_t band = 0, distance, span;

    CHECK(start(&visualizer, SHOW_LOOK_STAGE));

    for (size_t hop = 0; hop < 200; hop++) {
        tone_hop(hop, 1000.0, 200000.0);
        visualizer_analyze(&visualizer, frames);
        visualizer_render(&visualizer, pixels);
    }

    while (band + 1 < FEATURES_BAND_COUNT &&
           visualizer.features.edges[band + 1] <= 1000.f)
        band++;
    // As look_stage.c places the bands: over the inner part of each half
    span = (size_t)((1.f - SHOW_STAGE_WING) * (float)(LEDS / 2));
    distance = band * span / (FEATURES_BAND_COUNT - 1);

    CHECK(brightness(pixels[LEDS / 2 + distance]) > 0);
    CHECK(brightness(pixels[(LEDS - 1) / 2 - distance]) > 0);
    CHECK(brightness(pixels[LEDS / 2 + distance]) >
          brightness(pixels[LEDS / 2]));

    visualizer_deinit(&visualizer);
}

// 120 BPM kicks, from I2S words on: one low hit per kick
static void test_kicks_give_low_hits(void) {
    visualizer_t visualizer;
    const size_t hops = (size_t)(4.0 * FS / HOP_SIZE);
    unsigned beats = 0;

    CHECK(start(&visualizer, SHOW_LOOK_PULSE));

    for (size_t hop = 0; hop < hops; hop++) {
        kick_hop(hop, 0.5);
        visualizer_analyze(&visualizer, frames);
        beats += visualizer_render(&visualizer, pixels)
                     ->hits[FEATURES_LOW]
                     .fired;
    }

    // Kicks at 0, 0.5 .. 3.5 s
    CHECK(beats == 8);

    visualizer_deinit(&visualizer);
}

/**
 * The same tuning again changes nothing, as the menu may publish it on any
 * hop: tuned on every hop, a visualizer draws exactly what it draws tuned
 * once, in every look. For the default look, tuned once is not tuned at
 * all: with nothing saved, the lights are what they were before the menu.
 * Kicks and a tone over noise, as test_golden
 */
static void test_default_tuning_changes_nothing(void) {
    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        visualizer_t plain, tuned;
        visualizer_config_t settings = config();
        visualizer_tuning_t tuning = visualizer_default_tuning();
        static uint32_t tuned_pixels[LEDS];
        uint32_t noise = 12345;
        size_t different = 0;

        tuning.show.look = (show_look_t)look;

        CHECK(visualizer_init(&plain, &settings));
        // The auto-gain would hide a wrong gain in these loud signals
        CHECK(plain.gain == tuning.gain);
        CHECK(visualizer_init(&tuned, &settings));
        if (look != SHOW_LOOK)
            visualizer_tune(&plain, &tuning);

        for (size_t hop = 0; hop < 400; hop++) {
            for (size_t i = 0; i < HOP_SIZE; i++) {
                double t = (double)(hop * HOP_SIZE + i) / FS;
                double beat_t = fmod(t, 0.5);

                signal_put_mono(frames, i,
                                20000.0 * signal_noise(&noise) +
                                    150000.0 * sin(2.0 * M_PI * 1000.0 * t) +
                                    600000.0 * exp(-beat_t / 0.04) *
                                        sin(2.0 * M_PI * 55.0 * beat_t));
            }

            visualizer_tune(&tuned, &tuning);
            visualizer_analyze(&plain, frames);
            visualizer_analyze(&tuned, frames);
            visualizer_render(&plain, pixels);
            visualizer_render(&tuned, tuned_pixels);
            different += memcmp(pixels, tuned_pixels, sizeof(pixels)) != 0;
        }

        CHECK(different == 0);

        visualizer_deinit(&plain);
        visualizer_deinit(&tuned);
    }
}

// The Gain reaches the features, so the song parts take their level before
// it: at init and on every tuning
static void test_gain_reaches_the_parts(void) {
    visualizer_config_t settings = config();
    visualizer_tuning_t tuning = visualizer_default_tuning();
    visualizer_t visualizer;

    CHECK(visualizer_init(&visualizer, &settings));
    CHECK(fabsf(visualizer.features.gain_db -
                20.f * log10f(VISUALIZER_GAIN)) < 1e-4f);

    tuning.gain = 3.f;
    visualizer_tune(&visualizer, &tuning);
    CHECK(fabsf(visualizer.features.gain_db - 20.f * log10f(3.f)) < 1e-4f);

    visualizer_deinit(&visualizer);
}

int main(void) {
    test_rejects_invalid();
    test_silence_is_dark();
    test_tone_lights_its_position();
    test_kicks_give_low_hits();
    test_default_tuning_changes_nothing();
    test_gain_reaches_the_parts();

    return CHECK_REPORT();
}
