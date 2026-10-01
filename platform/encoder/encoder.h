#ifndef ENCODER_H
#define ENCODER_H

/**
 * A rotary encoder with a push button (e.g. a KY-040), all three contacts to
 * ground and pulled up. A PIO state machine counts the turns in hardware
 * (encoder.pio), so nothing is missed however seldom it is read; the button
 * is a plain GPIO.
 *
 * No debouncing of the button: read every 33 ms or so, a contact bouncing
 * for a few milliseconds gives at most one reading in the middle of it,
 * either still released or already pressed, never a second press. The turns
 * need none: the PIO program's counting cancels bounce out.
 *
 * Any core may read it; no interrupts.
 */

#include <hardware/pio.h>
#include <pico/types.h>
#include <stdint.h>

typedef struct {
    // A (the module's CLK) and B (DT): any two pins
    uint a_pin;
    uint b_pin;
    // The push button (SW)
    uint switch_pin;
} encoder_pins_t;

/**
 * On a state machine of pio, e.g. pio2. The pins get the internal pull-ups
 * too, in parallel with any on the board. False if already initialized, a
 * pin is out of range or used twice, or pio has no free state machine or
 * room for the program
 */
[[nodiscard]] bool encoder_init(PIO pio, const encoder_pins_t *pins);

/**
 * Where the encoder has turned since encoder_init(): 2 counts per quadrature
 * cycle, positive with A leading B. Wraps around at the ends of int32_t:
 * take differences modulo 2^32 (see lib/knob)
 */
int32_t encoder_position(void);

// Whether the button is held down now
bool encoder_pressed(void);

// Only after a successful encoder_init(). Leaves the pins as inputs
void encoder_deinit(void);

#endif
