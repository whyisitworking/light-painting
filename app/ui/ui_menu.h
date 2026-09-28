#ifndef APP_UI_UI_MENU_H
#define APP_UI_UI_MENU_H

/**
 * The screens and how the switch moves through them:
 *
 *   status ──centre──► Menu ──► Look · Sound · Effects · System
 *
 * On a page, up and down move between rows, left and right change the
 * focused setting at once (held, they repeat), and the centre opens the
 * page a row leads to. Left on the "‹ title" row, or the centre held, goes
 * back a level. 30 s without input, the status screen returns. Reset to
 * defaults, on System, asks for a second press within 3 s.
 */

#include "settings.h"

#include <lvgl/lvgl.h>

// Called after a setting changed, with all the settings. id is
// SETTINGS_ID_COUNT when all of them changed at once (reset to defaults)
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

#endif
