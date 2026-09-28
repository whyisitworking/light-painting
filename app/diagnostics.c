#include "diagnostics.h"

#include "config.h"
#include "i2s.h"
#include "swapchain.h"
#include "ws2812.h"

#include <math.h>
#include <pico/time.h>
#include <stdint.h>
#include <stdio.h>

/*
 * From the linker script. Core 0's stack grows down from the top of
 * SCRATCH_Y, through what SCRATCH_Y and SCRATCH_X hold no data in: core 1
 * has its own stack elsewhere (PICO_CORE1_STACK_SIZE=0)
 */
extern uint32_t __StackTop[];
extern uint32_t __scratch_x_end__[];
extern uint32_t __scratch_y_start__[];
extern uint32_t __scratch_y_end__[];

// Left below core 0's frame when filling its stack, for the calls it makes
constexpr size_t FILL_MARGIN_WORDS = 64;

// Core 0 to core 1, the reports
static swapchain_t link;
static bool is_init = false;

// Core 0's alone
static stats_t stats;
static uint32_t window_start_us;
static uint32_t work_start_us;
static uint32_t audio_lost;

// Set before core 1 runs, read by it
static const uint32_t *core0_bottom;
static size_t core0_word_count;
static const uint32_t *core1_bottom;
static size_t core1_word_count;

// Core 1's alone: the report taken last
static stats_report_t latest;

void diagnostics_fill_core0_stack(void) {
    uint32_t here;
    // Data in SCRATCH_Y sits right below the stack: it may only grow to it
    // (As addresses: comparing the arrays themselves is -Warray-compare)
    uintptr_t bottom =
        (uintptr_t)__scratch_y_end__ != (uintptr_t)__scratch_y_start__
            ? (uintptr_t)__scratch_y_end__
            : (uintptr_t)__scratch_x_end__;
    uintptr_t below =
        ((uintptr_t)&here & ~(uintptr_t)3) - FILL_MARGIN_WORDS * 4;

    core0_bottom = (const uint32_t *)bottom;
    core0_word_count = ((uintptr_t)__StackTop - bottom) / 4;

    if (below > bottom)
        stats_stack_fill((uint32_t *)bottom, (below - bottom) / 4);
}

void diagnostics_fill_core1_stack(uint32_t *bottom, size_t word_count) {
    stats_stack_fill(bottom, word_count);
    core1_bottom = bottom;
    core1_word_count = word_count;
}

bool diagnostics_init(void) {
    if (!swapchain_init(&link, sizeof(stats_report_t)))
        return false;

    stats_reset(&stats);
    window_start_us = time_us_32();
    is_init = true;

    return true;
}

void diagnostics_start_work(void) { work_start_us = time_us_32(); }

void diagnostics_end_hop(const visualizer_t *visualizer,
                         const int32_t *frames, const sound_t *sound) {
    uint32_t now_us = time_us_32();
    i2s_stats_t audio;
    ws2812_stats_t leds;
    float period_s;

    if (!is_init)
        return;

    stats_add_hop(&stats, frames, AUDIO_HOP_SIZE, sound,
                  now_us - work_start_us);

    if (now_us - window_start_us < DIAGNOSTICS_PERIOD_MS * 1000)
        return;

    audio = i2s_take_stats();
    leds = ws2812_take_stats();
    audio_lost += (uint32_t)audio.buffers_lost;
    period_s = (float)(now_us - window_start_us) / 1e6f;
    window_start_us = now_us;

    // The producer buffer is core 0's alone until the swap publishes it
    *(stats_report_t *)swapchain_producer_buffer(&link) = stats_report(
        &stats, period_s, visualizer->effects.hop_period_s,
        visualizer->features.ceiling_db, audio_lost,
        (uint32_t)leds.frames_latched);
    swapchain_producer_swap(&link);
}

const stats_report_t *diagnostics_take(void) {
    if (!is_init || !swapchain_consumer_swap(&link))
        return nullptr;

    // Copied: the stacks' peaks are added, the consumer buffer is read only
    latest = *(const stats_report_t *)swapchain_consumer_buffer(&link);
    // A race by the letter of C: core 0 may be writing this stack right
    // now, but aligned 32-bit loads are single-copy atomic on the M33,
    // and the peak only ever grows
    latest.core0_stack_peak = stats_stack_peak(core0_bottom, core0_word_count);
    latest.core0_stack_size = core0_word_count * sizeof(uint32_t);
    latest.core1_stack_peak = stats_stack_peak(core1_bottom, core1_word_count);
    latest.core1_stack_size = core1_word_count * sizeof(uint32_t);

    return &latest;
}

static void print_level(const char *name, float dbfs) {
    if (isinf(dbfs))
        printf("%s none", name);
    else
        printf("%s %.1f dBFS", name, (double)dbfs);
}

void diagnostics_print(const stats_report_t *report) {
    print_level("Mic L", report->left_dbfs);
    print_level(", R", report->right_dbfs);
    printf("\nCeiling %.1f dB, loudness %.3f, beats %.1f/s\n",
           (double)report->ceiling_db, (double)report->loudness,
           (double)report->beats_per_s);
    printf("Work avg %.0f us, max %.0f us (%.0f %% of a hop)\n",
           (double)report->work_avg_us, (double)report->work_max_us,
           (double)report->work_max_percent);
    printf("Audio lost %lu, LEDs %.1f fps\n",
           (unsigned long)report->audio_lost, (double)report->led_fps);
    printf("Stack menu %zu of %zu B, lights %zu of %zu B\n",
           report->core1_stack_peak, report->core1_stack_size,
           report->core0_stack_peak, report->core0_stack_size);
}
