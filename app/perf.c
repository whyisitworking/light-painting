#include "perf.h"

#ifdef PERF_STATS

#include "i2s.h"
#include "neopixel.h"

#include <pico/time.h>
#include <stdio.h>

// Timing of one main loop stage, reported and reset about once per second
typedef struct {
    uint32_t count;
    uint32_t total_us;
    uint32_t max_us;
} perf_stat_t;

static const char *const stage_names[PERF_STAGE_COUNT] = {
    [PERF_WAIT] = "wait",
    [PERF_ANALYZE] = "analyze",
    [PERF_RENDER] = "render",
};

static perf_stat_t stages[PERF_STAGE_COUNT];
static uint32_t lap_us, report_us, beats;

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

void perf_init(void) { report_us = time_us_32(); }

void perf_begin(void) { lap_us = time_us_32(); }

void perf_lap(perf_stage_t stage) {
    uint32_t now = time_us_32();

    perf_add(&stages[stage], now - lap_us);
    lap_us = now;
}

void perf_end(const visualizer_t *visualizer, const features_t *sound) {
    i2s_stats_t audio;
    neopixel_stats_t leds;

    if (sound->beat)
        beats++;

    if (lap_us - report_us < 1000000)
        return;

    report_us = lap_us;

    for (int stage = 0; stage < PERF_STAGE_COUNT; stage++)
        perf_print(stage_names[stage], &stages[stage]);

    audio = i2s_take_stats();
    leds = neopixel_take_stats();

    printf("Audio buffers dropped %zu, LED frames dropped %zu\n",
           audio.dropped, leds.dropped);
    // To tune FEATURES_MIN_CEILING_DB: a quiet room should read loudness ~0
    // and no beats
    printf("Ceiling %.1f dB, loudness %.3f, beats %lu\n",
           (double)visualizer->features.ceiling_db, (double)sound->loudness,
           (unsigned long)beats);
    beats = 0;
    printf("IRQ hits %zu\n", audio.irq_hits);
    printf("Frames latched %zu\n", leds.frames_latched);
}

#endif
