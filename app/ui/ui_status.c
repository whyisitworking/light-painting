#include "ui_status.h"

#include "palette.h"
#include "ui_names.h"

#include <math.h>

// Cells of the palette swatch
constexpr size_t SWATCH_CELLS = 32;

// Labels in front of values, the filled part of the bar, and its track
constexpr uint32_t MUTED_COLOR = 0x8888A0;
constexpr uint32_t ACCENT_COLOR = 0xFF3D8B;
constexpr uint32_t TRACK_COLOR = 0x2A2A38;

// Around the screen, between its blocks, and within a block. The blocks
// are 52, 32 and 32 lines high (Montserrat 14 lines are 16, 28 lines 30),
// so 12 + 52 + 16 + 32 + 16 + 32 + 12 = 172, the screen's height
constexpr int32_t MARGIN = 12;
constexpr int32_t BLOCK_GAP = 16;
constexpr int32_t INNER_GAP = 6;

// The swatch and the bar, alike: rounded into pills
constexpr int32_t STRIP_HEIGHT = 10;
constexpr int32_t STRIP_RADIUS = LV_RADIUS_CIRCLE;

static struct {
    lv_obj_t *note;
    lv_obj_t *mode;
    lv_obj_t *palette;
    lv_obj_t *swatch[SWATCH_CELLS];
    lv_obj_t *brightness_bar;
    lv_obj_t *brightness;
} view;

static uint8_t lcd_channel(float value) {
    return (uint8_t)lroundf(fminf(fmaxf(value, 0.f), 1.f) * 255.f);
}

/**
 * Palette colours are what the effects compute, before the strip's gamma.
 * The LCD applies a gamma of its own, about 2.2 too: sent as they are, they
 * look as they do on the strip. Not color_gamma(), which core 0 uses
 */
static lv_color_t lcd_color(rgb_t color) {
    return lv_color_make(lcd_channel(color.r), lcd_channel(color.g),
                         lcd_channel(color.b));
}

// A column of what belongs together, INNER_GAP apart
static lv_obj_t *block_create(lv_obj_t *parent) {
    lv_obj_t *block = lv_obj_create(parent);

    lv_obj_remove_style_all(block);
    lv_obj_set_size(block, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(block, INNER_GAP, 0);

    return block;
}

// A line across the screen: a muted name on the left, then its contents
static lv_obj_t *row_create(lv_obj_t *parent, const char *name) {
    lv_obj_t *row = lv_obj_create(parent), *label;

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    label = lv_label_create(row);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, lv_color_hex(MUTED_COLOR), 0);

    return row;
}

static lv_obj_t *swatch_create(lv_obj_t *parent) {
    lv_obj_t *swatch = lv_obj_create(parent);

    lv_obj_remove_style_all(swatch);
    lv_obj_set_size(swatch, LV_PCT(100), STRIP_HEIGHT);
    lv_obj_set_flex_flow(swatch, LV_FLEX_FLOW_ROW);
    // The square cells are cut to the rounded corners (LVGL draws the
    // swatch into a layer and masks it)
    lv_obj_set_style_radius(swatch, STRIP_RADIUS, 0);
    lv_obj_set_style_clip_corner(swatch, true, 0);

    for (size_t i = 0; i < SWATCH_CELLS; i++) {
        lv_obj_t *cell = lv_obj_create(swatch);

        lv_obj_remove_style_all(cell);
        lv_obj_set_height(cell, LV_PCT(100));
        lv_obj_set_flex_grow(cell, 1);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        view.swatch[i] = cell;
    }

    return swatch;
}

// Shaped like the swatch: the accent filled part on a dark track
static lv_obj_t *bar_create(lv_obj_t *parent) {
    lv_obj_t *bar = lv_bar_create(parent);

    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), STRIP_HEIGHT);
    lv_obj_set_style_radius(bar, STRIP_RADIUS, 0);
    lv_obj_set_style_radius(bar, STRIP_RADIUS, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(TRACK_COLOR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(ACCENT_COLOR),
                              LV_PART_INDICATOR);

    return bar;
}

lv_obj_t *ui_status_create(void) {
    lv_obj_t *screen = lv_obj_create(nullptr), *block, *row;

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, MARGIN, 0);
    lv_obj_set_style_pad_row(screen, BLOCK_GAP, 0);
    lv_obj_set_scrollable(screen, false);

    // What this is, and what it shows
    block = block_create(screen);
    row = row_create(block, "Light Painting");
    view.note = lv_label_create(row);
    lv_obj_set_style_text_color(view.note, lv_color_hex(MUTED_COLOR), 0);
    view.mode = lv_label_create(block);
    lv_obj_set_style_text_font(view.mode, &lv_font_montserrat_28, 0);

    block = block_create(screen);
    row = row_create(block, "Palette");
    view.palette = lv_label_create(row);
    swatch_create(block);

    block = block_create(screen);
    row = row_create(block, "Brightness");
    view.brightness = lv_label_create(row);
    view.brightness_bar = bar_create(block);

    return screen;
}

void ui_status_note(const char *note) {
    lv_label_set_text(view.note, note != nullptr ? note : "");
}

void ui_status_show(const ui_status_t *status) {
    ui_status_note(status->note);
    lv_label_set_text(view.mode, ui_names_mode(status->mode));
    lv_label_set_text(view.palette, ui_names_palette(status->palette));

    // The palette from end to end, as the effects place colours on it
    for (size_t i = 0; i < SWATCH_CELLS; i++)
        lv_obj_set_style_bg_color(
            view.swatch[i],
            lcd_color(palette_color(status->palette,
                                    (float)i / (float)(SWATCH_CELLS - 1))),
            0);

    lv_bar_set_value(view.brightness_bar, status->brightness_percent,
                     LV_ANIM_OFF);
    lv_label_set_text_fmt(view.brightness, "%d %%", status->brightness_percent);
}
