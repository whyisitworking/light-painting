#include "neopixel.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "swapchain.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    // Number of LEDs
    size_t count;

    // The PIO block
    PIO pio;

    // The State Machine of the PIO block
    uint pio_sm;

    // Offset inside PIO block codemem
    uint pio_offset;

    // DMA channel used to receive burst data
    uint dma_channel;

    // The swapchain to use
    swapchain_t *swapchain;

    // Whether the driver is initialized
    bool is_init;

    // Whether the driver is transmitting
    bool is_transmitting;

    // Whether a frame is in flight, from DMA start until it is latched
    volatile bool is_sending;
} neopixel_t;

static neopixel_t driver = {
    .swapchain = NULL,
    .is_init = false,
    .is_transmitting = false,
    .is_sending = false,
};

// Latched frames, updated from the PIO interrupt
static volatile size_t irq_hit = 0;

/**
 * Starts sending the newest frame, if there is one we have not sent yet.
 * Must run with interrupts disabled or from the PIO interrupt.
 */
static void send_fresh_frame() {
    if (!swapchain_consumer_swap(driver.swapchain)) {
        // Nothing new, the LEDs keep showing the last frame
        driver.is_sending = false;
        return;
    }

    driver.is_sending = true;
    dma_channel_set_read_addr(
        driver.dma_channel, swapchain_consumer_buffer(driver.swapchain), true);
}

// The state machine raises its IRQ once a frame has been latched
static void pio_irq_handler() {
    pio_interrupt_clear(driver.pio, driver.pio_sm);
    irq_hit++;

    // Stopping, the frame in flight is being drained
    if (!driver.is_transmitting)
        return;

    send_fresh_frame();
}

size_t neopixel_required_buffer_size(size_t led_count) {
    return led_count * sizeof(uint32_t);
}

bool neopixel_init(swapchain_t *swapchain, size_t count, uint pin) {
    PIO pio;
    uint pio_sm, pio_offset;
    int dma_channel;
    dma_channel_config dma_config;

    if (driver.is_init)
        return false;

    // Find any PIO (3 on RP2350) with room for the program and a free State
    // Machine, and load the program there
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &neopixel_program, &pio, &pio_sm, &pio_offset, pin, 1, true))
        return false;

    if ((dma_channel = dma_claim_unused_channel(false)) == -1) {
        pio_remove_program_and_unclaim_sm(&neopixel_program, pio, pio_sm,
                                          pio_offset);
        return false;
    }

    // Initialize the loaded PIO program
    neopixel_program_init(pio, pio_sm, pio_offset, pin);

    // Setup the DMA for data bursts
    dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, pio_sm, true));
    dma_channel_configure(dma_channel, &dma_config, &pio->txf[pio_sm], NULL,
                          count, false);

    // Frame latched interrupt, 'irq 0 rel' raises the flag numbered after
    // the state machine
    pio_interrupt_clear(pio, pio_sm);
    irq_set_exclusive_handler(pio_get_irq_num(pio, 0), pio_irq_handler);
    irq_set_enabled(pio_get_irq_num(pio, 0), true);

    driver.pio = pio;
    driver.pio_sm = pio_sm;
    driver.pio_offset = pio_offset;
    driver.count = count;
    driver.dma_channel = (uint)dma_channel;
    driver.swapchain = swapchain;
    driver.is_init = true;

    return true;
}

bool neopixel_is_init() { return driver.is_init; }

size_t neopixel_led_count() { return driver.count; }

void neopixel_start_transmission() {
    uint32_t saved_irq;

    if (!driver.is_init || driver.is_transmitting)
        return;

    saved_irq = save_and_disable_interrupts();

    pio_interrupt_clear(driver.pio, driver.pio_sm);
    pio_set_irq0_source_enabled(
        driver.pio, (pio_interrupt_source_t)(pis_interrupt0 + driver.pio_sm),
        true);
    driver.is_transmitting = true;

    // A frame may already be waiting
    send_fresh_frame();

    restore_interrupts(saved_irq);
}

void neopixel_frame_ready() {
    uint32_t saved_irq;

    if (!driver.is_transmitting)
        return;

    saved_irq = save_and_disable_interrupts();

    // If busy, the frame goes out as soon as the current one is latched
    if (!driver.is_sending)
        send_fresh_frame();

    restore_interrupts(saved_irq);
}

void neopixel_stop_transmission() {
    uint32_t saved_irq;
    uint32_t tx_stall = 1u << (PIO_FDEBUG_TXSTALL_LSB + driver.pio_sm);

    if (!driver.is_init || !driver.is_transmitting)
        return;

    saved_irq = save_and_disable_interrupts();

    driver.is_transmitting = false;
    pio_set_irq0_source_enabled(
        driver.pio, (pio_interrupt_source_t)(pis_interrupt0 + driver.pio_sm),
        false);
    dma_channel_abort(driver.dma_channel);

    restore_interrupts(saved_irq);

    // The words already in the FIFO still go out and get latched. Only when
    // the state machine stalls on the empty FIFO again can a new frame start
    // cleanly (up to 8 + 1 pixels and the reset, ~250us). TXSTALL, unlike the
    // latch IRQ, also comes when the abort left nothing to send
    if (driver.is_sending) {
        driver.pio->fdebug = tx_stall;

        while (!(driver.pio->fdebug & tx_stall))
            tight_loop_contents();
    }

    pio_interrupt_clear(driver.pio, driver.pio_sm);
    driver.is_sending = false;
}

size_t neopixel_get_pixel_count() { return driver.count; }

void neopixel_print_irq_hits() {
    printf("Frames latched %zu\n", irq_hit);
    irq_hit = 0;
}

void neopixel_deinit() {
    if (!driver.is_init)
        return;

    neopixel_stop_transmission();

    // Release the interrupt and the DMA channel
    irq_set_enabled(pio_get_irq_num(driver.pio, 0), false);
    irq_remove_handler(pio_get_irq_num(driver.pio, 0), pio_irq_handler);
    dma_channel_unclaim(driver.dma_channel);

    // PIO ciao, this also unclaims the State Machine
    neopixel_program_deinit(driver.pio, driver.pio_sm);
    pio_remove_program(driver.pio, &neopixel_program, driver.pio_offset);

    driver = (neopixel_t){
        .swapchain = NULL,
        .is_init = false,
        .is_transmitting = false,
        .is_sending = false,
    };
}