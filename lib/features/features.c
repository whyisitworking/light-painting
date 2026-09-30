#include "features.h"

#include <math.h>

/*
 * Per hop: bin power → band power → dB → auto-gain ceiling → 0..1 target
 * level → attack/decay smoothing → loudness and centroid → hits in three
 * regions. All time constants become per hop factors at init.
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
        .hit_threshold = FEATURES_HIT_THRESHOLD,
    };
}

void features_tune(features_t *this, const features_tuning_t *tuning) {
    if (is_positive(tuning->attack_ms))
        this->tuning.attack_ms = tuning->attack_ms;
    if (is_positive(tuning->decay_ms))
        this->tuning.decay_ms = tuning->decay_ms;
    if (isfinite(tuning->min_ceiling_db))
        this->tuning.min_ceiling_db = tuning->min_ceiling_db;
    if (is_positive(tuning->hit_threshold))
        this->tuning.hit_threshold = tuning->hit_threshold;

    this->attack_k =
        smoothing_factor(this->hop_period_s, this->tuning.attack_ms);
    this->decay_k = smoothing_factor(this->hop_period_s, this->tuning.decay_ms);
}

// The bands wholly inside [low_hz, high_hz), at least one
static void region_bands(const features_t *this, float low_hz, float high_hz,
                         size_t *from, size_t *to) {
    size_t b = 0;

    while (b + 1 < FEATURES_BAND_COUNT && this->edges[b] < low_hz)
        b++;
    *from = b;
    // A little slack: the top edge is FEATURES_HIGH_HZ up to rounding
    while (b < FEATURES_BAND_COUNT && this->edges[b + 1] <= high_hz * 1.001f)
        b++;
    *to = b > *from ? b : *from + 1;
}

// Each region's bands and factors, and the hit state as after silence
static void init_regions(features_t *this) {
    static const struct {
        float low_hz;
        float high_hz;
        float smooth_ms;
        float refractory_ms;
        float min_rise_db;
    } defs[FEATURES_REGION_COUNT] = {
        [FEATURES_LOW] = {0.f, FEATURES_LOW_MAX_HZ, FEATURES_LOW_SMOOTH_MS,
                          FEATURES_LOW_REFRACTORY_MS, FEATURES_LOW_MIN_RISE_DB},
        [FEATURES_MID] = {FEATURES_LOW_MAX_HZ, FEATURES_MID_MAX_HZ,
                          FEATURES_MID_SMOOTH_MS, FEATURES_MID_REFRACTORY_MS,
                          FEATURES_MID_MIN_RISE_DB},
        [FEATURES_HIGH] = {FEATURES_HIGH_MIN_HZ, FEATURES_HIGH_HZ,
                           FEATURES_HIGH_SMOOTH_MS,
                           FEATURES_HIGH_REFRACTORY_MS,
                           FEATURES_HIGH_MIN_RISE_DB},
    };
    float floor_db = this->tuning.min_ceiling_db - FEATURES_RANGE_DB;

    for (size_t r = 0; r < FEATURES_REGION_COUNT; r++) {
        features_region_state_t *region = &this->regions[r];

        region_bands(this, defs[r].low_hz, defs[r].high_hz, &region->from,
                     &region->to);
        region->smooth_k =
            smoothing_factor(this->hop_period_s, defs[r].smooth_ms);
        region->refractory_s = defs[r].refractory_ms / 1000.f;
        region->min_rise_db = defs[r].min_rise_db;
        region->average = 0.f;
        region->since_s = region->refractory_s;
        region->armed = true;
    }

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        this->hit_power[b] = 0.f;
        for (size_t l = 0; l < FEATURES_HIT_LAG; l++)
            this->hit_db[l][b] = floor_db;
    }
    this->hit_slot = 0;
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

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        this->levels[b] = 0.f;

    this->bin_count = bin_count;
    this->bin_hz = bin_hz;
    this->hop_period_s = hop_period_s;
    this->tuning = features_default_tuning();
    features_tune(this, &this->tuning);
    this->ceiling_db = this->tuning.min_ceiling_db;
    this->ceiling_rise_k =
        smoothing_factor(hop_period_s, FEATURES_CEILING_RISE_MS);
    this->ceiling_fall_db = FEATURES_CEILING_FALL_DB_PER_S * hop_period_s;
    this->hit_average_k =
        smoothing_factor(hop_period_s, FEATURES_HIT_AVERAGE_MS);
    this->groove_k = smoothing_factor(hop_period_s, FEATURES_GROOVE_MS);
    init_regions(this);
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

// Spectral flux per region, see FEATURES_HIT_THRESHOLD
static void detect_hits(features_t *this, const float *power, float floor_db) {
    for (size_t r = 0; r < FEATURES_REGION_COUNT; r++) {
        features_region_state_t *region = &this->regions[r];
        features_hit_t *hit = &this->sound.hits[r];
        float rise = 0.f, trigger;

        for (size_t b = region->from; b < region->to; b++) {
            float db;

            this->hit_power[b] +=
                (power[b] - this->hit_power[b]) * region->smooth_k;
            db = fmaxf(10.f * log10f(this->hit_power[b] + SILENCE_POWER),
                       floor_db);
            // Against the earlier dB lifted to today's floor: a floor that
            // climbs with the auto-gain is no rise
            rise += fmaxf(db - fmaxf(this->hit_db[this->hit_slot][b], floor_db),
                          0.f);
            this->hit_db[this->hit_slot][b] = db;
        }
        rise /= (float)(region->to - region->from);

        region->since_s += this->hop_period_s;
        trigger = region->average * this->tuning.hit_threshold +
                  region->min_rise_db;
        *hit = (features_hit_t){};

        if (rise <= trigger) {
            region->armed = true;
        } else if (region->armed && region->since_s >= region->refractory_s) {
            hit->fired = true;
            hit->strength = clamp01((rise - trigger) / FEATURES_HIT_FULL_DB);
            region->since_s = 0.f;
            region->armed = false;
        }

        // Updated after the comparison, so a hit does not raise its own bar
        region->average += (rise - region->average) * this->hit_average_k;
    }

    this->hit_slot = (this->hit_slot + 1) % FEATURES_HIT_LAG;

    this->sound.beat = this->sound.hits[FEATURES_LOW].fired;
    this->sound.beat_strength = this->sound.hits[FEATURES_LOW].strength;
}

// Levels first, then hits, against the fresh floor
const sound_t *features_update(features_t *this, const float *bins) {
    float power[FEATURES_BAND_COUNT], db[FEATURES_BAND_COUNT];
    float loudest = -1000.f, floor_db, sum = 0.f, weighted = 0.f;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        power[b] = band_power(this, bins, b);
        db[b] = 10.f * log10f(power[b] + SILENCE_POWER);

        if (db[b] > loudest)
            loudest = db[b];
    }

    // Auto-gain: up towards the loudest band within the rise time, back down
    // slowly, never below the silence floor
    if (loudest > this->ceiling_db)
        this->ceiling_db += (loudest - this->ceiling_db) * this->ceiling_rise_k;
    else
        this->ceiling_db -= this->ceiling_fall_db;
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
    this->sound.groove +=
        (this->sound.loudness - this->sound.groove) * this->groove_k;

    detect_hits(this, power, floor_db);

    return &this->sound;
}

void features_deinit([[maybe_unused]] features_t *this) {}
