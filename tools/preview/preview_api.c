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
    // The look selected when the settings were last applied
    int selected;
    // The gain of the settings, and the trim of the input, as factors
    float gain;
    float trim;

    settings_t settings;
    spectrum_t spectrum;
    features_t features;
    show_t shows[SHOW_LOOK_COUNT];

    // Samples from the page, and the hop being collected
    float input[PREVIEW_INPUT_CAPACITY];
    float hop[PREVIEW_HOP_SIZE];
    size_t collected;

    int32_t frames[2 * PREVIEW_HOP_SIZE];
    uint32_t pixels[SHOW_LOOK_COUNT][PREVIEW_LED_COUNT];
    int hops;

    // The newest sound, a copy: the bands are the features' own
    float loudness;
    float centroid;
    int hits;
    int part;
    int drops;

    int16_t range[6];
} preview;

// Every setting to the stages, as visualizer_tune() does, each show on its
// own look. A look newly selected starts clean, as on the board
static void apply(void) {
    visualizer_tuning_t tuning = settings_tuning(&preview.settings);

    if (tuning.gain > 0.f && isfinite(tuning.gain))
        preview.gain = tuning.gain;

    features_set_gain(&preview.features, preview.gain);
    features_tune(&preview.features, &tuning.features);

    if ((int)tuning.show.look != preview.selected) {
        preview.selected = (int)tuning.show.look;
        show_restart(&preview.shows[preview.selected]);
    }

    for (int look = 0; look < SHOW_LOOK_COUNT; look++) {
        show_tuning_t show = tuning.show;

        show.look = (show_look_t)look;
        show_tune(&preview.shows[look], &show);
    }
}

static void release(void) {
    if (!preview.ready)
        return;

    for (int look = 0; look < SHOW_LOOK_COUNT; look++)
        show_deinit(&preview.shows[look]);
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

    for (; made < SHOW_LOOK_COUNT; made++)
        if (!show_init(&preview.shows[made], PREVIEW_LED_COUNT,
                       FEATURES_BAND_COUNT, hop_period_s, 0, 1))
            break;

    if (made < SHOW_LOOK_COUNT) {
        while (made-- > 0)
            show_deinit(&preview.shows[made]);
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
    int selected = settings_get(&preview.settings, SETTINGS_LOOK);

    for (size_t i = 0; i < PREVIEW_HOP_SIZE; i++) {
        int32_t word = sample_word(preview.hop[i] * preview.trim);

        preview.frames[2 * i] = word;
        preview.frames[2 * i + 1] = word;
    }

    spectrum_analyze(&preview.spectrum, preview.frames, preview.gain);
    sound = features_update(&preview.features,
                            spectrum_bins(&preview.spectrum));

    for (int look = 0; look < SHOW_LOOK_COUNT; look++)
        if (preview.gallery || look == selected)
            show_render(&preview.shows[look], sound, preview.pixels[look]);

    preview.loudness = sound->loudness;
    preview.centroid = sound->centroid;
    preview.hits += sound->hits[FEATURES_LOW].fired;
    preview.part = sound->part;
    preview.drops += sound->event == PARTS_DROP;
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

int preview_look_count(void) { return SHOW_LOOK_COUNT; }

const char *preview_look_name(int look) {
    return ui_names_look((show_look_t)look);
}

int preview_scene_count(void) { return SCENE_COUNT; }

const char *preview_scene_name(int scene) {
    return ui_names_scene((scene_t)scene);
}

const uint32_t *preview_pixels(int look) {
    return look >= 0 && look < SHOW_LOOK_COUNT ? preview.pixels[look]
                                               : nullptr;
}

float preview_loudness(void) { return preview.loudness; }
float preview_centroid(void) { return preview.centroid; }
int preview_hits(void) { return preview.hits; }
int preview_part(void) { return preview.part; }

const char *preview_part_name(int part) {
    return ui_names_part((parts_part_t)part);
}

int preview_drops(void) { return preview.drops; }
int preview_hops(void) { return preview.hops; }
const float *preview_bands(void) { return preview.features.levels; }
