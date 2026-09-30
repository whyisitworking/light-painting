#ifndef APP_UI_UI_PORT_H
#define APP_UI_UI_PORT_H

/**
 * LVGL's connection to the board: its millisecond clock, the LCD as its
 * display and the rotary encoder as its encoder input. LVGL draws into one
 * of two 20-line buffers while the other goes out to the LCD, and sleeps
 * until a transfer is done instead of spinning.
 *
 * The encoder is read every 33 ms: its clicks (lib/knob) move the focus, or
 * go to the focused object as LV_KEY_LEFT and LV_KEY_RIGHT while the group
 * is editing; its button presses it, a short press clicks and one held
 * UI_LONG_PRESS_MS is a long press. Objects take the input once added to
 * the default group. Clicks turned with the button held are dropped (LVGL
 * turns only a released knob).
 *
 * Core 1 only, after st7789_init() and encoder_init().
 */

#include <lvgl/lvgl.h>

// Initializes LVGL with the LCD as its display and the encoder as its input,
// on a default group. nullptr if memory runs out
lv_display_t *ui_port_init(void);

#endif
