#ifndef APP_UI_UI_DIAGNOSTICS_H
#define APP_UI_UI_DIAGNOSTICS_H

/**
 * The diagnostics page: both microphones' levels as meters, then the sound
 * analysis, the lights' work and losses, and both cores' stack peaks, from
 * the newest report (app/diagnostics.h)
 *
 *   <  Diagnostics
 *   L  ======----------------   -62 dBFS
 *   R  ======----------------   -61 dBFS
 *   Ceiling -24 dB    Loud 0.42     Hits 2.0/s
 *   Work 1.9, max 2.6 ms               Load 50 %
 *   Audio lost 0                   LEDs 150 fps
 *   Menu 5.1 / 16 KB           Lights 1.2 / 8 KB
 *
 * The meters span -90 to 0 dBFS. Audio lost above 0 and a load above 80 %
 * wear the accent
 */

#include "stats.h"

#include <lvgl/lvgl.h>

// Creates the screen, without loading it
lv_obj_t *ui_diagnostics_create(void);

// Shows a report, or "--" everywhere for nullptr
void ui_diagnostics_show(const stats_report_t *report);

#endif
