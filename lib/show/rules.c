#include "rules.h"

#include <math.h>

void rules_init(rules_t *this, float hop_period_s) {
    uint32_t hops_per_s = (uint32_t)lroundf(1.f / hop_period_s);

    *this = (rules_t){
        .hops_per_s = hops_per_s,
        // Every remembered flash more than a second ago
        .hop = hops_per_s,
        .armed = true,
    };
}

// The strip's light, see rules.h
static float light(const rgb_t *frame, size_t led_count) {
    uint32_t sum = 0;

    for (size_t i = 0; i < led_count; i++)
        sum += (uint32_t)color_gamma(frame[i].r) + color_gamma(frame[i].g) +
               color_gamma(frame[i].b);

    return (float)sum / (3.f * 255.f * (float)led_count);
}

// Flashes in the last second
static size_t recent_flashes(const rules_t *this) {
    size_t count = 0;

    for (size_t i = 0; i < RULES_FLASHES_PER_S; i++)
        count += this->hop - this->flashes[i] < this->hops_per_s;

    return count;
}

float rules_apply(rules_t *this, rgb_t *frame, size_t led_count) {
    float now;

    // 1. Hue-safe mixing
    for (size_t i = 0; i < led_count; i++) {
        rgb_t *pixel = &frame[i];
        float most = fmaxf(pixel->r, fmaxf(pixel->g, pixel->b));

        if (most > 1.f)
            *pixel = color_rgb_scale(*pixel, 1.f / most);
    }
    now = light(frame, led_count);

    // 2. The flash guard
    if (!this->armed) {
        // The flash is on until the light falls back from its peak
        this->peak = fmaxf(this->peak, now);
        if (now < this->peak - RULES_FLASH_RISE) {
            this->armed = true;
            this->low = now;
        }
    } else if (now - this->low <= RULES_FLASH_RISE) {
        this->low = fminf(this->low, now);
    } else if (recent_flashes(this) < RULES_FLASHES_PER_S) {
        this->flashes[this->next] = this->hop;
        this->next = (this->next + 1) % RULES_FLASHES_PER_S;
        this->armed = false;
        this->peak = now;
    } else {
        // Held: light goes as the value to the gamma, so scaled by k, the
        // light by about k ^ COLOR_GAMMA
        float held = this->low + 0.5f * RULES_FLASH_RISE;
        float k = powf(held / now, 1.f / COLOR_GAMMA);

        for (size_t i = 0; i < led_count; i++)
            frame[i] = color_rgb_scale(frame[i], k);
        now = light(frame, led_count);
    }
    this->hop++;

    return now;
}
