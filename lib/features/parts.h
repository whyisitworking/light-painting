#ifndef PARTS_H
#define PARTS_H

/**
 * Song parts: where in the song the sound is, from a few running averages
 * and a small state machine, once per hop. Nothing is recorded.
 *
 * The level is the mean of the bands' dB before the auto-gain and before the
 * Gain (features_set_gain()): the auto-gain follows a loud part within
 * seconds, so its loudness would call a long drop calm, and the power summed
 * over the bands is the kick's alone, deaf to a build's rising noise and
 * snares. The loudness (after the auto-gain) only
 * tells silence.
 *
 *   calm   the recent level (PARTS_RECENT_MS) below the song's typical level
 *          (PARTS_TYPICAL_MS)
 *   build  the level rising for PARTS_BUILD_MIN_S, with the mid and high hits
 *          more frequent or the tone brighter
 *   gap    near silence within a fraction of a second, after sound
 *   high   the level well above typical
 *
 * and two events, each on one hop: a drop (a low hit with the level jumping,
 * during or right after a build or a gap, into high) and a lift (from calm
 * to high with no build before it, and none while the level is still rising,
 * which may yet be a build; it lands on the next low hit). Safeguards: a
 * drop needs a build or a gap first, none within PARTS_DROP_SPACING_S of
 * the last, changes between calm and high need a clear margin and
 * PARTS_MIN_PART_S in the part, a gap only when the room is nearly silent,
 * and silence changes no part but into a gap, or at the song's end into
 * calm. Disabled, the part is calm or high by the level alone, with no
 * build, gap, drop or lift.
 *
 * Every number is a first guess, to be tuned on songs with the timeline
 * tool (tools/timeline)
 */

#include <stddef.h>

// Running averages of the level: fast for gaps and drops, short and recent
// for the rise of a build, typical for the song's usual level. Each starts as
// the mean of what it heard so far, so none starts from nothing
constexpr float PARTS_FAST_MS = 100.f;
constexpr float PARTS_SHORT_MS = 1000.f;
constexpr float PARTS_RECENT_MS = 4000.f;
constexpr float PARTS_TYPICAL_MS = 45000.f;

// Below this loudness (after the auto-gain, 0..1), or PARTS_SILENT_DB below
// the typical level, the room counts as silent: the averages hold still, so
// a pause or a fade into one does not drag the song's levels down, and the
// part stays (a gap may still begin). Held so long, the averages no longer
// say anything: PARTS_GAP_MAX_S under PARTS_SILENT, or PARTS_QUIET_HOLD_S
// held by either kind of silence, ends the song, into calm with every
// average starting over from what comes next
constexpr float PARTS_SILENT = 0.02f;
constexpr float PARTS_SILENT_DB = 20.f;
constexpr float PARTS_QUIET_HOLD_S = 10.f;

// High from PARTS_HIGH_ENTER_DB above typical, calm again from
// PARTS_HIGH_LEAVE_DB below, after at least PARTS_MIN_PART_S in the part.
// Calm to high is a lift, on the beat: it waits for the next low hit, at
// most PARTS_LIFT_WAIT_S, and is called off if the level falls back, a
// rise begins or silence holds the averages
constexpr float PARTS_HIGH_ENTER_DB = 2.f;
constexpr float PARTS_HIGH_LEAVE_DB = 1.f;
constexpr float PARTS_MIN_PART_S = 4.f;
constexpr float PARTS_LIFT_WAIT_S = 2.f;

// A build: the short average PARTS_BUILD_RISE_DB above the recent one, and
// the mid and high hits over the last second more frequent than over the
// last four by PARTS_BUILD_ACTIVITY times, or the tone brighter by
// PARTS_BUILD_BRIGHTER, for PARTS_BUILD_MIN_S. It ends after
// PARTS_BUILD_END_S without rising, or at PARTS_BUILD_MAX_S. Its progress
// reaches 1 at PARTS_BUILD_FULL_S
constexpr float PARTS_BUILD_RISE_DB = 1.5f;
constexpr float PARTS_BUILD_ACTIVITY = 1.2f;
constexpr float PARTS_BUILD_BRIGHTER = 0.02f;
constexpr float PARTS_BUILD_MIN_S = 2.f;
constexpr float PARTS_BUILD_END_S = 1.f;
constexpr float PARTS_BUILD_MAX_S = 16.f;
constexpr float PARTS_BUILD_FULL_S = 8.f;

// A gap: the loudness and its fast average below PARTS_GAP_LEVEL while its
// short average is still above PARTS_GAP_BEFORE: sound that stopped, not a
// quiet start. Sound again (the loudness above twice the gap level) ends it;
// PARTS_GAP_MAX_S of it ends the song, into calm
constexpr float PARTS_GAP_LEVEL = 0.05f;
constexpr float PARTS_GAP_BEFORE = 0.15f;
constexpr float PARTS_GAP_MAX_S = 3.f;

// A drop: a low hit with the fast level PARTS_DROP_JUMP_DB above the short
// one, in a build (after its first PARTS_BUILD_MIN_S, where a build's own
// start jumps) or within PARTS_DROP_WINDOW_S of one ending; or a low hit
// within PARTS_DROP_WINDOW_S of a gap ending, where the sound coming back is
// the jump. At least PARTS_DROP_SPACING_S after the last drop
constexpr float PARTS_DROP_JUMP_DB = 3.f;
constexpr float PARTS_DROP_WINDOW_S = 1.5f;
constexpr float PARTS_DROP_SPACING_S = 15.f;

typedef enum {
    PARTS_CALM,
    PARTS_BUILD,
    PARTS_GAP,
    PARTS_HIGH,
    PARTS_COUNT
} parts_part_t;

typedef enum { PARTS_NONE, PARTS_DROP, PARTS_LIFT } parts_event_t;

typedef struct {
    float hop_period_s;
    bool enabled;

    // Running averages of the level in dB, of the mid and high hits (per
    // hop, 0 or 1) and of the tone; the loudness, fast and short, for gaps;
    // and what each keeps moving per hop
    float fast_db;
    float short_db;
    float recent_db;
    float typical_db;
    float activity_short;
    float activity_long;
    float tone_short;
    float tone_long;
    float fast_loudness;
    float short_loudness;
    float fast_k;
    float short_k;
    float recent_k;
    float typical_k;
    // Audible hops so far: each average is the mean of all of them until
    // there are its time constant's worth. 0 again when a song ends
    float audible_hops;
    // How long the room has been silent, and how long silence of either
    // kind has held the averages
    float silent_s;
    float held_s;

    // How long the energy has been rising, and not rising in a build
    float rising_s;
    float stalled_s;
    float since_drop_s;
    // Time left in which a drop may still follow a build or a gap, and
    // whether it was a gap (no jump needed then)
    float drop_window_s;
    bool after_gap;
    // A lift waiting for its beat, and for how long
    bool lift_waiting;
    float lift_wait_s;

    parts_part_t part;
    float part_time_s;
    float build_progress;
    parts_event_t event;
} parts_t;

// Starts calm, enabled
void parts_init(parts_t *this, float hop_period_s);

// Off: calm or high by the energy alone, no build, gap, drop or lift
void parts_enable(parts_t *this, bool enabled);

/**
 * One hop: the level (dB, before the auto-gain), the loudness and the tone
 * (the features' loudness and centroid), whether a low hit fired, and
 * whether a mid or a high one did
 */
void parts_update(parts_t *this, float level_db, float loudness, float tone,
                  bool low_hit, bool mid_or_high_hit);

#endif
