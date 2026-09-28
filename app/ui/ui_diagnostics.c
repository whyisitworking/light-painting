#include "ui_diagnostics.h"

#include "ui_theme.h"

#include <math.h>
#include <stdio.h>

// The meters' span in dBFS: a quiet room reads about -85
constexpr int32_t METER_MIN_DBFS = -90;
constexpr int32_t METER_HEIGHT = 8;

// Above this share of a hop, the worst work wears the accent
constexpr float LOAD_WARNING_PERCENT = 80.f;

// Text starts 12 px in, as on the other screens. 8 + 16 + 6 * 16 +
// 6 * 5 + 8 = 158 of the 172 lines
constexpr int32_t MARGIN = 12;
constexpr int32_t EDGE = 8;
constexpr int32_t ROW_GAP = 5;

// What stands for a value before the first report
static const char *const NO_VALUE = "--";

static struct {
    lv_obj_t *meters[2];
    lv_obj_t *levels[2];
    lv_obj_t *ceiling;
    lv_obj_t *loudness;
    lv_obj_t *beats;
    lv_obj_t *work;
    lv_obj_t *load;
    lv_obj_t *lost;
    lv_obj_t *leds;
    // Core 1's, the menu, then core 0's, the lights
    lv_obj_t *stacks[2];
} view;

// A line across the screen, its contents spread to both ends
static lv_obj_t *row_create(lv_obj_t *parent) {
    lv_obj_t *row = lv_obj_create(parent);

    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    return row;
}

// A muted name, then its value, whose label it returns
static lv_obj_t *cell_create(lv_obj_t *row, const char *name) {
    lv_obj_t *cell = lv_obj_create(row), *label;

    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(cell, 5, 0);

    label = lv_label_create(cell);
    lv_label_set_text(label, name);
    lv_obj_add_style(label, ui_theme_muted(), 0);

    return lv_label_create(cell);
}

static void meter_create(lv_obj_t *parent, size_t channel, const char *name) {
    lv_obj_t *row = row_create(parent), *label = lv_label_create(row);

    lv_label_set_text(label, name);
    lv_obj_add_style(label, ui_theme_muted(), 0);

    view.meters[channel] = ui_theme_bar_create(row, METER_HEIGHT);
    lv_obj_set_flex_grow(view.meters[channel], 1);
    lv_bar_set_range(view.meters[channel], METER_MIN_DBFS, 0);

    view.levels[channel] = lv_label_create(row);
}

lv_obj_t *ui_diagnostics_create(void) {
    lv_obj_t *screen = lv_obj_create(nullptr), *title, *row;

    lv_obj_add_style(screen, ui_theme_screen(), 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(screen, MARGIN, 0);
    lv_obj_set_style_pad_ver(screen, EDGE, 0);
    lv_obj_set_style_pad_row(screen, ROW_GAP, 0);
    lv_obj_set_scrollable(screen, false);

    title = lv_label_create(screen);
    lv_label_set_text(title, LV_SYMBOL_LEFT "  Diagnostics");
    lv_obj_add_style(title, ui_theme_muted(), 0);

    meter_create(screen, 0, "L");
    meter_create(screen, 1, "R");

    row = row_create(screen);
    view.ceiling = cell_create(row, "Ceiling");
    view.loudness = cell_create(row, "Loud");
    view.beats = cell_create(row, "Beats");

    row = row_create(screen);
    view.work = cell_create(row, "Work");
    view.load = cell_create(row, "Load");

    row = row_create(screen);
    view.lost = cell_create(row, "Audio lost");
    view.leds = cell_create(row, "LEDs");

    row = row_create(screen);
    view.stacks[0] = cell_create(row, "Menu");
    view.stacks[1] = cell_create(row, "Lights");

    return screen;
}

// Wears the accent, or not
static void set_warning(lv_obj_t *label, bool warning) {
    lv_obj_remove_style(label, ui_theme_warning(), 0);
    if (warning)
        lv_obj_add_style(label, ui_theme_warning(), 0);
}

static void show_text(lv_obj_t *label, const char *format, double value) {
    char text[24];

    snprintf(text, sizeof(text), format, value);
    lv_label_set_text(label, text);
}

// "none" and an empty meter for no sound at all
static void show_level(size_t channel, float dbfs) {
    int32_t value = METER_MIN_DBFS;

    if (isinf(dbfs) || isnan(dbfs)) {
        lv_label_set_text(view.levels[channel], "none");
    } else {
        show_text(view.levels[channel], "%.0f dBFS", (double)dbfs);
        value = (int32_t)lroundf(fmaxf(dbfs, (float)METER_MIN_DBFS));
    }

    lv_bar_set_value(view.meters[channel], value, LV_ANIM_OFF);
}

static void show_stack(lv_obj_t *label, size_t peak, size_t size) {
    char text[24];

    snprintf(text, sizeof(text), "%.1f / %zu KB", (double)peak / 1024.0,
             size / 1024);
    lv_label_set_text(label, text);
}

static void show_nothing(void) {
    lv_obj_t *values[] = {
        view.levels[0], view.levels[1], view.ceiling, view.loudness,
        view.beats,     view.work,      view.load,    view.lost,
        view.leds,      view.stacks[0], view.stacks[1],
    };

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        lv_label_set_text(values[i], NO_VALUE);

    for (size_t channel = 0; channel < 2; channel++)
        lv_bar_set_value(view.meters[channel], METER_MIN_DBFS, LV_ANIM_OFF);

    set_warning(view.load, false);
    set_warning(view.lost, false);
}

void ui_diagnostics_show(const stats_report_t *report) {
    char text[32];

    if (report == nullptr) {
        show_nothing();
        return;
    }

    show_level(0, report->left_dbfs);
    show_level(1, report->right_dbfs);

    show_text(view.ceiling, "%.0f dB", (double)report->ceiling_db);
    show_text(view.loudness, "%.2f", (double)report->loudness);
    show_text(view.beats, "%.1f/s", (double)report->beats_per_s);

    snprintf(text, sizeof(text), "%.1f, max %.1f ms",
             (double)report->work_avg_us / 1000.0,
             (double)report->work_max_us / 1000.0);
    lv_label_set_text(view.work, text);
    show_text(view.load, "%.0f %%", (double)report->work_max_percent);
    set_warning(view.load, report->work_max_percent > LOAD_WARNING_PERCENT);

    snprintf(text, sizeof(text), "%lu", (unsigned long)report->audio_lost);
    lv_label_set_text(view.lost, text);
    set_warning(view.lost, report->audio_lost > 0);
    show_text(view.leds, "%.0f fps", (double)report->led_fps);

    show_stack(view.stacks[0], report->core1_stack_peak,
               report->core1_stack_size);
    show_stack(view.stacks[1], report->core0_stack_peak,
               report->core0_stack_size);
}
