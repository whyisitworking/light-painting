#ifndef APP_UI_UI_STATUS_H
#define APP_UI_UI_STATUS_H

/**
 * The status screen, shown while nobody uses the menu: the mode, the
 * palette with a swatch of its colours, and the LED brightness.
 *
 *   Light Painting
 *   River
 *   Palette                 Synthwave
 *   ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇
 *   Brightness  ━━━━━━━━━━━━━━━  100 %
 */

#include "effects.h"

#include <lvgl/lvgl.h>

// What the screen shows
typedef struct {
    effects_mode_t mode;
    palette_t palette;
    int brightness_percent;
} ui_status_t;

// Creates the screen, without loading it. nullptr if memory runs out
lv_obj_t *ui_status_create(void);

// Shows these values on the screen
void ui_status_show(const ui_status_t *status);

#endif
