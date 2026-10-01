#ifndef RULES_H
#define RULES_H

/**
 * The rules every frame obeys whatever the look drew, in this order:
 *
 *   1. Hue-safe mixing: blocks add up; an LED over full is scaled down as a
 *      whole, so it keeps its colour instead of bleaching towards white.
 *   2. Flash guard: at most RULES_FLASHES_PER_S flashes of the whole strip
 *      in any second (WCAG 2.3.1, three flashes). WCAG's flash is a pair of
 *      opposite changes of light of RULES_FLASH_RISE or more: a flash starts
 *      when the light climbs more than RULES_FLASH_RISE above its lowest
 *      since the last flash ended, and ends when the light falls that much
 *      from its peak. A further start within the second is held to half the
 *      rise. A slow brightening is one rise, at most one flash, never held
 *      back unless three came just before. Not a setting: it cannot be
 *      turned off.
 *
 * Light is what the LEDs give: each channel through the gamma curve
 * (color_gamma(), what the strip is sent), 0..1 of full, averaged over r, g,
 * b and all LEDs, before the Brightness setting. WCAG measures flashes in
 * relative luminance, which is light too, not the gamma-coded value
 */

#include "color.h"

#include <stddef.h>
#include <stdint.h>

constexpr size_t RULES_FLASHES_PER_S = 3;
// WCAG's flash: changes of a tenth of the most light
constexpr float RULES_FLASH_RISE = 0.1f;

typedef struct {
    // Hops per second, and the hop count now
    uint32_t hops_per_s;
    uint32_t hop;
    // The hops of the last flashes, oldest overwritten first
    uint32_t flashes[RULES_FLASHES_PER_S];
    size_t next;
    // The lowest light since the last flash ended, and while a flash is on
    // (armed false) its peak
    float low;
    float peak;
    bool armed;
} rules_t;

void rules_init(rules_t *this, float hop_period_s);

/**
 * Applies the rules to led_count LEDs, in place. Returns the strip's light
 * after them, 0..1
 */
float rules_apply(rules_t *this, rgb_t *frame, size_t led_count);

#endif
