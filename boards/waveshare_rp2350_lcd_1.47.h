// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

/**
 * Waveshare RP2350-LCD-1.47-A, which Pico SDK 2.3.1 has no header for.
 * Modelled on the SDK's waveshare_rp2350_lcd_1.28.h and pico2.h, with the
 * pins from Waveshare's schematic (RP2350-LCD-1.47-A.pdf).
 *
 * Unlike the Pico 2 there is no SMPS (an RT9193 LDO instead), no LED on
 * GP25 and no VBUS or VSYS sense: GP23 and GP24 are not connected, GP25 and
 * GP29 are plain header pins. The header exposes GP0-GP9 and GP25-GP29.
 * No default UART or I2C either: those header pins are ours to assign
 */

#ifndef _BOARDS_WAVESHARE_RP2350_LCD_1_47_H
#define _BOARDS_WAVESHARE_RP2350_LCD_1_47_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// For board detection
#define WAVESHARE_RP2350_LCD_1_47

// --- RP2350 VARIANT ---
#define PICO_RP2350A 1

// --- LED ---
// A WS2812B, powered from 3.3 V
#ifndef PICO_DEFAULT_WS2812_PIN
#define PICO_DEFAULT_WS2812_PIN 22
#endif

// --- SPI ---
// SPI0 drives the LCD. GP16 is its DC and GP17 its CS, driven as GPIOs
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 0
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN 18
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN 19
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN 16
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN 17
#endif

// --- LCD ---
// ST7789V3, 172 x 320 IPS. The backlight switches through a MOSFET, high on
#ifndef WAVESHARE_LCD_SPI
#define WAVESHARE_LCD_SPI 0
#endif
#ifndef WAVESHARE_LCD_DC_PIN
#define WAVESHARE_LCD_DC_PIN 16
#endif
#ifndef WAVESHARE_LCD_CS_PIN
#define WAVESHARE_LCD_CS_PIN 17
#endif
#ifndef WAVESHARE_LCD_SCLK_PIN
#define WAVESHARE_LCD_SCLK_PIN 18
#endif
#ifndef WAVESHARE_LCD_TX_PIN
#define WAVESHARE_LCD_TX_PIN 19
#endif
#ifndef WAVESHARE_LCD_RST_PIN
#define WAVESHARE_LCD_RST_PIN 20
#endif
#ifndef WAVESHARE_LCD_BL_PIN
#define WAVESHARE_LCD_BL_PIN 21
#endif

// --- TF CARD ---
// SPI1 in SPI mode
#ifndef WAVESHARE_SD_SCLK_PIN
#define WAVESHARE_SD_SCLK_PIN 10
#endif
#ifndef WAVESHARE_SD_MOSI_PIN
#define WAVESHARE_SD_MOSI_PIN 11
#endif
#ifndef WAVESHARE_SD_MISO_PIN
#define WAVESHARE_SD_MISO_PIN 12
#endif
#ifndef WAVESHARE_SD_CS_PIN
#define WAVESHARE_SD_CS_PIN 15
#endif

// --- FLASH ---
// W25Q128JV, 16 MB

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

// As on Waveshare's RP2350-LCD-1.28
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 3
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (16 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
