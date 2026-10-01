#include "blocks.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool blocks_init(blocks_t *this, size_t led_count, float hop_period_s,
                 size_t bend_count) {
    rgb_t *frame;
    float *sparks;

    if (led_count < 2 || !(hop_period_s > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    sparks = (float *)calloc(led_count, sizeof(float));

    if (frame == nullptr || sparks == nullptr) {
        free(frame);
        free(sparks);
        return false;
    }

    *this = (blocks_t){
        .led_count = led_count,
        .hop_period_s = hop_period_s,
        .bend_count = bend_count,
        .bend_keep = expf(-hop_period_s / BLOCKS_BEND_FADE_S),
        .frame = frame,
        .sparks = sparks,
    };

    return true;
}

void blocks_clear(blocks_t *this) {
    memset(this->frame, 0, this->led_count * sizeof(rgb_t));
}

void blocks_reset(blocks_t *this) {
    memset(this->sparks, 0, this->led_count * sizeof(float));
    memset(this->bursts, 0, sizeof(this->bursts));
    memset(this->beams, 0, sizeof(this->beams));
}

void blocks_wash(blocks_t *this, size_t from, size_t to, rgb_t color) {
    if (to > this->led_count)
        to = this->led_count;

    for (size_t i = from; i < to; i++)
        color_rgb_add(&this->frame[i], color);
}

void blocks_burst(blocks_t *this, float centre, float max_radius,
                  float growth, float level, float fade_s, rgb_t color) {
    blocks_burst_t *slot = &this->bursts[0];

    // A free slot, or the dimmest
    for (size_t i = 0; i < BLOCKS_MAX_BURSTS && slot->active; i++)
        if (!this->bursts[i].active || this->bursts[i].level < slot->level)
            slot = &this->bursts[i];

    *slot = (blocks_burst_t){
        .centre = centre,
        .max_radius = max_radius,
        .growth = growth,
        .level = level,
        .keep = fade_s > 0.f ? expf(-this->hop_period_s / fade_s) : 0.f,
        .color = color,
        .active = true,
    };
}

void blocks_spark(blocks_t *this, size_t index, float level) {
    if (index < this->led_count && level > this->sparks[index])
        this->sparks[index] = level;
}

void blocks_beam(blocks_t *this, float head, float velocity, float tail,
                 float level, rgb_t color) {
    blocks_beam_t *slot = &this->beams[0];

    // Standing still it would never leave the strip nor give way
    if (!(fabsf(velocity) > 0.f))
        return;

    // A free slot, or the one that travelled furthest
    for (size_t i = 0; i < BLOCKS_MAX_BEAMS && slot->active; i++)
        if (!this->beams[i].active ||
            this->beams[i].travelled > slot->travelled)
            slot = &this->beams[i];

    *slot = (blocks_beam_t){
        .head = head,
        .velocity = velocity,
        .tail = tail > 1.f ? tail : 1.f,
        .level = level,
        .color = color,
        .active = true,
    };
}

// Adds color at an LED if it is on the strip
static void put(blocks_t *this, long index, rgb_t color) {
    if (index >= 0 && (size_t)index < this->led_count)
        color_rgb_add(&this->frame[index], color);
}

static void draw_burst(blocks_t *this, blocks_burst_t *burst) {
    float reach;

    burst->radius += burst->growth * this->hop_period_s;
    if (burst->radius > burst->max_radius)
        burst->radius = burst->max_radius;

    // Full inside, one LED of soft edge
    reach = burst->radius + 0.5f;
    for (long i = (long)ceilf(burst->centre - reach);
         i <= (long)floorf(burst->centre + reach); i++) {
        float shape = reach - fabsf((float)i - burst->centre);

        if (shape > 0.f)
            put(this, i,
                color_rgb_scale(burst->color,
                                burst->level * (shape < 1.f ? shape : 1.f)));
    }

    burst->level *= burst->keep;
    if (burst->level < BLOCKS_DARK)
        burst->active = false;
}

static void draw_beam(blocks_t *this, blocks_beam_t *beam) {
    float behind = beam->velocity >= 0.f ? -1.f : 1.f;
    float last = (float)(this->led_count - 1);

    beam->head += beam->velocity * this->hop_period_s;
    beam->travelled += fabsf(beam->velocity) * this->hop_period_s;

    // Past the end it heads for by its whole tail: gone
    if ((beam->velocity < 0.f && beam->head < -beam->tail) ||
        (beam->velocity > 0.f && beam->head > last + beam->tail)) {
        beam->active = false;
        return;
    }

    // Heading into a bend, towards the end of the strip: it turns the
    // corner and fades. Leaving one, as a beam launched from an end does,
    // it stays whole
    if (this->bend_count > 0 &&
        ((beam->velocity < 0.f && beam->head < (float)this->bend_count) ||
         (beam->velocity > 0.f &&
          beam->head > last - (float)this->bend_count)))
        beam->level *= this->bend_keep;

    // The head, then the tail fading as the square of the distance
    for (long d = 0; (float)d < beam->tail; d++) {
        float fade = 1.f - (float)d / beam->tail;

        put(this, lroundf(beam->head + behind * (float)d),
            color_rgb_scale(beam->color, beam->level * fade * fade));
    }

    if (beam->level < BLOCKS_DARK)
        beam->active = false;
}

void blocks_draw(blocks_t *this, rgb_t spark_color, float spark_fade_s) {
    float keep = spark_fade_s > 0.f ? expf(-this->hop_period_s / spark_fade_s)
                                    : 0.f;

    for (size_t i = 0; i < BLOCKS_MAX_BURSTS; i++)
        if (this->bursts[i].active)
            draw_burst(this, &this->bursts[i]);

    for (size_t i = 0; i < BLOCKS_MAX_BEAMS; i++)
        if (this->beams[i].active)
            draw_beam(this, &this->beams[i]);

    for (size_t i = 0; i < this->led_count; i++) {
        if (this->sparks[i] == 0.f)
            continue;

        color_rgb_add(&this->frame[i],
                      color_rgb_scale(spark_color, this->sparks[i]));
        this->sparks[i] *= keep;
        if (this->sparks[i] < BLOCKS_DARK)
            this->sparks[i] = 0.f;
    }
}

void blocks_deinit(blocks_t *this) {
    free(this->frame);
    free(this->sparks);
}
