#ifndef i2s_PIO_H
#define i2s_PIO_H

/**
 * PIO based i2s Stereo
 */

#include "swapchain.h"
#include <pico/types.h>

#define I2S_UNUSED_MSB_BITS 1
#define I2S_UNUSED_LSB_BITS 7
#define I2S_MAX_AMP 0x00FFFFFFUL
#define I2S_MAX_AMP_F ((float)I2S_MAX_AMP)

#define i2s_sanitize_sample(sample) ((sample << 1) >> 8)
#define i2s_normalize_sample(sample) ((float)sample / I2S_MAX_AMP_F)

size_t i2s_required_buffer_size(size_t sample_count);

bool i2s_init(swapchain_t *swapchain, size_t sample_count, uint sck_pin,
              uint ws_pin, uint data_pin);

size_t i2s_sample_count();

void i2s_start_sampling();

void i2s_stop_sampling();

void i2s_print_irq_hits();

void i2s_deinit();

#endif
