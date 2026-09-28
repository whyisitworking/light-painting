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

// Header pins, SCK and WS must be consecutive
constexpr unsigned MIC_SCK_PIN = 26;
constexpr unsigned MIC_WS_PIN = 27;
constexpr unsigned MIC_DATA_PIN = 28;

static_assert(MIC_WS_PIN == MIC_SCK_PIN + 1,
              "MIC_WS_PIN must follow MIC_SCK_PIN, one side-set drives both");

constexpr unsigned LED_DATA_PIN = 8;

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

// The 5-way switch on the header, its common pin to ground (header pin 11,
// next to GP0-GP4). Swap these to match how it is mounted
constexpr unsigned JOYSTICK_UP_PIN = 0;
constexpr unsigned JOYSTICK_DOWN_PIN = 1;
constexpr unsigned JOYSTICK_LEFT_PIN = 2;
constexpr unsigned JOYSTICK_RIGHT_PIN = 3;
constexpr unsigned JOYSTICK_CENTRE_PIN = 4;

// Without input for this long, the menu goes back to the status screen
constexpr uint32_t UI_IDLE_TIMEOUT_MS = 30'000;

// Settings are saved this long after the last change, and tried again this
// long after a failed save
constexpr uint32_t UI_SAVE_DELAY_MS = 3'000;
constexpr uint32_t UI_SAVE_RETRY_MS = 30'000;

// Core 1's stack, for the menu. LVGL asks for more than 8 KB
constexpr size_t UI_STACK_SIZE = 16 * 1024;

// Sparkle pattern, renders are deterministic for a seed
constexpr uint32_t VISUALIZER_SEED = 1;

#endif
