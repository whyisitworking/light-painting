#ifndef APP_UI_UI_NAMES_H
#define APP_UI_UI_NAMES_H

/**
 * What the menu calls the modes and palettes
 */

#include "effects.h"

// "?" if out of range
const char *ui_names_mode(effects_mode_t mode);
const char *ui_names_palette(palette_t palette);

#endif
