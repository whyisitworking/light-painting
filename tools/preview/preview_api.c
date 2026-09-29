#include "preview_api.h"

#include "features.h"
#include "spectrum.h"
#include "ui_names.h"
#include "visualizer.h"

#include <math.h>
#include <string.h>

static struct {
    bool ready;
    bool gallery;
    // The gain of the settings, and the trim of the input, as factors
    float gain;
    float trim;

    settings_t settings;
    spectrum_t spectrum;
    features_t features;
    effects_t effects[EFFECTS_MODE_COUNT];

    // Samples from the page, and the hop being collected
    float input[PREVIEW_INPUT_CAPACITY];
    float hop[PREVIEW_HOP_SIZE];
    size_t collected;

    int32_t frames[2 * PREVIEW_HOP_SIZE];
    uint32_t pixels[EFFECTS_MODE_COUNT][PREVIEW_LED_COUNT];
    int hops;

    // The newest sound, a copy: the bands are the features' own
    float loudness;
    float centroid;
    int beats;

    int16_t range[6];
} preview;

// Every setting to the stages, as visualizer_tune() does, each effects
// instance on its own mode
static void apply(void) {
    visualizer_tuning_t tuning = settings_tuning(&preview.settings);

    if (tuning.gain > 0.f && isfinite(tuning.gain))
        preview.gain = tuning.gain;

    features_tune(&preview.features, &tuning.features);

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++) {
        effects_tuning_t effects = tuning.effects;

        effects.mode = (effects_mode_t)mode;
        effects_tune(&preview.effects[mode], &effects);
    }
}

static void release(void) {
    if (!preview.ready)
        return;

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++)
        effects_deinit(&preview.effects[mode]);
    features_deinit(&preview.features);
    spectrum_deinit(&preview.spectrum);
    preview.ready = false;
}

int preview_init(float sample_rate) {
    float hop_period_s;
    int made = 0;

    release();
    memset(&preview, 0, sizeof(preview));

    if (!(sample_rate > 0.f) ||
        !spectrum_init(&preview.spectrum, PREVIEW_FFT_SIZE, PREVIEW_HOP_SIZE))
        return 0;

    hop_period_s = (float)PREVIEW_HOP_SIZE / sample_rate;

    if (!features_init(&preview.features,
                       spectrum_bin_count(&preview.spectrum),
                       sample_rate / (float)PREVIEW_FFT_SIZE, hop_period_s)) {
        spectrum_deinit(&preview.spectrum);
        return 0;
    }

    for (; made < EFFECTS_MODE_COUNT; made++)
        if (!effects_init(&preview.effects[made], PREVIEW_LED_COUNT,
                          FEATURES_BAND_COUNT, hop_period_s, 1))
            break;

    if (made < EFFECTS_MODE_COUNT) {
        while (made-- > 0)
            effects_deinit(&preview.effects[made]);
        features_deinit(&preview.features);
        spectrum_deinit(&preview.spectrum);
        return 0;
    }

    preview.ready = true;
    preview.trim = 1.f;
    settings_reset(&preview.settings);
    apply();

    return 1;
}

void preview_deinit(void) {
    release();
    memset(&preview, 0, sizeof(preview));
}

float *preview_input(void) { return preview.input; }

// The I2S word of a sample, as tests/signals.h makes it: 24 bits, shifted
// up by 7 (one delay bit and 7 unused)
static int32_t sample_word(float sample) {
    float scaled = sample * 8388607.f;

    if (scaled > 8388607.f)
        scaled = 8388607.f;
    else if (scaled < -8388608.f)
        scaled = -8388608.f;

    return (int32_t)((uint32_t)(int32_t)lroundf(scaled) << 7);
}

// One hop, as visualizer_analyze() and visualizer_render() do
static void render_hop(void) {
    const sound_t *sound;
    int selected = settings_get(&preview.settings, SETTINGS_MODE);

    for (size_t i = 0; i < PREVIEW_HOP_SIZE; i++) {
        int32_t word = sample_word(preview.hop[i] * preview.trim);

        preview.frames[2 * i] = word;
        preview.frames[2 * i + 1] = word;
    }

    spectrum_analyze(&preview.spectrum, preview.frames, preview.gain);
    sound = features_update(&preview.features,
                            spectrum_bins(&preview.spectrum));

    for (int mode = 0; mode < EFFECTS_MODE_COUNT; mode++)
        if (preview.gallery || mode == selected)
            effects_render(&preview.effects[mode], sound,
                           preview.pixels[mode]);

    preview.loudness = sound->loudness;
    preview.centroid = sound->centroid;
    preview.beats += sound->beat;
    preview.hops++;
}

int preview_push(int count) {
    int hops = 0;

    if (!preview.ready || count <= 0)
        return 0;
    if ((size_t)count > PREVIEW_INPUT_CAPACITY)
        count = (int)PREVIEW_INPUT_CAPACITY;

    for (int i = 0; i < count; i++) {
        preview.hop[preview.collected++] = preview.input[i];

        if (preview.collected == PREVIEW_HOP_SIZE) {
            render_hop();
            preview.collected = 0;
            hops++;
        }
    }

    return hops;
}

void preview_set_input_trim_db(float db) {
    preview.trim = isfinite(db) ? powf(10.f, db / 20.f) : 1.f;
}

void preview_set_gallery(int on) { preview.gallery = on != 0; }

int preview_setting_count(void) { return SETTINGS_ID_COUNT; }

const int16_t *preview_setting_range(int id) {
    const settings_range_t *range;

    if (id < 0)
        return nullptr;

    range = settings_range((settings_id_t)id);
    if (range == nullptr)
        return nullptr;

    preview.range[0] = range->min;
    preview.range[1] = range->max;
    preview.range[2] = range->step;
    preview.range[3] = range->initial;
    preview.range[4] = range->divisor;
    preview.range[5] = range->wraps;

    return preview.range;
}

int preview_get(int id) {
    return id < 0 ? 0 : settings_get(&preview.settings, (settings_id_t)id);
}

int preview_set(int id, int value) {
    if (id < 0)
        return 0;

    settings_set(&preview.settings, (settings_id_t)id, value);
    if (preview.ready)
        apply();

    return settings_get(&preview.settings, (settings_id_t)id);
}

void preview_reset(void) {
    settings_reset(&preview.settings);
    if (preview.ready)
        apply();
}

int preview_mode_setting_count(int mode) {
    size_t count = 0;

    if (mode < 0 || mode >= EFFECTS_MODE_COUNT)
        return 0;

    settings_mode_ids((effects_mode_t)mode, &count);

    return (int)count;
}

int preview_mode_setting_id(int mode, int index) {
    size_t count = 0;
    const settings_id_t *ids;

    if (mode < 0 || mode >= EFFECTS_MODE_COUNT || index < 0)
        return -1;

    ids = settings_mode_ids((effects_mode_t)mode, &count);

    return (size_t)index < count ? (int)ids[index] : -1;
}

int preview_mode_count(void) { return EFFECTS_MODE_COUNT; }

const char *preview_mode_name(int mode) {
    return ui_names_mode((effects_mode_t)mode);
}

int preview_palette_count(void) { return PALETTE_COUNT; }

const char *preview_palette_name(int palette) {
    return ui_names_palette((palette_t)palette);
}

const uint32_t *preview_pixels(int mode) {
    return mode >= 0 && mode < EFFECTS_MODE_COUNT ? preview.pixels[mode]
                                                   : nullptr;
}

float preview_loudness(void) { return preview.loudness; }
float preview_centroid(void) { return preview.centroid; }
int preview_beats(void) { return preview.beats; }
int preview_hops(void) { return preview.hops; }
const float *preview_bands(void) { return preview.features.levels; }
