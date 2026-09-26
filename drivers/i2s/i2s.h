#ifndef i2s_PIO_H
#define i2s_PIO_H

/**
 * PIO based i2s Stereo
 */

#include "swapchain.h"
#include <pico/types.h>

size_t i2s_required_buffer_size(size_t sample_count);

bool i2s_init(swapchain_t *swapchain, size_t sample_count, uint sck_pin,
              uint ws_pin, uint data_pin);

size_t i2s_sample_count();

/**
 * Actual sample rate in Hz, derived from the system clock: the fastest SCK
 * not above the INMP441 maximum, using an integer PIO clock divider
 * (48828.125 Hz at 150 MHz)
 */
float i2s_sample_rate();

void i2s_start_sampling();

void i2s_stop_sampling();

void i2s_print_irq_hits();

void i2s_deinit();

#endif
