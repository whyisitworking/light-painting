#ifndef STATS_H
#define STATS_H

/**
 * Stats: what the diagnostics show, added up hop by hop over a window,
 * then closed into a report. Hardware independent: the clock, the driver
 * counters and where the stacks are come from the app.
 *
 *   each hop:     stats_add_hop()
 *   each window:  stats_report(), which starts the next
 */

#include "features.h"

#include <stddef.h>
#include <stdint.h>

// One microphone's samples in the window, exact: 24-bit squares stay below
// 2^46, so 2^18 samples (about 5 s) fit, far more than a window holds
typedef struct {
    int64_t sum;
    uint64_t sum_squares;
} stats_channel_t;

// The window being added up
typedef struct {
    stats_channel_t left;
    stats_channel_t right;
    // Samples per channel
    uint64_t sample_count;
    uint32_t hop_count;
    uint64_t work_total_us;
    uint32_t work_max_us;
    uint32_t hit_count;
    float loudness_total;
} stats_t;

// A closed window
typedef struct {
    // How long it was
    float period_s;
    // dBFS, 0 for a full-scale sine as in the INMP441 datasheet, an offset
    // taken out. -INFINITY: no sound at all, below SILENT's -120 dBFS,
    // e.g. a missing microphone
    float left_dbfs;
    float right_dbfs;
    // The auto-gain ceiling at its end
    float ceiling_db;
    // Mean loudness, 0..1
    float loudness;
    // Low hits per second
    float hits_per_s;
    // Per hop, from the audio taken to the frame handed to the LEDs
    float work_avg_us;
    float work_max_us;
    // work_max_us of a hop's period, in percent
    float work_max_percent;
    // Audio buffers never analyzed, since start
    uint32_t audio_lost;
    // Frames the strip latched per second
    float led_fps;
    // Bytes of each core's stack used at most, and its size. Not the
    // stats': the app fills them in
    size_t core0_stack_peak;
    size_t core0_stack_size;
    size_t core1_stack_peak;
    size_t core1_stack_size;
} stats_report_t;

// An empty window
void stats_reset(stats_t *this);

/**
 * Adds a hop: frame_count stereo frames as the I2S driver delivers them
 * (left word first), the sound rendered from them, and the work it took
 */
void stats_add_hop(stats_t *this, const int32_t *frames, size_t frame_count,
                   const sound_t *sound, uint32_t work_us);

/**
 * Closes the window, period_s long, into a report, and starts the next.
 * The rest is passed in as it stands at its end: ceiling_db, audio_lost
 * since start, and the frames the strip latched during it
 */
stats_report_t stats_report(stats_t *this, float period_s, float hop_period_s,
                            float ceiling_db, uint32_t audio_lost,
                            uint32_t frames_latched);

// A stack's words hold this until first used, see stats_stack_fill()
constexpr uint32_t STATS_STACK_FILL = 0x5AC4F111u;

/*
 * Stacks: filled before they are used, the bytes used at most show where
 * the fill ends. They grow down: from the bottom up to the first word no
 * longer the fill, nothing was ever used
 */

void stats_stack_fill(uint32_t *bottom, size_t word_count);
size_t stats_stack_peak(const uint32_t *bottom, size_t word_count);

#endif
