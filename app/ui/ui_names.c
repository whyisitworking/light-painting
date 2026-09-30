#include "ui_names.h"

static const char *const looks[] = {
    [SHOW_LOOK_PULSE] = "Pulse",
    [SHOW_LOOK_FLOW] = "Flow",
    [SHOW_LOOK_STAGE] = "Stage",
    [SHOW_LOOK_SWEEP] = "Sweep",
    [SHOW_LOOK_STORM] = "Storm",
};

static const char *const scenes[] = {
    [SCENE_NEON_NOIR] = "Neon Noir",
    [SCENE_EMBER] = "Ember",
    [SCENE_DUSK] = "Dusk",
    [SCENE_ACID] = "Acid",
    [SCENE_ICE] = "Ice",
};

static const char *const parts[] = {
    [PARTS_CALM] = "Calm",
    [PARTS_BUILD] = "Build",
    [PARTS_GAP] = "Gap",
    [PARTS_HIGH] = "High",
};

static_assert(sizeof(looks) / sizeof(looks[0]) == SHOW_LOOK_COUNT,
              "every look needs a name");
static_assert(sizeof(scenes) / sizeof(scenes[0]) == SCENE_COUNT,
              "every scene needs a name");
static_assert(sizeof(parts) / sizeof(parts[0]) == PARTS_COUNT,
              "every song part needs a name");

const char *ui_names_look(show_look_t look) {
    return look < SHOW_LOOK_COUNT ? looks[look] : "?";
}

const char *ui_names_scene(scene_t scene) {
    return scene < SCENE_COUNT ? scenes[scene] : "?";
}

const char *ui_names_part(parts_part_t part) {
    return part < PARTS_COUNT ? parts[part] : "?";
}
