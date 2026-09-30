#include "ui_menu.h"

#include "config.h"
#include "ui_diagnostics.h"
#include "ui_names.h"
#include "ui_status.h"
#include "ui_theme.h"

#include <stdint.h>
#include <stdio.h>

constexpr int32_t ROW_HEIGHT = 28;

// The screen's edge to a row, and a row's edge to its text: text starts
// 12 px in, as on the status screen
constexpr int32_t SCREEN_INSET = 4;
constexpr int32_t ROW_INSET = 8;

// How often the idle time is checked
constexpr uint32_t IDLE_CHECK_MS = 1000;

// A second press within this long confirms a reset
constexpr uint32_t CONFIRM_MS = 3000;

// How a setting's stored value reads
typedef enum {
    FORMAT_LOOK,
    FORMAT_SCENE,
    FORMAT_NUMBER,
    // A factor in tenths: 15 reads 1.5x
    FORMAT_TIMES,
    // A number in tenths: 30 reads 3.0
    FORMAT_TENTHS,
    FORMAT_PERCENT,
    FORMAT_DB,
    // 1 reads On, 0 Off
    FORMAT_ON_OFF,
} format_t;

typedef enum {
    PAGE_STATUS,
    PAGE_MENU,
    PAGE_LOOK,
    PAGE_SOUND,
    PAGE_SYSTEM,
    PAGE_DIAGNOSTICS,
    PAGE_COUNT
} page_t;

// A row of a page: back to its parent, to another page, a setting, or the
// reset to defaults
typedef enum { ROW_BACK, ROW_PAGE, ROW_SETTING, ROW_RESET } row_kind_t;

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
    {.kind = ROW_PAGE, .name = "System", .page = PAGE_SYSTEM},
};

static const row_t look_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Look",
     .id = SETTINGS_LOOK,
     .format = FORMAT_LOOK},
    {.kind = ROW_SETTING,
     .name = "Scene",
     .id = SETTINGS_SCENE,
     .format = FORMAT_SCENE},
    {.kind = ROW_SETTING,
     .name = "Brightness",
     .id = SETTINGS_BRIGHTNESS,
     .format = FORMAT_PERCENT},
    {.kind = ROW_SETTING,
     .name = "Song parts",
     .id = SETTINGS_SONG_PARTS,
     .format = FORMAT_ON_OFF},
};

static const row_t sound_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Gain",
     .id = SETTINGS_GAIN,
     .format = FORMAT_TIMES},
    {.kind = ROW_SETTING,
     .name = "Hit sensitivity",
     .id = SETTINGS_HIT_SENSITIVITY,
     .format = FORMAT_TENTHS},
    {.kind = ROW_SETTING,
     .name = "Quiet floor",
     .id = SETTINGS_QUIET_FLOOR,
     .format = FORMAT_DB},
};

static const row_t system_rows[] = {
    {.kind = ROW_SETTING,
     .name = "Screen",
     .id = SETTINGS_BACKLIGHT,
     .format = FORMAT_PERCENT},
    {.kind = ROW_PAGE, .name = "Diagnostics", .page = PAGE_DIAGNOSTICS},
    {.kind = ROW_RESET, .name = "Reset to defaults"},
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
    [PAGE_SYSTEM] = {
        .title = "System",
        .parent = PAGE_MENU,
        .rows = system_rows,
        .row_count = sizeof(system_rows) / sizeof(system_rows[0]),
    },
    // Its screen is ui_diagnostics', see diagnostics_create()
    [PAGE_DIAGNOSTICS] = {
        .title = "Diagnostics",
        .parent = PAGE_SYSTEM,
    },
};

static const row_t back_row = {.kind = ROW_BACK};

static struct {
    settings_t *settings;
    ui_menu_changed_t *changed;
    page_t page;
    const char *note;
    // While the reset waits for its second press: gives up on it
    lv_timer_t *reset_timer;
    // The newest diagnostics, nullptr before the first
    const stats_report_t *report;
} menu;

static void show(page_t page);

static void format(char *text, size_t size, const row_t *row) {
    int value = settings_get(menu.settings, row->id);

    switch (row->format) {
    case FORMAT_LOOK:
        snprintf(text, size, "%s", ui_names_look((show_look_t)value));
        break;
    case FORMAT_SCENE:
        snprintf(text, size, "%s", ui_names_scene((scene_t)value));
        break;
    case FORMAT_TIMES:
        snprintf(text, size, "%d.%dx", value / 10, value % 10);
        break;
    case FORMAT_TENTHS:
        snprintf(text, size, "%d.%d", value / 10, value % 10);
        break;
    case FORMAT_PERCENT:
        snprintf(text, size, "%d %%", value);
        break;
    case FORMAT_DB:
        snprintf(text, size, "%d dB", value);
        break;
    case FORMAT_ON_OFF:
        snprintf(text, size, "%s", value != 0 ? "On" : "Off");
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

/**
 * Screens change after the key's events are done with the current one. The
 * key is ignored until released: held on, it would go on repeating into the
 * next screen, and right held on a page's row change its first setting
 */
static void open(page_t page) {
    lv_indev_t *keypad = lv_indev_active();

    if (keypad != nullptr)
        lv_indev_wait_release(keypad);

    lv_async_call(open_async, (void *)(uintptr_t)page);
}

static void back(void) { open(pages[menu.page].parent); }

// Every setting row of the screen shows its value again
static void show_values(lv_obj_t *screen) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(screen); i++) {
        lv_obj_t *row_obj = lv_obj_get_child(screen, i);
        const row_t *row = lv_obj_get_user_data(row_obj);

        if (row != nullptr && row->kind == ROW_SETTING)
            show_value(row_obj);
    }
}

// No second press in time: the reset row reads as before
static void reset_expired(lv_timer_t *timer) {
    lv_label_set_text(lv_timer_get_user_data(timer), "");
    menu.reset_timer = nullptr;
}

// Stops waiting for a second press, and clears label unless nullptr
static void reset_cancel(lv_obj_t *label) {
    if (menu.reset_timer != nullptr) {
        lv_timer_delete(menu.reset_timer);
        menu.reset_timer = nullptr;
    }

    if (label != nullptr)
        lv_label_set_text(label, "");
}

// The first press asks for a second within CONFIRM_MS, which resets every
// setting
static void reset(lv_obj_t *row_obj) {
    lv_obj_t *label = lv_obj_get_child(row_obj, 1);

    if (menu.reset_timer == nullptr) {
        menu.reset_timer = lv_timer_create(reset_expired, CONFIRM_MS, label);
        lv_timer_set_repeat_count(menu.reset_timer, 1);
        lv_label_set_text(label, "Press again");
        return;
    }

    reset_cancel(nullptr);
    settings_reset(menu.settings);
    ui_theme_set_scene((scene_t)settings_get(menu.settings, SETTINGS_SCENE));
    menu.changed(menu.settings, SETTINGS_ALL);
    show_values(lv_obj_get_parent(row_obj));
    lv_label_set_text(label, "Done");
}

static void change(lv_obj_t *row_obj, const row_t *row, int steps) {
    if (!settings_step(menu.settings, row->id, steps))
        return;

    // The accent follows the scene at once, on this row too
    if (row->id == SETTINGS_SCENE)
        ui_theme_set_scene(
            (scene_t)settings_get(menu.settings, SETTINGS_SCENE));

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
        else if (row->kind == ROW_RESET)
            reset(row_obj);
        break;
    case LV_EVENT_LONG_PRESSED:
        back();
        break;
    case LV_EVENT_FOCUSED:
    case LV_EVENT_DEFOCUSED:
        if (row->kind == ROW_SETTING)
            show_value(row_obj);
        // Leaving the row cancels a reset waiting for its second press
        if (row->kind == ROW_RESET)
            reset_cancel(lv_obj_get_child(row_obj, 1));
        break;
    case LV_EVENT_DELETE:
        // The screen goes, the timer must not reach its label
        if (row->kind == ROW_RESET)
            reset_cancel(nullptr);
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
    lv_obj_set_style_pad_hor(row_obj, ROW_INSET, 0);
    lv_obj_set_style_radius(row_obj, 6, 0);
    lv_obj_add_style(row_obj, ui_theme_focus(), LV_STATE_FOCUSED);
    lv_obj_set_scrollable(row_obj, false);
    lv_obj_set_scroll_on_focus(row_obj, true);
    lv_obj_set_user_data(row_obj, (void *)row);
    lv_obj_add_event_cb(row_obj, row_event, LV_EVENT_ALL, (void *)row);
    lv_group_add_obj(lv_group_get_default(), row_obj);

    label = lv_label_create(row_obj);
    lv_label_set_text(label, name);

    return row_obj;
}

// A row of a page's screen: its name and its value, arrow or reset note
static lv_obj_t *page_row(lv_obj_t *screen, const row_t *row) {
    lv_obj_t *row_obj = row_create(screen, row, row->name);

    if (row->kind == ROW_SETTING) {
        lv_label_create(row_obj);
        show_value(row_obj);
    } else if (row->kind == ROW_RESET) {
        lv_label_set_text(lv_label_create(row_obj), "");
    } else {
        lv_obj_t *arrow = lv_label_create(row_obj);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    }

    return row_obj;
}

// A page's screen, focused on the row leading back to from, if it has one,
// otherwise on its first row
static lv_obj_t *page_create(page_t page, page_t from) {
    const page_def_t *def = &pages[page];
    lv_obj_t *screen = lv_obj_create(nullptr), *row_obj, *focus;
    char title[32];

    lv_obj_add_style(screen, ui_theme_screen(), 0);

    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(screen, 6, 0);
    lv_obj_set_style_pad_hor(screen, SCREEN_INSET, 0);
    lv_obj_set_style_pad_row(screen, 2, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    snprintf(title, sizeof(title), LV_SYMBOL_LEFT "  %s", def->title);
    focus = row_create(screen, &back_row, title);
    lv_obj_add_style(focus, ui_theme_muted(), 0);

    for (size_t i = 0; i < def->row_count; i++) {
        const row_t *row = &def->rows[i];

        row_obj = page_row(screen, row);

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
        .look = (show_look_t)settings_get(menu.settings, SETTINGS_LOOK),
        .scene = (scene_t)settings_get(menu.settings, SETTINGS_SCENE),
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

// Left, or the centre held, goes back: the page has nothing else to do
static void diagnostics_event(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);

    if ((code == LV_EVENT_KEY && lv_event_get_key(event) == LV_KEY_LEFT) ||
        code == LV_EVENT_LONG_PRESSED)
        back();
}

static lv_obj_t *diagnostics_create(void) {
    lv_obj_t *screen = ui_diagnostics_create(), *keys;

    ui_diagnostics_show(menu.report);

    // Nothing to see, it takes the keys, outside the screen's layout
    keys = lv_obj_create(screen);
    lv_obj_remove_style_all(keys);
    lv_obj_set_ignore_layout(keys, true);
    lv_obj_add_event_cb(keys, diagnostics_event, LV_EVENT_ALL, nullptr);
    lv_group_add_obj(lv_group_get_default(), keys);
    lv_group_focus_obj(keys);

    return screen;
}

// Loads a page's screen, deleting the one it replaces
static void show(page_t page) {
    lv_obj_t *screen;

    if (page == PAGE_STATUS)
        screen = status_create();
    else if (page == PAGE_DIAGNOSTICS)
        screen = diagnostics_create();
    else
        screen = page_create(page, menu.page);

    menu.page = page;
    lv_screen_load_anim(screen, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
}

static void idle_check([[maybe_unused]] lv_timer_t *timer) {
    // The diagnostics are watched, not used: they stay until left
    if (menu.page != PAGE_STATUS && menu.page != PAGE_DIAGNOSTICS &&
        lv_display_get_inactive_time(nullptr) >= UI_IDLE_TIMEOUT_MS)
        show(PAGE_STATUS);
}

void ui_menu_note(const char *note) {
    menu.note = note;

    if (menu.page == PAGE_STATUS)
        ui_status_note(note);
}

void ui_menu_report(const stats_report_t *report) {
    menu.report = report;

    if (menu.page == PAGE_DIAGNOSTICS)
        ui_diagnostics_show(report);
}

void ui_menu_refresh(void) {
    ui_theme_set_scene((scene_t)settings_get(menu.settings, SETTINGS_SCENE));

    // The status screen is rebuilt, a page shows its values again
    if (menu.page == PAGE_STATUS)
        show(PAGE_STATUS);
    else
        show_values(lv_screen_active());

    lv_display_trigger_activity(nullptr);
}

void ui_menu_start(settings_t *settings, ui_menu_changed_t *changed) {
    menu.settings = settings;
    menu.changed = changed;

    ui_theme_init((scene_t)settings_get(settings, SETTINGS_SCENE));

    show(PAGE_STATUS);
    lv_timer_create(idle_check, IDLE_CHECK_MS, nullptr);
}
