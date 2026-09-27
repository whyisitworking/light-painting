#ifndef WS2812_H
#define WS2812_H

/**
 * PIO based WS2812 output. The main loop fills ws2812_frame() and
 * publishes it with ws2812_submit(); the driver sends the newest frame
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
} ws2812_stats_t;

// count LEDs on the data pin. False if already initialized, or if no PIO,
// DMA channel or memory is free
bool ws2812_init(size_t count, uint pin);

// Frames go out from now on, starting with any already submitted
void ws2812_start_transmission(void);

/**
 * The frame to fill next: count color_ws2812_t words. Valid until
 * ws2812_submit()
 */
uint32_t *ws2812_frame(void);

/**
 * Publishes the frame from ws2812_frame(). It is sent right away if the
 * strip is idle, otherwise as soon as the current frame is latched. Frames
 * are only sent when new, the LEDs hold the last one.
 */
void ws2812_submit(void);

// Aborts the frame in flight: the pixels already in the FIFO still go out
// and latch, and the LEDs keep what they got
void ws2812_stop_transmission(void);

/**
 * The counts since the previous call
 */
ws2812_stats_t ws2812_take_stats(void);

// Stops and releases the PIO, DMA channel, interrupt and memory
void ws2812_deinit(void);

#endif
