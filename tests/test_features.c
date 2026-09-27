#include "check.h"
#include "features.h"

#include <math.h>
#include <stdint.h>

#define BINS 256
#define FS 48828.125f
#define BIN_HZ (FS / 512.f)
#define HOP (256.f / FS)

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

    CHECK(!features_init(&features, 1, BIN_HZ, HOP));
    CHECK(!features_init(&features, BINS, 0.f, HOP));
    CHECK(!features_init(&features, BINS, BIN_HZ, 0.f));
}

static void test_tone_lands_in_its_band(void) {
    features_t features;
    const sound_t *sound = NULL;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

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
    const sound_t *sound = NULL;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

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

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

    for (int n = 0; n * HOP < 22.f; n++) {
        const sound_t *sound;

        fill_noise(-88.f);
        sound = features_update(&features, bins);

        if (n * HOP < 2.f)
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
    const sound_t *sound = NULL;
    size_t band;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));
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

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));
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
    const sound_t *sound = NULL;

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

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

        CHECK(features_init(&features, BINS, BIN_HZ, HOP));

        for (int n = 0; n * HOP < 5.f; n++) {
            float t = n * HOP, since_kick = fmodf(t, 0.5f);
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

        CHECK(features_init(&features, BINS, BIN_HZ, HOP));

        for (int n = 0; n * HOP < 22.f; n++) {
            fill_noise(levels_db[l]);
            if (features_update(&features, bins)->beat && n * HOP >= 2.f)
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

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

    fill(1e-5f);
    bins[1] = 0.3f;
    for (int n = 0; n * HOP < 5.f; n++)
        if (features_update(&features, bins)->beat && n * HOP >= 2.f)
            beats++;
    CHECK(beats == 0);

    features_deinit(&features);

    CHECK(features_init(&features, BINS, BIN_HZ, HOP));

    fill(0.f);
    for (int n = 0; n < 1000; n++)
        if (features_update(&features, bins)->beat)
            beats++;
    CHECK(beats == 0);

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
    test_no_beat_on_noise();

    return CHECK_REPORT();
}
