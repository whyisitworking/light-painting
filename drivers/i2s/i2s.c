#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "i2s.pio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "swapchain.h"
#include <stdio.h>
#include <stdlib.h>

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

    // Two DMA channels chained to each other (ping-pong): when one fills
    // its buffer the other takes over in hardware, with no gap
    uint dma_channels[2];

    // The buffer each DMA channel writes, or will write when chained to
    void *buffers[2];

    // What malloc returned for the extra buffer the driver owns
    void *extra_mem;

    // log2 of the buffer size in bytes, the DMA write ring size
    uint ring_bits;

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
    for (uint index = 0; index < 2; index++) {
        uint channel = driver.dma_channels[index];

        if (!dma_channel_get_irq0_status(channel))
            continue;

        dma_channel_acknowledge_irq0(channel);
        irq_hit++;

        // The other channel already took over. Publish this full buffer and
        // arm a free one for when the other channel chains back
        driver.buffers[index] = swapchain_producer_exchange(
            driver.swapchain, driver.buffers[index]);
        dma_channel_set_write_addr(channel, driver.buffers[index], false);
    }
}

size_t i2s_required_buffer_size(size_t sample_count) {
    return sample_count * sizeof(uint32_t);
}

size_t i2s_required_buffer_alignment(size_t sample_count) {
    // DMA write rings wrap on a boundary of their own size
    return i2s_required_buffer_size(sample_count);
}

/**
 * log2 of the buffer size in bytes if it can be a DMA write ring (a power of
 * two from 2 to 32768 bytes), 0 otherwise
 */
static uint ring_size_bits(size_t sample_count) {
    size_t bytes = i2s_required_buffer_size(sample_count);

    for (uint bits = 1; bits <= 15; bits++)
        if (((size_t)1 << bits) == bytes)
            return bits;

    return 0;
}

static void configure_channel(uint index, bool trigger) {
    uint channel = driver.dma_channels[index];
    dma_channel_config dma_config = dma_channel_get_default_config(channel);

    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    // Writes wrap within the aligned buffer: even a very late interrupt can
    // never make a transfer run past it
    channel_config_set_ring(&dma_config, true, driver.ring_bits);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&dma_config,
                            pio_get_dreq(driver.pio, driver.pio_sm, false));
    channel_config_set_irq_quiet(&dma_config, false);
    // When the buffer is full, the other channel starts right away
    channel_config_set_chain_to(&dma_config, driver.dma_channels[index ^ 1]);
    dma_channel_configure(channel, &dma_config, driver.buffers[index],
                          &driver.pio->rxf[driver.pio_sm], driver.sample_count,
                          trigger);
    dma_channel_set_irq0_enabled(channel, true);
}

bool i2s_init(swapchain_t *swapchain, size_t sample_count, uint sck_pin,
              uint ws_pin, uint data_pin) {
    PIO pio;
    uint pio_sm, pio_offset, gpio_start, gpio_end, ring_bits;
    int dma_channel_a, dma_channel_b;
    size_t alignment;
    uintptr_t extra;
    void *extra_mem;

    if (driver.is_init)
        return false;

    // SCK, WS & Data must be
    if (sck_pin >= NUM_BANK0_GPIOS || ws_pin >= NUM_BANK0_GPIOS ||
        data_pin >= NUM_BANK0_GPIOS)
        return false;

    // SCK and WS **MUST** be consecutive pins in that order
    if (sck_pin + 1 != ws_pin)
        return false;

    // Data is sampled, SCK and WS are driven: they must not share a pin
    if (data_pin == sck_pin || data_pin == ws_pin)
        return false;

    // Whole stereo frames (a left and a right word) per buffer, so that
    // every buffer starts with a left word
    if (sample_count == 0 || sample_count % 2 != 0)
        return false;

    // Buffers are DMA write rings: a power of two in size, 2 to 32768 bytes,
    // aligned to their size (see i2s_required_buffer_alignment)
    ring_bits = ring_size_bits(sample_count);
    alignment = i2s_required_buffer_alignment(sample_count);

    if (ring_bits == 0 ||
        (uintptr_t)swapchain_producer_buffer(swapchain) % alignment != 0)
        return false;

    // The second buffer in flight, owned by the driver
    extra_mem = malloc(i2s_required_buffer_size(sample_count) + alignment - 1);

    if (extra_mem == NULL)
        return false;

    extra = ((uintptr_t)extra_mem + alignment - 1) &
            ~(uintptr_t)(alignment - 1);

    // Range of GPIOs the state machine drives or samples
    gpio_start = MIN(sck_pin, data_pin);
    gpio_end = MAX(ws_pin, data_pin);

    // Find any PIO (3 on RP2350) with room for the program and a free State
    // Machine, and load the program there
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &i2s_program, &pio, &pio_sm, &pio_offset, gpio_start,
            gpio_end - gpio_start + 1, true)) {
        free(extra_mem);
        return false;
    }

    // Check if two unused dma channels are available
    dma_channel_a = dma_claim_unused_channel(false);
    dma_channel_b = dma_claim_unused_channel(false);

    if (dma_channel_a == -1 || dma_channel_b == -1) {
        // Give up what was claimed before returning
        if (dma_channel_a != -1)
            dma_channel_unclaim((uint)dma_channel_a);
        if (dma_channel_b != -1)
            dma_channel_unclaim((uint)dma_channel_b);
        pio_remove_program_and_unclaim_sm(&i2s_program, pio, pio_sm,
                                          pio_offset);
        free(extra_mem);
        // Guard if not
        return false;
    }

    // Initialize the loaded PIO program
    i2s_program_init(pio, pio_sm, pio_offset, sck_pin, ws_pin, data_pin);

    // Setup interrupts, the channels are configured when sampling starts
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    driver.sample_count = sample_count;
    driver.sample_rate = i2s_program_sample_rate(clock_get_hz(clk_sys));
    driver.pio = pio;
    driver.pio_sm = pio_sm;
    driver.pio_offset = pio_offset;
    driver.dma_channels[0] = (uint)dma_channel_a;
    driver.dma_channels[1] = (uint)dma_channel_b;
    driver.buffers[0] = swapchain_producer_buffer(swapchain);
    driver.buffers[1] = (void *)extra;
    driver.extra_mem = extra_mem;
    driver.ring_bits = ring_bits;
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

    // The second channel waits to be chained to, the first starts now
    configure_channel(1, false);
    configure_channel(0, true);

    driver.is_sampling = true;
}

void i2s_stop_sampling() {
    if (!driver.is_init || !driver.is_sampling)
        return;

    // RP2350-E5: aborting chained channels can re-trigger them. Disable both
    // before aborting either; dma_channel_cleanup then also clears their
    // CHAIN_TO, interrupt enables and any interrupt raised by the abort
    for (uint index = 0; index < 2; index++)
        hw_clear_bits(&dma_hw->ch[driver.dma_channels[index]].al1_ctrl,
                      DMA_CH0_CTRL_TRIG_EN_BITS);

    for (uint index = 0; index < 2; index++)
        dma_channel_cleanup(driver.dma_channels[index]);

    driver.is_sampling = false;
}

void i2s_print_irq_hits() {
    size_t hits;
    uint32_t saved_irq;

    // Take and reset in one go: printf can block and interrupts keep counting
    saved_irq = save_and_disable_interrupts();
    hits = irq_hit;
    irq_hit = 0;
    restore_interrupts(saved_irq);

    printf("IRQ hits %zu\n", hits);
}

void i2s_deinit() {
    // Check if valid in memory
    if (!driver.is_init)
        return;

    i2s_stop_sampling();

    // Release the interrupt, the DMA channels and the extra buffer
    irq_set_enabled(DMA_IRQ_0, false);
    irq_remove_handler(DMA_IRQ_0, dma_irq_handler);
    dma_channel_unclaim(driver.dma_channels[0]);
    dma_channel_unclaim(driver.dma_channels[1]);
    free(driver.extra_mem);

    // This also unclaims the State Machine
    i2s_program_deinit(driver.pio, driver.pio_sm);
    pio_remove_program(driver.pio, &i2s_program, driver.pio_offset);

    driver = (i2s_t){
        .swapchain = NULL,
        .is_init = false,
        .is_sampling = false,
    };
}
