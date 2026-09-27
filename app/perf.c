#include "perf.h"

#ifdef PERF_STATS

#include "i2s.h"
#include "ws2812.h"

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

// Loop iterations since the last report: each analyzes one audio buffer and
// submits one LED frame
static long hops;

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

void perf_end(const visualizer_t *visualizer, const sound_t *sound) {
    i2s_stats_t audio;
    ws2812_stats_t leds;

    hops++;

    if (sound->beat)
        beats++;

    if (lap_us - report_us < 1'000'000)
        return;

    report_us = lap_us;

    // Before printing, which takes a while: the counts then cover the same
    // hops
    audio = i2s_take_stats();
    leds = ws2812_take_stats();

    for (int stage = 0; stage < PERF_STAGE_COUNT; stage++)
        perf_print(stage_names[stage], &stages[stage]);

    // Worked out from the counts rather than counted. A frame submitted but
    // not latched yet is still in flight: the LED count can be off by 1
    printf("Audio buffers lost %ld, LED frames skipped %ld\n",
           (long)audio.irq_hits - hops, hops - (long)leds.frames_latched);
    hops = 0;
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
