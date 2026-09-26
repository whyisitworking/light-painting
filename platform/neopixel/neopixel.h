#ifndef NEOPIXEL_H
#define NEOPIXEL_H

/**
 * PIO based WS2812 output. The main loop fills neopixel_frame() and
 * publishes it with neopixel_submit(); the driver sends the newest frame
 * each time the previous one is latched
 */

#include <pico/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // Frames sent and latched by the LEDs
    size_t frames_latched;
    // Frames replaced by newer ones before they could be sent
    size_t dropped;
} neopixel_stats_t;

// count LEDs on the data pin. False if already initialized, or if no PIO,
// DMA channel or memory is free
bool neopixel_init(size_t count, uint pin);

// Frames go out from now on, starting with any already submitted
void neopixel_start_transmission(void);

/**
 * The frame to fill next: count color_neopixel_t words. Valid until
 * neopixel_submit()
 */
uint32_t *neopixel_frame(void);

/**
 * Publishes the frame from neopixel_frame(). It is sent right away if the
 * strip is idle, otherwise as soon as the current frame is latched. Frames
 * are only sent when new, the LEDs hold the last one.
 */
void neopixel_submit(void);

// Aborts the frame in flight: the pixels already in the FIFO still go out
// and latch, and the LEDs keep what they got
void neopixel_stop_transmission(void);

/**
 * The counts since the previous call
 */
neopixel_stats_t neopixel_take_stats(void);

// Stops and releases the PIO, DMA channel, interrupt and memory
void neopixel_deinit(void);

#endif
