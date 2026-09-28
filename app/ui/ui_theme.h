#ifndef APP_UI_UI_THEME_H
#define APP_UI_UI_THEME_H

/**
 * The look of every screen: a dark slate, light text, muted labels, and one
 * accent colour that follows the palette the strip shows, taken from the
 * middle of it (hot pink for Synthwave, orange for Fire, teal for Ocean,
 * cyan for Rainbow).
 *
 * Not black: an LCD's black is its backlight showing through, a dark grey
 * anyway, and near-black shades run together on it. So the background is
 * a slate, and the bar's track sits well above it.
 *
 * Shared LVGL styles: set_palette() recolours every object using them at
 * once, the row being changed included. The focused row, the bars and
 * warnings wear it.
 */

#include "palette.h"

#include <lvgl/lvgl.h>

// Once, before any screen is built, with the palette shown first
void ui_theme_init(palette_t palette);

// The accent follows this palette from now on
void ui_theme_set_palette(palette_t palette);

// A palette colour as the LCD shows it: sent as it is, the LCD's own gamma
// (about 2.2, like the strip's) makes it look as on the strip
lv_color_t ui_theme_palette_color(palette_t palette, float position);

// Each screen: its background and text colour
const lv_style_t *ui_theme_screen(void);

// Labels in front of values
const lv_style_t *ui_theme_muted(void);

// A focused row (selector LV_STATE_FOCUSED): the accent, with dark text
const lv_style_t *ui_theme_focus(void);

// The bars: their track (LV_PART_MAIN), their filled part
// (LV_PART_INDICATOR)
const lv_style_t *ui_theme_track(void);
const lv_style_t *ui_theme_fill(void);

// Values that need a look, e.g. audio lost: the accent as their colour
const lv_style_t *ui_theme_warning(void);

// A bar shaped like the palette swatch, height px high and rounded into a
// pill: the accent filled part (LV_PART_INDICATOR) on a track
lv_obj_t *ui_theme_bar_create(lv_obj_t *parent, int32_t height);

#endif
