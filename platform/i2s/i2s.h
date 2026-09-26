#ifndef I2S_H
#define I2S_H

/**
 * PIO based i2s Stereo
 *
 * The DMA streams the samples into a ring, and each completed chunk is
 * published for the main loop to take with i2s_wait_buffer()
 */

#include <pico/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    // DMA interrupts, one per completed buffer
    size_t irq_hits;
    // Buffers replaced by newer ones before i2s_wait_buffer() took them
    size_t dropped;
} i2s_stats_t;

/**
 * sample_count: 32-bit words per buffer, a power of two from 2 to 4096
 * (whole stereo frames, left word first; the DMA ring holds two)
 * sck_pin, ws_pin: consecutive, driven by the state machine
 * data_pin: sampled, distinct from sck_pin and ws_pin
 */
bool i2s_init(size_t sample_count, uint sck_pin, uint ws_pin, uint data_pin);

/**
 * Actual sample rate in Hz, derived from the system clock: the fastest SCK
 * not above the INMP441 maximum, using an integer PIO clock divider
 * (48828.125 Hz at 150 MHz)
 */
float i2s_sample_rate(void);

/**
 * Starts the DMA, a buffer arrives every sample_count / 2 frames. The state
 * machine runs from i2s_init() on. After i2s_stop_sampling() and a restart,
 * a buffer may start with a right word: audio sums both to mono, which
 * does not change that sum
 */
void i2s_start_sampling(void);

// Stops the DMA and waits until it is safe to restart
void i2s_stop_sampling(void);

/**
 * Waits until a buffer arrived that was not taken yet, and takes it:
 * sample_count words, pairs of left and right. Valid until the next call.
 * Sampling must be running: before i2s_start_sampling(), after
 * i2s_stop_sampling() or without a successful i2s_init() this waits
 * forever
 */
const int32_t *i2s_wait_buffer(void);

/**
 * The counts since the previous call
 */
i2s_stats_t i2s_take_stats(void);

// Stops sampling and releases the PIO, DMA channel, interrupt and memory
void i2s_deinit(void);

#endif
