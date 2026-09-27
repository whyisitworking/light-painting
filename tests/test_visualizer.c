#include "check.h"
#include "color.h"
#include "signals.h"
#include "visualizer.h"

#define FFT_SIZE 512
#define HOP 256
#define FS 48828.125
#define LEDS 300

static int32_t frames[2 * HOP];
static uint32_t pixels[LEDS];

static visualizer_config_t config(effects_mode_t mode) {
    return (visualizer_config_t){
        .sample_rate = (float)FS,
        .fft_size = FFT_SIZE,
        .hop = HOP,
        .led_count = LEDS,
        .gain = 1.5f,
        .mode = mode,
        .palette = PALETTE_RAINBOW,
        .seed = 1,
    };
}

static unsigned brightness(uint32_t pixel) {
    color_ws2812_t color = {.value = pixel};
    return color.grba.r + color.grba.g + color.grba.b;
}

// A tone of a 24-bit amplitude, 0 for silence, continuous across hops
static void tone_hop(size_t hop, double hz, double amplitude) {
    for (size_t i = 0; i < HOP; i++)
        i2s_put_mono(frames, i,
                     amplitude * sin(2.0 * M_PI * hz *
                                     (double)(hop * HOP + i) / FS));
}

// 55 Hz kicks decaying over 40 ms, one per period
static void kick_hop(size_t hop, double period_s) {
    for (size_t i = 0; i < HOP; i++) {
        double t = fmod((double)(hop * HOP + i) / FS, period_s);

        i2s_put_mono(frames, i,
                     600000.0 * exp(-t / 0.04) * sin(2.0 * M_PI * 55.0 * t));
    }
}

static void test_rejects_invalid(void) {
    visualizer_t visualizer;
    visualizer_config_t bad;

    bad = config(EFFECTS_SPECTRUM);
    bad.sample_rate = 0.f;
    CHECK(!visualizer_init(&visualizer, &bad));

    bad = config(EFFECTS_SPECTRUM);
    bad.fft_size = 100;
    CHECK(!visualizer_init(&visualizer, &bad));

    bad = config(EFFECTS_SPECTRUM);
    bad.hop = FFT_SIZE + 1;
    CHECK(!visualizer_init(&visualizer, &bad));

    // Fails late, in the effects, after the spectrum and features are set up
    bad = config(EFFECTS_SPECTRUM);
    bad.led_count = 1;
    CHECK(!visualizer_init(&visualizer, &bad));
}

static void test_silence_is_dark(void) {
    visualizer_t visualizer;
    visualizer_config_t silent = config(EFFECTS_SPECTRUM);
    bool dark = true;

    CHECK(visualizer_init(&visualizer, &silent));

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

// A 1 kHz tone lights the spectrum where its band is, not the ends
static void test_tone_lights_its_position(void) {
    visualizer_t visualizer;
    visualizer_config_t spectrum = config(EFFECTS_SPECTRUM);
    size_t band = 0, led;

    CHECK(visualizer_init(&visualizer, &spectrum));

    for (size_t hop = 0; hop < 200; hop++) {
        tone_hop(hop, 1000.0, 200000.0);
        visualizer_analyze(&visualizer, frames);
        visualizer_render(&visualizer, pixels);
    }

    while (band + 1 < FEATURES_BAND_COUNT &&
           visualizer.features.edges[band + 1] <= 1000.f)
        band++;
    led = band * (LEDS - 1) / (FEATURES_BAND_COUNT - 1);

    CHECK(brightness(pixels[led]) > 0);
    CHECK(pixels[0] == 0);
    CHECK(pixels[LEDS - 1] == 0);

    visualizer_deinit(&visualizer);
}

// 120 BPM kicks, from I2S words on: one beat per kick
static void test_kicks_give_beats(void) {
    visualizer_t visualizer;
    visualizer_config_t ripples = config(EFFECTS_RIPPLES);
    const size_t hops = (size_t)(4.0 * FS / HOP);
    unsigned beats = 0;

    CHECK(visualizer_init(&visualizer, &ripples));

    for (size_t hop = 0; hop < hops; hop++) {
        kick_hop(hop, 0.5);
        visualizer_analyze(&visualizer, frames);
        beats += visualizer_render(&visualizer, pixels)->beat;
    }

    // Kicks at 0, 0.5 .. 3.5 s
    CHECK(beats == 8);

    visualizer_deinit(&visualizer);
}

int main(void) {
    test_rejects_invalid();
    test_silence_is_dark();
    test_tone_lights_its_position();
    test_kicks_give_beats();

    return CHECK_REPORT();
}
