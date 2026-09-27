#ifndef APP_UI_UI_PORT_H
#define APP_UI_UI_PORT_H

/**
 * LVGL's connection to the board: its millisecond clock, the LCD as its
 * display and the 5-way switch as its keypad. LVGL draws into one of two
 * 20-line buffers while the other goes out to the LCD, and sleeps until a
 * transfer is done instead of spinning.
 *
 * The switch reads as keys, every 33 ms: up and down move the focus
 * (LV_KEY_PREV, LV_KEY_NEXT), left and right go to the focused object
 * (LV_KEY_LEFT, LV_KEY_RIGHT) and the centre presses it (LV_KEY_ENTER).
 * Held for 400 ms, a key repeats every 100 ms. Objects take the keys once
 * added to the default group.
 *
 * Core 1 only, after st7789_init() and joystick_init().
 */

#include <lvgl/lvgl.h>

// Initializes LVGL with the LCD as its display and the switch as its keypad,
// on a default group. nullptr if memory runs out
lv_display_t *ui_port_init(void);

#endif
