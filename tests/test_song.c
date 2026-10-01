#include "check.h"
#include "song.h"

#include <math.h>
#include <string.h>

constexpr float FS = 48828.125f;
constexpr size_t HOP = 256;

// RMS of the samples between two times, in dB of 24-bit units; -INFINITY
// for silence. Runs a fresh song from the start
static double rms_db(float from_s, float to_s) {
    static int32_t frames[2 * HOP];
    song_t song;
    size_t from = (size_t)(from_s * FS), to = (size_t)(to_s * FS);
    double sum = 0.0;
    size_t count = 0;

    CHECK(song_init(&song, FS, 7));

    for (size_t n = 0; n < to; n += HOP) {
        song_fill(&song, frames, HOP);
        for (size_t i = 0; i < HOP; i++)
            if (n + i >= from && n + i < to) {
                double sample = (double)(frames[2 * i] >> 7);

                sum += sample * sample;
                count++;
            }
    }

    return 10.0 * log10(sum / (double)count);
}

static void test_rejects_invalid(void) {
    song_t song;

    CHECK(!song_init(&song, 0.f, 1));
    CHECK(!song_init(&song, -1.f, 1));
    CHECK(song_init(&song, FS, 0));
}

// The same seed gives the same song, left and right alike
static void test_deterministic(void) {
    static int32_t a[2 * HOP], b[2 * HOP];
    song_t one, two;
    size_t differing = 0, unequal_sides = 0;

    CHECK(song_init(&one, FS, 3));
    CHECK(song_init(&two, FS, 3));

    for (size_t hop = 0; hop < 2000; hop++) {
        song_fill(&one, a, HOP);
        song_fill(&two, b, HOP);
        differing += memcmp(a, b, sizeof(a)) != 0;
        for (size_t i = 0; i < HOP; i++)
            unequal_sides += a[2 * i] != a[2 * i + 1];
    }

    CHECK(differing == 0);
    CHECK(unequal_sides == 0);
}

// The sections sound as the header says, and the loop starts over
static void test_sections(void) {
    double calm = rms_db(2.f, 11.f), build_start = rms_db(12.f, 14.f),
           build_end = rms_db(22.f, 24.f), drop = rms_db(27.f, 44.f),
           calm_again = rms_db(46.f, 48.f), next_build = rms_db(70.f, 72.f);

    printf("song: calm %.1f dB, build %.1f to %.1f dB, drop %.1f dB, calm "
           "again %.1f dB\n",
           calm, build_start, build_end, drop, calm_again);

    CHECK(isinf(rms_db(24.05f, 24.95f)));
    CHECK(isinf(rms_db(72.05f, 72.95f)));
    CHECK(drop > calm + 10.0);
    CHECK(build_end > build_start + 6.0);
    CHECK(fabs(calm_again - calm) < 3.0);
    CHECK(fabs(next_build - build_end) < 3.0);
}

int main(void) {
    test_rejects_invalid();
    test_deterministic();
    test_sections();

    return CHECK_REPORT();
}
