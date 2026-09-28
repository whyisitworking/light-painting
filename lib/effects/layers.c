#include "effects_internal.h"

#include <math.h>
#include <string.h>

/*
 * Layers on top of the mode's frame, in this order: Chase slides the image,
 * Symmetry folds it, Diffuse blurs it, Trails lets it linger. Trails is
 * last so that what lingers is the finished image, and a moving image
 * leaves a glowing smear.
 */

// Remembered colours below this are dark on the strip (it is 1e-3 ^ 2.2 of
// full scale after gamma) and go to 0, so a fade ends at black instead of
// wandering through ever smaller floats
constexpr float TRAILS_FLOOR = 1e-3f;

static float capped(float value) { return value > 1.f ? 1.f : value; }

static float faded(float value, float k) {
    value *= k;

    return value < TRAILS_FLOOR ? 0.f : value;
}

// Each colour becomes the brighter of the new one and the last one faded.
// The maximum, not the sum, so it cannot pile up past what was drawn
static void trails(effects_t *this) {
    float k = this->layers.trails_k;

    if (this->tuning.trails_ms <= 0.f) {
        // Switched off: forget, so that switching on shows no old frame
        if (this->layers.trails_active) {
            memset(this->layers.previous, 0,
                   this->led_count * sizeof(rgb_t));
            this->layers.trails_active = false;
        }

        return;
    }

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t *frame = &this->frame[i], *previous = &this->layers.previous[i];

        frame->r = fmaxf(frame->r, faded(previous->r, k));
        frame->g = fmaxf(frame->g, faded(previous->g, k));
        frame->b = fmaxf(frame->b, faded(previous->b, k));

        *previous = (rgb_t){capped(frame->r), capped(frame->g),
                            capped(frame->b)};
    }

    this->layers.trails_active = true;
}

static void chase([[maybe_unused]] effects_t *this) {}

// The strip becomes n segments, each the whole drawn frame squeezed into
// it, every odd one reversed. An output LED is the mean of the source LEDs
// it covers, so a narrow feature dims instead of vanishing
static void symmetry(effects_t *this) {
    size_t count = this->led_count, segments = this->tuning.symmetry;
    rgb_t *out = this->layers.scratch;

    // Too short to fold, or nothing to do
    if (segments < 2 || count < 2 * segments)
        return;

    for (size_t j = 0; j < segments; j++) {
        size_t start = j * count / segments;
        size_t length = (j + 1) * count / segments - start;

        for (size_t p = 0; p < length; p++) {
            size_t from = p * count / length, to = (p + 1) * count / length;
            float weight = 1.f / (float)(to - from);
            rgb_t sum = {0.f, 0.f, 0.f};

            for (size_t s = from; s < to; s++)
                color_rgb_add(&sum, this->frame[s]);

            out[start + (j % 2 == 0 ? p : length - 1 - p)] =
                color_rgb_scale(sum, weight);
        }
    }

    memcpy(this->frame, out, count * sizeof(rgb_t));
}

// A blur over three LEDs, weights a, 1 - 2a, a with a = amount / 3, so the
// full amount is a plain average. The ends count as their own missing
// neighbour: nothing leaks out of the strip
static void diffuse(effects_t *this) {
    size_t count = this->led_count;
    float a = this->tuning.diffuse / 3.f, self = 1.f - 2.f * a;
    const rgb_t *in = this->frame;
    rgb_t *out = this->layers.scratch;

    if (this->tuning.diffuse <= 0.f)
        return;

    for (size_t i = 0; i < count; i++) {
        rgb_t left = in[i > 0 ? i - 1 : 0];
        rgb_t right = in[i + 1 < count ? i + 1 : count - 1];

        out[i] = (rgb_t){
            self * in[i].r + a * (left.r + right.r),
            self * in[i].g + a * (left.g + right.g),
            self * in[i].b + a * (left.b + right.b),
        };
    }

    memcpy(this->frame, out, count * sizeof(rgb_t));
}

void effects_layers_apply(effects_t *this) {
    chase(this);
    symmetry(this);
    diffuse(this);
    trails(this);
}
