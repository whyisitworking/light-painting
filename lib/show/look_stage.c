#include "show_internal.h"

void show_reset_stage(show_t *this) { this->stage.flash = 0.f; }

/**
 * Stage: a mirrored equaliser, bass at the centre, in the accent colour, its
 * contrast raised by squaring the levels; the outer part of each half and
 * the bends are the wings, a field wash that breathes with the groove. Half
 * as bright in calm; squeezed towards the centre as a build goes on; high
 * hits spark on the strongest bars; a drop flashes the bars in the hit
 * colour
 */
void show_look_stage(show_t *this, const sound_t *sound) {
    size_t half = this->half_led_count;
    float squeeze = sound->part == PARTS_BUILD
                        ? 1.f - SHOW_STAGE_SQUEEZE * sound->build_progress
                        : 1.f;
    float span = squeeze * (1.f - SHOW_STAGE_WING) * (float)half;
    float scale = sound->part == PARTS_CALM ? SHOW_STAGE_CALM_LEVEL : 1.f;
    float wing = (SHOW_STAGE_WING_WASH + SHOW_STAGE_WING_GROOVE * sound->groove) *
                 scale * show_presence(sound);
    bool spark = sound->hits[FEATURES_HIGH].fired;
    rgb_t bar, field = show_color(this, SCENE_FIELD);

    if (sound->event == PARTS_DROP)
        this->stage.flash = 1.f;
    bar = show_mix(show_color(this, SCENE_ACCENT), show_color(this, SCENE_HIT),
                   this->stage.flash);
    this->stage.flash *= this->stage.flash_keep;

    for (size_t d = 0; d < half; d++) {
        float level;

        if ((float)d >= span) {
            show_put_mirrored(this, d, color_rgb_scale(field, wing));
            continue;
        }

        level = show_band_at(this, sound,
                             (float)d / span * (float)(this->band_count - 1));
        level *= level * scale;
        show_put_mirrored(this, d, color_rgb_scale(bar, level));

        if (spark && level > SHOW_STAGE_SPARK_LEVEL &&
            show_random_unit(this) < SHOW_STAGE_SPARK_SHARE)
            blocks_spark(&this->blocks,
                         show_mirror_index(this, d, show_random(this) & 1),
                         1.f);
    }

    blocks_draw(&this->blocks, show_color(this, SCENE_HIT), SHOW_SPARK_FADE_S);
}
