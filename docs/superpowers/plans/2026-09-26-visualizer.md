# Visualizer Implementation Plan

> Historical record: paths and names are from before the lib/, platform/, app/ restructuring, see the README for the current layout.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `main.c`'s linear bin-to-hue mapping with a two-stage visualizer:
- `features/` turns FFT bins into a description of the sound.
- `effects/` turns that description into 300 pixels, with six modes and a shared colour layer.

**Architecture:** Two pure C17 modules with no Pico SDK dependency, unit-tested on the host with CTest:
- `features/` holds log bands, dB with auto-gain, attack/decay, loudness, centroid and beat detection.
- `effects/` holds the six modes, the palettes, drift, warmth, beat flash and gamma. It emits `color_neopixel_t` words.
- `main.c` calls `features_update` then `effects_render` once per audio hop.

**Tech Stack:** C17, Pico SDK 2.3.1 (firmware only), CMake + Ninja, CTest host tests with `tests/check.h`.

**Spec:** `docs/superpowers/specs/2026-09-26-visualizer-design.md`

## Global Constraints

- **Language:** C17. `features/` and `effects/` must not include any Pico SDK header, so they build on the host.
- **Warnings:** the firmware must build with the per-source `PROJECT_WARNINGS`: `-Wall -Wextra -Werror -Wno-unused-function -Wno-maybe-uninitialized -Wpointer-arith -Wcast-align`.
- **Allocation:** everything is allocated at init; the render and update paths never allocate.
- **Code style:** follow the existing code: 4-space indent, 80 columns, `this` as the first parameter, `bool` init returning `true` on success, short `//` comments explaining why.
- **Commits:** one per task, on branch `fix/review-bugs`, ending with the `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` trailer. Never commit on `main`.
- **Audio constants:** fs = 48828.125 Hz, `AUDIO_FFT_SIZE` 512, `AUDIO_FFT_HOP` 256. That gives bin width 95.367 Hz, 256 bins, and a hop of 5.243 ms.
- **Strip:** 300 LEDs on a 40 A supply, so there is no brightness or current cap.
- **Mode and palette selection:** compile-time constants `VISUALIZER_MODE` and `VISUALIZER_PALETTE` in `main.c`, stored in runtime state through `effects_set_mode` and `effects_set_palette`.

**Commands used throughout**
- Host tests: `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
- Firmware: `export PATH=$HOME/.pico-sdk/cmake/v4.3.4/bin:$HOME/.pico-sdk/ninja/v1.13.2:$PATH && cmake -S . -B build -G Ninja && ninja -C build`

## File structure

| File | Responsibility |
|---|---|
| `features/features.h`, `features/features.c` | bins → `features_t` (bands, loudness, centroid, beat) |
| `features/CMakeLists.txt` | firmware library `features` |
| `effects/palette.h`, `effects/palette.c` | palette colour lookup (wrap/reflect), gamma table |
| `effects/effects.h`, `effects/effects.c` | modes, colour layer, pixel output |
| `effects/CMakeLists.txt` | firmware library `effects` |
| `tests/test_features.c`, `tests/test_palette.c`, `tests/test_effects.c` | host tests |
| `tests/CMakeLists.txt` | host targets (modified) |
| `CMakeLists.txt`, `main.c` | integration (modified, Task 8) |

---

### Task 1: features: bands, auto-gain, smoothing, loudness, centroid

**Files:**
- Create: `features/features.h`, `features/features.c`
- Create: `tests/test_features.c`
- Modify: `tests/CMakeLists.txt` (append)

**Interfaces:**
- Produces: `features_t`, `features_state_t`, `FEATURES_BAND_COUNT`, `bool features_init(features_state_t *this, size_t bin_count, float bin_hz, float hop_seconds)`, `const features_t *features_update(features_state_t *this, const float *bins)`, `void features_deinit(features_state_t *this)`. `bins` holds `bin_count` magnitudes, bin k centred at `k * bin_hz`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_features.c`:

```c
#include "check.h"
#include "features.h"

#include <math.h>
#include <string.h>

#define BINS 256
#define FS 48828.125f
#define BIN_HZ (FS / 512.f)
#define HOP (256.f / FS)

static float bins[BINS];

static void fill(float magnitude) {
    for (size_t k = 0; k < BINS; k++)
        bins[k] = magnitude;
}

static size_t band_of(const features_state_t *state, float hz) {
    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        if (state->edges[b] <= hz && hz < state->edges[b + 1])
            return b;
    return FEATURES_BAND_COUNT;
}

static size_t loudest_band(const features_t *features) {
    size_t loudest = 0;

    for (size_t b = 1; b < FEATURES_BAND_COUNT; b++)
        if (features->bands[b] > features->bands[loudest])
            loudest = b;

    return loudest;
}

static void test_rejects_invalid(void) {
    features_state_t state;

    CHECK(!features_init(&state, 1, BIN_HZ, HOP));
    CHECK(!features_init(&state, BINS, 0.f, HOP));
    CHECK(!features_init(&state, BINS, BIN_HZ, 0.f));
}

static void test_tone_lands_in_its_band(void) {
    features_state_t state;
    const features_t *features = NULL;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < 200; i++)
        features = features_update(&state, bins);

    CHECK(loudest_band(features) == band_of(&state, 10 * BIN_HZ));
    CHECK(features->bands[band_of(&state, 10 * BIN_HZ)] > 0.95f);

    features_deinit(&state);
}

static void test_silence_and_noise_stay_dark(void) {
    const float levels[] = {0.f, 1e-5f};

    for (size_t l = 0; l < 2; l++) {
        features_state_t state;
        const features_t *features = NULL;

        CHECK(features_init(&state, BINS, BIN_HZ, HOP));

        fill(levels[l]);
        for (int i = 0; i < 500; i++)
            features = features_update(&state, bins);

        for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
            CHECK(features->bands[b] == 0.f);
        CHECK(features->loudness == 0.f);
        CHECK(features->centroid == 0.f);

        features_deinit(&state);
    }
}

// A signal 20 dB quieter fills the range again once the ceiling fell
static void test_auto_gain(void) {
    features_state_t state;
    const features_t *features = NULL;
    size_t band;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));
    band = band_of(&state, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.95f);

    bins[10] = 0.01f;
    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.95f);

    features_deinit(&state);
}

static void test_attack_faster_than_decay(void) {
    features_state_t state;
    const features_t *features;
    size_t band;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));
    band = band_of(&state, 10 * BIN_HZ);

    fill(1e-6f);
    bins[10] = 0.1f;
    features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.3f);

    for (int i = 0; i < 1000; i++)
        features = features_update(&state, bins);

    bins[10] = 1e-6f;
    features = features_update(&state, bins);
    CHECK(features->bands[band] > 0.9f);

    features_deinit(&state);
}

static void test_loudness_and_centroid(void) {
    features_state_t state;
    const features_t *features = NULL;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    // Energy only low: centroid near 0; only high: near 1
    fill(1e-6f);
    bins[1] = 0.1f;
    for (int i = 0; i < 500; i++)
        features = features_update(&state, bins);
    CHECK(features->centroid < 0.2f);
    CHECK(features->loudness > 0.f);

    fill(1e-6f);
    bins[120] = 0.1f;
    for (int i = 0; i < 500; i++)
        features = features_update(&state, bins);
    CHECK(features->centroid > 0.8f);

    features_deinit(&state);
}

int main(void) {
    test_rejects_invalid();
    test_tone_lands_in_its_band();
    test_silence_and_noise_stay_dark();
    test_auto_gain();
    test_attack_faster_than_decay();
    test_loudness_and_centroid();

    return CHECK_REPORT();
}
```

Append to `tests/CMakeLists.txt`:

```cmake

add_library(features_host STATIC ${ROOT}/features/features.c)
target_include_directories(features_host PUBLIC ${ROOT}/features)
target_link_libraries(features_host PUBLIC m)

add_executable(test_features test_features.c)
target_link_libraries(test_features features_host)
add_test(NAME features COMMAND test_features)
set_tests_properties(features PROPERTIES TIMEOUT 30)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake -S tests -B build-tests && cmake --build build-tests`
Expected: FAIL. CMake reports `Cannot find source file: .../features/features.c`.

- [ ] **Step 3: Write the implementation**

Create `features/features.h`:

```c
#ifndef FEATURES_H
#define FEATURES_H

/**
 * Audio features: turns FFT magnitude bins into a compact description of
 * the sound, once per hop
 */

#include <stdbool.h>
#include <stddef.h>

// Log-spaced bands between FEATURES_LOW_HZ and FEATURES_HIGH_HZ
#define FEATURES_BAND_COUNT 32
#define FEATURES_LOW_HZ 60.f
#define FEATURES_HIGH_HZ 12000.f

// Levels span FEATURES_RANGE_DB below an auto-gain ceiling that follows the
// loudest band up at once, falls back slowly, and never goes below
// FEATURES_MIN_CEILING_DB so that silence stays dark. INMP441 noise floor
// (-87 dBFS) plus the x12 input gain is about -65 dB: tune on hardware
#define FEATURES_RANGE_DB 45.f
#define FEATURES_CEILING_FALL_DB_PER_S 6.f
#define FEATURES_MIN_CEILING_DB -50.f

// Per band smoothing time constants
#define FEATURES_ATTACK_MS 10.f
#define FEATURES_DECAY_MS 120.f

// A beat is bass energy (bands up to FEATURES_BEAT_MAX_HZ) above its moving
// average times FEATURES_BEAT_THRESHOLD, at most once per refractory time
#define FEATURES_BEAT_THRESHOLD 1.4f
#define FEATURES_BEAT_REFRACTORY_MS 150.f
#define FEATURES_BEAT_MAX_HZ 150.f
#define FEATURES_BEAT_AVERAGE_MS 1000.f

typedef struct {
    // FEATURES_BAND_COUNT levels, 0..1, smoothed
    const float *bands;
    // Mean band level, 0..1
    float loudness;
    // Level weighted band position, 0 (bass) .. 1 (treble), 0 in silence
    float centroid;
    // True on the hop a beat is detected
    bool beat;
    // 0..1, how far above the threshold the beat was
    float beat_strength;
} features_t;

typedef struct {
    size_t bin_count;
    float bin_hz;
    float hop_seconds;

    // Band edges in Hz, band b spans [edges[b], edges[b + 1])
    float edges[FEATURES_BAND_COUNT + 1];

    // Smoothed band levels, 0..1
    float levels[FEATURES_BAND_COUNT];

    // Auto-gain ceiling in dB and how far it falls per hop
    float ceiling_db;
    float ceiling_fall_db;

    // Per hop smoothing factors
    float attack_k;
    float decay_k;

    // Beat detection
    size_t bass_band_count;
    float bass_average;
    float average_k;
    float since_beat_s;

    features_t out;
} features_state_t;

/**
 * bin_count: number of magnitude bins (>= 2), bin k centred at k * bin_hz
 * hop_seconds: time between two features_update calls
 */
bool features_init(features_state_t *this, size_t bin_count, float bin_hz,
                   float hop_seconds);

/**
 * Updates from the newest bins and returns the features, valid until the
 * next call
 */
const features_t *features_update(features_state_t *this, const float *bins);

// Nothing is allocated today, kept for symmetry with the other modules
void features_deinit(features_state_t *this);

#endif
```

Create `features/features.c`:

```c
#include "features.h"

#include <math.h>

// Added to band powers so silence has a finite dB value
#define SILENCE_POWER 1e-12f

// Fraction of the way to the target per hop for a time constant
static float smoothing_factor(float hop_seconds, float time_constant_ms) {
    return 1.f - expf(-hop_seconds / (time_constant_ms / 1000.f));
}

static float clamp01(float value) {
    return value < 0.f ? 0.f : value > 1.f ? 1.f : value;
}

bool features_init(features_state_t *this, size_t bin_count, float bin_hz,
                   float hop_seconds) {
    float ratio;

    if (bin_count < 2 || !(bin_hz > 0.f) || !(hop_seconds > 0.f))
        return false;

    // Geometric edges, each band ~18 % wider than the one below
    ratio = powf(FEATURES_HIGH_HZ / FEATURES_LOW_HZ,
                 1.f / FEATURES_BAND_COUNT);
    for (size_t b = 0; b <= FEATURES_BAND_COUNT; b++)
        this->edges[b] = FEATURES_LOW_HZ * powf(ratio, (float)b);

    // Bands counted as bass for beats, at least one
    this->bass_band_count = 1;
    while (this->bass_band_count < FEATURES_BAND_COUNT &&
           this->edges[this->bass_band_count + 1] <= FEATURES_BEAT_MAX_HZ)
        this->bass_band_count++;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++)
        this->levels[b] = 0.f;

    this->bin_count = bin_count;
    this->bin_hz = bin_hz;
    this->hop_seconds = hop_seconds;
    this->ceiling_db = FEATURES_MIN_CEILING_DB;
    this->ceiling_fall_db = FEATURES_CEILING_FALL_DB_PER_S * hop_seconds;
    this->attack_k = smoothing_factor(hop_seconds, FEATURES_ATTACK_MS);
    this->decay_k = smoothing_factor(hop_seconds, FEATURES_DECAY_MS);
    this->average_k = smoothing_factor(hop_seconds, FEATURES_BEAT_AVERAGE_MS);
    this->bass_average = 0.f;
    this->since_beat_s = FEATURES_BEAT_REFRACTORY_MS / 1000.f;
    this->out = (features_t){.bands = this->levels};

    return true;
}

// Mean bin power within the band, or the power interpolated at the band's
// centre when the band is narrower than a bin (the lowest bands)
static float band_power(const features_state_t *this, const float *bins,
                        size_t band) {
    float low = this->edges[band], high = this->edges[band + 1];
    float sum = 0.f, x, fraction, p0, p1;
    size_t count = 0, k;

    for (k = (size_t)ceilf(low / this->bin_hz);
         k < this->bin_count && (float)k * this->bin_hz < high; k++) {
        sum += bins[k] * bins[k];
        count++;
    }

    if (count > 0)
        return sum / (float)count;

    x = sqrtf(low * high) / this->bin_hz;
    k = (size_t)x;

    if (k + 1 >= this->bin_count)
        return bins[this->bin_count - 1] * bins[this->bin_count - 1];

    fraction = x - (float)k;
    p0 = bins[k] * bins[k];
    p1 = bins[k + 1] * bins[k + 1];

    return p0 + (p1 - p0) * fraction;
}

const features_t *features_update(features_state_t *this, const float *bins) {
    float power[FEATURES_BAND_COUNT], db[FEATURES_BAND_COUNT];
    float loudest = -1000.f, floor_db, sum = 0.f, weighted = 0.f;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        power[b] = band_power(this, bins, b);
        db[b] = 10.f * log10f(power[b] + SILENCE_POWER);

        if (db[b] > loudest)
            loudest = db[b];
    }

    // Auto-gain: up to the loudest band at once, back down slowly, never
    // below the silence floor
    this->ceiling_db = fmaxf(this->ceiling_db - this->ceiling_fall_db, loudest);
    this->ceiling_db = fmaxf(this->ceiling_db, FEATURES_MIN_CEILING_DB);
    floor_db = this->ceiling_db - FEATURES_RANGE_DB;

    for (size_t b = 0; b < FEATURES_BAND_COUNT; b++) {
        float target = clamp01((db[b] - floor_db) / FEATURES_RANGE_DB);
        float k = target > this->levels[b] ? this->attack_k : this->decay_k;

        this->levels[b] += (target - this->levels[b]) * k;
        sum += this->levels[b];
        weighted += this->levels[b] * (float)b;
    }

    this->out.loudness = sum / FEATURES_BAND_COUNT;
    this->out.centroid =
        sum > 1e-6f ? weighted / sum / (FEATURES_BAND_COUNT - 1) : 0.f;
    this->out.beat = false;
    this->out.beat_strength = 0.f;

    return &this->out;
}

void features_deinit(features_state_t *this) { (void)this; }
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass, including `features`, with no compiler warnings.

Note on `test_silence_and_noise_stay_dark`: the levels must be exactly 0.f. Every target is clamped to 0, and levels start at 0, so `levels += (0 - 0) * k` stays exactly 0.

- [ ] **Step 5: Commit**

```bash
git add features tests/test_features.c tests/CMakeLists.txt
git commit -m "features: log bands with auto-gain and smoothing

FFT bins become FEATURES_BAND_COUNT (32) log-spaced band levels between
60 Hz and 12 kHz: mean bin power per band, or the power interpolated at
the band centre for bands narrower than a bin. Levels are dB mapped
between an auto-gain ceiling (up at once, falling 6 dB/s, never below
the silence floor) and 45 dB below it, smoothed with attack/decay time
constants. Loudness and spectral centroid summarize the bands.

Host tests: tone in its band, silence and low noise stay dark, auto-gain
refills the range, attack faster than decay, centroid low/high.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: features: beat detection

**Files:**
- Modify: `features/features.c` (`features_update`)
- Modify: `tests/test_features.c`

**Interfaces:**
- Consumes: `features_state_t` fields `bass_band_count`, `bass_average`, `average_k`, `since_beat_s` (Task 1).
- Produces: `features_t.beat` and `features_t.beat_strength`, filled in by `features_update`.

- [ ] **Step 1: Write the failing tests**

In `tests/test_features.c`, add these two functions before `int main(void) {`:

```c
// Kicks at 120 BPM: one beat per kick, on the kick
static void test_beat_per_kick(void) {
    features_state_t state;
    int beats = 0;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    for (int n = 0; n * HOP < 5.f; n++) {
        float t = n * HOP, since_kick = fmodf(t, 0.5f);
        const features_t *features;

        fill(1e-5f);
        bins[0] = bins[1] = 0.5f * expf(-since_kick / 0.05f);
        features = features_update(&state, bins);

        if (features->beat && t >= 1.f) {
            beats++;
            CHECK(since_kick < 0.02f);
            CHECK(features->beat_strength > 0.f);
            CHECK(features->beat_strength <= 1.f);
        }
    }

    CHECK(beats == 8);

    features_deinit(&state);
}

// A steady bass tone is not a beat, nor is silence
static void test_no_beat_without_onsets(void) {
    features_state_t state;
    int beats = 0;

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(1e-5f);
    bins[1] = 0.3f;
    for (int n = 0; n * HOP < 5.f; n++)
        if (features_update(&state, bins)->beat && n * HOP >= 2.f)
            beats++;
    CHECK(beats == 0);

    features_deinit(&state);

    CHECK(features_init(&state, BINS, BIN_HZ, HOP));

    fill(0.f);
    for (int n = 0; n < 1000; n++)
        if (features_update(&state, bins)->beat)
            beats++;
    CHECK(beats == 0);

    features_deinit(&state);
}
```

In `main`, add these calls before `return CHECK_REPORT();`:

```c
    test_beat_per_kick();
    test_no_beat_without_onsets();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure -R features`
Expected: FAIL with `CHECK(beats == 8) failed`, because beats are never set yet.

- [ ] **Step 3: Implement beat detection**

In `features/features.c`, in `features_update`, replace:

```c
    this->out.beat = false;
    this->out.beat_strength = 0.f;

    return &this->out;
```

with:

```c
    detect_beat(this, power, floor_db);

    return &this->out;
```

and add this function above `features_update`:

```c
// Bass energy above its moving average, once per refractory time, and only
// when audible above the auto-gain floor
static void detect_beat(features_state_t *this, const float *power,
                        float floor_db) {
    float bass = 0.f, trigger;

    for (size_t b = 0; b < this->bass_band_count; b++)
        bass += power[b];
    bass /= (float)this->bass_band_count;

    this->since_beat_s += this->hop_seconds;
    this->out.beat = false;
    this->out.beat_strength = 0.f;
    trigger = this->bass_average * FEATURES_BEAT_THRESHOLD;

    if (this->since_beat_s >= FEATURES_BEAT_REFRACTORY_MS / 1000.f &&
        bass > trigger && 10.f * log10f(bass + SILENCE_POWER) > floor_db) {
        this->out.beat = true;
        this->out.beat_strength =
            trigger > 0.f ? clamp01(bass / trigger - 1.f) : 1.f;
        this->since_beat_s = 0.f;
    }

    // Updated after the comparison, so a kick does not raise its own bar
    this->bass_average += (bass - this->bass_average) * this->average_k;
}
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add features/features.c tests/test_features.c
git commit -m "features: detect beats from bass energy

A beat is the mean power of the bands up to 150 Hz rising above 1.4x its
1 s moving average, at most once per 150 ms and only when audible above
the auto-gain floor. Strength is how far above the threshold it was.

Host tests: 120 BPM kicks give exactly one beat per kick, on the kick; a
steady bass tone and silence give none.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: palettes and gamma

**Files:**
- Create: `effects/palette.h`, `effects/palette.c`
- Create: `tests/test_palette.c`
- Modify: `tests/CMakeLists.txt` (append)

**Interfaces:**
- Produces: `palette_id_t` (`PALETTE_RAINBOW`, `PALETTE_SYNTHWAVE`, `PALETTE_FIRE`, `PALETTE_OCEAN`, `PALETTE_COUNT`), `rgb_t { float r, g, b; }`, `rgb_t palette_color(palette_id_t palette, float position)`, `uint8_t palette_gamma(float value)`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_palette.c`:

```c
#include "check.h"
#include "palette.h"

#include <math.h>

#define TOLERANCE 1e-5

static void check_rgb(rgb_t actual, float r, float g, float b) {
    CHECK_NEAR(actual.r, r, TOLERANCE);
    CHECK_NEAR(actual.g, g, TOLERANCE);
    CHECK_NEAR(actual.b, b, TOLERANCE);
}

static void test_endpoints(void) {
    check_rgb(palette_color(PALETTE_FIRE, 0.f), 0.15f, 0.f, 0.f);
    check_rgb(palette_color(PALETTE_FIRE, 1.f), 1.f, 1.f, 0.7f);
    check_rgb(palette_color(PALETTE_OCEAN, 0.f), 0.f, 0.05f, 0.2f);
    check_rgb(palette_color(PALETTE_RAINBOW, 0.f), 1.f, 0.f, 0.f);
}

// Rainbow wraps around, the others reflect at their ends
static void test_wrap_and_reflect(void) {
    rgb_t a, b;

    check_rgb(palette_color(PALETTE_RAINBOW, 1.f), 1.f, 0.f, 0.f);
    check_rgb(palette_color(PALETTE_RAINBOW, 0.5f), 0.f, 1.f, 1.f);

    a = palette_color(PALETTE_RAINBOW, 0.3f);
    b = palette_color(PALETTE_RAINBOW, 2.3f);
    check_rgb(b, a.r, a.g, a.b);

    a = palette_color(PALETTE_FIRE, 0.75f);
    b = palette_color(PALETTE_FIRE, 1.25f);
    check_rgb(b, a.r, a.g, a.b);

    a = palette_color(PALETTE_SYNTHWAVE, 0.2f);
    b = palette_color(PALETTE_SYNTHWAVE, -0.2f);
    check_rgb(b, a.r, a.g, a.b);
}

static void test_interpolates_between_stops(void) {
    // Fire stops 0 and 1 are (0.15, 0, 0) and (0.8, 0.05, 0), 4 segments
    check_rgb(palette_color(PALETTE_FIRE, 0.125f), 0.475f, 0.025f, 0.f);
}

static void test_gamma(void) {
    CHECK(palette_gamma(0.f) == 0);
    CHECK(palette_gamma(1.f) == 255);
    CHECK(palette_gamma(0.5f) == 55);
    CHECK(palette_gamma(-1.f) == 0);
    CHECK(palette_gamma(2.f) == 255);
    CHECK(palette_gamma(NAN) == 0);
}

int main(void) {
    test_endpoints();
    test_wrap_and_reflect();
    test_interpolates_between_stops();
    test_gamma();

    return CHECK_REPORT();
}
```

Append to `tests/CMakeLists.txt`:

```cmake

add_library(effects_host STATIC ${ROOT}/effects/palette.c)
target_include_directories(effects_host PUBLIC ${ROOT}/effects ${ROOT}/util)
# util/color.h defines non-inline static helpers, as does the firmware build
target_compile_options(effects_host PUBLIC -Wno-unused-function)
target_link_libraries(effects_host PUBLIC features_host m)

add_executable(test_palette test_palette.c)
target_link_libraries(test_palette effects_host)
add_test(NAME palette COMMAND test_palette)
set_tests_properties(palette PROPERTIES TIMEOUT 30)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake -S tests -B build-tests && cmake --build build-tests`
Expected: FAIL with `Cannot find source file: .../effects/palette.c`.

- [ ] **Step 3: Write the implementation**

Create `effects/palette.h`:

```c
#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>

typedef enum {
    PALETTE_RAINBOW,
    PALETTE_SYNTHWAVE,
    PALETTE_FIRE,
    PALETTE_OCEAN,
    PALETTE_COUNT
} palette_id_t;

// Linear 0..1 colour, before gamma
typedef struct {
    float r;
    float g;
    float b;
} rgb_t;

/**
 * Colour at a position, 0..1 spanning the palette. Beyond that the rainbow
 * wraps around and the other palettes reflect at their ends
 */
rgb_t palette_color(palette_id_t palette, float position);

/**
 * Linear 0..1 to a gamma 2.2 corrected 0..255 channel, clamped
 */
uint8_t palette_gamma(float value);

#endif
```

Create `effects/palette.c`:

```c
#include "palette.h"

#include <math.h>
#include <stdbool.h>

#define GAMMA 2.2f

typedef struct {
    const rgb_t *stops;
    unsigned count;
    // Wraps around instead of reflecting at the ends
    bool cyclic;
} palette_t;

static const rgb_t rainbow[] = {{1.f, 0.f, 0.f}, {1.f, 1.f, 0.f},
                                {0.f, 1.f, 0.f}, {0.f, 1.f, 1.f},
                                {0.f, 0.f, 1.f}, {1.f, 0.f, 1.f}};

static const rgb_t synthwave[] = {{0.05f, 0.f, 0.25f}, {0.55f, 0.f, 0.8f},
                                  {1.f, 0.1f, 0.6f},   {1.f, 0.5f, 0.1f},
                                  {0.1f, 0.9f, 1.f}};

static const rgb_t fire[] = {{0.15f, 0.f, 0.f},
                             {0.8f, 0.05f, 0.f},
                             {1.f, 0.4f, 0.f},
                             {1.f, 0.8f, 0.1f},
                             {1.f, 1.f, 0.7f}};

static const rgb_t ocean[] = {{0.f, 0.05f, 0.2f},
                              {0.f, 0.3f, 0.7f},
                              {0.f, 0.7f, 0.8f},
                              {0.3f, 1.f, 0.8f},
                              {0.9f, 1.f, 1.f}};

static const palette_t palettes[PALETTE_COUNT] = {
    [PALETTE_RAINBOW] = {rainbow, 6, true},
    [PALETTE_SYNTHWAVE] = {synthwave, 5, false},
    [PALETTE_FIRE] = {fire, 5, false},
    [PALETTE_OCEAN] = {ocean, 5, false},
};

rgb_t palette_color(palette_id_t palette, float position) {
    const palette_t *p = &palettes[palette < PALETTE_COUNT ? palette : 0];
    unsigned from, to;
    float scaled, fraction;
    rgb_t a, b;

    if (p->cyclic) {
        // Wrap into 0..1, the last stop blends back into the first
        position -= floorf(position);
        scaled = position * (float)p->count;
        from = (unsigned)scaled % p->count;
        to = (from + 1) % p->count;
        fraction = scaled - floorf(scaled);
    } else {
        // Reflect: 0..1 forwards, 1..2 backwards, and so on
        position -= 2.f * floorf(position / 2.f);
        if (position > 1.f)
            position = 2.f - position;

        scaled = position * (float)(p->count - 1);
        from = (unsigned)scaled;
        if (from > p->count - 2)
            from = p->count - 2;
        to = from + 1;
        fraction = scaled - (float)from;
    }

    a = p->stops[from];
    b = p->stops[to];

    return (rgb_t){a.r + (b.r - a.r) * fraction, a.g + (b.g - a.g) * fraction,
                   a.b + (b.b - a.b) * fraction};
}

uint8_t palette_gamma(float value) {
    static uint8_t table[256];
    static bool ready = false;

    if (!ready) {
        for (unsigned i = 0; i < 256; i++)
            table[i] = (uint8_t)lroundf(255.f * powf(i / 255.f, GAMMA));
        ready = true;
    }

    // Also catches NaN
    if (!(value > 0.f))
        return 0;

    if (value >= 1.f)
        return table[255];

    return table[lroundf(value * 255.f)];
}
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass, including `palette`.

- [ ] **Step 5: Commit**

```bash
git add effects/palette.h effects/palette.c tests/test_palette.c tests/CMakeLists.txt
git commit -m "effects: palettes and gamma

Rainbow, Synthwave, Fire and Ocean palettes as colour stops, looked up
by position with linear interpolation. The rainbow wraps around, the
others reflect at their ends, so drift can move any palette smoothly.
palette_gamma maps linear 0..1 to gamma 2.2 corrected 0..255 through a
256 entry table.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: effects core, spectrum modes, colour layer

**Files:**
- Create: `effects/effects.h`, `effects/effects.c`
- Create: `tests/test_effects.c`
- Modify: `tests/CMakeLists.txt` (effects_host sources, new test)

**Interfaces:**
- Consumes: `features_t` (Task 1), `rgb_t`, `palette_color` and `palette_gamma` (Task 3), `color_neopixel_from_rgb` from `util/color.h`.
- Produces:
  - `effects_mode_t`: `EFFECTS_SPECTRUM`, `EFFECTS_SPECTRUM_MIRRORED`, `EFFECTS_RIVER`, `EFFECTS_RIPPLES`, `EFFECTS_VU`, `EFFECTS_GLOW`, `EFFECTS_MODE_COUNT`.
  - `effects_palette_t`, a typedef of `palette_id_t`.
  - `effects_t`.
  - `bool effects_init(effects_t *this, size_t led_count, size_t band_count, float hop_seconds, uint32_t seed)`
  - `void effects_set_mode(effects_t *this, effects_mode_t mode)`
  - `void effects_set_palette(effects_t *this, effects_palette_t palette)`
  - `void effects_render(effects_t *this, const features_t *features, uint32_t *pixels)`
  - `void effects_deinit(effects_t *this)`
  - Internal helpers used by Tasks 5–7: `color_at`, `put_mirrored`, `band_at`, `scale`, `add`.

- [ ] **Step 1: Write the failing test**

Create `tests/test_effects.c`:

```c
#include "check.h"
#include "color.h"
#include "effects.h"

#include <string.h>

#define LEDS 300
#define BANDS FEATURES_BAND_COUNT
#define HOP (256.f / 48828.125f)

static float bands[BANDS];
static uint32_t pixels[LEDS];

static features_t quiet(void) {
    memset(bands, 0, sizeof(bands));
    return (features_t){.bands = bands};
}

static unsigned brightness(uint32_t pixel) {
    color_neopixel_t color = {.value = pixel};
    return color.grba.r + color.grba.g + color.grba.b;
}

static size_t brightest(size_t from, size_t to) {
    size_t best = from;

    for (size_t i = from; i < to; i++)
        if (brightness(pixels[i]) > brightness(pixels[best]))
            best = i;

    return best;
}

static bool all_dark(void) {
    for (size_t i = 0; i < LEDS; i++)
        if (pixels[i] != 0)
            return false;
    return true;
}

static void test_rejects_invalid(void) {
    effects_t effects;

    CHECK(!effects_init(&effects, 1, BANDS, HOP, 1));
    CHECK(!effects_init(&effects, LEDS, 1, HOP, 1));
    CHECK(!effects_init(&effects, LEDS, BANDS, 0.f, 1));
}

static void check_silence_is_dark(effects_mode_t mode) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, mode);

    for (int frame = 0; frame < 50; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

static void test_spectrum_band_position(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM);

    // Band 20 of 32 lands at 20 / 31 of the strip
    bands[20] = 1.f;
    effects_render(&effects, &features, pixels);
    CHECK_NEAR(brightest(0, LEDS), 20.0 * (LEDS - 1) / (BANDS - 1), 1.0);

    effects_deinit(&effects);
}

static void test_mirrored_spectrum_band_position(void) {
    effects_t effects;
    features_t features = quiet();
    size_t half = LEDS / 2;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM_MIRRORED);

    bands[20] = 1.f;
    effects_render(&effects, &features, pixels);

    // Bass in the centre: the same distance on both sides
    CHECK_NEAR(brightest(half, LEDS) - half,
               20.0 * (half - 1) / (BANDS - 1), 1.0);
    CHECK_NEAR(half - 1 - brightest(0, half),
               20.0 * (half - 1) / (BANDS - 1), 1.0);

    effects_deinit(&effects);
}

// A beat flashes the whole strip, then fades
static void test_beat_flash(void) {
    effects_t effects;
    features_t features = quiet();
    unsigned first;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_SPECTRUM);

    features.beat = true;
    features.beat_strength = 1.f;
    effects_render(&effects, &features, pixels);
    first = brightness(pixels[0]);
    CHECK(first > 0);

    features = quiet();
    for (int frame = 0; frame < 5; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(brightness(pixels[0]) < first);

    for (int frame = 0; frame < 200; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(all_dark());

    effects_deinit(&effects);
}

int main(void) {
    test_rejects_invalid();
    check_silence_is_dark(EFFECTS_SPECTRUM);
    check_silence_is_dark(EFFECTS_SPECTRUM_MIRRORED);
    test_spectrum_band_position();
    test_mirrored_spectrum_band_position();
    test_beat_flash();

    return CHECK_REPORT();
}
```

In `tests/CMakeLists.txt`, replace:

```cmake
add_library(effects_host STATIC ${ROOT}/effects/palette.c)
```

with:

```cmake
add_library(effects_host STATIC ${ROOT}/effects/effects.c
                                ${ROOT}/effects/palette.c)
```

and append:

```cmake

add_executable(test_effects test_effects.c)
target_link_libraries(test_effects effects_host)
add_test(NAME effects COMMAND test_effects)
set_tests_properties(effects PROPERTIES TIMEOUT 30)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake -S tests -B build-tests && cmake --build build-tests`
Expected: FAIL with `Cannot find source file: .../effects/effects.c`.

- [ ] **Step 3: Write the implementation**

Create `effects/effects.h`:

```c
#ifndef EFFECTS_H
#define EFFECTS_H

/**
 * Effects: turns audio features into LED pixels, one render per hop
 */

#include "features.h"
#include "palette.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// River: LEDs the history moves outward per frame
#define EFFECTS_RIVER_SPEED 1

// Ripples: LEDs a pulse travels per frame, pulses alive at once
#define EFFECTS_RIPPLE_SPEED 2.f
#define EFFECTS_RIPPLE_MAX 8

// VU: peak dot hold time, then LEDs it falls per frame
#define EFFECTS_PEAK_HOLD_MS 300.f
#define EFFECTS_PEAK_FALL 1.f

// Palette rotation period, 0 disables
#define EFFECTS_DRIFT_PERIOD_S 60.f

// Palette shift towards its end at full loudness, 0 disables
#define EFFECTS_WARMTH 0.25f

// White added on a beat of full strength, and its fade time constant
#define EFFECTS_FLASH_LEVEL 0.35f
#define EFFECTS_FLASH_MS 80.f

// Sparkles: chance per LED per frame at full treble, and fade per frame
#define EFFECTS_SPARKLE_RATE 0.03f
#define EFFECTS_SPARKLE_DECAY 0.8f

// Bands counted as bass (glow), and the top fraction counted as treble
#define EFFECTS_BASS_BANDS 5
#define EFFECTS_TREBLE_FRACTION 0.25f

typedef enum {
    EFFECTS_SPECTRUM,
    EFFECTS_SPECTRUM_MIRRORED,
    EFFECTS_RIVER,
    EFFECTS_RIPPLES,
    EFFECTS_VU,
    EFFECTS_GLOW,
    EFFECTS_MODE_COUNT
} effects_mode_t;

typedef palette_id_t effects_palette_t;

typedef struct {
    // Distance from the centre of the leading edge, in LEDs
    float position;
    float strength;
    float color_position;
    bool active;
} effects_ripple_t;

typedef struct {
    size_t led_count;
    size_t band_count;
    // LEDs from the centre to one end, (led_count + 1) / 2
    size_t half;
    float hop_seconds;

    effects_mode_t mode;
    effects_palette_t palette;

    // Time since init, drives the drift
    float time_s;

    // Current beat flash level and its fade per frame
    float flash;
    float flash_k;

    // Colours being rendered, led_count
    rgb_t *frame;
    // River history, half, index 0 at the centre
    rgb_t *river;
    // Sparkle levels, led_count
    float *sparkle;

    effects_ripple_t ripples[EFFECTS_RIPPLE_MAX];
    unsigned beat_count;

    // VU peak, in LEDs from the ends, and its remaining hold time
    float peak;
    float peak_hold_s;

    // xorshift32 state, never 0
    uint32_t random;
} effects_t;

/**
 * band_count: length of features_t.bands (>= 2)
 * seed: for the sparkles, renders are deterministic for a seed
 */
bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed);

void effects_set_mode(effects_t *this, effects_mode_t mode);

void effects_set_palette(effects_t *this, effects_palette_t palette);

/**
 * Renders one frame into led_count color_neopixel_t words
 */
void effects_render(effects_t *this, const features_t *features,
                    uint32_t *pixels);

void effects_deinit(effects_t *this);

#endif
```

Create `effects/effects.c`:

```c
#include "effects.h"

#include "color.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static rgb_t scale(rgb_t color, float k) {
    return (rgb_t){color.r * k, color.g * k, color.b * k};
}

static void add(rgb_t *pixel, rgb_t color) {
    pixel->r += color.r;
    pixel->g += color.g;
    pixel->b += color.b;
}

// Palette colour with the drift and the loudness warmth applied
static rgb_t color_at(const effects_t *this, const features_t *features,
                      float position) {
    float drift = EFFECTS_DRIFT_PERIOD_S > 0.f
                      ? this->time_s / EFFECTS_DRIFT_PERIOD_S
                      : 0.f;

    return palette_color(this->palette, position + drift +
                                            features->loudness *
                                                EFFECTS_WARMTH);
}

// Adds a colour at a distance from the centre, on both sides
static void put_mirrored(effects_t *this, size_t distance, rgb_t color) {
    size_t right = this->led_count / 2 + distance;
    size_t centre_left = (this->led_count - 1) / 2;

    if (right < this->led_count)
        add(&this->frame[right], color);

    // With an odd count both sides share the centre LED
    if (distance <= centre_left && centre_left - distance != right)
        add(&this->frame[centre_left - distance], color);
}

// Band level at a fractional band position, interpolated
static float band_at(const effects_t *this, const features_t *features,
                     float position) {
    size_t band = (size_t)position;
    float fraction;

    if (band >= this->band_count - 1)
        return features->bands[this->band_count - 1];

    fraction = position - (float)band;

    return features->bands[band] +
           (features->bands[band + 1] - features->bands[band]) * fraction;
}

static void render_spectrum(effects_t *this, const features_t *features) {
    for (size_t i = 0; i < this->led_count; i++) {
        float x = (float)i / (float)(this->led_count - 1);

        this->frame[i] = scale(
            color_at(this, features, x),
            band_at(this, features, x * (float)(this->band_count - 1)));
    }
}

// Bass in the centre, treble towards both ends
static void render_spectrum_mirrored(effects_t *this,
                                     const features_t *features) {
    for (size_t d = 0; d < this->half; d++) {
        float x = this->half > 1 ? (float)d / (float)(this->half - 1) : 0.f;

        put_mirrored(
            this, d,
            scale(color_at(this, features, x),
                  band_at(this, features, x * (float)(this->band_count - 1))));
    }
}

bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed) {
    size_t half = (led_count + 1) / 2;
    rgb_t *frame, *river;
    float *sparkle;

    if (led_count < 2 || band_count < 2 || !(hop_seconds > 0.f))
        return false;

    frame = (rgb_t *)calloc(led_count, sizeof(rgb_t));
    river = (rgb_t *)calloc(half, sizeof(rgb_t));
    sparkle = (float *)calloc(led_count, sizeof(float));

    if (frame == NULL || river == NULL || sparkle == NULL) {
        free(frame);
        free(river);
        free(sparkle);
        return false;
    }

    *this = (effects_t){
        .led_count = led_count,
        .band_count = band_count,
        .half = half,
        .hop_seconds = hop_seconds,
        .mode = EFFECTS_RIVER,
        .palette = PALETTE_SYNTHWAVE,
        .flash_k = expf(-hop_seconds / (EFFECTS_FLASH_MS / 1000.f)),
        .frame = frame,
        .river = river,
        .sparkle = sparkle,
        .random = seed != 0 ? seed : 1,
    };

    return true;
}

void effects_set_mode(effects_t *this, effects_mode_t mode) {
    if (mode < EFFECTS_MODE_COUNT)
        this->mode = mode;
}

void effects_set_palette(effects_t *this, effects_palette_t palette) {
    if (palette < PALETTE_COUNT)
        this->palette = palette;
}

void effects_render(effects_t *this, const features_t *features,
                    uint32_t *pixels) {
    memset(this->frame, 0, this->led_count * sizeof(rgb_t));

    switch (this->mode) {
    case EFFECTS_SPECTRUM:
        render_spectrum(this, features);
        break;
    case EFFECTS_SPECTRUM_MIRRORED:
        render_spectrum_mirrored(this, features);
        break;
    default:
        break;
    }

    if (features->beat)
        this->flash = fmaxf(this->flash,
                            features->beat_strength * EFFECTS_FLASH_LEVEL);

    for (size_t i = 0; i < this->led_count; i++) {
        rgb_t color = this->frame[i];

        pixels[i] = color_neopixel_from_rgb(palette_gamma(color.r + this->flash),
                                            palette_gamma(color.g + this->flash),
                                            palette_gamma(color.b + this->flash))
                        .value;
    }

    this->flash *= this->flash_k;
    this->time_s += this->hop_seconds;
}

void effects_deinit(effects_t *this) {
    free(this->frame);
    free(this->river);
    free(this->sparkle);
}
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass, including `effects`, with no warnings. The flash decays by `exp(-5.24/80)` per frame, so after 200 frames it is 1.8e-6. `palette_gamma` rounds anything below 1/510 to 0, which is why `all_dark()` holds at the end of `test_beat_flash`.

- [ ] **Step 5: Commit**

```bash
git add effects/effects.h effects/effects.c tests/test_effects.c tests/CMakeLists.txt
git commit -m "effects: core, spectrum modes and colour layer

effects_render turns features into color_neopixel_t pixels once per hop:
- spectrum: band levels along the strip, colour from the position
- mirrored spectrum: bass in the centre, treble towards both ends
- colour layer for every mode: palette drift over time, loudness warmth,
  beat flash (white by beat strength, fading over 80 ms), gamma 2.2

Mode and palette are runtime state (effects_set_mode/_palette) so a
future menu can change them; defaults River and Synthwave.

Host tests: silence stays dark, a band lights its position in both
spectrum modes, the beat flash appears then fades out.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: river mode

**Files:**
- Modify: `effects/effects.c`
- Modify: `tests/test_effects.c`

**Interfaces:**
- Consumes: `effects_t.river`, `put_mirrored`, `color_at`, `scale` (Task 4).
- Produces: `EFFECTS_RIVER` rendering.

- [ ] **Step 1: Write the failing tests**

In `tests/test_effects.c`, add this before `int main(void) {`:

```c
// A sound enters at the centre and flows outward one LED per frame
static void test_river_flows_outward(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_RIVER);

    features.loudness = 1.f;
    features.centroid = 0.5f;
    effects_render(&effects, &features, pixels);
    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2);

    features = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &features, pixels);

    CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2 + 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightest(0, LEDS / 2) ==
          LEDS / 2 - 1 - 10 * EFFECTS_RIVER_SPEED);
    CHECK(brightness(pixels[LEDS / 2]) == 0);

    effects_deinit(&effects);
}
```

In `main`, add these before `return CHECK_REPORT();`:

```c
    check_silence_is_dark(EFFECTS_RIVER);
    test_river_flows_outward();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure -R effects`
Expected: FAIL at `CHECK(brightest(LEDS / 2, LEDS) == LEDS / 2 + 10 * EFFECTS_RIVER_SPEED)`, because the river mode renders nothing yet.

- [ ] **Step 3: Implement the river**

In `effects/effects.c`, add this above `bool effects_init(`:

```c
// The colour of the sound enters at the centre and flows outward
static void render_river(effects_t *this, const features_t *features) {
    size_t speed =
        EFFECTS_RIVER_SPEED < this->half ? EFFECTS_RIVER_SPEED : this->half;
    rgb_t fresh = scale(color_at(this, features, features->centroid),
                        features->loudness);

    memmove(this->river + speed, this->river,
            (this->half - speed) * sizeof(rgb_t));

    for (size_t d = 0; d < speed; d++)
        this->river[d] = fresh;

    for (size_t d = 0; d < this->half; d++)
        put_mirrored(this, d, this->river[d]);
}
```

In `effects_render`, add this case before `default:`:

```c
    case EFFECTS_RIVER:
        render_river(this, features);
        break;
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add effects/effects.c tests/test_effects.c
git commit -m "effects: river mode

Every frame a pixel coloured by the spectral centroid, as bright as the
loudness, enters at the centre; the history flows outward on both sides
at EFFECTS_RIVER_SPEED LEDs per frame.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: ripples mode and sparkles

**Files:**
- Modify: `effects/effects.c`
- Modify: `tests/test_effects.c`

**Interfaces:**
- Consumes: `effects_t.ripples`, `beat_count`, `sparkle`, `random`, `put_mirrored`, `color_at`, `scale`, `add` (Task 4).
- Produces: `EFFECTS_RIPPLES` rendering, plus `render_sparkles(effects_t *, const features_t *)` and `band_mean(const features_t *, size_t from, size_t to)`, which Task 7 uses.

- [ ] **Step 1: Write the failing tests**

In `tests/test_effects.c`, add these before `int main(void) {`:

```c
// A beat launches a pulse from the centre that travels outward
static void test_ripple_travels(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_RIPPLES);

    features.beat = true;
    features.beat_strength = 1.f;
    effects_render(&effects, &features, pixels);

    features = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &features, pixels);

    CHECK_NEAR(brightest(LEDS / 2, LEDS), LEDS / 2 + 10 * EFFECTS_RIPPLE_SPEED,
               0.5);

    effects_deinit(&effects);
}

// Treble sparkles, reproducibly for a seed
static void test_sparkles_deterministic(void) {
    effects_t a, b;
    features_t features = quiet();
    uint32_t first[LEDS];
    bool lit = false;

    for (size_t band = BANDS * 3 / 4; band < BANDS; band++)
        bands[band] = 1.f;

    CHECK(effects_init(&a, LEDS, BANDS, HOP, 7));
    CHECK(effects_init(&b, LEDS, BANDS, HOP, 7));
    effects_set_mode(&a, EFFECTS_RIPPLES);
    effects_set_mode(&b, EFFECTS_RIPPLES);

    for (int frame = 0; frame < 20; frame++) {
        effects_render(&a, &features, first);
        effects_render(&b, &features, pixels);
        CHECK(memcmp(first, pixels, sizeof(pixels)) == 0);
    }

    for (size_t i = 0; i < LEDS; i++)
        lit = lit || pixels[i] != 0;
    CHECK(lit);

    effects_deinit(&a);
    effects_deinit(&b);
}
```

In `main`, add these before `return CHECK_REPORT();`:

```c
    check_silence_is_dark(EFFECTS_RIPPLES);
    test_ripple_travels();
    test_sparkles_deterministic();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure -R effects`
Expected: FAIL at the `test_ripple_travels` CHECK_NEAR and at `CHECK(lit)`. Without the mode the flash has faded to one flat level, so the brightest pixel is the first index, and there are no sparkles.

- [ ] **Step 3: Implement ripples and sparkles**

In `effects/effects.c`, add these above `bool effects_init(`:

```c
static uint32_t next_random(effects_t *this) {
    uint32_t x = this->random;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return this->random = x;
}

// 0 (inclusive) to 1 (exclusive)
static float random_unit(effects_t *this) {
    return (float)(next_random(this) >> 8) / 16777216.f;
}

static float band_mean(const features_t *features, size_t from, size_t to) {
    float sum = 0.f;

    for (size_t b = from; b < to; b++)
        sum += features->bands[b];

    return to > from ? sum / (float)(to - from) : 0.f;
}

// White sparkles appearing with the treble, fading each frame
static void render_sparkles(effects_t *this, const features_t *features) {
    size_t from =
        this->band_count -
        (size_t)((float)this->band_count * EFFECTS_TREBLE_FRACTION);
    float treble = band_mean(features, from, this->band_count);

    for (size_t i = 0; i < this->led_count; i++) {
        this->sparkle[i] *= EFFECTS_SPARKLE_DECAY;

        if (random_unit(this) < treble * EFFECTS_SPARKLE_RATE)
            this->sparkle[i] = 1.f;

        add(&this->frame[i],
            scale((rgb_t){1.f, 1.f, 1.f}, 0.8f * this->sparkle[i]));
    }
}

// Beats launch pulses from the centre, the treble sparkles
static void render_ripples(effects_t *this, const features_t *features) {
    if (features->beat) {
        effects_ripple_t *slot = NULL;

        // A free slot, or else the pulse furthest out
        for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
            effects_ripple_t *ripple = &this->ripples[r];

            if (!ripple->active) {
                slot = ripple;
                break;
            }

            if (slot == NULL || ripple->position > slot->position)
                slot = ripple;
        }

        *slot = (effects_ripple_t){
            .position = 0.f,
            .strength = features->beat_strength,
            .color_position = (float)(this->beat_count++ % 8) / 8.f,
            .active = true,
        };
    }

    for (size_t r = 0; r < EFFECTS_RIPPLE_MAX; r++) {
        effects_ripple_t *ripple = &this->ripples[r];
        float width, fade, brightness;
        rgb_t color;

        if (!ripple->active)
            continue;

        width = 3.f + 6.f * ripple->strength;
        fade = fmaxf(0.f, 1.f - ripple->position / (float)this->half);
        brightness = (0.5f + 0.5f * ripple->strength) * fade;
        color = color_at(this, features, ripple->color_position);

        // Brightest at the leading edge, fading behind it
        for (size_t k = 0; (float)k < width; k++) {
            float distance = ripple->position - (float)k;

            if (distance < 0.f)
                break;

            put_mirrored(this, (size_t)distance,
                         scale(color, brightness * (1.f - (float)k / width)));
        }

        ripple->position += EFFECTS_RIPPLE_SPEED;

        if (ripple->position - width >= (float)this->half)
            ripple->active = false;
    }

    render_sparkles(this, features);
}
```

In `effects_render`, add this case before `default:`:

```c
    case EFFECTS_RIPPLES:
        render_ripples(this, features);
        break;
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add effects/effects.c tests/test_effects.c
git commit -m "effects: beat ripples and treble sparkles

Each beat launches a pulse from the centre (up to 8, additive), wider
and brighter with the beat strength, colour stepping through the
palette per beat, travelling EFFECTS_RIPPLE_SPEED LEDs per frame and
fading towards the ends. Treble energy spawns white sparkles from a
seeded xorshift32, so renders are reproducible.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: VU and glow modes

**Files:**
- Modify: `effects/effects.c`
- Modify: `tests/test_effects.c`

**Interfaces:**
- Consumes: `effects_t.peak`, `peak_hold_s`, `color_at`, `add`, `scale`, `band_mean`, `render_sparkles` (Tasks 4 and 6).
- Produces: `EFFECTS_VU` and `EFFECTS_GLOW` rendering. All six modes are then complete.

- [ ] **Step 1: Write the failing tests**

In `tests/test_effects.c`, add these before `int main(void) {`:

```c
// Twin meters from both ends, peak dot holds then falls
static void test_vu(void) {
    effects_t effects;
    features_t features = quiet();
    size_t half = LEDS / 2, length = half / 2;

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_VU);

    features.loudness = 0.5f;
    effects_render(&effects, &features, pixels);

    for (size_t d = 0; d < length; d++) {
        CHECK(brightness(pixels[d]) > 0);
        CHECK(brightness(pixels[LEDS - 1 - d]) > 0);
    }
    // The peak dot right after the bar, then dark
    CHECK(brightness(pixels[length]) > 0);
    for (size_t d = length + 1; d < half; d++)
        CHECK(brightness(pixels[d]) == 0);

    // Held a while after the sound stopped
    features = quiet();
    for (int frame = 0; frame < 10; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(brightness(pixels[length]) > 0);
    CHECK(brightness(pixels[0]) == 0);

    // Then falling
    for (int frame = 0; frame < 90; frame++)
        effects_render(&effects, &features, pixels);
    CHECK(brightness(pixels[length]) == 0);
    CHECK(brightest(0, half) > 0);
    CHECK(brightest(0, half) < length);

    effects_deinit(&effects);
}

// The whole strip breathes with the bass
static void test_glow_follows_bass(void) {
    effects_t effects;
    features_t features = quiet();

    CHECK(effects_init(&effects, LEDS, BANDS, HOP, 1));
    effects_set_mode(&effects, EFFECTS_GLOW);

    for (size_t band = 0; band < EFFECTS_BASS_BANDS; band++)
        bands[band] = 1.f;
    effects_render(&effects, &features, pixels);

    for (size_t i = 0; i < LEDS; i++)
        CHECK(brightness(pixels[i]) > 0);

    effects_deinit(&effects);
}
```

In `main`, add these before `return CHECK_REPORT();`:

```c
    check_silence_is_dark(EFFECTS_VU);
    check_silence_is_dark(EFFECTS_GLOW);
    test_vu();
    test_glow_follows_bass();
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure -R effects`
Expected: FAIL in `test_vu` and `test_glow_follows_bass` (no light rendered).

- [ ] **Step 3: Implement VU and glow**

In `effects/effects.c`, add these above `bool effects_init(`:

```c
// Twin meters filling from both ends, with peak dots that hold then fall
static void render_vu(effects_t *this, const features_t *features) {
    float length = features->loudness * (float)this->half;

    if (length >= this->peak) {
        this->peak = length;
        this->peak_hold_s = EFFECTS_PEAK_HOLD_MS / 1000.f;
    } else if (this->peak_hold_s > 0.f) {
        this->peak_hold_s -= this->hop_seconds;
    } else {
        this->peak = fmaxf(length, this->peak - EFFECTS_PEAK_FALL);
    }

    for (size_t d = 0; d < this->half && (float)d < length; d++) {
        rgb_t color = color_at(this, features, (float)d / (float)this->half);

        add(&this->frame[d], color);
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], color);
    }

    if (this->peak >= 1.f) {
        size_t d = (size_t)this->peak;

        if (d >= this->half)
            d = this->half - 1;

        add(&this->frame[d], (rgb_t){1.f, 1.f, 1.f});
        if (this->led_count - 1 - d != d)
            add(&this->frame[this->led_count - 1 - d], (rgb_t){1.f, 1.f, 1.f});
    }
}

// The whole strip breathes with the bass, the treble sparkles
static void render_glow(effects_t *this, const features_t *features) {
    size_t bass_bands = EFFECTS_BASS_BANDS < this->band_count
                            ? EFFECTS_BASS_BANDS
                            : this->band_count;
    rgb_t color = scale(color_at(this, features, features->centroid),
                        band_mean(features, 0, bass_bands));

    for (size_t i = 0; i < this->led_count; i++)
        this->frame[i] = color;

    render_sparkles(this, features);
}
```

In `effects_render`, add these cases before `default:`:

```c
    case EFFECTS_VU:
        render_vu(this, features);
        break;
    case EFFECTS_GLOW:
        render_glow(this, features);
        break;
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass. The hold is 300 ms, i.e. 58 frames of 5.24 ms, so after 10 frames the dot is still at `length`, and after 100 frames it has fallen about 42 LEDs.

- [ ] **Step 5: Commit**

```bash
git add effects/effects.c tests/test_effects.c
git commit -m "effects: VU meters and bass glow

- VU: twin meters filling from both ends with the loudness, palette
  gradient, white peak dots held 300 ms then falling 1 LED per frame
- glow: the whole strip in the centroid colour, as bright as the bass
  bands, with treble sparkles

All six modes are now implemented; every mode is dark in silence.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: firmware integration

**Files:**
- Create: `features/CMakeLists.txt`, `effects/CMakeLists.txt`
- Modify: `CMakeLists.txt` (`add_subdirectory`, link)
- Modify: `main.c` (includes, remove the old mapping, init, loop)
- Modify: `docs/superpowers/specs/2026-09-26-visualizer-design.md` (the constant names)

**Interfaces:**
- Consumes: everything from Tasks 1–7, plus `i2s_sample_rate()`, `audio_get_frequency_bins`, `audio_get_frequency_bin_count` and `swapchain_producer_buffer`.
- Produces: the firmware running the visualizer. The mode and palette are chosen with `VISUALIZER_MODE` and `VISUALIZER_PALETTE` in `main.c`.

- [ ] **Step 1: Add the firmware libraries**

Create `features/CMakeLists.txt`:

```cmake
add_library(features)

target_sources(features
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/features.c)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/features.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(features
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
```

Create `effects/CMakeLists.txt`:

```cmake
add_library(effects)

target_sources(effects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/effects.c
        ${CMAKE_CURRENT_SOURCE_DIR}/palette.c)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/effects.c
    ${CMAKE_CURRENT_SOURCE_DIR}/palette.c
    PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS}")

target_include_directories(effects
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(effects features util)
```

In `CMakeLists.txt`, replace:

```cmake
add_subdirectory(audio)
```

with:

```cmake
add_subdirectory(audio)
add_subdirectory(features)
add_subdirectory(effects)
```

and in `target_link_libraries(light-painting`, replace:

```cmake
        audio
        swapchain
```

with:

```cmake
        audio
        features
        effects
        swapchain
```

- [ ] **Step 2: Use the modules in main.c**

In `main.c`:

1. Replace the include `#include "color.h"` with:

```c
#include "effects.h"
#include "features.h"
```

2. After `#define LED_DATA_PIN 8`, add:

```c

// Visualizer look, see effects/effects.h and effects/palette.h
#define VISUALIZER_MODE EFFECTS_RIVER
#define VISUALIZER_PALETTE PALETTE_SYNTHWAVE
```

3. Delete the functions `magnitude_to_color` and `visualizer_map_frequency_bins_to_pixels` entirely, from `static color_neopixel_t magnitude_to_color(float magnitude) {` down to the closing `}` of `visualizer_map_frequency_bins_to_pixels`.

4. In `main`, after `audio_t audio;`, add:

```c
    features_state_t features;
    effects_t effects;
    float hop_seconds;
```

5. Replace:

```c
    printf("Audio init!\n");
```

with:

```c
    printf("Audio init!\n");

    hop_seconds = AUDIO_FFT_HOP / i2s_sample_rate();

    if (!features_init(&features, audio_get_frequency_bin_count(&audio),
                       i2s_sample_rate() / AUDIO_FFT_SIZE, hop_seconds)) {
        printf("Could not initialize features\n");
        return EXIT_FAILURE;
    }

    if (!effects_init(&effects, LED_COUNT, FEATURES_BAND_COUNT, hop_seconds,
                      1)) {
        printf("Could not initialize effects\n");
        return EXIT_FAILURE;
    }

    effects_set_mode(&effects, VISUALIZER_MODE);
    effects_set_palette(&effects, VISUALIZER_PALETTE);

    printf("Visualizer init!\n");
```

6. Replace:

```c
        visualizer_map_frequency_bins_to_pixels(
            audio_get_frequency_bins(&audio),
            audio_get_frequency_bin_count(&audio),
            swapchain_producer_buffer(&led_swapchain),
            neopixel_get_pixel_count());
```

with:

```c
        effects_render(
            &effects,
            features_update(&features, audio_get_frequency_bins(&audio)),
            swapchain_producer_buffer(&led_swapchain));
```

- [ ] **Step 3: Align the spec with the constant names**

In `docs/superpowers/specs/2026-09-26-visualizer-design.md`:
- Replace `### Modes (\`EFFECTS_MODE\`, default \`EFFECTS_RIVER\`)` with `### Modes (\`VISUALIZER_MODE\` in main.c, default \`EFFECTS_RIVER\`)`.
- Replace `- **Palettes** (\`EFFECTS_PALETTE\`, default \`EFFECTS_SYNTHWAVE\`)` with `- **Palettes** (\`VISUALIZER_PALETTE\` in main.c, default \`PALETTE_SYNTHWAVE\`)`.

- [ ] **Step 4: Build the firmware and run the host tests**

Run: `export PATH=$HOME/.pico-sdk/cmake/v4.3.4/bin:$HOME/.pico-sdk/ninja/v1.13.2:$PATH && cmake -S . -B build -G Ninja && ninja -C build`
Expected: `light-painting.uf2` builds with no warnings or errors. The `-Werror` build also verifies that `main.c` no longer references `color.h` functions.

Run: `export PATH=$HOME/.pico-sdk/cmake/v4.3.4/bin:$HOME/.pico-sdk/ninja/v1.13.2:$PATH && cmake -S . -B build-perf -G Ninja -DPERF_STATS=ON && ninja -C build-perf`
Expected: builds cleanly (the PERF_STATS path is unchanged).

Run: `cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure`
Expected: all tests pass (fft, swapchain, color, audio, features, palette, effects).

- [ ] **Step 5: Commit**

```bash
git add features/CMakeLists.txt effects/CMakeLists.txt CMakeLists.txt main.c docs/superpowers/specs/2026-09-26-visualizer-design.md
git commit -m "main: drive the LEDs with the features and effects visualizer

Replaces the linear bin-to-hue mapping (bass on ~3 LEDs, 40 % of the
strip above the microphone's 15 kHz roll-off, silence bright red, no
smoothing or gamma) with features_update + effects_render once per hop.
The look is chosen with VISUALIZER_MODE and VISUALIZER_PALETTE
(default River, Synthwave).

Untested on hardware: FEATURES_MIN_CEILING_DB in particular needs
tuning so that a quiet room reads dark.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Spec coverage check

| Spec item | Task |
|---|---|
| Log bands, 60 Hz–12 kHz, interpolation for narrow bands | 1 |
| dB, ceiling up at once, falls 6 dB/s, never below MIN_CEILING_DB, RANGE_DB | 1 |
| Attack/decay time constants per hop | 1 |
| Loudness, centroid | 1 |
| Beat: bass ≤ 150 Hz, 1 s average, 1.4× threshold, 150 ms refractory, audible, strength | 2 |
| Palettes Rainbow/Synthwave/Fire/Ocean, wrap/reflect, gamma 2.2 table | 3 |
| Runtime mode/palette setters, drift, warmth, beat flash | 4 |
| Spectrum and mirrored spectrum | 4 |
| River | 5 |
| Ripples (max 8, strength-sized, palette steps), sparkles with seeded xorshift32 | 6 |
| VU (twin, peak hold then fall) and glow | 7 |
| Allocation at init only, about 5 KB | 4 |
| main.c integration; old mapping removed | 8 |
| Silence dark in all modes; tests per spec list | 4–7 |
