#ifndef APP_UI_UI_NAMES_H
#define APP_UI_UI_NAMES_H

/**
 * What the menu calls the looks, scenes and song parts
 */

#include "show.h"

// "?" if out of range
const char *ui_names_look(show_look_t look);
const char *ui_names_scene(scene_t scene);
const char *ui_names_part(parts_part_t part);

#endif
