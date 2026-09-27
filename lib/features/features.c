#include "features.h"

#include <math.h>

/*
 * Per hop: bin power → band power → dB → auto-gain ceiling → 0..1 target
 * level → attack/decay smoothing → loudness and centroid → beat detection
 * on the bass bands. All time constants become per hop factors at init.
 */

// Added to band powers so silence has a finite dB value
constexpr float SILENCE_POWER = 1e-12f;

// Fraction of the way to the target per hop for a time constant
static float smoothing_factor(float hop_period_s, float time_constant_ms) {
    return 1.f - expf(-hop_period_s / (time_constant_ms / 1000.f));
}

static float clamp01(float value) {
    return value < 0.f ? 0.f : value > 1.f ? 1.f : value;
}

static bool is_positive(float value) { return value > 0.f && isfinite(value); }

features_tuning_t features_default_tuning(void) {
    return (features_tuning_t){
        .attack_ms = FEATURES_ATTACK_MS,
        .decay_ms = FEATURES_DECAY_MS,
        .min_ceiling_db = FEATURES_MIN_CEILING_DB,
        .beat_threshold = FEATURES_BEAT_THRESHOLD,
    };
}

void features_tune(features_t *this, const features_tuning_t *tuning) {
    if (is_positive(tuning->attack_ms))
        this->tuning.attack_ms = tuning->attack_ms;
    if (is_positive(tuning->decay_ms))
        this->tuning.decay_ms = tuning->decay_ms;
    if (isfinite(tuning->min_ceiling_db))
        this->tuning.min_ceiling_db = tuning->min_ceiling_db;
    if (is_positive(tuning->beat_threshold))
        this->tuning.beat_threshold = tuning->beat_threshold;

    this->attack_k =
        smoothing_factor(this->hop_period_s, this->tuning.attack_ms);
    this->decay_k = smoothing_factor(this->hop_period_s, this->tuning.decay_ms);
}

// Precomputes the band edges and the per hop factors, starts silent
bool features_init(features_t *this, size_t bin_count, float bin_hz,
                   float hop_period_s) {
    float ratio;

    if (bin_count < 2 || !(bin_hz > 0.f) || !(hop_period_s > 0.f))
        return false;

    // Geometric edges, each band ~18 % wider than the one below
    ratio = powf(FEATURES_HIGH_HZ / FEATURES_LOW_HZ,
                 1.f / FEATURES_BAND_COUNT);
    for (size_t b = 0; b <= FEATURES_BAND_COUNT; b++)
        this->edges[b] = FEATURES_LOW_HZ * powf(ratio, (float)b);

    // Bands counted as bass for beats, at least one
    this->bass_band_count = 1;
    while (this->bass_band_count < FEATURES_BAND_COUNT &&
           this->edges[this->bass_band_count + 1] <= FEATURES_BEAT_MAX_HZ)
        this->bass_band_count++;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        this->levels[b] = 0.f;

    this->bin_count = bin_count;
    this->bin_hz = bin_hz;
    this->hop_period_s = hop_period_s;
    this->tuning = features_default_tuning();
    features_tune(this, &this->tuning);
    this->ceiling_db = this->tuning.min_ceiling_db;
    this->ceiling_fall_db = FEATURES_CEILING_FALL_DB_PER_S * hop_period_s;
    this->average_k = smoothing_factor(hop_period_s, FEATURES_BEAT_AVERAGE_MS);
    this->smooth_k = smoothing_factor(hop_period_s, FEATURES_BEAT_SMOOTH_MS);
    this->bass_smooth = 0.f;
    this->bass_average = 0.f;
    this->beat_armed = true;
    this->since_beat_s = FEATURES_BEAT_REFRACTORY_MS / 1000.f;
    this->sound = (sound_t){.bands = this->levels};

    return true;
}

// Mean bin power within the band, or the power interpolated at the band's
// centre when the band is narrower than a bin (the lowest bands)
static float band_power(const features_t *this, const float *bins,
                        size_t band) {
    float low = this->edges[band], high = this->edges[band + 1];
    float sum = 0.f, x, fraction, p0, p1;
    size_t count = 0, k;

    for (k = (size_t)ceilf(low / this->bin_hz);
         k < this->bin_count && (float)k * this->bin_hz < high; k++) {
        sum += bins[k] * bins[k];
        count++;
    }

    if (count > 0)
        return sum / (float)count;

    x = sqrtf(low * high) / this->bin_hz;
    k = (size_t)x;

    if (k + 1 >= this->bin_count)
        return bins[this->bin_count - 1] * bins[this->bin_count - 1];

    fraction = x - (float)k;
    p0 = bins[k] * bins[k];
    p1 = bins[k + 1] * bins[k + 1];

    return p0 + (p1 - p0) * fraction;
}

// Smoothed bass energy well above its moving average, once per onset and
// refractory time, and only when the bass is audible
static void detect_beat(features_t *this, const float *power) {
    float bass = 0.f, level = 0.f, trigger;

    for (size_t b = 0; b < this->bass_band_count; b++) {
        bass += power[b];
        level += this->levels[b];
    }
    bass /= (float)this->bass_band_count;
    level /= (float)this->bass_band_count;

    this->bass_smooth += (bass - this->bass_smooth) * this->smooth_k;

    this->since_beat_s += this->hop_period_s;
    this->sound.beat = false;
    this->sound.beat_strength = 0.f;
    trigger = this->bass_average * this->tuning.beat_threshold;

    if (this->bass_smooth <= trigger) {
        this->beat_armed = true;
    } else if (this->beat_armed &&
               this->since_beat_s >= FEATURES_BEAT_REFRACTORY_MS / 1000.f &&
               level > FEATURES_BEAT_MIN_LEVEL) {
        this->sound.beat = true;
        this->sound.beat_strength =
            trigger > 0.f ? clamp01(this->bass_smooth / trigger - 1.f) : 1.f;
        this->since_beat_s = 0.f;
        this->beat_armed = false;
    }

    // Updated after the comparison, so a kick does not raise its own bar
    this->bass_average +=
        (this->bass_smooth - this->bass_average) * this->average_k;
}

// Levels first, then beats, which use the fresh bass levels
const sound_t *features_update(features_t *this, const float *bins) {
    float power[FEATURES_BAND_COUNT], db[FEATURES_BAND_COUNT];
    float loudest = -1000.f, floor_db, sum = 0.f, weighted = 0.f;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        power[b] = band_power(this, bins, b);
        db[b] = 10.f * log10f(power[b] + SILENCE_POWER);

        if (db[b] > loudest)
            loudest = db[b];
    }

    // Auto-gain: up to the loudest band at once, back down slowly, never
    // below the silence floor
    this->ceiling_db = fmaxf(this->ceiling_db - this->ceiling_fall_db, loudest);
    this->ceiling_db = fmaxf(this->ceiling_db, this->tuning.min_ceiling_db);
    floor_db = this->ceiling_db - FEATURES_RANGE_DB;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        float target = clamp01((db[b] - floor_db) / FEATURES_RANGE_DB);
        float k = target > this->levels[b] ? this->attack_k : this->decay_k;

        this->levels[b] += (target - this->levels[b]) * k;
        sum += this->levels[b];
        weighted += this->levels[b] * (float)b;
    }

    this->sound.loudness = sum / FEATURES_BAND_COUNT;
    this->sound.centroid =
        sum > 1e-6f ? weighted / sum / (FEATURES_BAND_COUNT - 1) : 0.f;

    detect_beat(this, power);

    return &this->sound;
}

void features_deinit([[maybe_unused]] features_t *this) {}
