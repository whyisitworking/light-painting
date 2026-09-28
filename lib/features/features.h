#ifndef FEATURES_H
#define FEATURES_H

/**
 * Audio features: turns FFT magnitude bins into a compact description of
 * the sound, once per hop
 */

#include <stddef.h>

// Log-spaced bands between FEATURES_LOW_HZ and FEATURES_HIGH_HZ
constexpr size_t FEATURES_BAND_COUNT = 32;
constexpr float FEATURES_LOW_HZ = 60.f;
constexpr float FEATURES_HIGH_HZ = 12000.f;

// Levels span FEATURES_RANGE_DB below an auto-gain ceiling that follows the
// loudest band up at once, falls back slowly, and never goes below
// FEATURES_MIN_CEILING_DB so that silence stays dark. Band power is per
// bin: the INMP441 self-noise (-87 dBFS) with the x12 input gain (+21.6 dB:
// SPECTRUM_INPUT_GAIN 8 in spectrum.c times VISUALIZER_GAIN 1.5 in
// visualizer.h) is -65 dB in total but spread over N / 2 = 256 bins, -24 dB
// each, so a quiet room is about -88 dB per bin. Keep that below the floor,
// ceiling minus range = -77 dB, with 11 dB of margin for its fluctuations:
// tune on hardware
constexpr float FEATURES_RANGE_DB = 45.f;
constexpr float FEATURES_CEILING_FALL_DB_PER_S = 6.f;
constexpr float FEATURES_MIN_CEILING_DB = -32.f;

// Per band smoothing time constants
constexpr float FEATURES_ATTACK_MS = 10.f;
constexpr float FEATURES_DECAY_MS = 120.f;

// A beat is a jump of the bass energy (bands up to FEATURES_BEAT_MAX_HZ).
// The bass bands all come from the lowest ~2 bins, one noisy draw per hop,
// so the energy is first smoothed over FEATURES_BEAT_SMOOTH_MS: smoothed
// steady noise exceeds 2.6 times its average in only 0.01 % of hops, while
// 120 BPM kicks peak ~6.5 times above theirs. A beat fires when the
// smoothed energy exceeds FEATURES_BEAT_THRESHOLD times its average over
// FEATURES_BEAT_AVERAGE_MS and the bass is audible, its bands' mean
// smoothed level above FEATURES_BEAT_MIN_LEVEL. At most one beat per
// refractory time, and the energy must fall back below the trigger first.
// The latency bench (tests/test_latency.c) measures a synthetic 55 Hz kick
// reaching a beat after 9 to 12 ms with 30 ms of smoothing. 24 ms gained
// ~0.7 ms, less than a 5.24 ms hop, but gave 5.5 times the false beats on
// steady noise (44 against 8 over 30 seeds of 58 s at noise 20000), so 30
// ms is kept. Host bench evidence only: a synthetic kick, no hardware
constexpr float FEATURES_BEAT_SMOOTH_MS = 30.f;
constexpr float FEATURES_BEAT_THRESHOLD = 2.8f;
constexpr float FEATURES_BEAT_AVERAGE_MS = 1000.f;
constexpr float FEATURES_BEAT_MIN_LEVEL = 0.3f;
constexpr float FEATURES_BEAT_REFRACTORY_MS = 150.f;
constexpr float FEATURES_BEAT_MAX_HZ = 150.f;

// What can be changed while running, the constants above are the defaults
typedef struct {
    // FEATURES_ATTACK_MS, FEATURES_DECAY_MS: positive
    float attack_ms;
    float decay_ms;
    // FEATURES_MIN_CEILING_DB: finite
    float min_ceiling_db;
    // FEATURES_BEAT_THRESHOLD: positive
    float beat_threshold;
} features_tuning_t;

// The sound of one hop, what the effects render from
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
} sound_t;

// Module state, features_update() returns its sound member
typedef struct {
    // Input: bin k is centred at k * bin_hz, one update per hop_period_s
    size_t bin_count;
    float bin_hz;
    float hop_period_s;

    features_tuning_t tuning;

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

    sound_t sound;
} features_t;

/**
 * bin_count: number of magnitude bins (>= 2), bin k centred at k * bin_hz
 * hop_period_s: time between two features_update calls
 */
[[nodiscard]] bool features_init(features_t *this, size_t bin_count,
                                 float bin_hz, float hop_period_s);

// The FEATURES_* constants, what features_init() starts with
features_tuning_t features_default_tuning(void);

/**
 * Takes effect on the next update, without resetting the levels, the
 * auto-gain or the beat detection. A field out of its range (see
 * features_tuning_t) or NaN keeps its current value
 */
void features_tune(features_t *this, const features_tuning_t *tuning);

/**
 * Updates from the newest bins and returns the sound, valid until the next
 * call
 */
const sound_t *features_update(features_t *this, const float *bins);

// Nothing is allocated today, kept for symmetry with the other modules
void features_deinit(features_t *this);

#endif
