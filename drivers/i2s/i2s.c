#include "i2s.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "i2s.pio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "swapchain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    // DMA channel streaming the PIO RX FIFO into the ring, endlessly
    uint dma_channel;

    // Two chunks of sample_count words, the DMA write ring
    uint32_t *ring;

    // What malloc returned for the ring
    void *ring_mem;

    // log2 of the ring size in bytes
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

    // Whether transmission is ongoing, read by the interrupt handler
    volatile bool is_sampling;
} i2s_t;

static i2s_t driver = {
    .swapchain = NULL,
    .is_init = false,
    .is_sampling = false,
};

// Updated from the DMA interrupt
static volatile size_t irq_hit = 0;

// A ring chunk is full. The DMA already streams into the other one
static void dma_irq_handler() {
    size_t chunk_bytes = i2s_required_buffer_size(driver.sample_count);
    uintptr_t writing;
    uint32_t *full;

    // Acknowledge before looking where the DMA writes: a chunk completing
    // meanwhile raises the interrupt again instead of being cleared unseen
    dma_channel_acknowledge_irq0(driver.dma_channel);
    irq_hit++;

    // A completion still pending when sampling was stopped
    if (!driver.is_sampling)
        return;

    writing = (uintptr_t)dma_channel_hw_addr(driver.dma_channel)->write_addr;

    // The chunk the DMA is not writing, found from where it writes rather
    // than by counting interrupts, so a late or merged interrupt still picks
    // the newest full chunk
    full = writing - (uintptr_t)driver.ring < chunk_bytes
               ? driver.ring + driver.sample_count
               : driver.ring;

    memcpy(swapchain_producer_buffer(driver.swapchain), full, chunk_bytes);
    swapchain_producer_swap(driver.swapchain);
}

size_t i2s_required_buffer_size(size_t sample_count) {
    return sample_count * sizeof(uint32_t);
}

bool i2s_init(swapchain_t *swapchain, size_t sample_count, uint sck_pin,
              uint ws_pin, uint data_pin) {
    PIO pio;
    uint pio_sm, pio_offset, gpio_start, gpio_end, ring_bits;
    int dma_channel;
    dma_channel_config dma_config;
    size_t ring_bytes;
    void *ring_mem;
    uintptr_t ring;

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

    // The DMA writes a ring of two chunks, which must be a power of two in
    // size (up to 32768 bytes) and aligned to it
    if (sample_count > 4096)
        return false;

    ring_bytes = 2 * i2s_required_buffer_size(sample_count);

    for (ring_bits = 1; ((size_t)1 << ring_bits) < ring_bytes; ring_bits++)
        ;

    if (((size_t)1 << ring_bits) != ring_bytes)
        return false;

    ring_mem = malloc(ring_bytes + ring_bytes - 1);

    if (ring_mem == NULL)
        return false;

    ring = ((uintptr_t)ring_mem + ring_bytes - 1) &
           ~(uintptr_t)(ring_bytes - 1);

    // Range of GPIOs the state machine drives or samples
    gpio_start = MIN(sck_pin, data_pin);
    gpio_end = MAX(ws_pin, data_pin);

    // Find any PIO (3 on RP2350) with room for the program and a free State
    // Machine, and load the program there
    if (!pio_claim_free_sm_and_add_program_for_gpio_range(
            &i2s_program, &pio, &pio_sm, &pio_offset, gpio_start,
            gpio_end - gpio_start + 1, true)) {
        free(ring_mem);
        return false;
    }

    // Check if an unused dma channel is available
    if ((dma_channel = dma_claim_unused_channel(false)) == -1) {
        // Give up the State Machine and the program before returning
        pio_remove_program_and_unclaim_sm(&i2s_program, pio, pio_sm,
                                          pio_offset);
        free(ring_mem);
        // Guard if not
        return false;
    }

    // Initialize the loaded PIO program
    i2s_program_init(pio, pio_sm, pio_offset, sck_pin, ws_pin, data_pin);

    // Setup the DMA for data bursts: endless, it re-triggers itself after
    // every chunk (TRIGGER_SELF, RP2350 datasheet 12.6.2.2.1) with no gap,
    // and its writes wrap around the ring
    dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_ring(&dma_config, true, ring_bits);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, pio_sm, false));
    channel_config_set_irq_quiet(&dma_config, false);
    dma_channel_configure(
        dma_channel, &dma_config, (void *)ring, &pio->rxf[pio_sm],
        dma_encode_transfer_count_with_self_trigger(sample_count), false);

    driver.sample_count = sample_count;
    driver.sample_rate = i2s_program_sample_rate(clock_get_hz(clk_sys));
    driver.pio = pio;
    driver.pio_sm = pio_sm;
    driver.pio_offset = pio_offset;
    driver.dma_channel = (uint)dma_channel;
    driver.ring = (uint32_t *)ring;
    driver.ring_mem = ring_mem;
    driver.ring_bits = ring_bits;
    driver.swapchain = swapchain;
    driver.sck_pin = sck_pin;
    driver.ws_pin = ws_pin;
    driver.data_pin = data_pin;
    driver.is_init = true;

    // Setup interrupts, only now that the handler has everything it uses,
    // and without a completion flag a previous owner may have left behind
    dma_channel_acknowledge_irq0(dma_channel);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    return true;
}

size_t i2s_sample_count() { return driver.sample_count; }

float i2s_sample_rate() { return driver.sample_rate; }

void i2s_start_sampling() {
    if (!driver.is_init || driver.is_sampling)
        return;

    // Restart at the ring's first chunk, with the configuration stop cleared
    dma_channel_set_irq0_enabled(driver.dma_channel, true);
    hw_set_bits(&dma_hw->ch[driver.dma_channel].al1_ctrl,
                DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_set_write_addr(driver.dma_channel, driver.ring, true);

    driver.is_sampling = true;
}

void i2s_stop_sampling() {
    if (!driver.is_init || !driver.is_sampling)
        return;

    // RP2350-E5: an aborting channel can re-trigger itself on the abort's
    // last cycle; disable it first. Also stops TRIGGER_SELF restarting it
    hw_clear_bits(&dma_hw->ch[driver.dma_channel].al1_ctrl,
                  DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_set_irq0_enabled(driver.dma_channel, false);
    dma_channel_abort(driver.dma_channel);

    // Datasheet 12.6.8.3: the channel is only safe to restart once
    // CHAN_ABORT reads back zero, which can trail BUSY
    while (dma_hw->abort & (1u << driver.dma_channel))
        tight_loop_contents();

    dma_channel_acknowledge_irq0(driver.dma_channel);

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

    // Release the interrupt and the DMA channel
    dma_channel_set_irq0_enabled(driver.dma_channel, false);
    irq_set_enabled(DMA_IRQ_0, false);
    irq_remove_handler(DMA_IRQ_0, dma_irq_handler);
    dma_channel_unclaim(driver.dma_channel);
    free(driver.ring_mem);

    // This also unclaims the State Machine
    i2s_program_deinit(driver.pio, driver.pio_sm);
    pio_remove_program(driver.pio, &i2s_program, driver.pio_offset);

    driver = (i2s_t){
        .swapchain = NULL,
        .is_init = false,
        .is_sampling = false,
    };
}
