#ifndef APP_DIAGNOSTICS_H
#define APP_DIAGNOSTICS_H

/**
 * Diagnostics: core 0 measures the lights' loop (lib/stats) and, every
 * DIAGNOSTICS_PERIOD_MS, publishes a report through a lock-free swapchain.
 * Core 1 takes the newest, adds how much of each core's stack was ever
 * used, and shows or prints it. Neither core waits for the other, and the
 * lights never print.
 *
 *   core 0: diagnostics_fill_core0_stack();       first thing in main()
 *           diagnostics_init();
 *           diagnostics_fill_core1_stack(stack, word_count);  ui_start calls it
 *           each hop:
 *               frames = i2s_wait_buffer();
 *               diagnostics_start_work();
 *               ...                                analyze, render, submit
 *               diagnostics_end_hop(&visualizer, frames, sound);
 *   core 1: report = diagnostics_take();           nullptr if nothing new
 */

#include "stats.h"
#include "visualizer.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Core 0, first thing in main(): fills its stack's unused part, below
 * where it is now, for diagnostics_take() to see how much gets used
 */
void diagnostics_fill_core0_stack(void);

/**
 * Core 0, before launching core 1: fills core 1's stack, word_count words
 * from its bottom, likewise
 */
void diagnostics_fill_core1_stack(uint32_t *bottom, size_t word_count);

// Core 0, before the loop. False if memory runs out: the lights run on
// without diagnostics then
[[nodiscard]] bool diagnostics_init(void);

// Core 0: the hop's work starts, once its audio is taken
void diagnostics_start_work(void);

/**
 * Core 0: the hop's work is done, the frame handed to the LEDs. Adds the
 * hop, and publishes a report once DIAGNOSTICS_PERIOD_MS have passed
 */
void diagnostics_end_hop(const visualizer_t *visualizer,
                         const int32_t *frames, const sound_t *sound);

/**
 * Core 1: the newest report, with both stacks' peaks, or nullptr if none
 * arrived since the previous call. Valid until the next call
 */
const stats_report_t *diagnostics_take(void);

// Core 1: prints a report over USB
void diagnostics_print(const stats_report_t *report);

#endif
