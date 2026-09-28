#include "stats.h"

#include "spectrum.h"

#include <math.h>

// Full scale of the INMP441's 24-bit samples
constexpr double FULL_SCALE = 8388608.0;

void stats_reset(stats_t *this) { *this = (stats_t){0}; }

static void add_sample(stats_channel_t *channel, int32_t sample) {
    channel->sum += sample;
    channel->sum_squares += (uint64_t)((int64_t)sample * sample);
}

void stats_add_hop(stats_t *this, const int32_t *frames, size_t frame_count,
                   const sound_t *sound, uint32_t work_us) {
    for (size_t i = 0; i < frame_count; i++) {
        add_sample(&this->left, spectrum_sample(frames[2 * i]));
        add_sample(&this->right, spectrum_sample(frames[2 * i + 1]));
    }

    this->sample_count += frame_count;
    this->hop_count++;
    this->work_total_us += work_us;
    if (work_us > this->work_max_us)
        this->work_max_us = work_us;
    this->beat_count += sound->beat;
    this->loudness_total += sound->loudness;
}

// A mic that is present never reads below its self-noise (-87 dBFS for the
// INMP441), and a missing one reads about -141 dBFS through the data pin's
// bus keeper, which holds the other mic's last bit
constexpr double SILENT_DBFS = -120.0;

/**
 * A sine's peak is its RMS times sqrt(2), and 0 dBFS a sine peaking at
 * full scale. The standard deviation rather than the RMS: an offset is no
 * sound. -INFINITY if there is none
 */
static float level_dbfs(const stats_channel_t *channel, uint64_t count) {
    double mean, variance, dbfs;

    if (count == 0)
        return -INFINITY;

    mean = (double)channel->sum / (double)count;
    variance = (double)channel->sum_squares / (double)count - mean * mean;

    if (!(variance > 0.0))
        return -INFINITY;

    dbfs = 10.0 * log10(2.0 * variance / (FULL_SCALE * FULL_SCALE));

    return dbfs < SILENT_DBFS ? -INFINITY : (float)dbfs;
}

stats_report_t stats_report(stats_t *this, float period_s, float hop_period_s,
                            float ceiling_db, uint32_t audio_lost,
                            uint32_t frames_latched) {
    stats_report_t report = {
        .period_s = period_s,
        .left_dbfs = level_dbfs(&this->left, this->sample_count),
        .right_dbfs = level_dbfs(&this->right, this->sample_count),
        .ceiling_db = ceiling_db,
        .audio_lost = audio_lost,
    };

    if (this->hop_count > 0) {
        report.loudness = this->loudness_total / (float)this->hop_count;
        report.work_avg_us =
            (float)((double)this->work_total_us / this->hop_count);
        report.work_max_us = (float)this->work_max_us;
    }

    if (period_s > 0.f) {
        report.beats_per_s = (float)this->beat_count / period_s;
        report.led_fps = (float)frames_latched / period_s;
    }

    if (hop_period_s > 0.f)
        report.work_max_percent =
            report.work_max_us / (hop_period_s * 1e6f) * 100.f;

    stats_reset(this);

    return report;
}

void stats_stack_fill(uint32_t *bottom, size_t word_count) {
    for (size_t i = 0; i < word_count; i++)
        bottom[i] = STATS_STACK_FILL;
}

size_t stats_stack_peak(const uint32_t *bottom, size_t word_count) {
    size_t untouched = 0;

    while (untouched < word_count && bottom[untouched] == STATS_STACK_FILL)
        untouched++;

    return (word_count - untouched) * sizeof(uint32_t);
}
