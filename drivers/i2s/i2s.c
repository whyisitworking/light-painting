#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "i2s.pio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "swapchain.h"
#include <stdio.h>
#include <stdlib.h>

#define SWAPCHAIN_LENGTH 3

typedef struct {
    // Number of samples each buffer will contain
    size_t sample_count;

    // Resulting WS (sample) frequency in Hz
    float sample_rate;

    // Selected PIO bank
    PIO pio;

    // Selected State Machine in the PIO bank
    uint pio_sm;

    // Offset inside PIO instruction bank
    uint pio_offset;

    // DMA channel used to burst buffers to PIO
    uint dma_channel;

    // GPIO connected to the SCK(Serial ClocK) pin
    uint sck_pin;

    // GPIO connected to the WS(Word Select) pin
    uint ws_pin;

    // GPIO connected to the SD(Serial Data) pin
    uint data_pin;

    // Swapchain used to circle the buffers
    swapchain_t *swapchain;

    // Whether the driver is initialized
    bool is_init;

    // Whether transmission is ongoing
    bool is_sampling;
} i2s_t;

static i2s_t driver = {
    .swapchain = NULL,
    .is_init = false,
    .is_sampling = false,
};

// Updated from the DMA interrupt
static volatile size_t irq_hit = 0;

static void dma_irq_handler() {
    swapchain_producer_swap(driver.swapchain);
    irq_hit++;
    dma_channel_acknowledge_irq0(driver.dma_channel);
    dma_channel_set_write_addr(
        driver.dma_channel, swapchain_producer_buffer(driver.swapchain), true);
}

size_t i2s_required_buffer_size(size_t sample_count) {
    return sample_count * sizeof(uint32_t);
}

bool i2s_init(swapchain_t *swapchain, size_t sample_count, uint sck_pin,
              uint ws_pin, uint data_pin) {
    PIO pio;
    uint pio_sm, pio_offset, gpio_start, gpio_end;
    int dma_channel;
    dma_channel_config dma_config;

    if (driver.is_init)
        return false;

    // SCK, WS & Data must be
    if (sck_pin >= NUM_BANK0_GPIOS || ws_pin >= NUM_BANK0_GPIOS ||
        data_pin >= NUM_BANK0_GPIOS)
        return false;

    // SCK and WS **MUST** be consecutive pins in that order
    if (sck_pin + 1 != ws_pin)
        return false;

    // Range of GPIOs the state machine drives or samples
    gpio_start = MIN(sck_pin, data_pin);
    gpio_end = MAX(ws_pin, data_pin);

    // Find any PIO (3 on RP2350) with room for the program and a free State
    // Machine, and load the program there
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &i2s_program, &pio, &pio_sm, &pio_offset, gpio_start,
            gpio_end - gpio_start + 1, true))
        return false;

    // Check if an unused dma channel is available
    if ((dma_channel = dma_claim_unused_channel(false)) == -1) {
        // Give up the State Machine and the program before returning
        pio_remove_program_and_unclaim_sm(&i2s_program, pio, pio_sm,
                                          pio_offset);
        // Guard if not
        return false;
    }

    // Initialize the loaded PIO program
    i2s_program_init(pio, pio_sm, pio_offset, sck_pin, ws_pin, data_pin);

    // Setup the DMA for data bursts
    dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, pio_sm, false));
    channel_config_set_irq_quiet(&dma_config, false);
    dma_channel_configure(dma_channel, &dma_config, NULL, &pio->rxf[pio_sm],
                          sample_count, false);

    // Setup interrupts
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    driver.sample_count = sample_count;
    driver.sample_rate = i2s_program_sample_rate(clock_get_hz(clk_sys));
    driver.pio = pio;
    driver.pio_sm = pio_sm;
    driver.pio_offset = pio_offset;
    driver.dma_channel = (uint)dma_channel;
    driver.swapchain = swapchain;
    driver.sck_pin = sck_pin;
    driver.ws_pin = ws_pin;
    driver.data_pin = data_pin;
    driver.is_init = true;

    return true;
}

size_t i2s_sample_count() { return driver.sample_count; }

float i2s_sample_rate() { return driver.sample_rate; }

void i2s_start_sampling() {
    if (!driver.is_init || driver.is_sampling)
        return;

    dma_channel_set_write_addr(
        driver.dma_channel, swapchain_producer_buffer(driver.swapchain), true);

    driver.is_sampling = true;
}

void i2s_stop_sampling() {
    if (!driver.is_init || !driver.is_sampling)
        return;

    dma_channel_set_irq0_enabled(driver.dma_channel, false);
    dma_channel_abort(driver.dma_channel);
    dma_channel_acknowledge_irq0(driver.dma_channel);
    dma_channel_set_irq0_enabled(driver.dma_channel, true);

    driver.is_sampling = false;
}

void i2s_print_irq_hits() {
    printf("IRQ hits %zu\n", irq_hit);
    irq_hit = 0;
}

void i2s_deinit() {
    // Check if valid in memory
    if (!driver.is_init)
        return;

    i2s_stop_sampling();

    // Release the interrupt and the DMA channel
    dma_channel_set_irq0_enabled(driver.dma_channel, false);
    irq_set_enabled(DMA_IRQ_0, false);
    irq_remove_handler(DMA_IRQ_0, dma_irq_handler);
    dma_channel_unclaim(driver.dma_channel);

    // This also unclaims the State Machine
    i2s_program_deinit(driver.pio, driver.pio_sm);
    pio_remove_program(driver.pio, &i2s_program, driver.pio_offset);

    driver = (i2s_t){
        .swapchain = NULL,
        .is_init = false,
        .is_sampling = false,
    };
}
