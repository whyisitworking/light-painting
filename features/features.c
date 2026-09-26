#include "features.h"

#include <math.h>

// Added to band powers so silence has a finite dB value
#define SILENCE_POWER 1e-12f

// Fraction of the way to the target per hop for a time constant
static float smoothing_factor(float hop_seconds, float time_constant_ms) {
    return 1.f - expf(-hop_seconds / (time_constant_ms / 1000.f));
}

static float clamp01(float value) {
    return value < 0.f ? 0.f : value > 1.f ? 1.f : value;
}

bool features_init(features_state_t *this, size_t bin_count, float bin_hz,
                   float hop_seconds) {
    float ratio;

    if (bin_count < 2 || !(bin_hz > 0.f) || !(hop_seconds > 0.f))
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
    this->hop_seconds = hop_seconds;
    this->ceiling_db = FEATURES_MIN_CEILING_DB;
    this->ceiling_fall_db = FEATURES_CEILING_FALL_DB_PER_S * hop_seconds;
    this->attack_k = smoothing_factor(hop_seconds, FEATURES_ATTACK_MS);
    this->decay_k = smoothing_factor(hop_seconds, FEATURES_DECAY_MS);
    this->average_k = smoothing_factor(hop_seconds, FEATURES_BEAT_AVERAGE_MS);
    this->bass_average = 0.f;
    this->since_beat_s = FEATURES_BEAT_REFRACTORY_MS / 1000.f;
    this->out = (features_t){.bands = this->levels};

    return true;
}

// Mean bin power within the band, or the power interpolated at the band's
// centre when the band is narrower than a bin (the lowest bands)
static float band_power(const features_state_t *this, const float *bins,
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

// Bass energy above its moving average, once per refractory time, and only
// when audible above the auto-gain floor
static void detect_beat(features_state_t *this, const float *power,
                        float floor_db) {
    float bass = 0.f, trigger;

    for (size_t b = 0; b < this->bass_band_count; b++)
        bass += power[b];
    bass /= (float)this->bass_band_count;

    this->since_beat_s += this->hop_seconds;
    this->out.beat = false;
    this->out.beat_strength = 0.f;
    trigger = this->bass_average * FEATURES_BEAT_THRESHOLD;

    if (this->since_beat_s >= FEATURES_BEAT_REFRACTORY_MS / 1000.f &&
        bass > trigger && 10.f * log10f(bass + SILENCE_POWER) > floor_db) {
        this->out.beat = true;
        this->out.beat_strength =
            trigger > 0.f ? clamp01(bass / trigger - 1.f) : 1.f;
        this->since_beat_s = 0.f;
    }

    // Updated after the comparison, so a kick does not raise its own bar
    this->bass_average += (bass - this->bass_average) * this->average_k;
}

const features_t *features_update(features_state_t *this, const float *bins) {
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
    this->ceiling_db = fmaxf(this->ceiling_db, FEATURES_MIN_CEILING_DB);
    floor_db = this->ceiling_db - FEATURES_RANGE_DB;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        float target = clamp01((db[b] - floor_db) / FEATURES_RANGE_DB);
        float k = target > this->levels[b] ? this->attack_k : this->decay_k;

        this->levels[b] += (target - this->levels[b]) * k;
        sum += this->levels[b];
        weighted += this->levels[b] * (float)b;
    }

    this->out.loudness = sum / FEATURES_BAND_COUNT;
    this->out.centroid =
        sum > 1e-6f ? weighted / sum / (FEATURES_BAND_COUNT - 1) : 0.f;

    detect_beat(this, power, floor_db);

    return &this->out;
}

void features_deinit(features_state_t *this) { (void)this; }
