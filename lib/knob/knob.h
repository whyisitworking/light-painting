#ifndef KNOB_H
#define KNOB_H

/**
 * A rotary knob's clicks from its encoder's position: the position counts
 * edges (several per click) on a 32-bit counter that wraps, the knob turns
 * them into whole steps. A step comes once a whole click was turned from the
 * last one, so a half turn and back is none. Portable: the position comes
 * from wherever the encoder is read
 */

#include <stdint.h>

typedef struct {
    // The position last read, the counts past the last step, and the counts
    // one click takes
    int32_t last;
    int32_t pending;
    int32_t counts_per_step;
    bool reversed;
} knob_t;

/**
 * position: the encoder's now. counts_per_step: counts per click, 1 if less.
 * reversed: steps the other way, for an encoder wired or built the other way
 */
void knob_init(knob_t *this, int32_t position, int32_t counts_per_step,
               bool reversed);

// Whole steps since the last call, negative the other way
int knob_steps(knob_t *this, int32_t position);

#endif
