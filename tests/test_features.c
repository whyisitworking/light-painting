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

        if (sound->hits[FEATURES_LOW].fired)
            beats++;
        brightest = fmaxf(brightest, sound->loudness);
        for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
            brightest = fmaxf(brightest, sound->bands[b]);
    }

    CHECK(brightest < 0.05f);
    CHECK(beats == 0);

    features_deinit(&features);
}

// The ceiling is set per song: a part 20 dB quieter stays visibly smaller
// for tens of seconds and fills the range again only after about a minute
// and a half (20 dB at 15 dB a minute), and a short spike barely moves it
static void test_auto_gain(void) {
    features_t features;
    const sound_t *sound = nullptr;
    size_t band;
    const int second = (int)(1.f / HOP_PERIOD_S);

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    band = band_of(&features, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 1.f;
    for (int i = 0; i < 5 * second; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.95f);

    // A 50 ms spike 20 dB louder, then the same level as before
    bins[10] = 10.f;
    for (int i = 0; i < second / 20; i++)
        features_update(&features, bins);
    bins[10] = 1.f;
    for (int i = 0; i < second; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] > 0.9f);

    // 20 dB quieter: still well below full after 20 s, full after 100 s
    bins[10] = 0.1f;
    for (int i = 0; i < 20 * second; i++)
        sound = features_update(&features, bins);
    CHECK(sound->bands[band] < 0.8f);
    for (int i = 0; i < 80 * second; i++)
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

// Kicks at 120 BPM: one low hit per kick, on the kick. With noise mixed in
// 20 dB below the kicks too
static void test_low_hit_per_kick(void) {
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

            if (sound->hits[FEATURES_LOW].fired && t >= 1.f) {
                beats++;
                CHECK(since_kick < 0.02f);
                CHECK(sound->hits[FEATURES_LOW].strength > 0.f);
                CHECK(sound->hits[FEATURES_LOW].strength <= 1.f);
            }
        }

        CHECK(beats == 8);

        features_deinit(&features);
    }
}

// Kicks over a steady bass line 14 dB below them, as in a drop: nearly
// every kick is a low hit (a rise over one hop missed half of them)
static void test_low_hits_over_bass(void) {
    features_t features;
    int hits = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++) {
        float t = n * HOP_PERIOD_S, since_kick = fmodf(t, 0.5f);

        fill(1e-5f);
        bins[1] = 0.1f;
        bins[0] += 0.5f * expf(-since_kick / 0.05f);
        bins[1] += 0.5f * expf(-since_kick / 0.05f);
        if (features_update(&features, bins)->hits[FEATURES_LOW].fired &&
            t >= 1.f)
            hits++;
    }

    printf("kicks over a bass line: %d low hits of 8\n", hits);
    CHECK(hits >= 7);

    features_deinit(&features);
}

// Steady noise is not a hit at any level, however it fluctuates: fewer
// than 0.2 per second once settled, in any region
static void test_no_hit_on_noise(void) {
    const float levels_db[] = {-60.f, -40.f, -20.f};

    for (size_t l = 0; l < 3; l++) {
        features_t features;
        int beats = 0;

        CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

        for (int n = 0; n * HOP_PERIOD_S < 22.f; n++) {
            const sound_t *sound;

            fill_noise(levels_db[l]);
            sound = features_update(&features, bins);
            for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
                if (sound->hits[r].fired && n * HOP_PERIOD_S >= 2.f)
                    beats++;
        }

        CHECK(beats < 4);
        if (beats >= 4)
            printf("noise at %.0f dB: %d beats in 20 s\n", levels_db[l],
                   beats);

        features_deinit(&features);
    }
}

// A steady bass tone is not a hit, nor is silence
static void test_no_hit_without_onsets(void) {
    features_t features;
    int beats = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(1e-5f);
    bins[1] = 0.3f;
    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++)
        if (features_update(&features, bins)->hits[FEATURES_LOW].fired && n * HOP_PERIOD_S >= 2.f)
            beats++;
    CHECK(beats == 0);

    features_deinit(&features);

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(0.f);
    for (int n = 0; n < 1000; n++)
        if (features_update(&features, bins)->hits[FEATURES_LOW].fired)
            beats++;
    CHECK(beats == 0);

    features_deinit(&features);
}

// The groove follows the loudness over FEATURES_GROOVE_MS: 63 % of the way
// after one time constant, all of it after ten, and back down in silence
static void test_groove(void) {
    features_t features;
    const sound_t *sound = nullptr;
    const int time_constant = (int)(FEATURES_GROOVE_MS / 1000.f / HOP_PERIOD_S);

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < time_constant; i++)
        sound = features_update(&features, bins);
    CHECK_NEAR(sound->groove, 0.63f * sound->loudness, 0.05f);

    for (int i = 0; i < 9 * time_constant; i++)
        sound = features_update(&features, bins);
    CHECK_NEAR(sound->groove, sound->loudness, 0.01f);

    fill(0.f);
    for (int i = 0; i < 5 * time_constant; i++)
        sound = features_update(&features, bins);
    CHECK(sound->groove < 0.01f);

    features_deinit(&features);
}

// Hits of each region over 5 s of bursts every half second at one bin,
// decaying over 30 ms: counted after the first second
static void count_region_hits(size_t bin, int hits[FEATURES_REGION_COUNT]) {
    features_t features;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
        hits[r] = 0;

    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++) {
        float t = n * HOP_PERIOD_S, since = fmodf(t, 0.5f);
        const sound_t *sound;

        fill(1e-5f);
        bins[bin] += 0.3f * expf(-since / 0.03f);
        sound = features_update(&features, bins);

        for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
            if (sound->hits[r].fired && t >= 1.f) {
                hits[r]++;
                CHECK(since < 0.02f);
            }
    }

    features_deinit(&features);
}

// A burst hits its own region once per burst, and no other: bin 10 is
// 954 Hz (mid), bin 90 is 8.6 kHz (high), bin 1 is 95 Hz (low)
static void test_regions(void) {
    int hits[FEATURES_REGION_COUNT];

    count_region_hits(1, hits);
    CHECK(hits[FEATURES_LOW] == 8);
    CHECK(hits[FEATURES_MID] == 0 && hits[FEATURES_HIGH] == 0);

    count_region_hits(10, hits);
    CHECK(hits[FEATURES_MID] == 8);
    CHECK(hits[FEATURES_LOW] == 0 && hits[FEATURES_HIGH] == 0);

    count_region_hits(90, hits);
    CHECK(hits[FEATURES_HIGH] == 8);
    CHECK(hits[FEATURES_LOW] == 0 && hits[FEATURES_MID] == 0);
}

// Every band is in at most one region, the regions in order, none empty
static void test_region_bands(void) {
    features_t features;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    for (size_t r = 0; r < FEATURES_REGION_COUNT; r++) {
        CHECK(features.regions[r].from < features.regions[r].to);
        CHECK(features.regions[r].to <= FEATURES_BAND_COUNT);
        if (r > 0)
            CHECK(features.regions[r].from >= features.regions[r - 1].to);
    }
    CHECK(features.edges[features.regions[FEATURES_LOW].to] <=
          FEATURES_LOW_MAX_HZ * 1.001f);
    CHECK(features.edges[features.regions[FEATURES_HIGH].from] >=
          FEATURES_HIGH_MIN_HZ);
    CHECK(features.regions[FEATURES_HIGH].to == FEATURES_BAND_COUNT);

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
    CHECK(features.tuning.hit_threshold == FEATURES_HIT_THRESHOLD);
    CHECK(features.tuning.min_ceiling_db == FEATURES_MIN_CEILING_DB);
    CHECK(features.tuning.song_parts);

    features_deinit(&features);
}

// Out of range fields keep their values, the others still apply
static void test_tuning_ignores_invalid(void) {
    features_t features;
    features_tuning_t tuning;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    tuning = features_default_tuning();
    tuning.min_ceiling_db = INFINITY;
    tuning.hit_threshold = 4.f;
    features_tune(&features, &tuning);
    CHECK(features.tuning.min_ceiling_db == FEATURES_MIN_CEILING_DB);
    CHECK(features.tuning.hit_threshold == 4.f);

    tuning.min_ceiling_db = -20.f;
    tuning.hit_threshold = NAN;
    features_tune(&features, &tuning);
    CHECK(features.tuning.min_ceiling_db == -20.f);
    CHECK(features.tuning.hit_threshold == 4.f);

    features_deinit(&features);
}

// Low hits at 120 BPM with the given threshold, over noise 20 dB below
// the kicks
static int kick_hits(float hit_threshold) {
    features_t features;
    features_tuning_t tuning;
    int beats = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));
    tuning = features_default_tuning();
    tuning.hit_threshold = hit_threshold;
    features_tune(&features, &tuning);

    for (int n = 0; n * HOP_PERIOD_S < 5.f; n++) {
        float t = n * HOP_PERIOD_S, since_kick = fmodf(t, 0.5f);

        // Noise 20 dB below the kicks, as in test_low_hit_per_kick
        fill_noise(10.f * log10f(0.5f * 0.5f) - 20.f);
        bins[0] += 0.5f * expf(-since_kick / 0.05f);
        bins[1] += 0.5f * expf(-since_kick / 0.05f);
        if (features_update(&features, bins)->hits[FEATURES_LOW].fired && t >= 1.f)
            beats++;
    }

    features_deinit(&features);

    return beats;
}

// Kicks 20 dB over noise stand far above the rise's average at the default
// threshold, not 60 times above it
static void test_tuned_hit_threshold(void) {
    int beats_default = kick_hits(FEATURES_HIT_THRESHOLD),
        beats_high = kick_hits(60.f);

    printf("kicks: %d hits at the default threshold, %d at 60\n",
           beats_default, beats_high);
    CHECK(beats_default == 8);
    CHECK(beats_high == 0);
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

// A bass after silence, far louder than a microphone can deliver: the
// auto-gain ceiling climbs about 90 dB within a second, and the silent mid and
// high bands, held at its floor, climb with it. That is not a rise: no mid or
// high hits. Real input climbs at most about 1 dB over two hops, under the
// least rise, so this is the guard at its limit
static void test_no_hit_from_the_floor_climbing(void) {
    features_t features;
    int others = 0;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP_PERIOD_S));

    for (int n = 0; n * HOP_PERIOD_S < 3.f; n++) {
        const sound_t *sound;

        fill(0.f);
        if (n * HOP_PERIOD_S >= 1.f)
            bins[1] = 1e3f;
        sound = features_update(&features, bins);
        others += sound->hits[FEATURES_MID].fired +
                  sound->hits[FEATURES_HIGH].fired;
    }

    printf("floor climbing: %d mid or high hits\n", others);
    CHECK(others == 0);

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
    test_low_hit_per_kick();
    test_low_hits_over_bass();
    test_no_hit_on_noise();
    test_no_hit_without_onsets();
    test_no_hit_from_the_floor_climbing();
    test_default_tuning_changes_nothing();
    test_tuning_ignores_invalid();
    test_tuned_hit_threshold();
    test_tuned_quiet_floor();
    test_groove();
    test_regions();
    test_region_bands();

    return CHECK_REPORT();
}
