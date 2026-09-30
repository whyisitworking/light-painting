#include "check.h"
#include "scene.h"

#include <math.h>

static float brightest(rgb_t color) {
    return fmaxf(color.r, fmaxf(color.g, color.b));
}

// Every colour in range; the field darker than the accent, the hit the
// brightest, so the roles read as background, attention and peak
static void test_roles(void) {
    for (int scene = 0; scene < SCENE_COUNT; scene++) {
        rgb_t field = scene_color((scene_t)scene, SCENE_FIELD),
              accent = scene_color((scene_t)scene, SCENE_ACCENT),
              hit = scene_color((scene_t)scene, SCENE_HIT);

        for (int role = 0; role < SCENE_ROLE_COUNT; role++) {
            rgb_t color = scene_color((scene_t)scene, (scene_role_t)role);

            CHECK(color.r >= 0.f && color.r <= 1.f);
            CHECK(color.g >= 0.f && color.g <= 1.f);
            CHECK(color.b >= 0.f && color.b <= 1.f);
        }

        CHECK(brightest(field) < brightest(accent));
        CHECK(brightest(hit) >= brightest(accent));
        CHECK(field.r + field.g + field.b < hit.r + hit.g + hit.b);
    }
}

// Out of range: the first scene's field
static void test_out_of_range(void) {
    rgb_t first = scene_color(SCENE_NEON_NOIR, SCENE_FIELD),
          scene = scene_color(SCENE_COUNT, SCENE_FIELD),
          role = scene_color(SCENE_NEON_NOIR, SCENE_ROLE_COUNT);

    CHECK(scene.r == first.r && scene.g == first.g && scene.b == first.b);
    CHECK(role.r == first.r && role.g == first.g && role.b == first.b);
}

int main(void) {
    test_roles();
    test_out_of_range();

    return CHECK_REPORT();
}
