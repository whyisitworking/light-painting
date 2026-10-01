#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * Build time configuration: the board wiring and the visualizer settings
 */

#include <pico.h>
#include <stddef.h>
#include <stdint.h>

// Each analysis covers the last AUDIO_FFT_SIZE mono samples and runs every
// AUDIO_HOP_SIZE new ones. At fs = 48828 Hz, 512 / 256:
//   window 10.5 ms, bin width fs / size = 95 Hz, a new analysis every 5.2 ms
// Larger sizes resolve lower frequencies, smaller ones react faster
constexpr size_t AUDIO_FFT_SIZE = 512;
constexpr size_t AUDIO_HOP_SIZE = 256;

// One mono sample per stereo frame: a left and a right word
constexpr size_t AUDIO_WORDS_PER_FRAME = 2;

static_assert(AUDIO_FFT_SIZE >= 4 &&
                  (AUDIO_FFT_SIZE & (AUDIO_FFT_SIZE - 1)) == 0,
              "AUDIO_FFT_SIZE must be a power of two >= 4");
static_assert(AUDIO_HOP_SIZE >= 1 && AUDIO_HOP_SIZE <= AUDIO_FFT_SIZE,
              "AUDIO_HOP_SIZE must be between 1 and AUDIO_FFT_SIZE");
// The audio DMA streams into a hardware ring of two hops: a power of two, at
// most 32 KB
static_assert((AUDIO_HOP_SIZE & (AUDIO_HOP_SIZE - 1)) == 0 &&
                  AUDIO_HOP_SIZE <= 2048,
              "AUDIO_HOP_SIZE must be a power of two, at most 2048");

constexpr size_t LED_COUNT = 300;

// LEDs at each end of the strip that bend onto a side wall: beams heading
// there fade as they turn the corner. 0 until the strip is mounted and
// counted
constexpr size_t LED_BEND_COUNT = 0;

// Header pins, SCK and WS consecutive in that order, as Raspberry Pi's own
// I2S driver has them (pico-extras audio_i2s: BCLK at the base, LRCLK next).
// Both microphones drive DATA in turn; the pin's bus keeper holds it between
// them, so it needs no pull-down (see i2s.pio)
constexpr unsigned MIC_SCK_PIN = 1;
constexpr unsigned MIC_WS_PIN = 2;
constexpr unsigned MIC_DATA_PIN = 3;

static_assert(MIC_WS_PIN == MIC_SCK_PIN + 1,
              "MIC_WS_PIN must follow MIC_SCK_PIN, one side-set drives both");

// Through the board's 74HCT125, 3.3 V to 5 V
constexpr unsigned LED_DATA_PIN = 6;

// The LCD on the board, wired as boards/waveshare_rp2350_lcd_1.47.h says
constexpr unsigned LCD_SPI_INDEX = WAVESHARE_LCD_SPI;
constexpr unsigned LCD_SCK_PIN = WAVESHARE_LCD_SCLK_PIN;
constexpr unsigned LCD_MOSI_PIN = WAVESHARE_LCD_TX_PIN;
constexpr unsigned LCD_CS_PIN = WAVESHARE_LCD_CS_PIN;
constexpr unsigned LCD_DC_PIN = WAVESHARE_LCD_DC_PIN;
constexpr unsigned LCD_RESET_PIN = WAVESHARE_LCD_RST_PIN;
constexpr unsigned LCD_BACKLIGHT_PIN = WAVESHARE_LCD_BL_PIN;

// Landscape. The panel's 172 lines sit in the middle of the controller's
// 240, (240 - 172) / 2 = 34 from either end, so the offset holds upside
// down too
constexpr uint16_t LCD_WIDTH = 320;
constexpr uint16_t LCD_HEIGHT = 172;
constexpr uint16_t LCD_COLUMN_OFFSET = 0;
constexpr uint16_t LCD_ROW_OFFSET = 34;

// MADCTL: landscape (MX, MV, ML), RGB order. 0xB0 turns the picture 180°
constexpr uint8_t LCD_MADCTL = 0x70;

// The datasheet's maximum. The SPI divides 150 MHz by even numbers only, so
// it runs at 37.5 MHz
constexpr uint32_t LCD_SPI_HZ = 62'500'000;

// The rotary encoder with its push button (a KY-040): CLK (A), DT (B) and
// SW, each pulled up (CLK and DT on the module, all three by the pins' own
// pull-ups) and to ground by its contact. The three GPIOs straight above its header, see
// tools/board. It counts on the third PIO, left to it: I2S and the LEDs
// take the first free ones
constexpr unsigned ENCODER_A_PIN = 9;
constexpr unsigned ENCODER_B_PIN = 25;
constexpr unsigned ENCODER_SWITCH_PIN = 26;
constexpr unsigned ENCODER_PIO_INDEX = 2;

static_assert(ENCODER_PIO_INDEX < NUM_PIOS, "ENCODER_PIO_INDEX: no such PIO");

// First guesses, to check on the module once it arrives: counts per click
// (the program counts 2 per quadrature cycle, and a KY-040 clicks once a
// cycle), and whether turning clockwise counts down, not up
constexpr int32_t ENCODER_COUNTS_PER_CLICK = 2;
constexpr bool ENCODER_REVERSED = false;

// Held this long, the button is a long press: back a level in the menu,
// lock or unlock on the status screen. First guess
constexpr uint32_t UI_LONG_PRESS_MS = 1'000;

static_assert(UI_LONG_PRESS_MS <= UINT16_MAX,
              "UI_LONG_PRESS_MS: LVGL takes at most 65535 ms");

// Without input for this long, the menu goes back to the status screen
constexpr uint32_t UI_IDLE_TIMEOUT_MS = 30'000;

// Settings are saved this long after the last change, and tried again this
// long after a failed save
constexpr uint32_t UI_SAVE_DELAY_MS = 3'000;
constexpr uint32_t UI_SAVE_RETRY_MS = 30'000;

// Each diagnostics report covers this long, on the page and over USB
constexpr uint32_t DIAGNOSTICS_PERIOD_MS = 500;

// Core 1's stack, for the menu. LVGL asks for more than 8 KB
constexpr size_t UI_STACK_SIZE = 16 * 1024;

// Sparkle pattern, renders are deterministic for a seed
constexpr uint32_t VISUALIZER_SEED = 1;

#endif
