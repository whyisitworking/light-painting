#include "effects.h"

#include "color.h"
#include "effects_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// The renderer of each mode, see effects_internal.h
static effects_renderer_t *const renderers[EFFECTS_MODE_COUNT] = {
    [EFFECTS_MODE_SPECTRUM] = effects_mode_spectrum,
    [EFFECTS_MODE_SPECTRUM_MIRRORED] = effects_mode_spectrum_mirrored,
    [EFFECTS_MODE_RIVER] = effects_mode_river,
    [EFFECTS_MODE_RIPPLES] = effects_mode_ripples,
    [EFFECTS_MODE_VU] = effects_mode_vu,
    [EFFECTS_MODE_GLOW] = effects_mode_glow,
    [EFFECTS_MODE_POND] = effects_mode_pond,
    [EFFECTS_MODE_CYMATICS] = effects_mode_cymatics,
    [EFFECTS_MODE_FIRE] = effects_mode_fire,
    [EFFECTS_MODE_STORM] = effects_mode_storm,
    [EFFECTS_MODE_PINGPONG] = effects_mode_pingpong,
    [EFFECTS_MODE_SWARM] = effects_mode_swarm,
    [EFFECTS_MODE_PLASMA] = effects_mode_plasma,
    [EFFECTS_MODE_AURORA] = effects_mode_aurora,
    [EFFECTS_MODE_BLOOM] = effects_mode_bloom,
};

// Modes without a reset have no state to clear
static effects_reset_t *const resets[EFFECTS_MODE_COUNT] = {
    [EFFECTS_MODE_POND] = effects_reset_pond,
    [EFFECTS_MODE_FIRE] = effects_reset_fire,
    [EFFECTS_MODE_STORM] = effects_reset_storm,
    [EFFECTS_MODE_PINGPONG] = effects_reset_pingpong,
    [EFFECTS_MODE_SWARM] = effects_reset_swarm,
    [EFFECTS_MODE_BLOOM] = effects_reset_bloom,
};

// Floats the simulations' rows take, see slice_pool()
static size_t pool_floats(size_t led_count, size_t half_led_count,
                          size_t band_count) {
    // Pond: height, previous; Fire: heat; Storm: afterglow;
    // Bloom: u, v and the rows a step writes; Punch: two rows of bands
    return 7 * led_count + half_led_count + 2 * band_count;
}

static float *take(float **cursor, size_t count) {
    float *slice = *cursor;

    *cursor += count;

    return slice;
}

static void slice_pool(effects_t *this, float *pool) {
    float *cursor = pool;

    this->pool = pool;
    this->pond.height = take(&cursor, this->led_count);
    this->pond.previous = take(&cursor, this->led_count);
    this->fire.heat = take(&cursor, this->half_led_count);
    this->storm.afterglow = take(&cursor, this->led_count);
    this->bloom.u = take(&cursor, this->led_count);
    this->bloom.v = take(&cursor, this->led_count);
    this->bloom.next_u = take(&cursor, this->led_count);
    this->bloom.next_v = take(&cursor, this->led_count);
    this->punch.band_avg = take(&cursor, this->band_count);
    this->punch.bands = take(&cursor, this->band_count);
}

bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_period_s, uint32_t seed) {
    size_t half_led_count = (led_count + 1) / 2;
    rgb_t *frame, *river, *previous, *scratch;
    float *sparkles, *pool;

    if (led_count < 2 || band_count < 2 || !(hop_period_s > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    river = (rgb_t *)calloc(half_led_count, sizeof(rgb_t));
    sparkles = (float *)calloc(led_count, sizeof(float));
    previous = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    scratch = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    pool = (float *)calloc(pool_floats(led_count, half_led_count, band_count),
                  sizeof(float));

    if (frame == nullptr || river == nullptr || sparkles == nullptr ||
        previous == nullptr || scratch == nullptr || pool == nullptr) {
        free(frame);
        free(river);
        free(sparkles);
        free(previous);
        free(scratch);
        free(pool);
        return false;
    }

    effects_sin_init();

    *this = (effects_t){
        .tuning = effects_default_tuning(),
        .led_count = led_count,
        .band_count = band_count,
        .half_led_count = half_led_count,
        .hop_period_s = hop_period_s,
        .flash_k = expf(-hop_period_s / (EFFECTS_FLASH_MS / 1000.f)),
        .punch.average_k = expf(-hop_period_s / EFFECTS_PUNCH_AVERAGE_S),
        .frame = frame,
        .river.history = river,
        .sparkles.levels = sparkles,
        .random = seed != 0 ? seed : 1,
        .layers.previous = previous,
        .layers.scratch = scratch,
        .cymatics.nodes = 1.f,
    };
    slice_pool(this, pool);
    // One sub-step of a wave, in seconds, sets how much each one keeps
    this->pond.velocity_k =
        expf(-hop_period_s / (float)EFFECTS_POND_SUBSTEPS / EFFECTS_POND_DAMPING_S);
    this->pond.leak_k =
        expf(-hop_period_s / (float)EFFECTS_POND_SUBSTEPS / EFFECTS_POND_LEAK_S);
    this->fire.cool_k = expf(-hop_period_s / EFFECTS_FIRE_COOL_S);
    this->storm.glow_k = expf(-hop_period_s / EFFECTS_STORM_GLOW_S);
    this->storm.sky_k = expf(-hop_period_s / EFFECTS_STORM_SKY_S);

    for (size_t mode = 0; mode < EFFECTS_MODE_COUNT; mode++)
        if (resets[mode] != nullptr)
            resets[mode](this);
    effects_tune(this, &this->tuning);

    return true;
}

effects_tuning_t effects_default_tuning(void) {
    return (effects_tuning_t){
        .mode = EFFECTS_MODE,
        .palette = EFFECTS_PALETTE,
        .brightness = 1.f,
        .river_speed = EFFECTS_RIVER_SPEED,
        .ripple_speed = EFFECTS_RIPPLE_SPEED,
        .peak_hold_ms = EFFECTS_PEAK_HOLD_MS,
        .drift_period_s = EFFECTS_DRIFT_PERIOD_S,
        .warmth = EFFECTS_WARMTH,
        .flash_level = EFFECTS_FLASH_LEVEL,
        .sparkle_rate = EFFECTS_SPARKLE_RATE,
        .trails_ms = EFFECTS_TRAILS_MS,
        .diffuse = EFFECTS_DIFFUSE,
        .symmetry = EFFECTS_SYMMETRY,
        .chase_leds_per_s = EFFECTS_CHASE_LEDS_PER_S,
    };
}

static bool is_at_least(float value, float least) {
    return value >= least && isfinite(value);
}

static bool is_fraction(float value) { return value >= 0.f && value <= 1.f; }

void effects_tune(effects_t *this, const effects_tuning_t *tuning) {
    if (tuning->mode < EFFECTS_MODE_COUNT) {
        if (tuning->mode != this->tuning.mode && resets[tuning->mode] != nullptr)
            resets[tuning->mode](this);
        this->tuning.mode = tuning->mode;
    }
    if (tuning->palette < PALETTE_COUNT)
        this->tuning.palette = tuning->palette;
    if (is_fraction(tuning->brightness))
        this->tuning.brightness = tuning->brightness;
    if (tuning->river_speed >= 1)
        this->tuning.river_speed = tuning->river_speed;
    if (tuning->ripple_speed > 0.f && isfinite(tuning->ripple_speed))
        this->tuning.ripple_speed = tuning->ripple_speed;
    if (is_at_least(tuning->peak_hold_ms, 0.f))
        this->tuning.peak_hold_ms = tuning->peak_hold_ms;
    if (is_at_least(tuning->drift_period_s, 0.f))
        this->tuning.drift_period_s = tuning->drift_period_s;
    if (is_at_least(tuning->warmth, 0.f))
        this->tuning.warmth = tuning->warmth;
    if (is_fraction(tuning->flash_level))
        this->tuning.flash_level = tuning->flash_level;
    if (is_fraction(tuning->sparkle_rate))
        this->tuning.sparkle_rate = tuning->sparkle_rate;
    if (is_at_least(tuning->trails_ms, 0.f))
        this->tuning.trails_ms = tuning->trails_ms;
    if (is_fraction(tuning->diffuse))
        this->tuning.diffuse = tuning->diffuse;
    if (tuning->symmetry >= 1 && tuning->symmetry <= EFFECTS_SYMMETRY_MAX)
        this->tuning.symmetry = tuning->symmetry;
    if (isfinite(tuning->chase_leds_per_s))
        this->tuning.chase_leds_per_s = tuning->chase_leds_per_s;

    this->peak_hold_s = this->tuning.peak_hold_ms / 1000.f;
    // What a frame keeps of the one before, per hop: 0 with the trails off
    this->layers.trails_k =
        this->tuning.trails_ms > 0.f
            ? expf(-this->hop_period_s / (this->tuning.trails_ms / 1000.f))
            : 0.f;
}

// At most 1. Layers add up past it (a sparkle on a lit LED), and the
// brightness is to scale what is shown. NaN stays NaN, which shows dark
static float cap(float value) { return value > 1.f ? 1.f : value; }

// level relative to its slow average, expanded: see EFFECTS_PUNCH_GAIN.
// Silence is exactly 0 whatever the average was
static float punch_level(float level, float *average, float average_k) {
    float punched = level + EFFECTS_PUNCH_GAIN * (level - *average);

    // Updated after use: the first frame above the average shows the full
    // step
    *average += (level - *average) * (1.f - average_k);

    return punched < 0.f ? 0.f : punched > 1.f ? 1.f : punched;
}

// Fills this->punch.bands and returns the punched sound. beat, strength and
// centroid pass through
static sound_t punch_sound(effects_t *this, const sound_t *sound) {
    sound_t punched = *sound;

    // The first render has no history: a single frame shows as it is
    if (!this->punch.primed) {
        memcpy(this->punch.band_avg, sound->bands,
               this->band_count * sizeof(float));
        this->punch.loudness_avg = sound->loudness;
        this->punch.primed = true;
    }

    for (size_t b = 0; b < this->band_count; b++)
        this->punch.bands[b] = punch_level(
            sound->bands[b], &this->punch.band_avg[b], this->punch.average_k);
    punched.bands = this->punch.bands;
    punched.loudness = punch_level(
        sound->loudness, &this->punch.loudness_avg, this->punch.average_k);

    return punched;
}

void effects_render(effects_t *this, const sound_t *sound, uint32_t *pixels) {
    sound_t punched = punch_sound(this, sound);
    // A beat lifts what is lit and dark stays dark, unlike adding white
    float lift;

    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    // A mode without a renderer stays dark
    if (renderers[this->tuning.mode] != nullptr)
        renderers[this->tuning.mode](this, &punched);

    effects_layers_apply(this);

    if (sound->beat)
        this->flash =
            fmaxf(this->flash, sound->beat_strength * this->tuning.flash_level);

    lift = 1.f + EFFECTS_FLASH_GAIN * this->flash;

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = color_rgb_scale(
            (rgb_t){cap(this->frame[i].r * lift), cap(this->frame[i].g * lift),
                    cap(this->frame[i].b * lift)},
            this->tuning.brightness);

        pixels[i] = color_ws2812_from_rgb(color_gamma(color.r),
                                          color_gamma(color.g),
                                          color_gamma(color.b))
                        .value;
    }

    this->flash *= this->flash_k;

    // Wrapped where the drift repeats (two periods, for reflecting palettes
    // too): a float growing forever loses precision and freezes after ~36 h
    if (this->tuning.drift_period_s > 0.f)
        this->time_s = fmodf(this->time_s + this->hop_period_s,
                             2.f * this->tuning.drift_period_s);
    else
        this->time_s = 0.f;
}

void effects_deinit(effects_t *this) {
    free(this->frame);
    free(this->river.history);
    free(this->sparkles.levels);
    free(this->layers.previous);
    free(this->layers.scratch);
    free(this->pool);
}
