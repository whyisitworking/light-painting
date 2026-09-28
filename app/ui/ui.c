#include "ui.h"

#include "config.h"
#include "diagnostics.h"
#include "joystick.h"
#include "persist.h"
#include "settings.h"
#include "st7789.h"
#include "tuning_link.h"
#include "ui_menu.h"
#include "ui_port.h"

#include <pico/multicore.h>
#include <pico/runtime_init.h>
#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#ifdef BOOT_BUTTON_SHUFFLE
#include "boot_button.h"

#include <pico/rand.h>

// How often BOOT is read. A bounce, a few ms, gives one press at most
constexpr uint32_t BOOT_CHECK_MS = 50;
#endif

// How often a pending save is looked at
constexpr uint32_t SAVE_CHECK_MS = 250;

// Core 1's stack, 8-byte aligned as the Arm procedure call standard asks
static alignas(8) uint32_t stack[UI_STACK_SIZE / sizeof(uint32_t)];

// What the menu shows and changes
static settings_t settings;

// What the flash holds, and when to save if the settings differ from it
static settings_t saved;
static bool save_pending = false;
static uint32_t save_at_ms;

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// After each change in the menu: the backlight here, the rest on core 0.
// SETTINGS_ALL: all of them
static void changed(const settings_t *changed_settings, settings_id_t id) {
    visualizer_tuning_t tuning;

    save_pending = true;
    save_at_ms = now_ms() + UI_SAVE_DELAY_MS;

    if (id == SETTINGS_BACKLIGHT || id == SETTINGS_ALL)
        st7789_set_backlight(
            settings_value(changed_settings, SETTINGS_BACKLIGHT));

    if (id != SETTINGS_BACKLIGHT) {
        tuning = settings_tuning(changed_settings);
        tuning_link_publish(&tuning);
    }
}

// A while after the last change, if the settings differ from the saved
// ones. The flash is busy for up to about 400 ms: the menu pauses, the
// lights on core 0 do not
static void save_check([[maybe_unused]] lv_timer_t *timer) {
    if (!save_pending || (int32_t)(now_ms() - save_at_ms) < 0)
        return;

    if (memcmp(&settings, &saved, sizeof(settings)) == 0) {
        save_pending = false;
        return;
    }

    if (persist_save(&settings)) {
        saved = settings;
        save_pending = false;
        ui_menu_note("Saved");
    } else {
        save_at_ms = now_ms() + UI_SAVE_RETRY_MS;
        ui_menu_note("Not saved");
    }
}

#ifdef BOOT_BUTTON_SHUFFLE
// Each press of BOOT: a random look, shown, applied and saved like a change
// made in the menu
static void boot_check([[maybe_unused]] lv_timer_t *timer) {
    static bool was_pressed = false;
    static uint32_t random = 0;
    bool pressed = boot_button_read();

    if (pressed && !was_pressed) {
        while (random == 0)
            random = get_rand_32();

        settings_shuffle(&settings, &random);
        ui_menu_refresh();
        changed(&settings, SETTINGS_ALL);
    }

    was_pressed = pressed;
}
#endif

// The newest diagnostics, every DIAGNOSTICS_PERIOD_MS
static void report_check([[maybe_unused]] lv_timer_t *timer) {
    const stats_report_t *report = diagnostics_take();

    if (report == nullptr)
        return;

#ifdef PRINT_DIAGNOSTICS
    diagnostics_print(report);
#endif
    ui_menu_report(report);
}

#ifdef PRINT_DIAGNOSTICS
// Without the LCD, the diagnostics still go out over USB
static void print_forever(void) {
    while (true) {
        const stats_report_t *report;

        sleep_ms(DIAGNOSTICS_PERIOD_MS);
        if ((report = diagnostics_take()) != nullptr)
            diagnostics_print(report);
    }
}
#endif

// The LCD, the switch and LVGL. nullptr if any fails, which it reports
static lv_display_t *setup(void) {
    lv_display_t *display;

    // Here, on core 1: its DMA interrupt is enabled on the calling core
    if (!st7789_init(&(st7789_config_t){
            .spi_index = LCD_SPI_INDEX,
            .sck_pin = LCD_SCK_PIN,
            .mosi_pin = LCD_MOSI_PIN,
            .cs_pin = LCD_CS_PIN,
            .dc_pin = LCD_DC_PIN,
            .reset_pin = LCD_RESET_PIN,
            .backlight_pin = LCD_BACKLIGHT_PIN,
            .width = LCD_WIDTH,
            .height = LCD_HEIGHT,
            .column_offset = LCD_COLUMN_OFFSET,
            .row_offset = LCD_ROW_OFFSET,
            .madctl = LCD_MADCTL,
            .baud_hz = LCD_SPI_HZ,
        })) {
        printf("Could not initialize the LCD\n");
        return nullptr;
    }

    if (!joystick_init(&(joystick_pins_t){
            .up_pin = JOYSTICK_UP_PIN,
            .down_pin = JOYSTICK_DOWN_PIN,
            .left_pin = JOYSTICK_LEFT_PIN,
            .right_pin = JOYSTICK_RIGHT_PIN,
            .centre_pin = JOYSTICK_CENTRE_PIN,
        })) {
        printf("Could not initialize the joystick\n");
        return nullptr;
    }

    if ((display = ui_port_init()) == nullptr) {
        printf("Could not initialize LVGL\n");
        return nullptr;
    }

    return display;
}

static void ui_main(void) {
    lv_display_t *display;
    uint32_t wait_ms;

    // The stack ends at its bottom: past it, core 1 faults and stops rather
    // than write over what lies below, core 0's drivers among it. The lights
    // carry on (RP2350: the stack limit register, MSPLIM)
    runtime_init_per_core_install_stack_guard(stack);

    if ((display = setup()) == nullptr) {
#ifdef PRINT_DIAGNOSTICS
        print_forever();
#endif
        return;
    }

    ui_menu_start(&settings, changed);
    lv_timer_create(save_check, SAVE_CHECK_MS, nullptr);
    lv_timer_create(report_check, DIAGNOSTICS_PERIOD_MS, nullptr);
#ifdef BOOT_BUTTON_SHUFFLE
    lv_timer_create(boot_check, BOOT_CHECK_MS, nullptr);
#endif

    // The display memory holds noise after a reset: light it only once the
    // first frame is on it
    lv_refr_now(display);
    while (st7789_is_busy())
        tight_loop_contents();
    st7789_set_backlight(settings_value(&settings, SETTINGS_BACKLIGHT));

    printf("LCD init!\n");

    while (true) {
        wait_ms = lv_timer_handler();
        sleep_ms(wait_ms == LV_NO_TIMER_READY ? LV_DEF_REFR_PERIOD : wait_ms);
    }
}

void ui_start(const settings_t *initial) {
    settings = *initial;
    saved = *initial;
    diagnostics_fill_core1_stack(stack, sizeof(stack) / sizeof(stack[0]));
    multicore_launch_core1_with_stack(ui_main, stack, sizeof(stack));
}
