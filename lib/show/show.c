#include "show.h"

#include "show_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static show_look_fn *const looks[SHOW_LOOK_COUNT] = {
    [SHOW_LOOK_PULSE] = show_look_pulse,
    [SHOW_LOOK_FLOW] = show_look_flow,
    [SHOW_LOOK_STAGE] = show_look_stage,
};

// Looks without state of their own have no reset
static show_reset_fn *const resets[SHOW_LOOK_COUNT] = {
    [SHOW_LOOK_FLOW] = show_reset_flow,
    [SHOW_LOOK_STAGE] = show_reset_stage,
};

static float keep_for(float hop_period_s, float time_constant_s) {
    return expf(-hop_period_s / time_constant_s);
}

bool show_init(show_t *this, size_t led_count, size_t band_count,
               float hop_period_s, size_t bend_count, uint32_t seed) {
    size_t half_led_count = (led_count + 1) / 2;
    rgb_t *history;

    if (led_count < 2 || band_count < 2 || !(hop_period_s > 0.f))
        return false;

    history = (rgb_t *)calloc(half_led_count, sizeof(rgb_t));
    if (history == nullptr)
        return false;

    *this = (show_t){
        .led_count = led_count,
        .half_led_count = half_led_count,
        .band_count = band_count,
        .hop_period_s = hop_period_s,
        .tuning = show_default_tuning(),
        .random = seed != 0 ? seed : 1,
        .gap = 1.f,
        .gap_keep = keep_for(hop_period_s, SHOW_GAP_FADE_MS / 1000.f),
        .flow.history = history,
        .flow.boost_keep = keep_for(hop_period_s, SHOW_FLOW_BOOST_S),
        .stage.flash_keep = keep_for(hop_period_s, SHOW_STAGE_DROP_FADE_S),
    };

    if (!blocks_init(&this->blocks, led_count, hop_period_s, bend_count)) {
        free(history);
        return false;
    }

    rules_init(&this->rules, hop_period_s);

    return true;
}

show_tuning_t show_default_tuning(void) {
    return (show_tuning_t){
        .look = SHOW_LOOK,
        .scene = SHOW_SCENE,
        .brightness = 1.f,
    };
}

// The look starts clean: no blocks, no state of its own
static void restart(show_t *this) {
    blocks_reset(&this->blocks);
    if (resets[this->tuning.look] != nullptr)
        resets[this->tuning.look](this);
}

void show_tune(show_t *this, const show_tuning_t *tuning) {
    if (tuning->look < SHOW_LOOK_COUNT && tuning->look != this->tuning.look) {
        this->tuning.look = tuning->look;
        restart(this);
    }
    if (tuning->scene < SCENE_COUNT)
        this->tuning.scene = tuning->scene;
    if (tuning->brightness >= 0.f && tuning->brightness <= 1.f)
        this->tuning.brightness = tuning->brightness;
}

rgb_t show_color(const show_t *this, scene_role_t role) {
    if (this->swapped && role == SCENE_FIELD)
        role = SCENE_ACCENT;
    else if (this->swapped && role == SCENE_ACCENT)
        role = SCENE_FIELD;

    return scene_color(this->tuning.scene, role);
}

uint32_t show_random(show_t *this) {
    uint32_t x = this->random;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return this->random = x;
}

float show_random_unit(show_t *this) {
    return (float)(show_random(this) >> 8) / 16777216.f;
}

void show_render(show_t *this, const sound_t *sound, uint32_t *pixels) {
    rgb_t *frame = this->blocks.frame;

    if (sound->event != PARTS_NONE)
        this->swapped = !this->swapped;

    // A gap fades out; any other part shows at once. Faded to black, the
    // look starts clean, so nothing the gap hid comes back after it
    this->gap = sound->part == PARTS_GAP ? this->gap * this->gap_keep : 1.f;
    if (this->gap < BLOCKS_DARK && this->gap > 0.f) {
        this->gap = 0.f;
        restart(this);
    }

    blocks_clear(&this->blocks);
    looks[this->tuning.look](this, sound);

    if (this->gap < 1.f)
        for (size_t i = 0; i < this->led_count; i++)
            frame[i] = color_rgb_scale(frame[i], this->gap);

    rules_apply(&this->rules, frame, this->led_count);

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = color_rgb_scale(frame[i], this->tuning.brightness);

        pixels[i] = color_ws2812_from_rgb(color_gamma(color.r),
                                          color_gamma(color.g),
                                          color_gamma(color.b))
                        .value;
    }
}

void show_deinit(show_t *this) {
    blocks_deinit(&this->blocks);
    free(this->flow.history);
}
