#ifndef APP_UI_UI_STATUS_H
#define APP_UI_UI_STATUS_H

/**
 * The status screen, shown while nobody uses the menu: the mode, the
 * palette with a swatch of its colours, the LED brightness, and a note on
 * saving.
 *
 *   Light Painting                  Saved
 *   River
 *
 *   Palette                     Synthwave
 *   ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇
 *
 *   Brightness                      100 %
 *   ━━━━━━━━━━━━━━━━━━━━━━━━━━━──────────
 *
 * Three blocks 16 px apart, what belongs together 6 px apart within them:
 * with the 12 px margins they fill the 172 lines exactly
 */

#include "effects.h"

#include <lvgl/lvgl.h>

// What the screen shows
typedef struct {
    effects_mode_t mode;
    palette_t palette;
    int brightness_percent;
    // In the corner, e.g. how saving went. nullptr or "" for none
    const char *note;
} ui_status_t;

// Creates the screen, without loading it. nullptr if memory runs out
lv_obj_t *ui_status_create(void);

// Shows these values on the screen
void ui_status_show(const ui_status_t *status);

// Only changes the note
void ui_status_note(const char *note);

#endif
