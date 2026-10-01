#include "check.h"
#include "knob.h"

#include <stdint.h>

// Two counts per click, as the encoder's program counts a full cycle
constexpr int32_t COUNTS = 2;

// A position's worth of steps from a knob at start, one call
static int steps_to(int32_t start, int32_t position, bool reversed) {
    knob_t knob;

    knob_init(&knob, start, COUNTS, reversed);
    return knob_steps(&knob, position);
}

// Whole clicks turn into steps, each way, and reversed the other way
static void test_whole_clicks(void) {
    CHECK(steps_to(0, 0, false) == 0);
    CHECK(steps_to(0, 6, false) == 3);
    CHECK(steps_to(0, -6, false) == -3);
    CHECK(steps_to(0, 6, true) == -3);
    CHECK(steps_to(100, 94, false) == -3);
}

// Half a click is no step, and back again is none either: a step comes
// only once a whole click is done, counted from the last one
static void test_half_clicks(void) {
    knob_t knob;

    knob_init(&knob, 0, COUNTS, false);
    CHECK(knob_steps(&knob, 1) == 0);
    CHECK(knob_steps(&knob, 0) == 0);
    CHECK(knob_steps(&knob, -1) == 0);
    CHECK(knob_steps(&knob, 0) == 0);
    CHECK(knob_steps(&knob, 1) == 0);
    CHECK(knob_steps(&knob, 2) == 1);
    CHECK(knob_steps(&knob, 3) == 0);
    CHECK(knob_steps(&knob, 1) == 0);
    CHECK(knob_steps(&knob, 0) == -1);
}

// Read slower than it turns, the steps add up to the clicks turned
static void test_many_between_reads(void) {
    knob_t knob;
    int total = 0;

    knob_init(&knob, 0, COUNTS, false);
    total += knob_steps(&knob, 7);
    total += knob_steps(&knob, 13);
    total += knob_steps(&knob, 20);

    CHECK(total == 10);
}

// The position is a 32-bit counter that wraps; steps across the wrap are
// the same as anywhere else
static void test_wrap(void) {
    CHECK(steps_to(INT32_MAX - 1, INT32_MIN + 4, false) == 3);
    CHECK(steps_to(INT32_MIN + 1, INT32_MAX - 4, false) == -3);
}

// Fewer than one count per click is taken as one
static void test_invalid_counts(void) {
    knob_t knob;

    knob_init(&knob, 0, 0, false);
    CHECK(knob_steps(&knob, 2) == 2);
}

int main(void) {
    test_whole_clicks();
    test_half_clicks();
    test_many_between_reads();
    test_wrap();
    test_invalid_counts();

    return CHECK_REPORT();
}
