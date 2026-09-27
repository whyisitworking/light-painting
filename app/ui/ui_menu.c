#include "ui_menu.h"

#include "config.h"
#include "ui_names.h"
#include "ui_status.h"

#include <stdint.h>
#include <stdio.h>

// The focused row, and what is written on it
constexpr uint32_t FOCUS_COLOR = 0xFF3D8B;
constexpr uint32_t FOCUS_TEXT_COLOR = 0x1A0010;
constexpr uint32_t MUTED_COLOR = 0x8888A0;

constexpr int32_t ROW_HEIGHT = 28;

// How often the idle time is checked
constexpr uint32_t IDLE_CHECK_MS = 1000;

// How a setting's stored value reads
typedef enum {
    FORMAT_MODE,
    FORMAT_PALETTE,
    FORMAT_NUMBER,
    // Tenths: 20 reads 2.0
    FORMAT_TENTHS,
    // A factor in tenths: 15 reads 1.5x
    FORMAT_TIMES,
    FORMAT_PERCENT,
    // Per mille, read as a percentage: 30 reads 3.0 %
    FORMAT_PER_MILLE,
    FORMAT_DB,
    FORMAT_MS,
    // 0 reads Off
    FORMAT_SECONDS,
} format_t;

typedef enum {
    PAGE_STATUS,
    PAGE_MENU,
    PAGE_LOOK,
    PAGE_SOUND,
    PAGE_EFFECTS,
    PAGE_SYSTEM,
    PAGE_COUNT
} page_t;

// A row of a page: back to its parent, to another page, or a setting
typedef enum { ROW_BACK, ROW_PAGE, ROW_SETTING } row_kind_t;

typedef struct {
    row_kind_t kind;
    const char *name;
    // ROW_PAGE
    page_t page;
    // ROW_SETTING
    settings_id_t id;
    format_t format;
} row_t;

typedef struct {
    const char *title;
    page_t parent;
    const row_t *rows;
    size_t row_count;
} page_def_t;

static const row_t menu_rows[] = {
    {.kind = ROW_PAGE, .name = "Look", .page = PAGE_LOOK},
    {.kind = ROW_PAGE, .name = "Sound", .page = PAGE_SOUND},
    {.kind = ROW_PAGE, .name = "Effects", .page = PAGE_EFFECTS},
    {.kind = ROW_PAGE, .name = "System", .page = PAGE_SYSTEM},
};

static const row_t look_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Mode",
     .id = SETTINGS_MODE,
     .format = FORMAT_MODE},
    {.kind = ROW_SETTING,
     .name = "Palette",
     .id = SETTINGS_PALETTE,
     .format = FORMAT_PALETTE},
    {.kind = ROW_SETTING,
     .name = "Brightness",
     .id = SETTINGS_BRIGHTNESS,
     .format = FORMAT_PERCENT},
};

static const row_t sound_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Gain",
     .id = SETTINGS_GAIN,
     .format = FORMAT_TIMES},
    {.kind = ROW_SETTING,
     .name = "Beat threshold",
     .id = SETTINGS_BEAT_THRESHOLD,
     .format = FORMAT_TIMES},
    {.kind = ROW_SETTING,
     .name = "Quiet floor",
     .id = SETTINGS_QUIET_FLOOR,
     .format = FORMAT_DB},
    {.kind = ROW_SETTING,
     .name = "Attack",
     .id = SETTINGS_ATTACK,
     .format = FORMAT_MS},
    {.kind = ROW_SETTING,
     .name = "Decay",
     .id = SETTINGS_DECAY,
     .format = FORMAT_MS},
};

static const row_t effects_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Palette drift",
     .id = SETTINGS_DRIFT,
     .format = FORMAT_SECONDS},
    {.kind = ROW_SETTING,
     .name = "Warmth",
     .id = SETTINGS_WARMTH,
     .format = FORMAT_PERCENT},
    {.kind = ROW_SETTING,
     .name = "Beat flash",
     .id = SETTINGS_FLASH,
     .format = FORMAT_PERCENT},
    {.kind = ROW_SETTING,
     .name = "Sparkles",
     .id = SETTINGS_SPARKLES,
     .format = FORMAT_PER_MILLE},
    {.kind = ROW_SETTING,
     .name = "River speed",
     .id = SETTINGS_RIVER_SPEED,
     .format = FORMAT_NUMBER},
    {.kind = ROW_SETTING,
     .name = "Ripple speed",
     .id = SETTINGS_RIPPLE_SPEED,
     .format = FORMAT_TENTHS},
    {.kind = ROW_SETTING,
     .name = "VU peak hold",
     .id = SETTINGS_PEAK_HOLD,
     .format = FORMAT_MS},
};

static const row_t system_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Screen",
     .id = SETTINGS_BACKLIGHT,
     .format = FORMAT_PERCENT},
};

static const page_def_t pages[PAGE_COUNT] = {
    [PAGE_MENU] = {
        .title = "Menu",
        .parent = PAGE_STATUS,
        .rows = menu_rows,
        .row_count = sizeof(menu_rows) / sizeof(menu_rows[0]),
    },
    [PAGE_LOOK] = {
        .title = "Look",
        .parent = PAGE_MENU,
        .rows = look_rows,
        .row_count = sizeof(look_rows) / sizeof(look_rows[0]),
    },
    [PAGE_SOUND] = {
        .title = "Sound",
        .parent = PAGE_MENU,
        .rows = sound_rows,
        .row_count = sizeof(sound_rows) / sizeof(sound_rows[0]),
    },
    [PAGE_EFFECTS] = {
        .title = "Effects",
        .parent = PAGE_MENU,
        .rows = effects_rows,
        .row_count = sizeof(effects_rows) / sizeof(effects_rows[0]),
    },
    [PAGE_SYSTEM] = {
        .title = "System",
        .parent = PAGE_MENU,
        .rows = system_rows,
        .row_count = sizeof(system_rows) / sizeof(system_rows[0]),
    },
};

static const row_t back_row = {.kind = ROW_BACK};

static struct {
    settings_t *settings;
    ui_menu_changed_t *changed;
    page_t page;
    const char *note;
} menu;

static void show(page_t page);

static void format(char *text, size_t size, const row_t *row) {
    int value = settings_get(menu.settings, row->id);

    switch (row->format) {
    case FORMAT_MODE:
        snprintf(text, size, "%s", ui_names_mode((effects_mode_t)value));
        break;
    case FORMAT_PALETTE:
        snprintf(text, size, "%s", ui_names_palette((palette_t)value));
        break;
    case FORMAT_TENTHS:
        snprintf(text, size, "%d.%d", value / 10, value % 10);
        break;
    case FORMAT_TIMES:
        snprintf(text, size, "%d.%dx", value / 10, value % 10);
        break;
    case FORMAT_PERCENT:
        snprintf(text, size, "%d %%", value);
        break;
    case FORMAT_PER_MILLE:
        snprintf(text, size, "%d.%d %%", value / 10, value % 10);
        break;
    case FORMAT_DB:
        snprintf(text, size, "%d dB", value);
        break;
    case FORMAT_MS:
        snprintf(text, size, "%d ms", value);
        break;
    case FORMAT_SECONDS:
        if (value == 0)
            snprintf(text, size, "Off");
        else
            snprintf(text, size, "%d s", value);
        break;
    case FORMAT_NUMBER:
    default:
        snprintf(text, size, "%d", value);
        break;
    }
}

// A setting row's value, with arrows while it is the one left and right
// change
static void show_value(lv_obj_t *row_obj) {
    const row_t *row = lv_obj_get_user_data(row_obj);
    lv_obj_t *label = lv_obj_get_child(row_obj, 1);
    char value[24];

    format(value, sizeof(value), row);

    if (lv_obj_has_state(row_obj, LV_STATE_FOCUSED))
        lv_label_set_text_fmt(label, LV_SYMBOL_LEFT " %s " LV_SYMBOL_RIGHT,
                              value);
    else
        lv_label_set_text(label, value);
}

static void open_async(void *page) { show((page_t)(uintptr_t)page); }

// Screens change after the key's events are done with the current one
static void open(page_t page) {
    lv_async_call(open_async, (void *)(uintptr_t)page);
}

static void back(void) { open(pages[menu.page].parent); }

static void change(lv_obj_t *row_obj, const row_t *row, int steps) {
    if (!settings_step(menu.settings, row->id, steps))
        return;

    show_value(row_obj);
    menu.changed(menu.settings, row->id);
}

static void row_event(lv_event_t *event) {
    lv_obj_t *row_obj = lv_event_get_target(event);
    const row_t *row = lv_event_get_user_data(event);
    uint32_t key;

    switch (lv_event_get_code(event)) {
    case LV_EVENT_KEY:
        key = lv_event_get_key(event);
        if (row->kind == ROW_SETTING && key == LV_KEY_LEFT)
            change(row_obj, row, -1);
        else if (row->kind == ROW_SETTING && key == LV_KEY_RIGHT)
            change(row_obj, row, 1);
        else if (row->kind == ROW_BACK && key == LV_KEY_LEFT)
            back();
        else if (row->kind == ROW_PAGE && key == LV_KEY_RIGHT)
            open(row->page);
        break;
    case LV_EVENT_SHORT_CLICKED:
        if (row->kind == ROW_BACK)
            back();
        else if (row->kind == ROW_PAGE)
            open(row->page);
        break;
    case LV_EVENT_LONG_PRESSED:
        back();
        break;
    case LV_EVENT_FOCUSED:
    case LV_EVENT_DEFOCUSED:
        if (row->kind == ROW_SETTING)
            show_value(row_obj);
        break;
    default:
        break;
    }
}

static lv_obj_t *row_create(lv_obj_t *screen, const row_t *row,
                            const char *name) {
    lv_obj_t *row_obj = lv_obj_create(screen), *label;

    lv_obj_remove_style_all(row_obj);
    lv_obj_set_size(row_obj, LV_PCT(100), ROW_HEIGHT);
    lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(row_obj, 10, 0);
    lv_obj_set_style_radius(row_obj, 6, 0);
    lv_obj_set_style_bg_opa(row_obj, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(row_obj, lv_color_hex(FOCUS_COLOR),
                              LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(row_obj, lv_color_hex(FOCUS_TEXT_COLOR),
                                LV_STATE_FOCUSED);
    lv_obj_set_scrollable(row_obj, false);
    lv_obj_set_scroll_on_focus(row_obj, true);
    lv_obj_set_user_data(row_obj, (void *)row);
    lv_obj_add_event_cb(row_obj, row_event, LV_EVENT_ALL, (void *)row);
    lv_group_add_obj(lv_group_get_default(), row_obj);

    label = lv_label_create(row_obj);
    lv_label_set_text(label, name);

    return row_obj;
}

// A page's screen, focused on the row leading back to from, if it has one,
// otherwise on its first row
static lv_obj_t *page_create(page_t page, page_t from) {
    const page_def_t *def = &pages[page];
    lv_obj_t *screen = lv_obj_create(nullptr), *row_obj, *focus;
    char title[32];

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 6, 0);
    lv_obj_set_style_pad_row(screen, 2, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    snprintf(title, sizeof(title), LV_SYMBOL_LEFT "  %s", def->title);
    focus = row_create(screen, &back_row, title);
    lv_obj_set_style_text_color(focus, lv_color_hex(MUTED_COLOR), 0);

    for (size_t i = 0; i < def->row_count; i++) {
        const row_t *row = &def->rows[i];

        row_obj = row_create(screen, row, row->name);

        if (row->kind == ROW_SETTING) {
            lv_label_create(row_obj);
            show_value(row_obj);
        } else {
            lv_obj_t *arrow = lv_label_create(row_obj);
            lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        }

        if (i == 0 || (row->kind == ROW_PAGE && row->page == from))
            focus = row_obj;
    }

    lv_group_focus_obj(focus);

    return screen;
}

// The centre on the status screen opens the menu
static void status_event(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED)
        open(PAGE_MENU);
}

static lv_obj_t *status_create(void) {
    lv_obj_t *screen = ui_status_create(), *opener;

    ui_status_show(&(ui_status_t){
        .mode = (effects_mode_t)settings_get(menu.settings, SETTINGS_MODE),
        .palette = (palette_t)settings_get(menu.settings, SETTINGS_PALETTE),
        .brightness_percent =
            settings_get(menu.settings, SETTINGS_BRIGHTNESS),
        .note = menu.note,
    });

    // Nothing to see, it takes the keys
    opener = lv_obj_create(screen);
    lv_obj_remove_style_all(opener);
    lv_obj_add_event_cb(opener, status_event, LV_EVENT_SHORT_CLICKED,
                        nullptr);
    lv_group_add_obj(lv_group_get_default(), opener);
    lv_group_focus_obj(opener);

    return screen;
}

// Loads a page's screen, deleting the one it replaces
static void show(page_t page) {
    lv_obj_t *screen = page == PAGE_STATUS ? status_create()
                                           : page_create(page, menu.page);

    menu.page = page;
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static void idle_check([[maybe_unused]] lv_timer_t *timer) {
    if (menu.page != PAGE_STATUS &&
        lv_display_get_inactive_time(nullptr) >= UI_IDLE_TIMEOUT_MS)
        show(PAGE_STATUS);
}

void ui_menu_note(const char *note) {
    menu.note = note;

    if (menu.page == PAGE_STATUS)
        ui_status_note(note);
}

void ui_menu_start(settings_t *settings, ui_menu_changed_t *changed) {
    menu.settings = settings;
    menu.changed = changed;

    show(PAGE_STATUS);
    lv_timer_create(idle_check, IDLE_CHECK_MS, nullptr);
}
