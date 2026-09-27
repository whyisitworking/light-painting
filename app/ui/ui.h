#ifndef APP_UI_UI_H
#define APP_UI_UI_H

/**
 * The menu on the LCD, on core 1 with its own stack. ui_start() launches it
 * and returns at once: core 1 sets up the LCD and LVGL (about 125 ms), then
 * runs LVGL's timers forever. Core 0 carries on with the visualizer, which
 * never waits for the menu: each change in the menu is published through
 * the tuning link. If the LCD or LVGL cannot be set up, core 1 reports it
 * over USB and stops, and the lights carry on.
 */

#include "settings.h"

// Once, from core 0, after tuning_link_init(). The menu starts from a copy
// of settings
void ui_start(const settings_t *settings);

#endif
