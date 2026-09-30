#ifndef APP_UI_UI_THEME_H
#define APP_UI_UI_THEME_H

/**
 * The look of every screen: a dark slate, light text, muted labels, and one
 * accent colour that follows the scene the strip shows: its accent (hot
 * magenta for Neon Noir, amber for Ember and Dusk, lime for Acid, cyan for
 * Ice).
 *
 * Not black: an LCD's black is its backlight showing through, a dark grey
 * anyway, and near-black shades run together on it. So the background is
 * a slate, and the bar's track sits well above it.
 *
 * Shared LVGL styles: set_scene() recolours every object using them at
 * once, the row being changed included. The focused row, the bars and
 * warnings wear it.
 */

#include "scene.h"

#include <lvgl/lvgl.h>

// Once, before any screen is built, with the scene shown first
void ui_theme_init(scene_t scene);

// The accent follows this scene from now on
void ui_theme_set_scene(scene_t scene);

// A scene colour as the LCD shows it: sent as it is, the LCD's own gamma
// (about 2.2, like the strip's) makes it look as on the strip
lv_color_t ui_theme_scene_color(scene_t scene, scene_role_t role);

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

// A bar shaped like the scene swatch, height px high and rounded into a
// pill: the accent filled part (LV_PART_INDICATOR) on a track
lv_obj_t *ui_theme_bar_create(lv_obj_t *parent, int32_t height);

#endif
