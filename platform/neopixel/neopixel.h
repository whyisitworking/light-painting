#ifndef WS2812_PIO_H
#define WS2812_PIO_H

#include "swapchain.h"
#include <pico/types.h>

size_t neopixel_required_buffer_size(size_t led_count);

bool neopixel_init(swapchain_t *swapchain, size_t count, uint pin);

size_t neopixel_get_pixel_count();

void neopixel_start_transmission();

/**
 * Tells the driver a new frame was published with swapchain_producer_swap.
 * It is sent right away if the strip is idle, otherwise as soon as the
 * current frame is latched. Frames are only sent when new, the LEDs hold
 * the last one.
 */
void neopixel_frame_ready();

void neopixel_stop_transmission();

void neopixel_print_irq_hits();

void neopixel_deinit();

#endif