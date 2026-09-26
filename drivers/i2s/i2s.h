#ifndef i2s_PIO_H
#define i2s_PIO_H

/**
 * PIO based i2s Stereo
 */

#include "swapchain.h"
#include <pico/types.h>

size_t i2s_required_buffer_size(size_t sample_count);

/**
 * The buffers are DMA write rings and must be aligned to their size: create
 * the swapchain with swapchain_init_aligned and this alignment
 */
size_t i2s_required_buffer_alignment(size_t sample_count);

/**
 * sample_count: 32-bit words per buffer, a power of two from 2 to 8192
 * (whole stereo frames, left word first; buffers are DMA write rings)
 * swapchain: created with swapchain_init_aligned, see
 * i2s_required_buffer_alignment
 * sck_pin, ws_pin: consecutive, driven by the state machine
 * data_pin: sampled, distinct from sck_pin and ws_pin
 */
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
