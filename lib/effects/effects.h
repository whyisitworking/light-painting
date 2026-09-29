#ifndef EFFECTS_H
#define EFFECTS_H

/**
 * Effects: turns the sound (sound_t, from features) into LED pixels, one
 * render per hop.
 * Each mode is a renderer in its own mode_*.c file, see effects_internal.h
 */

#include "features.h"
#include "palette.h"

#include <stddef.h>
#include <stdint.h>

// What the strip shows, see the README for each mode
typedef enum {
    EFFECTS_MODE_SPECTRUM,
    EFFECTS_MODE_SPECTRUM_MIRRORED,
    EFFECTS_MODE_RIVER,
    EFFECTS_MODE_RIPPLES,
    EFFECTS_MODE_VU,
    EFFECTS_MODE_GLOW,
    EFFECTS_MODE_POND,
    EFFECTS_MODE_CYMATICS,
    EFFECTS_MODE_FIRE,
    EFFECTS_MODE_STORM,
    EFFECTS_MODE_COUNT
} effects_mode_t;

// The look, see the README for each mode and palette
constexpr effects_mode_t EFFECTS_MODE = EFFECTS_MODE_RIVER;
constexpr palette_t EFFECTS_PALETTE = PALETTE_SYNTHWAVE;

// River: LEDs the history moves outward per frame
constexpr size_t EFFECTS_RIVER_SPEED = 1;

// Ripples: LEDs a pulse travels per frame, pulses alive at once
constexpr float EFFECTS_RIPPLE_SPEED = 2.f;
constexpr size_t EFFECTS_RIPPLE_MAX_PULSES = 8;

// VU: peak dot hold time, then LEDs it falls per frame
constexpr float EFFECTS_PEAK_HOLD_MS = 300.f;
constexpr float EFFECTS_PEAK_FALL = 1.f;

// Palette rotation period, 0 disables
constexpr float EFFECTS_DRIFT_PERIOD_S = 60.f;

// Palette shift towards its end at full loudness, 0 disables
constexpr float EFFECTS_WARMTH = 0.25f;

// White added on a beat of full strength, and its fade time constant
constexpr float EFFECTS_FLASH_LEVEL = 0.35f;
constexpr float EFFECTS_FLASH_MS = 80.f;

// Sparkles: chance per LED per frame at full treble, and fade per frame
constexpr float EFFECTS_SPARKLE_RATE = 0.03f;
constexpr float EFFECTS_SPARKLE_DECAY = 0.8f;

// Layers, on top of any mode, all off by default. Trails: fade time
// constant of what was shown, 0 disables
constexpr float EFFECTS_TRAILS_MS = 0.f;

// Diffuse: blur between neighbouring LEDs per frame, 0 to 1 (1 averages
// three LEDs), 0 disables
constexpr float EFFECTS_DIFFUSE = 0.f;

// Symmetry: segments the strip is folded into, 1 disables
constexpr size_t EFFECTS_SYMMETRY = 1;
constexpr size_t EFFECTS_SYMMETRY_MAX = 4;

// Chase: LEDs per second the image slides along the strip, negative the
// other way, 0 disables
constexpr float EFFECTS_CHASE_LEDS_PER_S = 0.f;

// Pond: how far a wave travels per sub-step (at most 1, or the simulation
// blows up) and the sub-steps per frame, so LEDs per frame is the product
constexpr float EFFECTS_POND_WAVE_SPEED = 0.5f;
constexpr size_t EFFECTS_POND_SUBSTEPS = 3;

// Pond: time constants of the waves' motion dying and of the water level
// settling back (damping alone would leave a permanent offset)
constexpr float EFFECTS_POND_DAMPING_S = 1.f;
constexpr float EFFECTS_POND_LEAK_S = 2.f;

// Pond: a stone's width in LEDs, the height of a drizzle drop and the chance
// of one per frame at full treble, and how bright a wave of height 1/GAIN is
constexpr float EFFECTS_POND_STONE_WIDTH = 2.f;
constexpr float EFFECTS_POND_DRIZZLE_HEIGHT = 0.15f;
constexpr float EFFECTS_POND_DRIZZLE_RATE = 0.02f;
constexpr float EFFECTS_POND_GAIN = 2.f;

// Cymatics: most nodes of the standing wave, how long the nodes take to
// slide to a new count, how fast it breathes (turns per second), and the
// loudness that lights it fully is 1 / GAIN
constexpr float EFFECTS_CYMATICS_MAX_NODES = 24.f;
constexpr float EFFECTS_CYMATICS_SLEW_S = 0.15f;
constexpr float EFFECTS_CYMATICS_BREATH_HZ = 0.2f;
constexpr float EFFECTS_CYMATICS_GAIN = 2.5f;

// Fire: LEDs per frame the heat moves outward (0 to 1), its cooling time
// constant, how many LEDs from the centre the sparks land in, and how much
// of the tips the treble makes flicker
constexpr float EFFECTS_FIRE_SPEED = 0.6f;
constexpr float EFFECTS_FIRE_COOL_S = 0.35f;
constexpr size_t EFFECTS_FIRE_SPARK_LEDS = 6;
constexpr float EFFECTS_FIRE_FLICKER = 0.5f;

// Storm: LEDs of a bolt at beat strength 0 and 1, the side branches it
// gets, the time constants of the bolt's afterglow and of the sky flash, and
// the sky flash at full strength
constexpr size_t EFFECTS_STORM_MIN_LENGTH = 20;
constexpr size_t EFFECTS_STORM_MAX_LENGTH = 160;
constexpr size_t EFFECTS_STORM_BRANCHES = 3;
constexpr float EFFECTS_STORM_GLOW_S = 0.08f;
constexpr float EFFECTS_STORM_SKY_S = 0.15f;
constexpr float EFFECTS_STORM_SKY_LEVEL = 0.15f;

// Bands counted as bass (glow), and the top fraction counted as treble
constexpr size_t EFFECTS_BASS_BANDS = 5;
constexpr float EFFECTS_TREBLE_FRACTION = 0.25f;

// What can be changed while running, the constants above are the defaults
typedef struct {
    // EFFECTS_MODE, EFFECTS_PALETTE: one of the modes and palettes
    effects_mode_t mode;
    palette_t palette;
    // 0 to 1, 1 by default. Perceptual: each step looks equally brighter.
    // Low levels leave the strip's 8 bits few steps for the colours
    float brightness;
    // EFFECTS_RIVER_SPEED: at least 1
    size_t river_speed;
    // EFFECTS_RIPPLE_SPEED: positive
    float ripple_speed;
    // EFFECTS_PEAK_HOLD_MS: 0 or more
    float peak_hold_ms;
    // EFFECTS_DRIFT_PERIOD_S: 0 or more, 0 disables the drift
    float drift_period_s;
    // EFFECTS_WARMTH: 0 or more
    float warmth;
    // EFFECTS_FLASH_LEVEL, EFFECTS_SPARKLE_RATE: 0 to 1
    float flash_level;
    float sparkle_rate;
    // EFFECTS_TRAILS_MS: 0 or more, 0 disables the trails
    float trails_ms;
    // EFFECTS_DIFFUSE: 0 to 1, 0 disables the blur
    float diffuse;
    // EFFECTS_SYMMETRY: 1 to EFFECTS_SYMMETRY_MAX, 1 disables the folding
    size_t symmetry;
    // EFFECTS_CHASE_LEDS_PER_S: any finite value, 0 disables the sliding
    float chase_leds_per_s;
} effects_tuning_t;

// One pulse of the ripples mode, launched from the centre by a beat
typedef struct {
    // Distance from the centre of the leading edge, in LEDs
    float position;
    // Beat strength 0..1, sets width and brightness
    float strength;
    // Palette position, steps by 1/8 per beat
    float color_position;
    // False once it left the strip, the slot is free
    bool active;
} effects_pulse_t;

// Render state. Allocated by effects_init(), the render path never allocates
typedef struct {
    size_t led_count;
    size_t band_count;
    // LEDs from the centre to one end, (led_count + 1) / 2
    size_t half_led_count;
    // Time between two renders
    float hop_period_s;

    effects_tuning_t tuning;
    // tuning.peak_hold_ms in seconds
    float peak_hold_s;
    // What the brightness scales the flash by, after gamma
    float flash_duty;

    // Time since init modulo two drift periods, drives the drift. 0 while
    // the drift is disabled
    float time_s;
    // xorshift32 state, never 0: the one stream of every mode that needs
    // chance, see effects_random()
    uint32_t random;

    // Current beat flash level and its fade per frame
    float flash;
    float flash_k;

    // Colours being rendered, led_count
    rgb_t *frame;

    // Per mode state
    struct {
        // History, half_led_count, index 0 at the centre
        rgb_t *history;
    } river;

    struct {
        effects_pulse_t pulses[EFFECTS_RIPPLE_MAX_PULSES];
        unsigned beat_count;
    } ripples;

    struct {
        // Peak, in LEDs from the ends, and its remaining hold time
        float peak;
        float hold_s;
    } vu;

    // Shared by the modes that sparkle
    struct {
        // Levels, led_count
        float *levels;
    } sparkles;

    // The layers, see layers.c
    struct {
        // The frame as last shown (capped at 1) and a scratch row, both
        // led_count
        rgb_t *previous;
        rgb_t *scratch;
        // Trails: what is left of a colour after one frame, and whether
        // previous holds anything
        float trails_k;
        bool trails_active;
        // Chase: how far the image is shifted, 0 up to led_count LEDs
        float offset;
    } layers;

    // One allocation for the rows of the simulations below, sliced by
    // effects_init(). Freed by effects_deinit()
    float *pool;

    struct {
        // Height and the height one sub-step before, led_count each. The
        // two swap places every sub-step
        float *height;
        float *previous;
        // What a sub-step keeps of the motion, and of the height
        float velocity_k;
        float leak_k;
    } pond;

    struct {
        // Nodes now, slewed towards the loudest band's, and the breathing
        float nodes;
        float phase;
    } cymatics;

    struct {
        // Heat 0..1 from the centre out, half_led_count, and what a frame
        // keeps of it (a time constant, so the hop rate does not matter)
        float *heat;
        float cool_k;
    } fire;

    struct {
        // Brightness of what the last bolts left, led_count, and the dim
        // flash of the sky; what each keeps per frame
        float *afterglow;
        float sky;
        float glow_k;
        float sky_k;
    } storm;
} effects_t;

/**
 * band_count: length of sound_t.bands (>= 2)
 * seed: for the sparkles, renders are deterministic for a seed
 *
 * Starts on effects_default_tuning(). False if a count is too small,
 * hop_period_s is not positive or memory runs out
 */
[[nodiscard]] bool effects_init(effects_t *this, size_t led_count,
                                size_t band_count, float hop_period_s,
                                uint32_t seed);

// The EFFECTS_* constants, what effects_init() starts with
effects_tuning_t effects_default_tuning(void);

/**
 * Takes effect on the next render. A field out of its range (see
 * effects_tuning_t) or NaN keeps its current value
 */
void effects_tune(effects_t *this, const effects_tuning_t *tuning);

/**
 * Renders one frame into led_count color_ws2812_t words
 */
void effects_render(effects_t *this, const sound_t *sound, uint32_t *pixels);

// Only after a successful effects_init()
void effects_deinit(effects_t *this);

#endif
