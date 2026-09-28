#include "check.h"
#include "signals.h"
#include "stats.h"

#include <math.h>

constexpr size_t FRAMES = 256;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;
constexpr float PERIOD_S = 0.5f;

static int32_t frames[2 * FRAMES];
static float bands[FEATURES_BAND_COUNT];

// On the left, offset plus a sine of amplitude (of full scale), 64 samples
// a period so a hop holds whole periods. On the right, a constant
static void fill(double amplitude, double offset, int32_t right) {
    for (size_t i = 0; i < FRAMES; i++) {
        double left = offset + amplitude * 8388608.0 *
                                   sin(2.0 * M_PI * (double)i / 64.0);

        frames[2 * i] = signal_i2s_word((int32_t)lround(left));
        frames[2 * i + 1] = signal_i2s_word(right);
    }
}

static sound_t sound_of(float loudness, bool beat) {
    return (sound_t){.bands = bands, .loudness = loudness, .beat = beat};
}

static stats_report_t hops_of(stats_t *stats, size_t count) {
    sound_t sound = sound_of(0.f, false);

    for (size_t hop = 0; hop < count; hop++)
        stats_add_hop(stats, frames, FRAMES, &sound, 0);

    return stats_report(stats, PERIOD_S, HOP_PERIOD_S, -30.f, 0, 0);
}

// A sine peaking at 0.05 of full scale reads -26 dBFS, as the INMP441
// datasheet has 94 dB SPL. Nothing at all reads -INFINITY
static void test_levels(void) {
    stats_t stats;
    stats_report_t report;

    stats_reset(&stats);
    fill(0.05, 0.0, 0);
    report = hops_of(&stats, 10);

    CHECK_NEAR(report.left_dbfs, 20.0 * log10(0.05), 0.01);
    CHECK(isinf(report.right_dbfs) && report.right_dbfs < 0.f);
}

// An offset, as a microphone may have, is no sound: a steady one reads as
// nothing, and does not change the level of a sine on top of it
static void test_offset_is_no_sound(void) {
    stats_t stats;
    stats_report_t report;

    stats_reset(&stats);
    fill(0.05, 100000.0, 5000);
    report = hops_of(&stats, 10);

    CHECK_NEAR(report.left_dbfs, 20.0 * log10(0.05), 0.01);
    CHECK(isinf(report.right_dbfs) && report.right_dbfs < 0.f);
}

// Means, worsts and rates over the window, and what is passed through
static void test_window(void) {
    stats_t stats;
    stats_report_t report;

    stats_reset(&stats);
    fill(0.0, 0.0, 0);

    // Loudness 0.2 then 0.6, a beat on hops 0 and 5, work 1000..1900 us
    for (uint32_t hop = 0; hop < 10; hop++) {
        sound_t sound = sound_of(hop < 5 ? 0.2f : 0.6f, hop % 5 == 0);

        stats_add_hop(&stats, frames, FRAMES, &sound, 1000 + 100 * hop);
    }
    report = stats_report(&stats, PERIOD_S, HOP_PERIOD_S, -24.f, 3, 75);

    CHECK(report.period_s == PERIOD_S);
    CHECK_NEAR(report.loudness, 0.4, 1e-6);
    CHECK_NEAR(report.beats_per_s, 4.0, 1e-6);
    CHECK_NEAR(report.work_avg_us, 1450.0, 1e-3);
    CHECK_NEAR(report.work_max_us, 1900.0, 1e-3);
    CHECK_NEAR(report.work_max_percent,
               1900.0 / (HOP_PERIOD_S * 1e6) * 100.0, 1e-3);
    CHECK(report.ceiling_db == -24.f);
    CHECK(report.audio_lost == 3);
    CHECK_NEAR(report.led_fps, 150.0, 1e-6);
    // The app fills those in
    CHECK(report.core0_stack_peak == 0 && report.core1_stack_size == 0);
}

// A report starts the next window, empty
static void test_report_starts_anew(void) {
    stats_t stats;
    stats_report_t report;

    stats_reset(&stats);
    fill(0.05, 0.0, 1000);
    hops_of(&stats, 10);
    report = stats_report(&stats, PERIOD_S, HOP_PERIOD_S, -30.f, 0, 0);

    CHECK(isinf(report.left_dbfs) && report.left_dbfs < 0.f);
    CHECK(report.loudness == 0.f);
    CHECK(report.beats_per_s == 0.f);
    CHECK(report.work_avg_us == 0.f && report.work_max_us == 0.f);
}

int main(void) {
    test_levels();
    test_offset_is_no_sound();
    test_window();
    test_report_starts_anew();

    return CHECK_REPORT();
}
