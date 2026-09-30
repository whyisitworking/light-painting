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
// loudest band, and never goes below FEATURES_MIN_CEILING_DB so that silence
// stays dark. Band power is per
// bin: the INMP441 self-noise (-87 dBFS) with the x12 input gain (+21.6 dB:
// SPECTRUM_INPUT_GAIN 8 in spectrum.c times VISUALIZER_GAIN 1.5 in
// visualizer.h) is -65 dB in total but spread over N / 2 = 256 bins, -24 dB
// each, so a quiet room is about -88 dB per bin. Keep that below the floor,
// ceiling minus range = -77 dB, with 11 dB of margin for its fluctuations:
// tune on hardware
constexpr float FEATURES_RANGE_DB = 45.f;

// The ceiling is a volume control set once per song, not a compressor: it
// rises to a loud part with a time constant of FEATURES_CEILING_RISE_MS, so a
// spike much shorter (a door slam) lifts it only a little, and falls
// FEATURES_CEILING_FALL_DB_PER_S, 15 dB a minute, so a quiet part stays
// visibly quieter than the chorus before it. First guesses, tuned on songs
constexpr float FEATURES_CEILING_RISE_MS = 300.f;
constexpr float FEATURES_CEILING_FALL_DB_PER_S = 0.25f;
constexpr float FEATURES_MIN_CEILING_DB = -32.f;

// Per band smoothing time constants
constexpr float FEATURES_ATTACK_MS = 10.f;
constexpr float FEATURES_DECAY_MS = 120.f;

// Groove: how full the music is, the loudness averaged over this time
// constant, long enough to span a bar. First guess
constexpr float FEATURES_GROOVE_MS = 1500.f;

// Hits: the starts of sounds, in three regions of the spectrum. Each region
// is the bands wholly inside it: low up to FEATURES_LOW_MAX_HZ (kicks, bass),
// mid from there to FEATURES_MID_MAX_HZ (snares, claps, voices), high from
// FEATURES_HIGH_MIN_HZ on (hi-hats, cymbals). The names promise no
// instrument: room microphones cannot tell a snare from a clap.
//
// Spectral flux (Bello et al. 2005; on log-spaced bands as SuperFlux, Boeck
// and Widmer 2013): each band's power is smoothed over the region's
// SMOOTH_MS (the low bands come from ~2 FFT bins, one noisy draw per hop),
// turned to dB, held at the floor of the range so that sounds below it
// count for nothing (the earlier dB too: a floor climbing with the
// auto-gain is no rise), and its rise over the last FEATURES_HIT_LAG hops
// (falls count 0) averaged over the region. Two hops, not one: a kick's
// rise often falls across two hops, and over a steady bass line one hop's
// share was too small to count (the synthetic song's drop, 120 BPM kicks
// over a bass 14 dB below them: 9 of every 20 kicks at one hop, 13 to 20 at
// two). A hit fires when that rise exceeds its own average over
// FEATURES_HIT_AVERAGE_MS times FEATURES_HIT_THRESHOLD, plus the region's
// MIN_RISE_DB, once the rise fell back below that trigger and the region's
// refractory time passed. The low bands all come from the same ~2 bins, so
// their mean rise swings as one draw does and needs the higher minimum; the
// mid and high regions average many independent bands. With these, 16 seeds
// of 58 s of steady noise gave 5 false hits of any region, at most 1 per
// seed (tests/test_latency.c). In dB, so the volume does not matter.
// Strength: how far above the trigger, FEATURES_HIT_FULL_DB above is full.
// First guesses, checked by the latency bench
constexpr float FEATURES_LOW_MAX_HZ = 150.f;
constexpr float FEATURES_MID_MAX_HZ = 2000.f;
constexpr float FEATURES_HIGH_MIN_HZ = 5000.f;
constexpr float FEATURES_LOW_SMOOTH_MS = 30.f;
constexpr float FEATURES_MID_SMOOTH_MS = 15.f;
constexpr float FEATURES_HIGH_SMOOTH_MS = 10.f;
constexpr float FEATURES_LOW_REFRACTORY_MS = 150.f;
constexpr float FEATURES_MID_REFRACTORY_MS = 100.f;
constexpr float FEATURES_HIGH_REFRACTORY_MS = 60.f;
constexpr float FEATURES_HIT_AVERAGE_MS = 1000.f;
constexpr float FEATURES_HIT_THRESHOLD = 3.f;
constexpr float FEATURES_LOW_MIN_RISE_DB = 4.5f;
constexpr float FEATURES_MID_MIN_RISE_DB = 1.5f;
constexpr float FEATURES_HIGH_MIN_RISE_DB = 1.5f;
constexpr size_t FEATURES_HIT_LAG = 2;
constexpr float FEATURES_HIT_FULL_DB = 12.f;

// The regions of the hits
typedef enum {
    FEATURES_LOW,
    FEATURES_MID,
    FEATURES_HIGH,
    FEATURES_REGION_COUNT
} features_region_t;

// What can be changed while running, the constants above are the defaults
typedef struct {
    // FEATURES_ATTACK_MS, FEATURES_DECAY_MS: positive
    float attack_ms;
    float decay_ms;
    // FEATURES_MIN_CEILING_DB: finite
    float min_ceiling_db;
    // FEATURES_HIT_THRESHOLD: positive
    float hit_threshold;
} features_tuning_t;

// A hit of one region: whether one started this hop, and how strong, 0..1
typedef struct {
    bool fired;
    float strength;
} features_hit_t;

// The sound of one hop, what the effects render from
typedef struct {
    // FEATURES_BAND_COUNT levels, 0..1, smoothed
    const float *bands;
    // Mean band level, 0..1
    float loudness;
    // Level weighted band position, 0 (bass) .. 1 (treble), 0 in silence:
    // the tone, dark to bright
    float centroid;
    // Loudness averaged over FEATURES_GROOVE_MS, 0..1
    float groove;
    // The hits of this hop, by features_region_t
    features_hit_t hits[FEATURES_REGION_COUNT];
    // The low hit, until the effects move to the hits: the same values
    bool beat;
    float beat_strength;
} sound_t;

// What a region's hits need between hops
typedef struct {
    // Its bands, [from, to)
    size_t from;
    size_t to;
    // Per hop, the fraction of the way each band's power moves; the
    // refractory time in seconds; the least rise that counts, in dB
    float smooth_k;
    float refractory_s;
    float min_rise_db;
    // The mean rise, its running average, time since the last hit, and
    // whether the rise fell below the trigger since
    float average;
    float since_s;
    bool armed;
} features_region_state_t;

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

    // Auto-gain ceiling in dB, the fraction of the way it rises to a louder
    // band per hop, and how far it falls per hop
    float ceiling_db;
    float ceiling_rise_k;
    float ceiling_fall_db;

    // Per hop smoothing factors
    float attack_k;
    float decay_k;

    // Hits: each band's smoothed power; its dB (held at the floor) over the
    // last FEATURES_HIT_LAG hops, a ring whose oldest row is hit_slot; and
    // each region's state
    float hit_power[FEATURES_BAND_COUNT];
    float hit_db[FEATURES_HIT_LAG][FEATURES_BAND_COUNT];
    size_t hit_slot;
    features_region_state_t regions[FEATURES_REGION_COUNT];
    float hit_average_k;

    // What the groove keeps moving towards the loudness per hop
    float groove_k;

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
 * auto-gain or the hits. A field out of its range (see
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
