#include "check.h"
#include "color.h"
#include "flashes.h"
#include "show.h"
#include "show_internal.h"

#include <math.h>
#include <string.h>

constexpr size_t LEDS = 300;
constexpr size_t BANDS = FEATURES_BAND_COUNT;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;

static float bands[BANDS];
static uint32_t pixels[LEDS];

static sound_t silence(void) {
    memset(bands, 0, sizeof(bands));
    return (sound_t){.bands = bands, .part = PARTS_CALM};
}

// Music as the features would report it in a part: every band at half, a
// groove, a low hit every 100 ms, mid and high hits in between
static sound_t music(size_t hop, parts_part_t part) {
    sound_t sound = silence();

    for (size_t b = 0; b < BANDS; b++)
        bands[b] = 0.7f;
    sound.loudness = 0.6f;
    sound.groove = 0.5f;
    sound.centroid = 0.4f;
    sound.part = part;
    sound.build_progress = part == PARTS_BUILD ? 0.5f : 0.f;
    sound.hits[FEATURES_LOW] = (features_hit_t){hop % 19 == 0, 0.8f};
    sound.hits[FEATURES_MID] = (features_hit_t){hop % 19 == 9, 0.6f};
    sound.hits[FEATURES_HIGH] = (features_hit_t){hop % 5 == 2, 0.5f};

    return sound;
}

static bool start(show_t *show, show_look_t look) {
    show_tuning_t tuning = show_default_tuning();

    if (!show_init(show, LEDS, BANDS, HOP_PERIOD_S, 10, 1))
        return false;
    tuning.look = look;
    show_tune(show, &tuning);

    return true;
}

static size_t lit(void) {
    size_t count = 0;

    for (size_t i = 0; i < LEDS; i++)
        count += pixels[i] != 0;

    return count;
}

// A dark song: no groove and no bands, only hits in every region and a
// drop every 20 hops, far more than the parts allow
static sound_t dark_hits(size_t hop) {
    sound_t sound = music(hop, PARTS_HIGH);

    memset(bands, 0, sizeof(bands));
    sound.loudness = 0.f;
    sound.groove = 0.f;
    sound.event = hop % 20 == 0 ? PARTS_DROP : PARTS_NONE;

    return sound;
}

// The music with a drop every 20 hops
static sound_t dropping(size_t hop) {
    sound_t sound = music(hop, PARTS_HIGH);

    sound.event = hop % 20 == 0 ? PARTS_DROP : PARTS_NONE;

    return sound;
}

// The light of the pixels sent, 0..1 of full, as the flash guard measures it
static float light(void) {
    float sum = 0.f;

    for (size_t i = 0; i < LEDS; i++) {
        color_ws2812_t color = {.value = pixels[i]};

        sum += (float)(color.grba.r + color.grba.g + color.grba.b);
    }

    return sum / (3.f * 255.f * (float)LEDS);
}

static void test_rejects_invalid(void) {
    show_t show;

    CHECK(!show_init(&show, 1, BANDS, HOP_PERIOD_S, 0, 1));
    CHECK(!show_init(&show, LEDS, 1, HOP_PERIOD_S, 0, 1));
    CHECK(!show_init(&show, LEDS, BANDS, 0.f, 0, 1));
}

// Every look: black in silence, lit by music in every part but the gap
static void test_every_look(void) {
    static const parts_part_t parts[] = {PARTS_CALM, PARTS_BUILD, PARTS_HIGH};

    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        for (size_t p = 0; p < 3; p++) {
            show_t show;
            size_t most = 0;
            sound_t quiet;

            CHECK(start(&show, (show_look_t)look));
            for (size_t hop = 0; hop < 200; hop++) {
                sound_t sound = music(hop, parts[p]);

                show_render(&show, &sound, pixels);
                most = lit() > most ? lit() : most;
            }
            CHECK(most > 0);
            if (most == 0)
                printf("look %d dark in part %zu\n", look, p);

            // Silence after music fades to black, exactly (silence() clears
            // the bands music() filled)
            quiet = silence();
            for (size_t hop = 0; hop < 2000; hop++)
                show_render(&show, &quiet, pixels);
            CHECK(lit() == 0);

            show_deinit(&show);
        }
    }
}

// A gap fades every look to black within a fifth of a second
static void test_gap(void) {
    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        show_t show;

        CHECK(start(&show, (show_look_t)look));
        for (size_t hop = 0; hop < 200; hop++) {
            sound_t sound = music(hop, PARTS_HIGH);
            show_render(&show, &sound, pixels);
        }
        for (size_t hop = 0; hop < (size_t)(0.2f / HOP_PERIOD_S); hop++) {
            sound_t sound = music(hop, PARTS_GAP);
            show_render(&show, &sound, pixels);
        }
        CHECK(lit() == 0);

        show_deinit(&show);
    }
}

// A gap that ends the song (silence past PARTS_GAP_MAX_S, calm after it):
// nothing the gap hid comes back, every look stays black
static void test_gap_ends_black(void) {
    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        show_t show;
        sound_t quiet;
        size_t most = 0;

        CHECK(start(&show, (show_look_t)look));
        for (size_t hop = 0; hop < 200; hop++) {
            sound_t sound = music(hop, PARTS_HIGH);
            show_render(&show, &sound, pixels);
        }
        // Made after the music, which fills the shared bands
        quiet = silence();
        quiet.part = PARTS_GAP;
        for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 3.1f; hop++)
            show_render(&show, &quiet, pixels);
        quiet.part = PARTS_CALM;
        for (size_t hop = 0; hop < 400; hop++) {
            show_render(&show, &quiet, pixels);
            most = lit() > most ? lit() : most;
        }
        if (most > 0)
            printf("look %d: %zu LEDs lit after the gap\n", look, most);
        CHECK(most == 0);

        show_deinit(&show);
    }
}

// Sweep's drop fires its whole volley: every beam of it, the ones queued
// furthest off the strip too, is still on its way after a tenth of a second
static void test_sweep_volley(void) {
    show_t show;
    sound_t sound;
    size_t beams = 0;

    CHECK(start(&show, SHOW_LOOK_SWEEP));
    sound = silence();
    sound.part = PARTS_HIGH;
    sound.event = PARTS_DROP;
    show_render(&show, &sound, pixels);
    sound.event = PARTS_NONE;
    for (size_t hop = 0; (float)hop * HOP_PERIOD_S < 0.1f; hop++)
        show_render(&show, &sound, pixels);

    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++)
        beams += show.blocks.beams[i].active;
    CHECK(beams == 2 * SHOW_SWEEP_VOLLEY);

    show_deinit(&show);
}

// A drop or a lift swaps the field and the accent, and a second swaps back
static void test_swap(void) {
    show_t show;
    sound_t sound = music(1, PARTS_HIGH);
    rgb_t field, accent;

    CHECK(start(&show, SHOW_LOOK_PULSE));
    field = scene_color(SHOW_SCENE, SCENE_FIELD);
    accent = scene_color(SHOW_SCENE, SCENE_ACCENT);

    sound.event = PARTS_DROP;
    show_render(&show, &sound, pixels);
    CHECK(show_color(&show, SCENE_FIELD).b == accent.b);
    CHECK(show_color(&show, SCENE_ACCENT).b == field.b);

    sound.event = PARTS_LIFT;
    show_render(&show, &sound, pixels);
    CHECK(show_color(&show, SCENE_FIELD).b == field.b);

    show_deinit(&show);
}

// The most flashes in any second of a look's pixels (flashes.h), for 4000
// hops of a sound
static size_t most_flashes(show_look_t look, sound_t (*sound_at)(size_t)) {
    const size_t per_s = (size_t)lroundf(1.f / HOP_PERIOD_S), hops = 4000;
    static float lights[4000];
    show_t show;

    CHECK(start(&show, look));
    for (size_t hop = 0; hop < hops; hop++) {
        sound_t sound = sound_at(hop);

        show_render(&show, &sound, pixels);
        lights[hop] = light();
    }
    show_deinit(&show);

    return flashes_most(lights, hops, per_s, RULES_FLASH_RISE);
}

// Whatever a look draws, no second holds more than three flashes: over the
// music with drops, and over a dark song whose hits and drops stand out
// most, where the guard holds some back (in at least one look: Stage's drop
// lights only its bars, empty in the dark)
static void test_flash_guard_for_every_look(void) {
    bool at_limit = false;

    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        size_t loud = most_flashes((show_look_t)look, dropping),
               dark = most_flashes((show_look_t)look, dark_hits);

        printf("look %d: at most %zu flashes a second in music, %zu in the "
               "dark\n",
               look, loud, dark);
        CHECK(loud <= RULES_FLASHES_PER_S);
        CHECK(dark <= RULES_FLASHES_PER_S);
        at_limit = at_limit || dark == RULES_FLASHES_PER_S;
    }

    // The dark song drives the guard to its limit: it was tested, not idle
    CHECK(at_limit);
}

// Calm with a groove and no hits: every look shows something in every
// scene, at full brightness. The washes are above the strip's lowest step
static void test_calm_shows(void) {
    for (int look = 0; look < SHOW_LOOK_COUNT; look++)
        for (int scene = 0; scene < SCENE_COUNT; scene++) {
            show_tuning_t tuning = show_default_tuning();
            sound_t sound = silence();
            show_t show;

            sound.loudness = 0.5f;
            sound.groove = 0.5f;
            sound.centroid = 0.5f;
            CHECK(start(&show, (show_look_t)look));
            tuning.look = (show_look_t)look;
            tuning.scene = (scene_t)scene;
            show_tune(&show, &tuning);
            for (size_t hop = 0; hop < 200; hop++)
                show_render(&show, &sound, pixels);
            if (lit() == 0)
                printf("look %d dark in calm, scene %d\n", look, scene);
            CHECK(lit() > 0);

            show_deinit(&show);
        }
}

// The same seed and sound give the same pixels; another look starts clean
static void test_deterministic_and_clean_switch(void) {
    static uint32_t other[LEDS];
    show_t one, two;
    show_tuning_t tuning = show_default_tuning();
    size_t differing = 0;
    bool flow_clean = true;

    CHECK(start(&one, SHOW_LOOK_STORM));
    CHECK(start(&two, SHOW_LOOK_STORM));
    for (size_t hop = 0; hop < 400; hop++) {
        sound_t sound = music(hop, PARTS_HIGH);

        show_render(&one, &sound, pixels);
        show_render(&two, &sound, other);
        differing += memcmp(pixels, other, sizeof(pixels)) != 0;
    }
    CHECK(differing == 0);

    // A switch starts the new look clean: Flow with music, then Pulse
    // (bursts and sparks), then Flow again, with nothing of either left
    tuning.look = SHOW_LOOK_FLOW;
    show_tune(&one, &tuning);
    for (size_t hop = 0; hop < 200; hop++) {
        sound_t sound = music(hop, PARTS_HIGH);
        show_render(&one, &sound, pixels);
    }
    tuning.look = SHOW_LOOK_PULSE;
    show_tune(&one, &tuning);
    for (size_t hop = 0; hop < 20; hop++) {
        sound_t sound = music(hop, PARTS_HIGH);
        show_render(&one, &sound, pixels);
    }
    tuning.look = SHOW_LOOK_FLOW;
    show_tune(&one, &tuning);
    for (size_t i = 0; i < one.half_led_count; i++)
        flow_clean = flow_clean && one.flow.history[i].r == 0.f &&
                     one.flow.history[i].g == 0.f &&
                     one.flow.history[i].b == 0.f;
    for (size_t i = 0; i < LEDS; i++)
        flow_clean = flow_clean && one.blocks.sparks[i] == 0.f;
    for (size_t i = 0; i < BLOCKS_MAX_BURSTS; i++)
        flow_clean = flow_clean && !one.blocks.bursts[i].active;
    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++)
        flow_clean = flow_clean && !one.blocks.beams[i].active;
    CHECK(flow_clean);

    show_deinit(&one);
    show_deinit(&two);
}

// Brightness 0 is dark; out of range values keep the tuning
static void test_tuning(void) {
    show_t show;
    show_tuning_t tuning = show_default_tuning();

    CHECK(start(&show, SHOW_LOOK_PULSE));
    tuning.brightness = 0.f;
    show_tune(&show, &tuning);
    for (size_t hop = 0; hop < 100; hop++) {
        sound_t sound = music(hop, PARTS_HIGH);
        show_render(&show, &sound, pixels);
    }
    CHECK(lit() == 0);

    tuning.look = SHOW_LOOK_COUNT;
    tuning.scene = SCENE_COUNT;
    tuning.brightness = NAN;
    show_tune(&show, &tuning);
    CHECK(show.tuning.look == SHOW_LOOK_PULSE);
    CHECK(show.tuning.scene == SHOW_SCENE);
    CHECK(show.tuning.brightness == 0.f);

    show_deinit(&show);
}

int main(void) {
    test_rejects_invalid();
    test_every_look();
    test_gap();
    test_gap_ends_black();
    test_swap();
    test_sweep_volley();
    test_flash_guard_for_every_look();
    test_calm_shows();
    test_deterministic_and_clean_switch();
    test_tuning();

    return CHECK_REPORT();
}
