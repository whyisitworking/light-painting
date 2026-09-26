#ifndef WS2812_PIO_H
#define WS2812_PIO_H

#include "swapchain.h"
#include <pico/types.h>

size_t neopixel_required_buffer_size(size_t led_count);

bool neopixel_init(swapchain_t *swapchain, size_t count, uint pin);

size_t neopixel_get_pixel_count();

void neopixel_start_transmission();

void neopixel_stop_transmission();

void neopixel_print_irq_hits();

void neopixel_deinit();

#endif