#ifndef BOOT_BUTTON_H
#define BOOT_BUTTON_H

/**
 * The BOOT button, as an input while running. It is not on a GPIO: it pulls
 * the flash's chip select low (through 1 kOhm on the RP2350-LCD-1.47-A),
 * which the bootrom checks at reset. Reading it floats chip select for a
 * few microseconds, the pad's pull-up holding it high unless the button is
 * pressed, as in Raspberry Pi's pico-examples (picoboard/button).
 *
 * The flash cannot be read meanwhile, by either core: safe here only
 * because the firmware runs from RAM (copy_to_ram), so neither core does.
 * Interrupts of the calling core are masked while reading. Pressed while
 * running, BOOT does nothing else: the bootrom only looks at it at reset.
 */

// Whether the button is pressed now
bool boot_button_read(void);

#endif
