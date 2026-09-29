#include "ui_names.h"

static const char *const modes[] = {
    [EFFECTS_MODE_SPECTRUM] = "Spectrum",
    [EFFECTS_MODE_SPECTRUM_MIRRORED] = "Mirrored",
    [EFFECTS_MODE_RIVER] = "River",
    [EFFECTS_MODE_RIPPLES] = "Ripples",
    [EFFECTS_MODE_VU] = "VU meters",
    [EFFECTS_MODE_GLOW] = "Glow",
    [EFFECTS_MODE_POND] = "Pond",
};

static const char *const palettes[] = {
    [PALETTE_RAINBOW] = "Rainbow",
    [PALETTE_SYNTHWAVE] = "Synthwave",
    [PALETTE_FIRE] = "Fire",
    [PALETTE_OCEAN] = "Ocean",
};

static_assert(sizeof(modes) / sizeof(modes[0]) == EFFECTS_MODE_COUNT,
              "every mode needs a name");
static_assert(sizeof(palettes) / sizeof(palettes[0]) == PALETTE_COUNT,
              "every palette needs a name");

const char *ui_names_mode(effects_mode_t mode) {
    return mode < EFFECTS_MODE_COUNT ? modes[mode] : "?";
}

const char *ui_names_palette(palette_t palette) {
    return palette < PALETTE_COUNT ? palettes[palette] : "?";
}
