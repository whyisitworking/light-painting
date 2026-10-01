/**
 * Commands are written with spi_write_blocking(), which returns once the
 * last bit has shifted out, so DC and CS only ever change while the bus is
 * idle. A pixel transfer then switches the SPI to 16-bit frames and hands the
 * TX FIFO to a DMA channel. Its completion interrupt waits for the last few
 * frames to leave the FIFO, raises CS and reports the transfer as done.
 */

#include "st7789.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <math.h>
#include <pico/stdlib.h>

// Command codes, datasheet chapter 9
constexpr uint8_t SLPOUT = 0x11;
constexpr uint8_t INVON = 0x21;
constexpr uint8_t DISPON = 0x29;
constexpr uint8_t CASET = 0x2A;
constexpr uint8_t RASET = 0x2B;
constexpr uint8_t RAMWR = 0x2C;
constexpr uint8_t MADCTL = 0x36;
constexpr uint8_t COLMOD = 0x3A;
constexpr uint8_t PORCTRL = 0xB2;
constexpr uint8_t GCTRL = 0xB7;
constexpr uint8_t VCOMS = 0xBB;
constexpr uint8_t LCMCTRL = 0xC0;
constexpr uint8_t VDVVRHEN = 0xC2;
constexpr uint8_t VRHS = 0xC3;
constexpr uint8_t VDVS = 0xC4;
constexpr uint8_t FRCTRL2 = 0xC6;
constexpr uint8_t PWCTRL1 = 0xD0;
constexpr uint8_t GATESEL = 0xD6;
constexpr uint8_t PVGAMCTRL = 0xE0;
constexpr uint8_t NVGAMCTRL = 0xE1;

// Datasheet 7.4.5: a reset pulse of at least 10 us (up to 9 us may be taken
// for a spike), then no Sleep Out for 120 ms. After Sleep Out, 5 ms before
// the next command
constexpr uint32_t RESET_PULSE_US = 20;
constexpr uint32_t RESET_TO_SLEEP_OUT_MS = 120;
constexpr uint32_t SLEEP_OUT_MS = 5;

// Backlight PWM frequency: well above what eyes and cameras pick up
constexpr uint32_t BACKLIGHT_PWM_HZ = 25'000;

typedef struct {
    uint8_t command;
    uint8_t length;
    uint8_t data[14];
} command_t;

// This panel's settings, sent after Sleep Out and MADCTL. The power,
// voltage and gamma values depend on the panel: Waveshare's, for this one
static const command_t panel_commands[] = {
    // 16 bits per pixel (datasheet: 55h to write 16-bit pixels)
    {COLMOD, 1, {0x55}},
    {PORCTRL, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}},
    {GCTRL, 1, {0x35}},
    {VCOMS, 1, {0x35}},
    {LCMCTRL, 1, {0x2C}},
    {VDVVRHEN, 1, {0x01}},
    {VRHS, 1, {0x13}},
    {VDVS, 1, {0x20}},
    // 60 Hz
    {FRCTRL2, 1, {0x0F}},
    {PWCTRL1, 2, {0xA4, 0xA1}},
    {GATESEL, 1, {0xA1}},
    {PVGAMCTRL,
     14,
     {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12,
      0x28, 0x30}},
    {NVGAMCTRL,
     14,
     {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14,
      0x29, 0x32}},
    // An IPS panel: colours are inverted otherwise
    {INVON, 0, {}},
};

typedef struct {
    st7789_config_t config;
    spi_inst_t *spi;
    uint dma_channel;
    dma_channel_config dma_config;
    // PWM counts per backlight period
    uint16_t backlight_wrap;
    bool is_init;

    // From the start of a transfer until its interrupt
    volatile bool is_busy;
    st7789_done_t *done;
    void *context;
} st7789_t;

static st7789_t driver = {
    .is_init = false,
    .is_busy = false,
};

// The source of st7789_fill(), read by the DMA
static uint16_t fill_color;

// One command and its parameters, with CS already low. Returns once sent
static void send_command(uint8_t command, const uint8_t *data, size_t length) {
    gpio_put(driver.config.dc_pin, 0);
    spi_write_blocking(driver.spi, &command, 1);

    if (length > 0) {
        gpio_put(driver.config.dc_pin, 1);
        spi_write_blocking(driver.spi, data, length);
    }
}

static void send_command_alone(uint8_t command, const uint8_t *data,
                               size_t length) {
    gpio_put(driver.config.cs_pin, 0);
    send_command(command, data, length);
    gpio_put(driver.config.cs_pin, 1);
}

static void dma_irq_handler(void) {
    st7789_done_t *done = driver.done;
    void *context = driver.context;

    if (!dma_channel_get_irq1_status(driver.dma_channel))
        return;

    dma_channel_acknowledge_irq1(driver.dma_channel);

    // The DMA has filled the FIFO for the last time, up to 8 frames (3.4 us
    // at 37.5 MHz) are still shifting out
    while (spi_is_busy(driver.spi))
        tight_loop_contents();

    gpio_put(driver.config.cs_pin, 1);

    // Only transmitting, the RX FIFO filled up and overran meanwhile
    while (spi_is_readable(driver.spi))
        (void)spi_get_hw(driver.spi)->dr;
    spi_get_hw(driver.spi)->icr = SPI_SSPICR_RORIC_BITS;

    driver.is_busy = false;

    if (done != nullptr)
        done(context);
}

/**
 * Sets the window and starts the DMA: count pixels from source, advancing
 * through it or repeating its first one
 */
static void start_transfer(uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, const uint16_t *source,
                           bool advance, st7789_done_t *done, void *context) {
    uint16_t x0 = x + driver.config.column_offset, x1 = x0 + width - 1;
    uint16_t y0 = y + driver.config.row_offset, y1 = y0 + height - 1;
    uint8_t columns[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t rows[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};

    while (driver.is_busy)
        tight_loop_contents();

    driver.is_busy = true;
    driver.done = done;
    driver.context = context;

    // The format only changes while the SPI is idle: it pauses the SPI
    spi_set_format(driver.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_put(driver.config.cs_pin, 0);
    send_command(CASET, columns, sizeof(columns));
    send_command(RASET, rows, sizeof(rows));
    send_command(RAMWR, nullptr, 0);
    gpio_put(driver.config.dc_pin, 1);

    spi_set_format(driver.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    channel_config_set_read_increment(&driver.dma_config, advance);
    dma_channel_configure(driver.dma_channel, &driver.dma_config,
                          &spi_get_hw(driver.spi)->dr, source,
                          dma_encode_transfer_count((uint)width * height),
                          true);
}

static bool is_valid(const st7789_config_t *config) {
    uint pins[] = {config->sck_pin,  config->mosi_pin,  config->cs_pin,
                   config->dc_pin,   config->reset_pin, config->backlight_pin};

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        if (pins[i] >= NUM_BANK0_GPIOS)
            return false;

    return config->spi_index < NUM_SPIS && config->width > 0 &&
           config->height > 0 && config->baud_hz > 0 &&
           config->baud_hz <= ST7789_MAX_BAUD_HZ;
}

static void init_output(uint pin, bool level) {
    gpio_init(pin);
    gpio_put(pin, level);
    gpio_set_dir(pin, GPIO_OUT);
}

static void init_backlight(uint pin) {
    pwm_config config = pwm_get_default_config();

    driver.backlight_wrap =
        (uint16_t)(clock_get_hz(clk_sys) / BACKLIGHT_PWM_HZ - 1);
    pwm_config_set_wrap(&config, driver.backlight_wrap);

    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_set_gpio_level(pin, 0);
    pwm_init(pwm_gpio_to_slice_num(pin), &config, true);
}

bool st7789_init(const st7789_config_t *config) {
    int dma_channel;

    if (driver.is_init || !is_valid(config))
        return false;

    if ((dma_channel = dma_claim_unused_channel(false)) == -1)
        return false;

    driver.config = *config;
    driver.spi = spi_get_instance(config->spi_index);
    driver.dma_channel = (uint)dma_channel;
    driver.is_busy = false;

    init_backlight(config->backlight_pin);
    init_output(config->cs_pin, 1);
    init_output(config->dc_pin, 1);
    init_output(config->reset_pin, 1);

    spi_init(driver.spi, config->baud_hz);
    gpio_set_function(config->sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(config->mosi_pin, GPIO_FUNC_SPI);

    driver.dma_config = dma_channel_get_default_config(driver.dma_channel);
    channel_config_set_transfer_data_size(&driver.dma_config, DMA_SIZE_16);
    channel_config_set_write_increment(&driver.dma_config, false);
    channel_config_set_dreq(&driver.dma_config, spi_get_dreq(driver.spi, true));

    // On the calling core: the NVIC is per core
    dma_channel_acknowledge_irq1(driver.dma_channel);
    dma_channel_set_irq1_enabled(driver.dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    gpio_put(config->reset_pin, 0);
    sleep_us(RESET_PULSE_US);
    gpio_put(config->reset_pin, 1);
    sleep_ms(RESET_TO_SLEEP_OUT_MS);

    send_command_alone(SLPOUT, nullptr, 0);
    sleep_ms(SLEEP_OUT_MS);

    send_command_alone(MADCTL, &config->madctl, 1);
    for (size_t i = 0; i < sizeof(panel_commands) / sizeof(panel_commands[0]);
         i++)
        send_command_alone(panel_commands[i].command, panel_commands[i].data,
                           panel_commands[i].length);
    send_command_alone(DISPON, nullptr, 0);

    driver.is_init = true;

    return true;
}

void st7789_set_backlight(float level) {
    if (!driver.is_init)
        return;

    level = fminf(fmaxf(level, 0.f), 1.f);

    // A level above the wrap keeps the output high the whole period
    pwm_set_gpio_level(driver.config.backlight_pin,
                       (uint16_t)lroundf(level * (driver.backlight_wrap + 1)));
}

bool st7789_is_busy(void) { return driver.is_busy; }

void st7789_draw(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                 const uint16_t *pixels, st7789_done_t *done, void *context) {
    if (!driver.is_init || width == 0 || height == 0)
        return;

    start_transfer(x, y, width, height, pixels, true, done, context);
}

void st7789_fill(uint16_t color) {
    if (!driver.is_init)
        return;

    // Waits for any transfer still reading the previous colour
    while (driver.is_busy)
        tight_loop_contents();

    fill_color = color;
    start_transfer(0, 0, driver.config.width, driver.config.height,
                   &fill_color, false, nullptr, nullptr);

    while (driver.is_busy)
        tight_loop_contents();
}

void st7789_deinit(void) {
    if (!driver.is_init)
        return;

    while (driver.is_busy)
        tight_loop_contents();

    st7789_set_backlight(0.f);

    irq_set_enabled(DMA_IRQ_1, false);
    irq_remove_handler(DMA_IRQ_1, dma_irq_handler);
    dma_channel_cleanup(driver.dma_channel);
    dma_channel_unclaim(driver.dma_channel);

    spi_deinit(driver.spi);

    driver.is_init = false;
}
