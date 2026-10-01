#include "parts.h"

#include <math.h>

static float keep_factor(float hop_period_s, float time_constant_ms) {
    return 1.f - expf(-hop_period_s / (time_constant_ms / 1000.f));
}

void parts_init(parts_t *this, float hop_period_s) {
    *this = (parts_t){
        .hop_period_s = hop_period_s,
        .enabled = true,
        .fast_k = keep_factor(hop_period_s, PARTS_FAST_MS),
        .short_k = keep_factor(hop_period_s, PARTS_SHORT_MS),
        .recent_k = keep_factor(hop_period_s, PARTS_RECENT_MS),
        .typical_k = keep_factor(hop_period_s, PARTS_TYPICAL_MS),
        .since_drop_s = PARTS_DROP_SPACING_S,
        .part = PARTS_CALM,
    };
}

void parts_enable(parts_t *this, bool enabled) { this->enabled = enabled; }

static void enter(parts_t *this, parts_part_t part) {
    if (part == this->part)
        return;

    // Leaving a build or a gap opens the window for a drop
    if (this->part == PARTS_BUILD || this->part == PARTS_GAP) {
        this->drop_window_s = PARTS_DROP_WINDOW_S;
        this->after_gap = this->part == PARTS_GAP;
    }

    // A new part starts fresh: a rise before a gap is not a build after it,
    // and a lift waiting in calm is called off
    this->part = part;
    this->part_time_s = 0.f;
    this->rising_s = 0.f;
    this->lift_waiting = false;
}

// The song ended: calm, and every average starts over from what comes next
static void end_song(parts_t *this) {
    enter(this, PARTS_CALM);
    this->audible_hops = 0.f;
    this->silent_s = 0.f;
    this->held_s = 0.f;
    this->drop_window_s = 0.f;
}

// Calm or high by the level, with the margins and the least time in a part
static parts_part_t by_level(const parts_t *this) {
    bool settled = this->part_time_s >= PARTS_MIN_PART_S;
    float above = this->recent_db - this->typical_db;

    if (this->part == PARTS_HIGH)
        return settled && above < -PARTS_HIGH_LEAVE_DB ? PARTS_CALM
                                                       : PARTS_HIGH;
    if (this->part == PARTS_CALM)
        return settled && above > PARTS_HIGH_ENTER_DB ? PARTS_HIGH
                                                      : PARTS_CALM;

    // Out of a build or a gap: straight to where the level is
    return above > 0.f ? PARTS_HIGH : PARTS_CALM;
}

// Moves an average towards value: by k, or while there are fewer than its
// time constant's worth of audible hops, to the mean of all of them
static void average(float *mean, float value, float k, float hops) {
    *mean += (value - *mean) * fmaxf(k, 1.f / hops);
}

// Returns whether silence held the averages still
static bool update_averages(parts_t *this, float level_db, float loudness,
                            float tone, bool mid_or_high_hit) {
    float hit = mid_or_high_hit ? 1.f : 0.f, hops;
    bool silent = loudness < PARTS_SILENT;
    bool quiet = !silent && this->audible_hops > 0.f &&
                 level_db < this->typical_db - PARTS_SILENT_DB;

    this->fast_loudness += (loudness - this->fast_loudness) * this->fast_k;
    this->short_loudness += (loudness - this->short_loudness) * this->short_k;
    this->silent_s = silent ? this->silent_s + this->hop_period_s : 0.f;
    this->held_s =
        silent || quiet ? this->held_s + this->hop_period_s : 0.f;

    // Silence holds the rest: a pause, or the fade into one, must not drag
    // the song's levels down
    if (silent || quiet)
        return true;

    hops = this->audible_hops += 1.f;
    average(&this->fast_db, level_db, this->fast_k, hops);
    average(&this->short_db, level_db, this->short_k, hops);
    average(&this->recent_db, level_db, this->recent_k, hops);
    average(&this->typical_db, level_db, this->typical_k, hops);
    average(&this->activity_short, hit, this->short_k, hops);
    average(&this->activity_long, hit, this->recent_k, hops);
    average(&this->tone_short, tone, this->short_k, hops);
    average(&this->tone_long, tone, this->recent_k, hops);

    return false;
}

static void drop(parts_t *this) {
    this->event = PARTS_DROP;
    this->since_drop_s = 0.f;
    this->rising_s = 0.f;
    enter(this, PARTS_HIGH);
    this->drop_window_s = 0.f;
}

void parts_update(parts_t *this, float level_db, float loudness, float tone,
                  bool low_hit, bool mid_or_high_hit) {
    float dt = this->hop_period_s;
    bool held, rising, gap, jump, may_drop;

    held = update_averages(this, level_db, loudness, tone, mid_or_high_hit);

    this->event = PARTS_NONE;
    this->part_time_s += dt;
    this->since_drop_s += dt;
    this->drop_window_s = fmaxf(this->drop_window_s - dt, 0.f);

    // Held this long, the averages say nothing of what comes next
    if (this->silent_s > PARTS_GAP_MAX_S ||
        this->held_s > PARTS_QUIET_HOLD_S)
        end_song(this);

    if (!this->enabled) {
        if (!held)
            enter(this, by_level(this));
        this->drop_window_s = 0.f;
        this->build_progress = 0.f;
        return;
    }

    // Silence says nothing of a rise: the averages stand still in it
    rising = !held && this->short_db > this->recent_db + PARTS_BUILD_RISE_DB &&
             (this->activity_short >
                  this->activity_long * PARTS_BUILD_ACTIVITY ||
              this->tone_short > this->tone_long + PARTS_BUILD_BRIGHTER);
    this->rising_s = rising ? this->rising_s + dt : 0.f;
    this->stalled_s = rising ? 0.f : this->stalled_s + dt;

    gap = loudness < PARTS_GAP_LEVEL && this->fast_loudness < PARTS_GAP_LEVEL &&
          this->short_loudness > PARTS_GAP_BEFORE;
    jump = this->fast_db > this->short_db + PARTS_DROP_JUMP_DB;
    may_drop = low_hit && this->since_drop_s >= PARTS_DROP_SPACING_S;

    switch (this->part) {
    case PARTS_GAP:
        if (loudness > 2.f * PARTS_GAP_LEVEL) {
            // Sound coming back is the jump
            if (may_drop)
                drop(this);
            else
                enter(this, by_level(this));
        } else if (this->part_time_s > PARTS_GAP_MAX_S) {
            end_song(this);
        }
        break;

    case PARTS_BUILD:
        // In silence only a gap may begin: the averages stand still
        if (gap)
            enter(this, PARTS_GAP);
        else if (held)
            break;
        else if (may_drop && jump && this->part_time_s >= PARTS_BUILD_MIN_S)
            drop(this);
        else if (this->stalled_s > PARTS_BUILD_END_S ||
                 this->part_time_s > PARTS_BUILD_MAX_S)
            enter(this, by_level(this));
        break;

    case PARTS_CALM:
    case PARTS_HIGH:
    default:
        if (gap)
            enter(this, PARTS_GAP);
        else if (held)
            // The level stands still in silence: a waiting lift is off
            this->lift_waiting = false;
        else if (this->drop_window_s > 0.f && may_drop &&
                 (jump || this->after_gap))
            drop(this);
        else if (this->rising_s >= PARTS_BUILD_MIN_S)
            enter(this, PARTS_BUILD);
        else if (this->part != PARTS_CALM || by_level(this) != PARTS_HIGH) {
            this->lift_waiting = false;
            enter(this, by_level(this));
        } else if (rising) {
            // A rise under way may be a build: no lift until it is decided
            this->lift_waiting = false;
        } else {
            // A lift, on the beat: the next low hit, or at the latest
            // PARTS_LIFT_WAIT_S on
            this->lift_wait_s =
                this->lift_waiting ? this->lift_wait_s + dt : 0.f;
            this->lift_waiting = true;
            if (low_hit || this->lift_wait_s >= PARTS_LIFT_WAIT_S) {
                this->event = PARTS_LIFT;
                enter(this, PARTS_HIGH);
            }
        }
        break;
    }

    this->build_progress =
        this->part == PARTS_BUILD
            ? fminf(this->part_time_s / PARTS_BUILD_FULL_S, 1.f)
            : 0.f;
}
