#ifndef ST7789_H
#define ST7789_H

/**
 * ST7789 LCD over SPI, write only. Commands go out in 8-bit SPI frames and
 * pixels in 16-bit ones, so the RGB565 values leave in memory order, MSB
 * first as the controller expects: no byte swapping. A DMA channel sends the
 * pixels in the background and its interrupt (DMA_IRQ_1) reports the end of
 * each transfer.
 *
 * Init values for the Waveshare RP2350-LCD-1.47-A panel (ST7789V3, 172 x 320
 * IPS), from the ST7789V3 datasheet and Waveshare's demo.
 */

#include <pico/types.h>
#include <stddef.h>
#include <stdint.h>

// Datasheet: serial write clock cycle at least 16 ns
constexpr uint32_t ST7789_MAX_BAUD_HZ = 62'500'000;

typedef struct {
    // SPI instance (0 or 1) and its SCK and TX pins
    uint spi_index;
    uint sck_pin;
    uint mosi_pin;
    // Driven as GPIOs: chip select, data/command, reset
    uint cs_pin;
    uint dc_pin;
    uint reset_pin;
    // PWM, high on
    uint backlight_pin;
    // Visible area in the orientation madctl sets, and where it starts in
    // the controller's memory
    uint16_t width;
    uint16_t height;
    uint16_t column_offset;
    uint16_t row_offset;
    // MADCTL: orientation (MY, MX, MV), refresh order and colour order
    uint8_t madctl;
    // At most ST7789_MAX_BAUD_HZ. The SPI divides its clock by even numbers
    // only: the actual rate may be lower
    uint32_t baud_hz;
} st7789_config_t;

// Called from the DMA interrupt once a transfer has been sent
typedef void st7789_done_t(void *context);

/**
 * Resets and sets up the panel, leaving the backlight off and the display
 * memory as it was: draw first, then turn the backlight on. Blocks for about
 * 125 ms. Run it on the core that is to take the DMA interrupt.
 *
 * False if already initialized, if the config is invalid, or if no DMA
 * channel is free
 */
[[nodiscard]] bool st7789_init(const st7789_config_t *config);

// Backlight level, 0 (off) to 1 (full), by PWM
void st7789_set_backlight(float level);

// Whether a transfer is in progress
bool st7789_is_busy(void);

/**
 * Sends width x height RGB565 pixels, row by row, to the rectangle at (x, y)
 * of the visible area. Waits for a transfer in progress first, then returns
 * while DMA sends: pixels must stay untouched until done(context) is
 * called, from the DMA interrupt. done may be nullptr
 */
void st7789_draw(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                 const uint16_t *pixels, st7789_done_t *done, void *context);

// Fills the visible area with one RGB565 colour, and waits until it is sent
void st7789_fill(uint16_t color);

// Only after a successful st7789_init(). Leaves the backlight off
void st7789_deinit(void);

#endif
