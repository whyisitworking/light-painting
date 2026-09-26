#include "neopixel.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"
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
} neopixel_t;

static neopixel_t driver = {
    .swapchain = NULL,
    .is_init = false,
    .is_transmitting = false,
};

static void dma_irq_handler() {
    swapchain_consumer_swap(driver.swapchain);
    dma_channel_acknowledge_irq1(driver.dma_channel);
    pio_sm_exec(driver.pio, driver.pio_sm,
                pio_encode_jmp(driver.pio_offset + neopixel_offset_sync));
    dma_channel_set_read_addr(
        driver.dma_channel, swapchain_consumer_buffer(driver.swapchain), true);
}

size_t neopixel_required_buffer_size(size_t led_count) {
    return led_count * sizeof(uint32_t);
}

bool neopixel_init(swapchain_t *swapchain, size_t count, uint pin) {
    PIO pio;
    int pio_sm, dma_channel;
    uint pio_offset;
    dma_channel_config dma_config;

    if (driver.is_init)
        return false;

    // Start with PIO0
    pio = pio0;

    // Check if the program can be loaded in the pio
    if (!pio_can_add_program(pio, &neopixel_program)) {
        // Try the next, PIO1
        pio = pio1;

        if (!pio_can_add_program(pio, &neopixel_program)) {
            // Guard if not
            return false;
        }
    }

    // Try to grab an unused State Machine
    if ((pio_sm = pio_claim_unused_sm(pio, false)) == -1) {
        return false;
    }

    if ((dma_channel = dma_claim_unused_channel(false)) == -1) {
        pio_sm_unclaim(pio, pio_sm);
        return false;
    }

    // Load the PIO program in memory and initialize it
    pio_offset = pio_add_program(pio, &neopixel_program);
    neopixel_program_init(pio, pio_sm, pio_offset, pin);

    // Setup the DMA for data bursts
    dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, pio_sm, true));
    channel_config_set_irq_quiet(&dma_config, false);
    dma_channel_configure(dma_channel, &dma_config, &pio->txf[pio_sm], NULL,
                          count, false);

    dma_channel_set_irq1_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    driver.pio = pio;
    driver.pio_sm = (uint)pio_sm;
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
    if (!driver.is_init || driver.is_transmitting)
        return;

    dma_channel_set_read_addr(
        driver.dma_channel, swapchain_consumer_buffer(driver.swapchain), true);
    driver.is_transmitting = true;
}

void neopixel_stop_transmission() {
    if (!driver.is_init || !driver.is_transmitting)
        return;

    dma_channel_set_irq1_enabled(driver.dma_channel, false);
    dma_channel_abort(driver.dma_channel);
    dma_channel_acknowledge_irq1(driver.dma_channel);
    dma_channel_set_irq1_enabled(driver.dma_channel, true);

    driver.is_transmitting = false;
}

size_t neopixel_get_pixel_count() { return driver.count; }

void neopixel_deinit() {
    if (!driver.is_init)
        return;

    neopixel_stop_transmission();

    // PIO ciao
    neopixel_program_deinit(driver.pio, driver.pio_sm);
    // This also unclaims the State Machine
    pio_remove_program(driver.pio, &neopixel_program, driver.pio_offset);

    driver = (neopixel_t){
        .swapchain = NULL,
        .is_init = false,
        .is_transmitting = false,
    };
}