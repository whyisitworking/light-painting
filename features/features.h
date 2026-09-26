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
// FEATURES_MIN_CEILING_DB so that silence stays dark. INMP441 noise floor
// (-87 dBFS) plus the x12 input gain is about -65 dB: tune on hardware
#define FEATURES_RANGE_DB 45.f
#define FEATURES_CEILING_FALL_DB_PER_S 6.f
#define FEATURES_MIN_CEILING_DB -50.f

// Per band smoothing time constants
#define FEATURES_ATTACK_MS 10.f
#define FEATURES_DECAY_MS 120.f

// A beat is bass energy (bands up to FEATURES_BEAT_MAX_HZ) above its moving
// average times FEATURES_BEAT_THRESHOLD, at most once per refractory time
#define FEATURES_BEAT_THRESHOLD 1.4f
#define FEATURES_BEAT_REFRACTORY_MS 150.f
#define FEATURES_BEAT_MAX_HZ 150.f
#define FEATURES_BEAT_AVERAGE_MS 1000.f

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
    float bass_average;
    float average_k;
    float since_beat_s;

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
