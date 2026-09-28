#include "ui_theme.h"

#include <math.h>

// A slate rather than black, see ui_theme.h
constexpr uint32_t BACKGROUND_COLOR = 0x111317;
constexpr uint32_t TEXT_COLOR = 0xECECF1;
constexpr uint32_t MUTED_COLOR = 0x8A8F98;
constexpr uint32_t TRACK_COLOR = 0x2C2F37;

// Where on the palette the accent comes from: its middle, the most vivid
// point of each
constexpr float ACCENT_POSITION = 0.5f;

// How much of the accent tints the dark text on it, of 255
constexpr uint8_t ON_ACCENT_TINT = 24;

static lv_style_t screen, muted, focus, track, fill;

lv_color_t ui_theme_palette_color(palette_t palette, float position) {
    rgb_t color = palette_color(palette, position);

    return lv_color_make(
        (uint8_t)lroundf(fminf(fmaxf(color.r, 0.f), 1.f) * 255.f),
        (uint8_t)lroundf(fminf(fmaxf(color.g, 0.f), 1.f) * 255.f),
        (uint8_t)lroundf(fminf(fmaxf(color.b, 0.f), 1.f) * 255.f));
}

static void set_accent(palette_t palette) {
    lv_color_t accent = ui_theme_palette_color(palette, ACCENT_POSITION);

    lv_style_set_bg_color(&focus, accent);
    lv_style_set_text_color(&focus,
                            lv_color_mix(accent, lv_color_black(),
                                         ON_ACCENT_TINT));
    lv_style_set_bg_color(&fill, accent);
}

void ui_theme_init(palette_t palette) {
    lv_style_init(&screen);
    lv_style_set_bg_color(&screen, lv_color_hex(BACKGROUND_COLOR));
    lv_style_set_bg_opa(&screen, LV_OPA_COVER);
    lv_style_set_text_color(&screen, lv_color_hex(TEXT_COLOR));

    lv_style_init(&muted);
    lv_style_set_text_color(&muted, lv_color_hex(MUTED_COLOR));

    lv_style_init(&focus);
    lv_style_set_bg_opa(&focus, LV_OPA_COVER);

    lv_style_init(&track);
    lv_style_set_bg_color(&track, lv_color_hex(TRACK_COLOR));
    lv_style_set_bg_opa(&track, LV_OPA_COVER);

    lv_style_init(&fill);
    lv_style_set_bg_opa(&fill, LV_OPA_COVER);

    set_accent(palette);
}

void ui_theme_set_palette(palette_t palette) {
    set_accent(palette);

    // Everything wearing them is redrawn
    lv_obj_report_style_change(&focus);
    lv_obj_report_style_change(&fill);
}

const lv_style_t *ui_theme_screen(void) { return &screen; }

const lv_style_t *ui_theme_muted(void) { return &muted; }

const lv_style_t *ui_theme_focus(void) { return &focus; }

const lv_style_t *ui_theme_track(void) { return &track; }

const lv_style_t *ui_theme_fill(void) { return &fill; }
