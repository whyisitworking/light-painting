#ifndef JOYSTICK_H
#define JOYSTICK_H

/**
 * A 5-way navigation switch: up, down, left, right and a centre press, each
 * a contact to a common pin wired to ground. The pins use the internal
 * pull-ups, so a pressed direction reads low.
 *
 * No debouncing: read every 33 ms or so, a contact bouncing for a few
 * milliseconds gives at most one reading in the middle of it, either still
 * released or already pressed, never a second press.
 */

#include <pico/types.h>

// The directions joystick_read() reports, as a bit mask
typedef enum {
    JOYSTICK_UP = 1u << 0,
    JOYSTICK_DOWN = 1u << 1,
    JOYSTICK_LEFT = 1u << 2,
    JOYSTICK_RIGHT = 1u << 3,
    JOYSTICK_CENTRE = 1u << 4,
} joystick_direction_t;

typedef struct {
    uint up_pin;
    uint down_pin;
    uint left_pin;
    uint right_pin;
    uint centre_pin;
} joystick_pins_t;

// False if already initialized or a pin does not exist
[[nodiscard]] bool joystick_init(const joystick_pins_t *pins);

// The directions pressed right now, joystick_direction_t bits
unsigned joystick_read(void);

// Only after a successful joystick_init(). Leaves the pins as inputs
void joystick_deinit(void);

#endif
