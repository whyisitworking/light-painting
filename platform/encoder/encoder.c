#include "encoder.h"

#include "encoder.pio.h"

#include <hardware/gpio.h>

typedef struct {
    // The state machine and where its program is
    PIO pio;
    uint sm;
    uint offset;
    encoder_pins_t pins;
    bool is_init;
} encoder_t;

static encoder_t driver;

// A pin the PIO can read: on the chip, and within the 32 pins its GPIO base
// gives it (RP2350: 0 or 16)
static bool pin_ok(PIO pio, uint pin) {
    uint base = pio_get_gpio_base(pio);

    return pin < NUM_BANK0_GPIOS && pin >= base && pin < base + 32;
}

static void pin_setup(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

bool encoder_init(PIO pio, const encoder_pins_t *pins) {
    int sm, offset;

    if (driver.is_init)
        return false;

    if (!pin_ok(pio, pins->a_pin) || !pin_ok(pio, pins->b_pin) ||
        pins->switch_pin >= NUM_BANK0_GPIOS || pins->a_pin == pins->b_pin ||
        pins->switch_pin == pins->a_pin || pins->switch_pin == pins->b_pin)
        return false;

    if ((sm = pio_claim_unused_sm(pio, false)) < 0)
        return false;

    // One call, so nothing can take the room between a check and the add
    if ((offset = pio_add_program(pio, &encoder_program)) < 0) {
        pio_sm_unclaim(pio, (uint)sm);
        return false;
    }

    pin_setup(pins->a_pin);
    pin_setup(pins->b_pin);
    pin_setup(pins->switch_pin);

    driver = (encoder_t){
        .pio = pio,
        .sm = (uint)sm,
        .offset = (uint)offset,
        .pins = *pins,
        .is_init = true,
    };
    encoder_program_init(pio, driver.sm, driver.offset, pins->a_pin,
                         pins->b_pin);

    return true;
}

int32_t encoder_position(void) {
    if (!driver.is_init)
        return 0;

    // The newest position, written there by the program, not a queue
    return (int32_t)driver.pio->rxf_putget[driver.sm][0];
}

bool encoder_pressed(void) {
    return driver.is_init && !gpio_get(driver.pins.switch_pin);
}

void encoder_deinit(void) {
    if (!driver.is_init)
        return;

    pio_sm_set_enabled(driver.pio, driver.sm, false);
    pio_remove_program(driver.pio, &encoder_program, driver.offset);
    pio_sm_unclaim(driver.pio, driver.sm);
    gpio_disable_pulls(driver.pins.a_pin);
    gpio_disable_pulls(driver.pins.b_pin);
    gpio_disable_pulls(driver.pins.switch_pin);

    driver.is_init = false;
}
