#include "knob.h"

void knob_init(knob_t *this, int32_t position, int32_t counts_per_step,
               bool reversed) {
    *this = (knob_t){
        .last = position,
        .counts_per_step = counts_per_step >= 1 ? counts_per_step : 1,
        .reversed = reversed,
    };
}

int knob_steps(knob_t *this, int32_t position) {
    // Modulo 2^32: right across the counter's wrap, and reversed the same
    // way, so no value overflows
    uint32_t moved = (uint32_t)position - (uint32_t)this->last;
    int32_t steps;

    this->last = position;
    this->pending += (int32_t)(this->reversed ? 0u - moved : moved);

    // Whole clicks, towards zero: what is left waits for the rest of a click
    steps = this->pending / this->counts_per_step;
    this->pending -= steps * this->counts_per_step;

    return (int)steps;
}
