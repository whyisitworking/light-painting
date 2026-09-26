#ifndef FEATURES_H
#define FEATURES_H

/**
 * Audio features: turns FFT magnitude bins into a compact description of
 * the sound, once per hop
 */

#include <stdbool.h>
#include <stddef.h>

// Log-spaced bands between FEATURES_LOW_HZ and FEATURES_HIGH_HZ
#define FEATURES_BAND_COUNT 32
#define FEATURES_LOW_HZ 60.f
#define FEATURES_HIGH_HZ 12000.f

// Levels span FEATURES_RANGE_DB below an auto-gain ceiling that follows the
// loudest band up at once, falls back slowly, and never goes below
// FEATURES_MIN_CEILING_DB so that silence stays dark. Band power is per
// bin: the INMP441 self-noise (-87 dBFS) with the x12 input gain (+21.6 dB:
// AUDIO_INPUT_GAIN 8 in audio.c times VISUALIZER_GAIN 1.5 in app/config.h)
// is -65 dB in total but spread over N / 2 = 256 bins, -24 dB each, so a
// quiet room is about -88 dB per bin. Keep that below the floor, ceiling
// minus range = -77 dB, with 11 dB of margin for its fluctuations: tune on
// hardware
#define FEATURES_RANGE_DB 45.f
#define FEATURES_CEILING_FALL_DB_PER_S 6.f
#define FEATURES_MIN_CEILING_DB -32.f

// Per band smoothing time constants
#define FEATURES_ATTACK_MS 10.f
#define FEATURES_DECAY_MS 120.f

// A beat is a jump of the bass energy (bands up to FEATURES_BEAT_MAX_HZ).
// The bass bands all come from the lowest ~2 bins, one noisy draw per hop,
// so the energy is first smoothed over FEATURES_BEAT_SMOOTH_MS: smoothed
// steady noise exceeds 2.6 times its average in only 0.01 % of hops, while
// 120 BPM kicks peak ~6.5 times above theirs. A beat fires when the
// smoothed energy exceeds FEATURES_BEAT_THRESHOLD times its average over
// FEATURES_BEAT_AVERAGE_MS and the bass is audible, its bands' mean
// smoothed level above FEATURES_BEAT_MIN_LEVEL. At most one beat per
// refractory time, and the energy must fall back below the trigger first
#define FEATURES_BEAT_SMOOTH_MS 30.f
#define FEATURES_BEAT_THRESHOLD 2.8f
#define FEATURES_BEAT_AVERAGE_MS 1000.f
#define FEATURES_BEAT_MIN_LEVEL 0.3f
#define FEATURES_BEAT_REFRACTORY_MS 150.f
#define FEATURES_BEAT_MAX_HZ 150.f

typedef struct {
    // FEATURES_BAND_COUNT levels, 0..1, smoothed
    const float *bands;
    // Mean band level, 0..1
    float loudness;
    // Level weighted band position, 0 (bass) .. 1 (treble), 0 in silence
    float centroid;
    // True on the hop a beat is detected
    bool beat;
    // 0..1, how far above the threshold the beat was
    float beat_strength;
} features_t;

typedef struct {
    size_t bin_count;
    float bin_hz;
    float hop_seconds;

    // Band edges in Hz, band b spans [edges[b], edges[b + 1])
    float edges[FEATURES_BAND_COUNT + 1];

    // Smoothed band levels, 0..1
    float levels[FEATURES_BAND_COUNT];

    // Auto-gain ceiling in dB and how far it falls per hop
    float ceiling_db;
    float ceiling_fall_db;

    // Per hop smoothing factors
    float attack_k;
    float decay_k;

    // Beat detection
    size_t bass_band_count;
    float bass_smooth;
    float bass_average;
    float smooth_k;
    float average_k;
    float since_beat_s;
    // The energy fell below the trigger since the last beat
    bool beat_armed;

    features_t out;
} features_state_t;

/**
 * bin_count: number of magnitude bins (>= 2), bin k centred at k * bin_hz
 * hop_seconds: time between two features_update calls
 */
bool features_init(features_state_t *this, size_t bin_count, float bin_hz,
                   float hop_seconds);

/**
 * Updates from the newest bins and returns the features, valid until the
 * next call
 */
const features_t *features_update(features_state_t *this, const float *bins);

// Nothing is allocated today, kept for symmetry with the other modules
void features_deinit(features_state_t *this);

#endif
