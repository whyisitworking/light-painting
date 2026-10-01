#include "scene.h"

static const rgb_t scenes[SCENE_COUNT][SCENE_ROLE_COUNT] = {
    // Deep blue, hot magenta, white
    [SCENE_NEON_NOIR] = {{0.f, 0.02f, 0.35f}, {1.f, 0.f, 0.45f}, {1.f, 1.f, 1.f}},
    // Dark red, amber, gold
    [SCENE_EMBER] = {{0.35f, 0.02f, 0.f}, {1.f, 0.35f, 0.f}, {1.f, 0.75f, 0.25f}},
    // Teal, amber, warm white
    [SCENE_DUSK] = {{0.f, 0.3f, 0.3f}, {1.f, 0.45f, 0.05f}, {1.f, 0.85f, 0.6f}},
    // Violet, lime, white
    [SCENE_ACID] = {{0.3f, 0.f, 0.6f}, {0.45f, 1.f, 0.f}, {1.f, 1.f, 1.f}},
    // Navy, cyan, white
    [SCENE_ICE] = {{0.f, 0.05f, 0.3f}, {0.f, 0.8f, 1.f}, {1.f, 1.f, 1.f}},
};

rgb_t scene_color(scene_t scene, scene_role_t role) {
    if (scene >= SCENE_COUNT)
        scene = SCENE_NEON_NOIR;
    if (role >= SCENE_ROLE_COUNT)
        role = SCENE_FIELD;

    return scenes[scene][role];
}
