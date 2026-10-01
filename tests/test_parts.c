#include "check.h"
#include "features.h"
#include "parts.h"
#include "signals.h"
#include "song.h"
#include "spectrum.h"

#include <stdio.h>

constexpr float FS = 48828.125f;
constexpr size_t FFT_SIZE = 512;
constexpr size_t HOP = 256;
constexpr float HOP_PERIOD_S = (float)HOP / FS;

static const char *const names[PARTS_COUNT] = {"calm", "build", "gap",
                                               "high"};

// What a run saw: when each part was first entered after a time, the
// events, and the part at given times
typedef struct {
    float first[PARTS_COUNT][4];
    size_t entries[PARTS_COUNT];
    float drops[8];
    size_t drop_count;
    size_t lift_count;
} timeline_t;

static void note(timeline_t *timeline, float t, parts_part_t part,
                 parts_part_t previous, parts_event_t event, bool print) {
    if (part != previous && timeline->entries[part] < 4)
        timeline->first[part][timeline->entries[part]++] = t;
    if (event == PARTS_DROP && timeline->drop_count < 8)
        timeline->drops[timeline->drop_count++] = t;
    timeline->lift_count += event == PARTS_LIFT;

    if (print && (part != previous || event != PARTS_NONE))
        printf("  %6.2f s %s%s\n", (double)t, names[part],
               event == PARTS_DROP   ? " DROP"
               : event == PARTS_LIFT ? " LIFT"
                                     : "");
}

// The synthetic song (lib/song) through the real spectrum and features, two
// loops: calm, build, gap, drop, high, calm, twice, and nothing else
static void test_song(void) {
    static int32_t frames[2 * HOP];
    static timeline_t timeline;
    spectrum_t spectrum;
    features_t features;
    song_t song;
    parts_part_t previous = PARTS_CALM, at_30 = PARTS_COUNT,
                 at_40 = PARTS_COUNT, at_55 = PARTS_COUNT,
                 at_10 = PARTS_COUNT;

    CHECK(spectrum_init(&spectrum, FFT_SIZE, HOP));
    CHECK(features_init(&features, spectrum_bin_count(&spectrum),
                        FS / (float)FFT_SIZE, HOP_PERIOD_S));
    CHECK(song_init(&song, FS, 7));

    printf("song parts:\n");
    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 2.f * SONG_LENGTH_S;
         hop++) {
        float t = (float)(hop + 1) * HOP_PERIOD_S;
        const sound_t *sound;

        song_fill(&song, frames, HOP);
        spectrum_analyze(&spectrum, frames, 1.5f);
        sound = features_update(&features, spectrum_bins(&spectrum));
        note(&timeline, t, sound->part, previous, sound->event, true);
        previous = sound->part;

        if (hop == (size_t)(10.f / HOP_PERIOD_S))
            at_10 = sound->part;
        if (hop == (size_t)(30.f / HOP_PERIOD_S))
            at_30 = sound->part;
        if (hop == (size_t)(40.f / HOP_PERIOD_S))
            at_40 = sound->part;
        if (hop == (size_t)(55.f / HOP_PERIOD_S))
            at_55 = sound->part;
    }

    CHECK(at_10 == PARTS_CALM);
    CHECK(timeline.entries[PARTS_BUILD] == 2);
    CHECK(timeline.first[PARTS_BUILD][0] >= SONG_CALM_END_S &&
          timeline.first[PARTS_BUILD][0] < SONG_CALM_END_S + 4.f);
    CHECK(timeline.first[PARTS_BUILD][1] >= SONG_LENGTH_S + SONG_CALM_END_S &&
          timeline.first[PARTS_BUILD][1] <
              SONG_LENGTH_S + SONG_CALM_END_S + 4.f);
    CHECK(timeline.entries[PARTS_GAP] == 2);
    CHECK(timeline.first[PARTS_GAP][0] >= SONG_BUILD_END_S &&
          timeline.first[PARTS_GAP][0] < SONG_BUILD_END_S + 0.8f);
    CHECK(timeline.drop_count == 2);
    CHECK(timeline.drops[0] >= SONG_GAP_END_S &&
          timeline.drops[0] < SONG_GAP_END_S + 0.6f);
    CHECK(timeline.drops[1] >= SONG_LENGTH_S + SONG_GAP_END_S &&
          timeline.drops[1] < SONG_LENGTH_S + SONG_GAP_END_S + 0.6f);
    CHECK(at_30 == PARTS_HIGH);
    CHECK(at_40 == PARTS_HIGH);
    CHECK(at_55 == PARTS_CALM);
    CHECK(timeline.lift_count == 0);

    features_deinit(&features);
    spectrum_deinit(&spectrum);
}

// Feeds a parts_t seconds of one input, noting what happens
static void feed(parts_t *parts, timeline_t *timeline, float *t,
                 float seconds, float level_db, float loudness,
                 bool first_hop_low_hit) {
    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < seconds; hop++) {
        parts_part_t previous = parts->part;

        *t += HOP_PERIOD_S;
        parts_update(parts, level_db, loudness, 0.3f,
                     first_hop_low_hit && hop == 0, false);
        note(timeline, *t, parts->part, previous, parts->event, false);
    }
}

// A steady song is calm and stays so; a quiet start is not a gap
static void test_steady(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 5.f, -120.f, 0.f, false);
    feed(&parts, &timeline, &t, 60.f, -50.f, 0.4f, false);

    CHECK(parts.part == PARTS_CALM);
    CHECK(timeline.entries[PARTS_GAP] == 0);
    CHECK(timeline.entries[PARTS_BUILD] == 0);
    CHECK(timeline.drop_count == 0 && timeline.lift_count == 0);
}

// A jump 10 dB up with no build is a lift into high, never a drop, even
// with a low hit on it
static void test_lift(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -60.f, 0.3f, false);
    feed(&parts, &timeline, &t, 10.f, -50.f, 0.6f, true);

    CHECK(parts.part == PARTS_HIGH);
    CHECK(timeline.lift_count == 1);
    CHECK(timeline.drop_count == 0);
}

// Loud, half a second of silence, loud again with a low hit: a gap then a
// drop. The same 5 s later: a gap, but no drop so soon after the last
static void test_gap_drop_spacing(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -50.f, 0.5f, false);
    feed(&parts, &timeline, &t, 0.5f, -120.f, 0.f, false);
    feed(&parts, &timeline, &t, 5.f, -50.f, 0.5f, true);
    feed(&parts, &timeline, &t, 0.5f, -120.f, 0.f, false);
    feed(&parts, &timeline, &t, 5.f, -50.f, 0.5f, true);

    CHECK(timeline.entries[PARTS_GAP] == 2);
    CHECK(timeline.drop_count == 1);
    CHECK(timeline.drops[0] > 20.5f && timeline.drops[0] < 20.6f);
}

// Long silence after sound: a gap, then calm after PARTS_GAP_MAX_S
static void test_gap_ends(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -50.f, 0.5f, false);
    feed(&parts, &timeline, &t, 5.f, -120.f, 0.f, false);

    CHECK(timeline.entries[PARTS_GAP] == 1);
    CHECK(parts.part == PARTS_CALM);
}

// Disabled: the same as test_gap_drop_spacing and test_lift gives no gap,
// no build and no events, only calm and high
static void test_disabled(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    parts_enable(&parts, false);
    feed(&parts, &timeline, &t, 20.f, -60.f, 0.3f, false);
    feed(&parts, &timeline, &t, 0.5f, -120.f, 0.f, false);
    feed(&parts, &timeline, &t, 10.f, -50.f, 0.6f, true);

    CHECK(timeline.entries[PARTS_GAP] == 0);
    CHECK(timeline.entries[PARTS_BUILD] == 0);
    CHECK(timeline.drop_count == 0 && timeline.lift_count == 0);
    CHECK(parts.part == PARTS_HIGH);
}

// Calm, then a louder part (a lift into high): where the next tests start
static void loud_part(parts_t *parts, float *t) {
    static timeline_t before;

    parts_init(parts, HOP_PERIOD_S);
    feed(parts, &before, t, 30.f, -60.f, 0.4f, false);
    feed(parts, &before, t, 20.f, -50.f, 0.6f, false);
    CHECK(parts->part == PARTS_HIGH);
}

// A pause in a loud part: a gap, the song ends into calm, and what follows,
// 20 dB quieter, is a new song: calm, and no lift in the silence or after
static void test_silence_then_quieter(void) {
    static timeline_t after;
    parts_t parts;
    float t = 0.f;

    loud_part(&parts, &t);
    feed(&parts, &after, &t, 20.f, -120.f, 0.f, false);
    CHECK(parts.part == PARTS_CALM);
    feed(&parts, &after, &t, 20.f, -70.f, 0.3f, false);

    CHECK(after.entries[PARTS_GAP] == 1);
    CHECK(after.entries[PARTS_HIGH] == 0);
    CHECK(after.lift_count == 0 && after.drop_count == 0);
    CHECK(parts.part == PARTS_CALM);
}

// A loud part, then at once one far quieter, more than PARTS_SILENT_DB under
// the typical level: held at first, then after PARTS_QUIET_HOLD_S a new song,
// calm, with no event
static void test_quiet_after_loud(void) {
    static timeline_t after;
    parts_t parts;
    float t = 0.f;

    loud_part(&parts, &t);
    feed(&parts, &after, &t, PARTS_QUIET_HOLD_S - 1.f, -80.f, 0.3f, false);
    CHECK(parts.part == PARTS_HIGH);
    feed(&parts, &after, &t, 10.f, -80.f, 0.3f, false);

    CHECK(parts.part == PARTS_CALM);
    CHECK(after.lift_count == 0 && after.drop_count == 0);
}

// Silence and the quiet hold taking turns, the loudness hovering about
// PARTS_SILENT far under the song (after a fade, so no gap): together they
// hold the averages, and together they end the song after
// PARTS_QUIET_HOLD_S
static void test_hovering_quiet(void) {
    static timeline_t after;
    parts_t parts;
    float t = 0.f;

    loud_part(&parts, &t);
    feed(&parts, &after, &t, 10.f, -50.f, 0.1f, false);
    for (int turn = 0; turn < 60; turn++)
        feed(&parts, &after, &t, 50.f * HOP_PERIOD_S, -80.f,
             turn % 2 == 0 ? 0.015f : 0.03f, false);

    CHECK(after.entries[PARTS_GAP] == 0);
    CHECK(parts.part == PARTS_CALM);
    CHECK(after.lift_count == 0 && after.drop_count == 0);
}

// A lift waiting for its beat is called off while silence holds the
// averages: after it the wait starts over, and 1.5 s more is not enough
static void test_lift_off_in_silence(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -60.f, 0.3f, false);
    for (size_t hop = 0; !parts.lift_waiting && hop < 4000; hop++)
        parts_update(&parts, -50.f, 0.6f, 0.3f, false, false);
    CHECK(parts.lift_waiting);

    feed(&parts, &timeline, &t, 1.f, -50.f, 0.6f, false);
    feed(&parts, &timeline, &t, 2.f, -85.f, 0.3f, false);
    feed(&parts, &timeline, &t, 1.5f, -50.f, 0.6f, false);

    CHECK(timeline.lift_count == 0);
    CHECK(parts.part == PARTS_CALM);
}

// A quiet build (not loud enough before for a gap), then silence: no other
// part in the silence but the calm the song ends in, and no event
static void test_silence_in_a_build(void) {
    static timeline_t before, after;
    parts_t parts;
    float t = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &before, &t, 30.f, -60.f, 0.1f, false);
    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 6.f; hop++) {
        parts_part_t previous = parts.part;
        float rise = (float)hop * HOP_PERIOD_S;

        t += HOP_PERIOD_S;
        parts_update(&parts, -60.f + 2.f * rise, 0.1f, 0.3f + 0.05f * rise,
                     false, hop % 20 == 0);
        note(&before, t, parts.part, previous, parts.event, false);
    }
    CHECK(parts.part == PARTS_BUILD);

    feed(&parts, &after, &t, 20.f, -120.f, 0.f, false);

    CHECK(after.entries[PARTS_HIGH] == 0 && after.entries[PARTS_BUILD] == 0);
    CHECK(after.entries[PARTS_CALM] == 1);
    CHECK(after.lift_count == 0 && after.drop_count == 0);
    CHECK(parts.part == PARTS_CALM);
}

// A lift lands on a low hit, the beat, not whenever the level decides: low
// hits every half second after a 10 dB jump
static void test_lift_on_the_beat(void) {
    static timeline_t timeline;
    parts_t parts;
    float t = 0.f;
    bool on_hit = false;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -60.f, 0.3f, false);
    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 10.f; hop++) {
        parts_part_t previous = parts.part;
        bool low_hit = hop % 96 == 37;

        t += HOP_PERIOD_S;
        parts_update(&parts, -50.f, 0.6f, 0.3f, low_hit, false);
        note(&timeline, t, parts.part, previous, parts.event, false);
        if (parts.event == PARTS_LIFT)
            on_hit = low_hit;
    }

    CHECK(timeline.lift_count == 1);
    CHECK(on_hit);
    CHECK(parts.part == PARTS_HIGH);
}

// Without a low hit the lift still comes, PARTS_LIFT_WAIT_S late at most
static void test_lift_without_a_beat(void) {
    static timeline_t timeline;
    parts_t parts, alone;
    float t = 0.f, t_alone = 0.f, lifted = 0.f, level = 0.f;

    parts_init(&parts, HOP_PERIOD_S);
    feed(&parts, &timeline, &t, 20.f, -60.f, 0.3f, false);
    alone = parts;
    t_alone = t;
    // When the level alone would lift: the same input, a hit on every hop
    for (size_t hop = 0; level == 0.f && hop < 2000; hop++) {
        t_alone += HOP_PERIOD_S;
        parts_update(&alone, -50.f, 0.6f, 0.3f, true, false);
        if (alone.event == PARTS_LIFT)
            level = t_alone;
    }
    for (size_t hop = 0; lifted == 0.f && hop < 4000; hop++) {
        t += HOP_PERIOD_S;
        parts_update(&parts, -50.f, 0.6f, 0.3f, false, false);
        if (parts.event == PARTS_LIFT)
            lifted = t;
    }

    CHECK(level > 0.f && lifted > 0.f);
    CHECK(lifted - level > PARTS_LIFT_WAIT_S - 2.f * HOP_PERIOD_S &&
          lifted - level < PARTS_LIFT_WAIT_S + 2.f * HOP_PERIOD_S);
}

// Steady noise through the spectrum and features; at 30 s the Gain triples
// (+9.5 dB), and the features are told: the parts take their level before
// the Gain, so nothing changes
static void test_gain_change(void) {
    static int32_t frames[2 * HOP];
    spectrum_t spectrum;
    features_t features;
    uint32_t seed = 3;
    size_t lifts = 0, highs = 0;

    CHECK(spectrum_init(&spectrum, FFT_SIZE, HOP));
    CHECK(features_init(&features, spectrum_bin_count(&spectrum),
                        FS / (float)FFT_SIZE, HOP_PERIOD_S));

    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 60.f; hop++) {
        float gain = (float)hop * HOP_PERIOD_S < 30.f ? 1.5f : 4.5f;
        const sound_t *sound;

        for (size_t i = 0; i < HOP; i++)
            signal_put_mono(frames, i, 524288.0 * signal_noise(&seed));
        features_set_gain(&features, gain);
        spectrum_analyze(&spectrum, frames, gain);
        sound = features_update(&features, spectrum_bins(&spectrum));
        lifts += sound->event == PARTS_LIFT;
        highs += sound->part == PARTS_HIGH;
    }

    CHECK(lifts == 0 && highs == 0);

    features_deinit(&features);
    spectrum_deinit(&spectrum);
}

// The features pass the parts on, and the tuning turns them off
static void test_features_tuning(void) {
    features_t features;
    features_tuning_t tuning;

    CHECK(features_init(&features, FFT_SIZE / 2, FS / (float)FFT_SIZE,
                        HOP_PERIOD_S));
    CHECK(features.tuning.song_parts);
    CHECK(features.parts.enabled);

    tuning = features_default_tuning();
    tuning.song_parts = false;
    features_tune(&features, &tuning);
    CHECK(!features.parts.enabled);

    features_deinit(&features);
}

int main(void) {
    test_song();
    test_steady();
    test_lift();
    test_gap_drop_spacing();
    test_gap_ends();
    test_disabled();
    test_silence_then_quieter();
    test_quiet_after_loud();
    test_hovering_quiet();
    test_lift_off_in_silence();
    test_silence_in_a_build();
    test_lift_on_the_beat();
    test_lift_without_a_beat();
    test_gain_change();
    test_features_tuning();

    return CHECK_REPORT();
}
