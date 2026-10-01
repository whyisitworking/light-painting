#ifndef APP_UI_UI_MENU_H
#define APP_UI_UI_MENU_H

/**
 * The screens and how the encoder moves through them:
 *
 *   status ──press──► Menu ──► Look · Sound · System
 *                                                └──► Diagnostics
 *
 * On the status screen, turning steps through the looks and a press opens
 * the menu. A long press (UI_LONG_PRESS_MS) locks the controls, a padlock
 * shows, and nothing changes anything (the BOOT shuffle neither) until a
 * long press unlocks. A restart unlocks too.
 *
 * On a page, under its title, the first row is "‹ Back". Turning moves
 * between rows; a press opens the page a row leads to, goes back, or starts
 * editing a setting: its value shows arrows, turning changes it at once,
 * and the next press ends editing. A long press goes back a level anywhere.
 * 30 s without input, the status screen returns. Reset to defaults, on
 * System, asks for a second press within 3 s. The Diagnostics page shows
 * the newest report until left: it does not go back to the status screen
 * on its own.
 */

#include "settings.h"
#include "stats.h"

#include <lvgl/lvgl.h>

// Called after a setting changed, with all the settings. id is
// SETTINGS_ALL when all of them changed at once (reset to defaults)
typedef void ui_menu_changed_t(const settings_t *settings, settings_id_t id);

/**
 * Shows the status screen and takes the switch from then on. settings are
 * shown and changed in place, and must outlive the menu. Once, on the
 * LVGL core, after ui_port_init()
 */
void ui_menu_start(settings_t *settings, ui_menu_changed_t *changed);

// A note for the status screen's corner, e.g. how saving went. nullptr for
// none. note must outlive its use: a string literal
void ui_menu_note(const char *note);

// The settings changed outside the menu: shows them again, as if a key had
// just been pressed
void ui_menu_refresh(void);

// Whether the controls are locked (see above): nothing may change the
// settings then
bool ui_menu_locked(void);

// The newest diagnostics, shown on their page. report must stay valid
// until the next call
void ui_menu_report(const stats_report_t *report);

#endif
