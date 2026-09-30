#include "ui_status.h"

#include "ui_names.h"
#include "ui_theme.h"

// Cells of the scene swatch: field, accent, hit
constexpr size_t SWATCH_CELLS = SCENE_ROLE_COUNT;

// Around the screen, between its blocks, and within a block. The blocks
// are 52, 32 and 32 lines high (Montserrat 14 lines are 16, 28 lines 30),
// so 12 + 52 + 16 + 32 + 16 + 32 + 12 = 172, the screen's height
constexpr int32_t MARGIN = 12;
constexpr int32_t BLOCK_GAP = 16;
constexpr int32_t INNER_GAP = 6;

// The swatch and the bar, alike: rounded into pills
constexpr int32_t STRIP_HEIGHT = 10;
constexpr int32_t STRIP_RADIUS = LV_RADIUS_CIRCLE;

// The padlock: a shackle, an arch of LOCK_LINE, over a body, in the accent
constexpr int32_t LOCK_WIDTH = 12;
constexpr int32_t LOCK_SHACKLE = 7;
constexpr int32_t LOCK_BODY = 7;
constexpr int32_t LOCK_LINE = 2;

static struct {
    lv_obj_t *note;
    lv_obj_t *lock;
    lv_obj_t *look;
    lv_obj_t *scene;
    lv_obj_t *swatch[SWATCH_CELLS];
    lv_obj_t *brightness_bar;
    lv_obj_t *brightness;
} view;

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
    lv_obj_add_style(label, ui_theme_muted(), 0);

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

// A padlock drawn with two shapes: LVGL's symbols have none
static lv_obj_t *lock_create(lv_obj_t *parent) {
    lv_obj_t *lock = lv_obj_create(parent), *shackle, *body;

    lv_obj_remove_style_all(lock);
    lv_obj_set_size(lock, LOCK_WIDTH, LOCK_SHACKLE + LOCK_BODY - LOCK_LINE);
    // The shackle's hidden lower half reaches past it
    lv_obj_set_scrollable(lock, false);

    // An arch: a rounded outline whose bottom the body covers
    shackle = lv_obj_create(lock);
    lv_obj_remove_style_all(shackle);
    lv_obj_set_size(shackle, LOCK_WIDTH - 2 * LOCK_LINE, 2 * LOCK_SHACKLE);
    lv_obj_align(shackle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(shackle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(shackle, LOCK_LINE, 0);
    lv_obj_add_style(shackle, ui_theme_outline(), 0);

    body = lv_obj_create(lock);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LOCK_WIDTH, LOCK_BODY);
    lv_obj_align(body, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(body, 2, 0);
    lv_obj_add_style(body, ui_theme_fill(), 0);

    return lock;
}

lv_obj_t *ui_status_create(void) {
    lv_obj_t *screen = lv_obj_create(nullptr), *block, *row, *corner;

    lv_obj_add_style(screen, ui_theme_screen(), 0);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, MARGIN, 0);
    lv_obj_set_style_pad_row(screen, BLOCK_GAP, 0);
    lv_obj_set_scrollable(screen, false);

    // What this is, and what it shows
    block = block_create(screen);
    row = row_create(block, "Light Painting");
    corner = lv_obj_create(row);
    lv_obj_remove_style_all(corner);
    lv_obj_set_size(corner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(corner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(corner, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(corner, 6, 0);
    view.lock = lock_create(corner);
    view.note = lv_label_create(corner);
    lv_obj_add_style(view.note, ui_theme_muted(), 0);
    view.look = lv_label_create(block);
    lv_obj_set_style_text_font(view.look, &lv_font_montserrat_28, 0);

    block = block_create(screen);
    row = row_create(block, "Scene");
    view.scene = lv_label_create(row);
    swatch_create(block);

    block = block_create(screen);
    row = row_create(block, "Brightness");
    view.brightness = lv_label_create(row);
    view.brightness_bar = ui_theme_bar_create(block, STRIP_HEIGHT);

    return screen;
}

void ui_status_note(const char *note) {
    lv_label_set_text(view.note, note != nullptr ? note : "");
}

void ui_status_show(const ui_status_t *status) {
    ui_status_note(status->note);
    lv_obj_set_hidden(view.lock, !status->locked);
    lv_label_set_text(view.look, ui_names_look(status->look));
    lv_label_set_text(view.scene, ui_names_scene(status->scene));

    // The scene's three colours, field to hit
    for (size_t i = 0; i < SWATCH_CELLS; i++)
        lv_obj_set_style_bg_color(
            view.swatch[i],
            ui_theme_scene_color(status->scene, (scene_role_t)i), 0);

    lv_bar_set_value(view.brightness_bar, status->brightness_percent,
                     LV_ANIM_OFF);
    lv_label_set_text_fmt(view.brightness, "%d %%", status->brightness_percent);
}
