#include "check.h"
#include "features.h"

#include <math.h>
#include <stdint.h>

constexpr size_t BINS = 256;
constexpr float FS = 48828.125f;
constexpr float BIN_HZ = FS / 512.f;
constexpr float HOP_PERIOD_S = 256.f / FS;

static float bins[BINS];
static uint32_t random_state = 1;

static void fill(float magnitude) {
    for (size_t k = 0; k < BINS; k++)
        bins[k] = magnitude;
}

// xorshift32, 0 (exclusive) to 1 (inclusive)
static float random_unit(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (float)((random_state >> 8) + 1) / 16777216.f;
}

// Noise as the FFT sees it: Rayleigh magnitudes, so exponentially
// distributed bin power with the given mean in dB
static void fill_noise(float power_db) {
    float power = powf(10.f, power_db / 10.f);

    for (size_t k = 0; k < BINS; k++)
        bins[k] = sqrtf(-power * logf(random_unit()));
}

static size_t band_of(const features_t *features, float hz) {
    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        if (features->edges[b] <= hz && hz < features->edges[b + 1])
            return b;
    return FEATURES_BAND_COUNT;
}

static size_t loudest_band(const sound_t *sound) {
    size_t loudest = 0;

    for (size_t b = 1; b < FEATURES_BAND_COUNT; b++)
        if (sound->bands[b] > sound->bands[loudest])
            loudest = b;

    return loudest;
}

static void test_rejects_invalid(void) {
    features_t features;

    CHECK(!features_init(&features, 1, BIN_HZ, HOP_PERIOD_S));
    CHECK(!features_init(&features, BINS, 0.f, HOP_PERIOD_S));
    CHECK(!features_init(&features, BINS, BIN_HZ, 0.f));
}

static void test_tone_lands_in_its_band(void) {
    features_t features;
    const sound_t *sound = nullptr;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < 200; i++)
        sound = features_update(&features, bins);

    CHECK(loudest_band(sound) == band_of(&features, 10 * BIN_HZ));
    CHECK(sound->bands[band_of(&features, 10 * BIN_HZ)] > 0.95f);

    features_deinit(&features);
}

static void test_silence_stays_dark(void) {
    features_t features;
    const sound_t *sound = nullptr;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(0.f);
    for (int i = 0; i < 500; i++)
        sound = features_update(&features, bins);

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        CHECK(sound->bands[b] == 0.f);
    CHECK(sound->loudness == 0.f);
    CHECK(sound->centroid == 0.f);

    features_deinit(&features);
}

// A quiet room: the INMP441 self-noise (-87 dBFS) through the audio chain
// is about -88 dB of power per bin. Levels and loudness below 0.05 light
// nothing, the gamma table maps anything under 14.5 / 255 to 0
static void test_microphone_noise_stays_dark(void) {
    features_t features;
    float brightest = 0.f;
    int beats = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    for (int n = 0; n * HOP_PERIOD_S < 22.f; n++) {
        const sound_t *sound;

        fill_noise(-88.f);
        sound = features_update(&features, bins);

        if (n * HOP_PERIOD_S < 2.f)
            continue;

        if (sound->beat)
            beats++;
        brightest = fmaxf(brightest, sound->loudness);
        for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
            brightest = fmaxf(brightest, sound->bands[b]);
    }

    CHECK(brightest < 0.05f);
    CHECK(beats == 0);

    features_deinit(&features);
}

// A signal 20 dB quieter fills the range again once the ceiling fell, both
// above FEATURES_MIN_CEILING_DB
static void test_auto_gain(void) {
    features_t features;
    const sound_t *sound = nullptr;
    size_t band;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    band = band_of(&features, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 1.f;
    for (int i = 0; i < 1000; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.95f);

    bins[10] = 0.1f;
    for (int i = 0; i < 1000; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.95f);

    features_deinit(&features);
}

static void test_attack_faster_than_decay(void) {
    features_t features;
    const sound_t *sound;
    size_t band;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    band = band_of(&features, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.3f);

    for (int i = 0; i < 1000; i++)
        sound = features_update(&features, bins);

    bins[10] = 1e-6f;
    sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.9f);

    features_deinit(&features);
}

static void test_loudness_and_centroid(void) {
    features_t features;
    const sound_t *sound = nullptr;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    // Energy only low: centroid near 0; only high: near 1
    fill(1e-6f);
    bins[1] = 0.1f;
    for (int i = 0; i < 500; i++)
        sound = features_update(&features, bins);
    CHECK(sound->centroid < 0.2f);
    CHECK(sound->loudness > 0.f);

    fill(1e-6f);
    bins[120] = 0.1f;
    for (int i = 0; i < 500; i++)
        sound = features_update(&features, bins);
    CHECK(sound->centroid > 0.8f);

    features_deinit(&features);
}

// Kicks at 120 BPM: one beat per kick, on the kick. With noise mixed in
// 20 dB below the kicks too
static void test_beat_per_kick(void) {
    for (int noisy = 0; noisy < 2; noisy++) {
        features_t features;
        int beats = 0;

        CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

        for (int n = 0; n * HOP_PERIOD_S < 5.f; n++) {
            float t = n * HOP_PERIOD_S, since_kick = fmodf(t, 0.5f);
            const sound_t *sound;

            if (noisy)
                fill_noise(10.f * log10f(0.5f * 0.5f) - 20.f);
            else
                fill(1e-5f);
            bins[0] += 0.5f * expf(-since_kick / 0.05f);
            bins[1] += 0.5f * expf(-since_kick / 0.05f);
            sound = features_update(&features, bins);

            if (sound->beat && t >= 1.f) {
                beats++;
                CHECK(since_kick < 0.02f);
                CHECK(sound->beat_strength > 0.f);
                CHECK(sound->beat_strength <= 1.f);
            }
        }

        CHECK(beats == 8);

        features_deinit(&features);
    }
}

// Steady noise is not a beat at any level, however it fluctuates: fewer
// than 0.2 per second once settled
static void test_no_beat_on_noise(void) {
    const float levels_db[] = {-60.f, -40.f, -20.f};

    for (size_t l = 0; l < 3; l++) {
        features_t features;
        int beats = 0;

        CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

        for (int n = 0; n * HOP_PERIOD_S < 22.f; n++) {
            fill_noise(levels_db[l]);
            if (features_update(&features, bins)->beat &&
                n * HOP_PERIOD_S >= 2.f)
                beats++;
        }

        CHECK(beats < 4);
        if (beats >= 4)
            printf("noise at %.0f dB: %d beats in 20 s\n", levels_db[l],
                   beats);

        features_deinit(&features);
    }
}

// A steady bass tone is not a beat, nor is silence
static void test_no_beat_without_onsets(void) {
    features_t features;
    int beats = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(1e-5f);
    bins[1] = 0.3f;
    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++)
        if (features_update(&features, bins)->beat && n * HOP_PERIOD_S >= 2.f)
            beats++;
    CHECK(beats == 0);

    features_deinit(&features);

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(0.f);
    for (int n = 0; n < 1000; n++)
        if (features_update(&features, bins)->beat)
            beats++;
    CHECK(beats == 0);

    features_deinit(&features);
}

// Tuned to the defaults, the factors are the ones features_init() computed
static void test_default_tuning_changes_nothing(void) {
    features_t features;
    features_tuning_t defaults = features_default_tuning();
    float attack_k, decay_k;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    attack_k = features.attack_k;
    decay_k = features.decay_k;

    features_tune(&features, &defaults);
    CHECK(features.attack_k == attack_k);
    CHECK(features.decay_k == decay_k);
    CHECK(features.tuning.beat_threshold == FEATURES_BEAT_THRESHOLD);
    CHECK(features.tuning.min_ceiling_db == FEATURES_MIN_CEILING_DB);

    features_deinit(&features);
}

// Out of range fields keep their values, the others still apply
static void test_tuning_ignores_invalid(void) {
    features_t features;
    features_tuning_t tuning;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    tuning = features_default_tuning();
    tuning.attack_ms = 0.f;
    tuning.decay_ms = NAN;
    tuning.min_ceiling_db = INFINITY;
    tuning.beat_threshold = 4.f;
    features_tune(&features, &tuning);

    CHECK(features.tuning.attack_ms == FEATURES_ATTACK_MS);
    CHECK(features.tuning.decay_ms == FEATURES_DECAY_MS);
    CHECK(features.tuning.min_ceiling_db == FEATURES_MIN_CEILING_DB);
    CHECK(features.tuning.beat_threshold == 4.f);

    features_deinit(&features);
}

// Hops for a tone to bring its band to 0.63, with the given attack
static int hops_to_rise(float attack_ms) {
    features_t features;
    features_tuning_t tuning;
    size_t band;
    int hops = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    tuning = features_default_tuning();
    tuning.attack_ms = attack_ms;
    features_tune(&features, &tuning);
    band = band_of(&features, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    while (features_update(&features, bins)->bands[band] < 0.63f && hops < 1000)
        hops++;

    features_deinit(&features);

    return hops;
}

static void test_tuned_attack(void) {
    CHECK(hops_to_rise(50.f) > hops_to_rise(FEATURES_ATTACK_MS));
}

// Beats at 120 BPM with the given threshold, as in test_beat_per_kick
static int kick_beats(float beat_threshold) {
    features_t features;
    features_tuning_t tuning;
    int beats = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    tuning = features_default_tuning();
    tuning.beat_threshold = beat_threshold;
    features_tune(&features, &tuning);

    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++) {
        float t = n * HOP_PERIOD_S, since_kick = fmodf(t, 0.5f);

        fill(1e-5f);
        bins[0] += 0.5f * expf(-since_kick / 0.05f);
        bins[1] += 0.5f * expf(-since_kick / 0.05f);
        if (features_update(&features, bins)->beat && t >= 1.f)
            beats++;
    }

    features_deinit(&features);

    return beats;
}

// These kicks rise at most ~10 times above their average (the first one,
// while the average is still low): a threshold of 20 sees none
static void test_tuned_beat_threshold(void) {
    CHECK(kick_beats(FEATURES_BEAT_THRESHOLD) == 8);
    CHECK(kick_beats(20.f) == 0);
}

// A quiet tone shows with the default floor, not once the floor is raised
// above it
static void test_tuned_quiet_floor(void) {
    features_t features;
    features_tuning_t tuning;
    const sound_t *sound = nullptr;
    size_t band;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    band = band_of(&features, 10 * BIN_HZ);

    // About -46 dB, 0.6 up the default range
    fill(1e-6f);
    bins[10] = 5e-3f;
    for (int i = 0; i < 500; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.5f);

    tuning = features_default_tuning();
    tuning.min_ceiling_db = 0.f;
    features_tune(&features, &tuning);
    for (int i = 0; i < 500; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] < 0.1f);

    features_deinit(&features);
}

int main(void) {
    test_rejects_invalid();
    test_tone_lands_in_its_band();
    test_silence_stays_dark();
    test_microphone_noise_stays_dark();
    test_auto_gain();
    test_attack_faster_than_decay();
    test_loudness_and_centroid();
    test_beat_per_kick();
    test_no_beat_without_onsets();
    test_default_tuning_changes_nothing();
    test_tuning_ignores_invalid();
    test_tuned_attack();
    test_tuned_beat_threshold();
    test_tuned_quiet_floor();
    test_no_beat_on_noise();

    return CHECK_REPORT();
}
