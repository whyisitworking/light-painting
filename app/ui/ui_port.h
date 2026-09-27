#ifndef APP_UI_UI_PORT_H
#define APP_UI_UI_PORT_H

/**
 * LVGL's connection to the board: its millisecond clock, and the LCD as its
 * display. LVGL draws into one of two 20-line buffers while the other goes
 * out to the LCD, and sleeps until a transfer is done instead of spinning.
 * Core 1 only, after st7789_init().
 */

#include <lvgl/lvgl.h>

// Initializes LVGL with the LCD as its display. nullptr if memory runs out
lv_display_t *ui_port_init(void);

#endif
