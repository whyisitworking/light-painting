#include "joystick.h"

#include <hardware/gpio.h>

// The pin of each direction bit, in joystick_direction_t order
static uint pins[5];
static bool is_init = false;

bool joystick_init(const joystick_pins_t *config) {
    uint wanted[] = {config->up_pin, config->down_pin, config->left_pin,
                     config->right_pin, config->centre_pin};

    if (is_init)
        return false;

    // gpio_get_all() reads GPIO 0 to 31
    for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]); i++)
        if (wanted[i] >= NUM_BANK0_GPIOS || wanted[i] >= 32)
            return false;

    for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]); i++) {
        pins[i] = wanted[i];
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    is_init = true;

    return true;
}

unsigned joystick_read(void) {
    // All the pins sampled at once
    uint32_t levels = gpio_get_all();
    unsigned pressed = 0;

    if (!is_init)
        return 0;

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        if (!(levels & (1u << pins[i])))
            pressed |= 1u << i;

    return pressed;
}

void joystick_deinit(void) {
    if (!is_init)
        return;

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        gpio_disable_pulls(pins[i]);

    is_init = false;
}
