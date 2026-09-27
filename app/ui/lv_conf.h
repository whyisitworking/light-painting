/**
 * LVGL configuration, for LVGL v9.6.0 (third_party/lvgl). Only what differs
 * from LVGL's defaults, which lv_conf_internal.h fills in for the rest.
 *
 * Plain #defines, unlike the constexpr constants elsewhere: LVGL tests them
 * with #if, and compiles them as C11.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*
 * Memory: LVGL's own allocator on a static pool, apart from malloc. Sized
 * from LVGL's recommendation for a UI with many widgets, measured later
 */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (48 * 1024)

/*
 * Core 1 runs LVGL alone, in a loop: no operating system, no draw threads
 */
#define LV_USE_OS LV_OS_NONE

/*
 * Rendering: RGB565 as the ST7789 takes it, 30 frames/s at most
 */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_DEF_REFR_PERIOD 33

/*
 * The software renderer draws into RGB565, blends A8 glyph masks and, for a
 * layer with transparency, ARGB8888. None of its other formats is used:
 * compiled in, they took 136 KB of RAM (the code runs from RAM)
 */
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_DRAW_SW_SUPPORT_A8 1
#define LV_DRAW_SW_SUPPORT_ARGB8888 1
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0
#define LV_DRAW_SW_SUPPORT_RGB565A8 0
#define LV_DRAW_SW_SUPPORT_RGB888 0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8 0
#define LV_DRAW_SW_SUPPORT_AL88 0
#define LV_DRAW_SW_SUPPORT_I1 0

/*
 * Stop on a null pointer or a failed allocation rather than carry on
 */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/*
 * Theme and fonts: dark, Montserrat 14 for text and 28 for the status
 * screen's headline
 */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT LV_FONT_DEFAULT_MONTSERRAT_14

/*
 * Layouts and widgets: the menu is built from base objects in flex columns,
 * labels and bars. Every other widget is left out, the default theme would
 * otherwise keep their code
 */
#define LV_USE_FLEX 1
#define LV_USE_GRID 0
#define LV_USE_OBSERVER 0

#define LV_USE_LABEL 1
#define LV_USE_BAR 1

#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_ARCLABEL 0
#define LV_USE_BUTTON 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGE 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

#endif
