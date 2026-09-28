#include "boot_button.h"

#include <hardware/gpio.h>
#include <hardware/structs/ioqspi.h>
#include <hardware/structs/sio.h>
#include <hardware/sync.h>
#include <pico/platform.h>

// Chip select is the second of the QSPI pads
constexpr unsigned CS_PAD = 1;

// In RAM whatever the build: the flash is unreadable while this runs
bool __no_inline_not_in_flash_func(boot_button_read)(void) {
    uint32_t saved_irq = save_and_disable_interrupts();
    bool pressed;

    // Chip select floats: the flash is out of reach until it is restored
    hw_write_masked(&ioqspi_hw->io[CS_PAD].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Time for the pull-up to lift the floating line, as in pico-examples
    for (volatile int i = 0; i < 1000; i++)
        ;

    // Pressed, the button pulls it low
    pressed = !(sio_hw->gpio_hi_in & SIO_GPIO_HI_IN_QSPI_CSN_BITS);

    hw_write_masked(&ioqspi_hw->io[CS_PAD].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(saved_irq);

    return pressed;
}
