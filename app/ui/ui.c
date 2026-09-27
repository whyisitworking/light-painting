#include "ui.h"

#include "config.h"
#include "joystick.h"
#include "st7789.h"
#include "ui_port.h"
#include "ui_status.h"
#include "visualizer.h"

#include <pico/multicore.h>
#include <pico/time.h>
#include <stdio.h>

// Backlight level, once the first frame is on the screen
constexpr float BACKLIGHT_LEVEL = 0.8f;

// Core 1's stack, 8-byte aligned as the Arm procedure call standard asks
static alignas(8) uint32_t stack[UI_STACK_SIZE / sizeof(uint32_t)];

static void ui_main(void) {
    lv_display_t *display;
    lv_obj_t *status;
    uint32_t wait_ms;

    // Here, on core 1: its DMA interrupt is enabled on the calling core
    if (!st7789_init(&(st7789_config_t){
            .spi_index = LCD_SPI_INDEX,
            .sck_pin = LCD_SCK_PIN,
            .mosi_pin = LCD_MOSI_PIN,
            .cs_pin = LCD_CS_PIN,
            .dc_pin = LCD_DC_PIN,
            .reset_pin = LCD_RESET_PIN,
            .backlight_pin = LCD_BACKLIGHT_PIN,
            .width = LCD_WIDTH,
            .height = LCD_HEIGHT,
            .column_offset = LCD_COLUMN_OFFSET,
            .row_offset = LCD_ROW_OFFSET,
            .madctl = LCD_MADCTL,
            .baud_hz = LCD_SPI_HZ,
        })) {
        printf("Could not initialize the LCD\n");
        return;
    }

    if (!joystick_init(&(joystick_pins_t){
            .up_pin = JOYSTICK_UP_PIN,
            .down_pin = JOYSTICK_DOWN_PIN,
            .left_pin = JOYSTICK_LEFT_PIN,
            .right_pin = JOYSTICK_RIGHT_PIN,
            .centre_pin = JOYSTICK_CENTRE_PIN,
        })) {
        printf("Could not initialize the joystick\n");
        return;
    }

    if ((display = ui_port_init()) == nullptr) {
        printf("Could not initialize LVGL\n");
        return;
    }

    // The compiled-in look, until the menu can change it
    status = ui_status_create();
    ui_status_show(&(ui_status_t){
        .mode = VISUALIZER_MODE,
        .palette = VISUALIZER_PALETTE,
        .brightness_percent = 100,
    });
    lv_screen_load(status);

    // The display memory holds noise after a reset: light it only once the
    // first frame is on it
    lv_refr_now(display);
    while (st7789_is_busy())
        tight_loop_contents();
    st7789_set_backlight(BACKLIGHT_LEVEL);

    printf("LCD init!\n");

    while (true) {
        wait_ms = lv_timer_handler();
        sleep_ms(wait_ms == LV_NO_TIMER_READY ? LV_DEF_REFR_PERIOD : wait_ms);
    }
}

void ui_start(void) {
    multicore_launch_core1_with_stack(ui_main, stack, sizeof(stack));
}
