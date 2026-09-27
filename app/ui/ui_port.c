#include "ui_port.h"

#include "config.h"
#include "st7789.h"

#include <hardware/sync.h>
#include <pico/time.h>

// Lines per draw buffer, about a ninth of the screen each
constexpr size_t DRAW_BUFFER_LINES = 20;

// LVGL draws into one while the other is sent. LV_DRAW_BUF_ALIGN is 4
static alignas(4) uint16_t draw_buffers[2][LCD_WIDTH * DRAW_BUFFER_LINES];

static uint32_t tick_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// Starts sending an area, LVGL goes on drawing the next into the other buffer.
// pixels is one of draw_buffers, hence aligned for RGB565 values
static void flush([[maybe_unused]] lv_display_t *display, const lv_area_t *area,
                  uint8_t *pixels) {
    st7789_draw((uint16_t)area->x1, (uint16_t)area->y1,
                (uint16_t)lv_area_get_width(area),
                (uint16_t)lv_area_get_height(area),
                (const uint16_t *)(const void *)pixels, nullptr, nullptr);
}

/**
 * Called by LVGL when it needs the buffer being sent, returns once sent.
 * Sleeps instead: checked with interrupts masked, so the transfer ending
 * between the check and __wfi() still wakes it, as in i2s_wait_buffer()
 */
static void flush_wait([[maybe_unused]] lv_display_t *display) {
    uint32_t saved_irq = save_and_disable_interrupts();

    while (st7789_is_busy()) {
        __wfi();
        restore_interrupts(saved_irq);
        saved_irq = save_and_disable_interrupts();
    }

    restore_interrupts(saved_irq);
}

lv_display_t *ui_port_init(void) {
    lv_display_t *display;

    lv_init();
    lv_tick_set_cb(tick_ms);

    display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    if (display == nullptr)
        return nullptr;

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffers[0], draw_buffers[1],
                           sizeof(draw_buffers[0]),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_flush_wait_cb(display, flush_wait);

    return display;
}
