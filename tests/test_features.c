#include "check.h"
#include "features.h"

#include <math.h>
#include <string.h>

#define BINS 256
#define FS 48828.125f
#define BIN_HZ (FS / 512.f)
#define HOP (256.f / FS)

static float bins[BINS];

static void fill(float magnitude) {
    for (size_t k = 0; k < BINS; k++)
        bins[k] = magnitude;
}

static size_t band_of(const features_state_t *state, float hz) {
    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        if (state->edges[b] <= hz && hz < state->edges[b + 1])
            return b;
    return FEATURES_BAND_COUNT;
}

static size_t loudest_band(const features_t *features) {
    size_t loudest = 0;

    for (size_t b = 1; b < FEATURES_BAND_COUNT; b++)
        if (features->bands[b] > features->bands[loudest])
            loudest = b;

    return loudest;
}

static void test_rejects_invalid(void) {
    features_state_t state;

    CHECK(!features_init(&state, 1, BIN_HZ, HOP));
    CHECK(!features_init(&state, BINS, 0.f, HOP));
    CHECK(!features_init(&state, BINS, BIN_HZ, 0.f));
}

static void test_tone_lands_in_its_band(void) {
    features_state_t state;
    const features_t *features = NULL;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < 200; i++)
        features = features_update(&state, bins);

    CHECK(loudest_band(features) == band_of(&state, 10 * BIN_HZ));
    CHECK(features->bands[band_of(&state, 10 * BIN_HZ)] > 0.95f);

    features_deinit(&state);
}

static void test_silence_and_noise_stay_dark(void) {
    const float levels[] = {0.f, 1e-5f};

    for (size_t l = 0; l < 2; l++) {
        features_state_t state;
        const features_t *features = NULL;

        CHECK(features_init(&state, BINS, BIN_HZ, HOP));

        fill(levels[l]);
        for (int i = 0; i < 500; i++)
            features = features_update(&state, bins);

        for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
            CHECK(features->bands[b] == 0.f);
        CHECK(features->loudness == 0.f);
        CHECK(features->centroid == 0.f);

        features_deinit(&state);
    }
}

// A signal 20 dB quieter fills the range again once the ceiling fell
static void test_auto_gain(void) {
    features_state_t state;
    const features_t *features = NULL;
    size_t band;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));
    band = band_of(&state, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.95f);

    bins[10] = 0.01f;
    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.95f);

    features_deinit(&state);
}

static void test_attack_faster_than_decay(void) {
    features_state_t state;
    const features_t *features;
    size_t band;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));
    band = band_of(&state, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.3f);

    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);

    bins[10] = 1e-6f;
    features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.9f);

    features_deinit(&state);
}

static void test_loudness_and_centroid(void) {
    features_state_t state;
    const features_t *features = NULL;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    // Energy only low: centroid near 0; only high: near 1
    fill(1e-6f);
    bins[1] = 0.1f;
    for (int i = 0; i < 500; i++)
        features = features_update(&state, bins);
    CHECK(features->centroid < 0.2f);
    CHECK(features->loudness > 0.f);

    fill(1e-6f);
    bins[120] = 0.1f;
    for (int i = 0; i < 500; i++)
        features = features_update(&state, bins);
    CHECK(features->centroid > 0.8f);

    features_deinit(&state);
}

// Kicks at 120 BPM: one beat per kick, on the kick
static void test_beat_per_kick(void) {
    features_state_t state;
    int beats = 0;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    for (int n = 0; n * HOP < 5.f; n++) {
        float t = n * HOP, since_kick = fmodf(t, 0.5f);
        const features_t *features;

        fill(1e-5f);
        bins[0] = bins[1] = 0.5f * expf(-since_kick / 0.05f);
        features = features_update(&state, bins);

        if (features->beat && t >= 1.f) {
            beats++;
            CHECK(since_kick < 0.02f);
            CHECK(features->beat_strength > 0.f);
            CHECK(features->beat_strength <= 1.f);
        }
    }

    CHECK(beats == 8);

    features_deinit(&state);
}

// A steady bass tone is not a beat, nor is silence
static void test_no_beat_without_onsets(void) {
    features_state_t state;
    int beats = 0;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(1e-5f);
    bins[1] = 0.3f;
    for (int n = 0; n * HOP < 5.f; n++)
        if (features_update(&state, bins)->beat && n * HOP >= 2.f)
            beats++;
    CHECK(beats == 0);

    features_deinit(&state);

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(0.f);
    for (int n = 0; n < 1000; n++)
        if (features_update(&state, bins)->beat)
            beats++;
    CHECK(beats == 0);

    features_deinit(&state);
}

int main(void) {
    test_rejects_invalid();
    test_tone_lands_in_its_band();
    test_silence_and_noise_stay_dark();
    test_auto_gain();
    test_attack_faster_than_decay();
    test_loudness_and_centroid();
    test_beat_per_kick();
    test_no_beat_without_onsets();

    return CHECK_REPORT();
}
