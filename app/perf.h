#ifndef APP_PERF_H
#define APP_PERF_H

/**
 * Opt-in performance statistics, built with -DPERF_STATS=ON: the main loop
 * stage timings and the driver counters, printed over USB about once per
 * second. Without it every call compiles to nothing.
 *
 *   perf_init();
 *   while (true) {
 *       perf_begin();
 *       ...                    // the work of a stage
 *       perf_lap(PERF_WAIT);   // ends that stage
 *       ...
 *       perf_end(&visualizer, sound);
 *   }
 */

#include "visualizer.h"

typedef enum {
    PERF_WAIT,
    PERF_ANALYZE,
    PERF_RENDER,
    PERF_STAGE_COUNT
} perf_stage_t;

#ifdef PERF_STATS

void perf_init(void);

// Starts timing a loop iteration
void perf_begin(void);

// Ends a stage: the time since perf_begin() or the previous lap
void perf_lap(perf_stage_t stage);

// Ends the iteration, and reports once a second has passed since the last
// report. Outside the timed stages, printing takes a while
void perf_end(const visualizer_t *visualizer, const sound_t *sound);

#else

static inline void perf_init(void) {}

static inline void perf_begin(void) {}

static inline void perf_lap(perf_stage_t stage) { (void)stage; }

static inline void perf_end(const visualizer_t *visualizer,
                            const sound_t *sound) {
    (void)visualizer;
    (void)sound;
}

#endif

#endif
