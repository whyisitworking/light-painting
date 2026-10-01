#include "blocks.h"
#include "check.h"

#include <math.h>

constexpr size_t LEDS = 300;
constexpr float HOP_PERIOD_S = 256.f / 48828.125f;
static const rgb_t WHITE = {1.f, 1.f, 1.f};

static float sum(const blocks_t *blocks) {
    float total = 0.f;

    for (size_t i = 0; i < blocks->led_count; i++)
        total += blocks->frame[i].r + blocks->frame[i].g + blocks->frame[i].b;

    return total;
}

static bool dark(const blocks_t *blocks, size_t from, size_t to) {
    for (size_t i = from; i < to; i++)
        if (blocks->frame[i].r != 0.f || blocks->frame[i].g != 0.f ||
            blocks->frame[i].b != 0.f)
            return false;
    return true;
}

static void test_rejects_invalid(void) {
    blocks_t blocks;

    CHECK(!blocks_init(&blocks, 1, HOP_PERIOD_S, 0));
    CHECK(!blocks_init(&blocks, LEDS, 0.f, 0));
}

static void test_wash(void) {
    blocks_t blocks;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_clear(&blocks);
    blocks_wash(&blocks, 10, 20, (rgb_t){0.1f, 0.2f, 0.3f});
    blocks_wash(&blocks, 15, 1000, (rgb_t){0.1f, 0.f, 0.f});

    CHECK(dark(&blocks, 0, 10));
    CHECK(blocks.frame[10].g == 0.2f);
    CHECK_NEAR(blocks.frame[15].r, 0.2f, 1e-6);
    CHECK(blocks.frame[LEDS - 1].r == 0.1f);

    blocks_deinit(&blocks);
}

// A burst grows to its radius and no further, fades, and frees its slot
static void test_burst(void) {
    blocks_t blocks;
    size_t lit = 0;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_burst(&blocks, 150.f, 10.f, 1e6f, 1.f, 0.05f, WHITE);

    blocks_clear(&blocks);
    blocks_draw(&blocks, WHITE, 0.06f);
    for (size_t i = 0; i < LEDS; i++)
        lit += blocks.frame[i].r > 0.f;
    CHECK(lit == 21);
    CHECK(blocks.frame[150].r == 1.f);
    CHECK(dark(&blocks, 0, 139) && dark(&blocks, 162, LEDS));

    for (int hop = 0; hop < 200; hop++) {
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    CHECK(sum(&blocks) == 0.f);
    CHECK(!blocks.bursts[0].active);

    blocks_deinit(&blocks);
}

// A full pool gives way to the new burst in place of the dimmest
static void test_burst_pool(void) {
    blocks_t blocks;
    size_t active = 0;
    bool dimmest_gone = true;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    for (size_t i = 0; i < BLOCKS_MAX_BURSTS; i++)
        blocks_burst(&blocks, (float)i, 1.f, 1.f, 0.5f + 0.01f * (float)i, 1.f,
                     WHITE);
    blocks_burst(&blocks, 200.f, 1.f, 1.f, 1.f, 1.f, WHITE);

    for (size_t i = 0; i < BLOCKS_MAX_BURSTS; i++) {
        active += blocks.bursts[i].active;
        dimmest_gone = dimmest_gone && blocks.bursts[i].centre != 0.f;
    }
    CHECK(active == BLOCKS_MAX_BURSTS);
    CHECK(dimmest_gone);

    blocks_deinit(&blocks);
}

// A beam moves at its speed with its head the brightest, and ends once its
// tail left the strip
static void test_beam(void) {
    blocks_t blocks;
    size_t brightest = 0;
    int hops = 0;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_beam(&blocks, 0.f, 1000.f, 20.f, 1.f, WHITE);

    for (int hop = 0; hop < 20; hop++) {
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    for (size_t i = 1; i < LEDS; i++)
        if (blocks.frame[i].r > blocks.frame[brightest].r)
            brightest = i;
    CHECK_NEAR(blocks.beams[0].head, 20.0 * 1000.0 * HOP_PERIOD_S, 1e-2);
    CHECK((size_t)lroundf(blocks.beams[0].head) == brightest);
    CHECK(dark(&blocks, brightest + 1, LEDS));

    while (blocks.beams[0].active && hops++ < 1000) {
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    CHECK(!blocks.beams[0].active);

    blocks_deinit(&blocks);
}

// Beams launched off the strip, further out than their tail (a volley
// queued behind the ends), come onto it rather than ending at once
static void test_beam_onto_strip(void) {
    blocks_t blocks;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_beam(&blocks, -60.f, 200.f, 20.f, 1.f, WHITE);
    blocks_beam(&blocks, (float)(LEDS - 1) + 60.f, -200.f, 20.f, 1.f, WHITE);

    for (int hop = 0; (float)hop * HOP_PERIOD_S < 0.5f; hop++) {
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    CHECK(blocks.beams[0].active && blocks.beams[1].active);
    CHECK(!dark(&blocks, 0, 100) && !dark(&blocks, LEDS - 100, LEDS));

    blocks_deinit(&blocks);
}

// A full pool gives way to the beam that travelled furthest, not to the
// dimmest: a fresh dim beam stays
static void test_beam_pool(void) {
    blocks_t blocks;
    bool fresh_stays = false, first_gone = true;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++) {
        blocks_beam(&blocks, 0.f, 100.f, 10.f,
                    i + 1 < BLOCKS_MAX_BEAMS ? 1.f : 0.5f, WHITE);
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    blocks_beam(&blocks, 0.f, 100.f, 10.f, 1.f, WHITE);

    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++) {
        fresh_stays = fresh_stays || blocks.beams[i].level == 0.5f;
        first_gone = first_gone &&
                     blocks.beams[i].head <
                         (float)BLOCKS_MAX_BEAMS * 100.f * HOP_PERIOD_S - 0.1f;
    }
    CHECK(fresh_stays);
    CHECK(first_gone);

    blocks_deinit(&blocks);
}

// A beam that does not move is refused: it would hold its slot for ever
static void test_beam_standing_still(void) {
    blocks_t blocks;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_beam(&blocks, 10.f, 0.f, 5.f, 1.f, WHITE);
    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++)
        CHECK(!blocks.beams[i].active);

    blocks_deinit(&blocks);
}

// Heading into a bend a beam fades: 30 LEDs at 200 LEDs per second leave
// little of it by the end of the strip, while without bends it stays whole.
// Leaving a bend, from where the strip starts, it stays whole too
static void test_beam_bend(void) {
    for (size_t bend = 0; bend <= 30; bend += 30) {
        blocks_t blocks;

        CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, bend));
        blocks_beam(&blocks, 250.f, 200.f, 10.f, 1.f, WHITE);

        while (blocks.beams[0].head < (float)(LEDS - 1)) {
            blocks_clear(&blocks);
            blocks_draw(&blocks, WHITE, 0.06f);
        }
        if (bend == 0)
            CHECK(blocks.beams[0].level == 1.f);
        else
            CHECK(blocks.beams[0].level < 0.2f);

        blocks_reset(&blocks);
        blocks_beam(&blocks, 0.f, 200.f, 10.f, 1.f, WHITE);
        for (int hop = 0; hop < 50; hop++) {
            blocks_clear(&blocks);
            blocks_draw(&blocks, WHITE, 0.06f);
        }
        CHECK(blocks.beams[0].level == 1.f);

        blocks_deinit(&blocks);
    }
}

// Sparks light at a level, fade to exactly 0, and reset clears everything
static void test_sparks_and_reset(void) {
    blocks_t blocks;

    CHECK(blocks_init(&blocks, LEDS, HOP_PERIOD_S, 0));
    blocks_spark(&blocks, 7, 0.8f);
    blocks_spark(&blocks, 7, 0.3f);
    blocks_spark(&blocks, LEDS, 1.f);
    blocks_clear(&blocks);
    blocks_draw(&blocks, (rgb_t){1.f, 0.f, 0.f}, 0.06f);
    CHECK(blocks.frame[7].r == 0.8f && blocks.frame[7].g == 0.f);

    for (int hop = 0; hop < 100; hop++) {
        blocks_clear(&blocks);
        blocks_draw(&blocks, WHITE, 0.06f);
    }
    CHECK(blocks.sparks[7] == 0.f);

    blocks_spark(&blocks, 3, 1.f);
    blocks_burst(&blocks, 10.f, 5.f, 1.f, 1.f, 1.f, WHITE);
    blocks_beam(&blocks, 10.f, 1.f, 5.f, 1.f, WHITE);
    blocks_reset(&blocks);
    blocks_clear(&blocks);
    blocks_draw(&blocks, WHITE, 0.06f);
    CHECK(sum(&blocks) == 0.f);

    blocks_deinit(&blocks);
}

int main(void) {
    test_rejects_invalid();
    test_wash();
    test_burst();
    test_burst_pool();
    test_beam();
    test_beam_onto_strip();
    test_beam_pool();
    test_beam_standing_still();
    test_beam_bend();
    test_sparks_and_reset();

    return CHECK_REPORT();
}
