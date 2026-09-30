/**
 * Latency bench: how long the whole pipeline (spectrum, features, show)
 * takes to react, and how often it reacts to nothing. Synthetic signals go
 * in as the I2S driver delivers them, and the sound of each hop is read
 * out. An analysis of hop h is complete when its last sample has arrived,
 * at (h + 1) hops, so that is when a reaction is timed.
 *
 * It prints what it measures, so a change of the analysis can be judged by
 * its numbers, and asserts bounds so that a change adding delay or false
 * hits fails. The latency bounds are 1.25 times the worst latency measured
 * with the current analysis, rounded up. The false hit bounds come from the
 * spread over many noise seeds: one seed is not evidence, the count of
 * false hits depends on the noise drawn.
 */

#include "check.h"
#include "signals.h"
#include "visualizer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

constexpr size_t FFT_SIZE = 512;
constexpr size_t HOP_SIZE = 256;
constexpr double FS = 48828.125;
constexpr size_t LEDS = 300;
constexpr double HOP_MS = 1000.0 * (double)HOP_SIZE / FS;

// Kicks at 120 BPM: a burst of 55 Hz decaying with a 40 ms time constant
constexpr double KICK_PERIOD_S = 0.5;

// The microphone's self-noise, and a noisy room, as sample amplitudes of
// 24-bit words
constexpr double QUIET_ROOM = 650.0;
constexpr double NOISY_ROOM = 20000.0;

// Bounds, see the header. Latencies are quantised to the 5.24 ms hop, so a
// slip of one hop fails by design: measured worst kick to low hit 6.4 ms
// (bound 8; the bass-only beat detector before the hits took 11.97 ms),
// tone step 8.44 ms (bound 11). False hits of any region in 58 s of noise
// 20000 over the 16 seeds run here: at most 1 per seed, 5 in total. The
// bounds are that maximum plus one
constexpr double MAX_HIT_LATENCY_MS = 8.0;
constexpr double MAX_BAND_LATENCY_MS = 11.0;
constexpr size_t MAX_FALSE_HITS_PER_SEED = 2;
constexpr size_t MAX_FALSE_HITS_TOTAL = 6;
constexpr size_t NOISE_SEEDS = 16;

// A tone of 1 kHz starting mid hop, after 20 quiet hops
constexpr size_t TONE_START = 20 * HOP_SIZE + 100;
constexpr size_t TONE_HOPS = 400;

// Fixed seed of the kick and tone signals' noise
constexpr uint32_t FIXED_SEED = 12345;

static double noise_amplitude;

typedef double sample_fn(size_t index, uint32_t *noise);
typedef void hop_fn(size_t hop, const sound_t *sound, void *context);

static size_t hops_for(double seconds) {
    return (size_t)(seconds * FS / (double)HOP_SIZE);
}

// Runs a signal through a default pipeline, hop by hop
static void run(sample_fn *sample, uint32_t seed, size_t hops, hop_fn *on_hop,
                void *context) {
    static int32_t frames[2 * HOP_SIZE];
    static uint32_t pixels[LEDS];
    visualizer_t visualizer;
    uint32_t noise = seed;

    CHECK(visualizer_init(&visualizer,
                          &(visualizer_config_t){
                              .sample_rate = (float)FS,
                              .fft_size = FFT_SIZE,
                              .hop_size = HOP_SIZE,
                              .led_count = LEDS,
                              .seed = 1,
                          }));

    for (size_t hop = 0; hop < hops; hop++) {
        for (size_t i = 0; i < HOP_SIZE; i++)
            signal_put_mono(frames, i, sample(hop * HOP_SIZE + i, &noise));

        visualizer_analyze(&visualizer, frames);
        on_hop(hop, visualizer_render(&visualizer, pixels), context);
    }

    visualizer_deinit(&visualizer);
}

static double kick_train(size_t index, uint32_t *noise) {
    double t = (double)index / FS, beat_t = fmod(t, KICK_PERIOD_S);

    return noise_amplitude * signal_noise(noise) +
           600000.0 * exp(-beat_t / 0.04) * sin(2.0 * M_PI * 55.0 * beat_t);
}

static double tone_step(size_t index, uint32_t *noise) {
    double t = (double)index / FS;

    return noise_amplitude * signal_noise(noise) +
           (index >= TONE_START ? 150000.0 * sin(2.0 * M_PI * 1000.0 * t)
                                : 0.0);
}

static double steady_noise(size_t index, uint32_t *noise) {
    (void)index;
    return noise_amplitude * signal_noise(noise);
}

// When each low hit was reported, in ms
typedef struct {
    double at_ms[512];
    size_t count;
} beats_t;

static void collect_beats(size_t hop, const sound_t *sound, void *context) {
    beats_t *beats = context;

    if (sound->hits[FEATURES_LOW].fired && beats->count < 512)
        beats->at_ms[beats->count++] = (double)(hop + 1) * HOP_MS;
}

static void sort(double *values, size_t count) {
    for (size_t i = 1; i < count; i++)
        for (size_t j = i; j > 0 && values[j - 1] > values[j]; j--) {
            double swap = values[j];

            values[j] = values[j - 1];
            values[j - 1] = swap;
        }
}

// The first low hit within 400 ms of each kick after the first 2 s, which
// the hit detection needs to settle
static void test_kicks(double noise) {
    static beats_t beats;
    double latency[32];
    size_t found = 0, missed = 0;

    noise_amplitude = noise;
    beats = (beats_t){};
    run(kick_train, FIXED_SEED, hops_for(12.0), collect_beats, &beats);

    for (size_t kick = 4; kick <= 22; kick++) {
        double onset_ms = 1000.0 * KICK_PERIOD_S * (double)kick;
        bool detected = false;

        for (size_t b = 0; b < beats.count && !detected; b++)
            if (beats.at_ms[b] >= onset_ms &&
                beats.at_ms[b] < onset_ms + 400.0) {
                latency[found++] = beats.at_ms[b] - onset_ms;
                detected = true;
            }

        missed += !detected;
    }

    sort(latency, found);
    printf("kicks over noise %6.0f: low hit after median %.1f ms, worst %.1f "
           "ms, "
           "missed %zu of %zu\n",
           noise, found > 0 ? latency[found / 2] : -1.0,
           found > 0 ? latency[found - 1] : -1.0, missed, found + missed);

    CHECK(missed == 0);
    CHECK(found > 0 && latency[found - 1] <= MAX_HIT_LATENCY_MS);
}

static float band_history[TONE_HOPS][FEATURES_BAND_COUNT];

static void record_bands(size_t hop, const sound_t *sound,
                         [[maybe_unused]] void *context) {
    memcpy(band_history[hop], sound->bands, sizeof(band_history[hop]));
}

// How long until the tone's band reaches half of its final level
static void test_tone_step(void) {
    size_t band = 0, hop;
    double onset_ms = 1000.0 * (double)TONE_START / FS, latency;
    float final_level;

    noise_amplitude = QUIET_ROOM;
    run(tone_step, FIXED_SEED, TONE_HOPS, record_bands, nullptr);

    for (size_t b = 1; b < FEATURES_BAND_COUNT; b++)
        if (band_history[TONE_HOPS - 1][b] > band_history[TONE_HOPS - 1][band])
            band = b;
    final_level = band_history[TONE_HOPS - 1][band];

    for (hop = TONE_START / HOP_SIZE; hop < TONE_HOPS; hop++)
        if (band_history[hop][band] >= 0.5f * final_level)
            break;
    latency = (double)(hop + 1) * HOP_MS - onset_ms;

    printf("tone step, band %zu: half level after %.1f ms (final %.2f)\n", band,
           latency, (double)final_level);

    CHECK(final_level > 0.5f);
    CHECK(latency <= MAX_BAND_LATENCY_MS);
}

// The seed of the k-th noise sequence, never zero, spread over the range
static uint32_t seed_for(size_t k) {
    return 2654435761u * (uint32_t)(k + 1);
}

// Hits of any region
static void count_beats(size_t hop, const sound_t *sound, void *context) {
    if (hop >= hops_for(2.0))
        for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
            *(size_t *)context += sound->hits[r].fired;
}

// False hits in 58 s of steady noise of one seed
static size_t false_beats(double noise, uint32_t seed) {
    size_t beats = 0;

    noise_amplitude = noise;
    run(steady_noise, seed, hops_for(60.0), count_beats, &beats);
    return beats;
}

// Steady noise is not a hit, at any level: the hits are measured in dB, so
// a louder room gives the same counts
static void test_steady_noise(double noise) {
    size_t total = 0, worst = 0;

    for (size_t k = 0; k < NOISE_SEEDS; k++) {
        size_t beats = false_beats(noise, seed_for(k));

        total += beats;
        worst = beats > worst ? beats : worst;
    }

    printf("steady noise %6.0f: %zu false hits in 58 s over %zu seeds, at "
           "most %zu per seed\n",
           noise, total, (size_t)NOISE_SEEDS, worst);

    CHECK(worst <= MAX_FALSE_HITS_PER_SEED);
    CHECK(total <= MAX_FALSE_HITS_TOTAL);
}

int main(void) {
    test_kicks(QUIET_ROOM);
    test_kicks(NOISY_ROOM);
    test_tone_step();
    test_steady_noise(QUIET_ROOM);
    test_steady_noise(NOISY_ROOM);

    return CHECK_REPORT();
}
