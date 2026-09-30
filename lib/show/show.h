#ifndef SHOW_H
#define SHOW_H

/**
 * The show: turns the sound (sound_t, from features) into LED pixels, one
 * frame per hop. A look draws with the shared blocks (blocks.h) and, where
 * they cannot say it, a block of its own; the scene gives the colours by
 * role (scene.h); the rules (rules.h) then keep every frame in check, for
 * every look. See the README for the looks.
 *
 * Song parts, for every look: a drop or a lift swaps the field and accent
 * colours, at once, on the beat; a gap fades everything to black within
 * SHOW_GAP_FADE_MS.
 *
 * Each look is a look_*.c file, see show_internal.h
 */

#include "blocks.h"
#include "features.h"
#include "rules.h"
#include "scene.h"

#include <stddef.h>
#include <stdint.h>

// What the strip shows
typedef enum {
    SHOW_LOOK_PULSE,
    SHOW_LOOK_FLOW,
    SHOW_LOOK_STAGE,
    SHOW_LOOK_SWEEP,
    SHOW_LOOK_STORM,
    SHOW_LOOK_COUNT
} show_look_t;

// The defaults: the look whose response is most instantly obvious, and the
// scene
constexpr show_look_t SHOW_LOOK = SHOW_LOOK_PULSE;
constexpr scene_t SHOW_SCENE = SCENE_NEON_NOIR;

// A gap fades to black with this time constant
constexpr float SHOW_GAP_FADE_MS = 30.f;

// Sparks fade with this time constant, in every look
constexpr float SHOW_SPARK_FADE_S = 0.06f;

// The washes are at full strength from this groove up, and fade with it
// below, so that silence is black in every look. First guess
constexpr float SHOW_PRESENCE_GROOVE = 0.2f;

// The strip's lowest step: color_gamma() sends 0 for a channel under about
// 0.057 (14.5 / 255 before the curve), so a wash below it never lights. The
// washes' first guesses put the dimmest field in any scene (0.3, Dusk and
// Ice) at about 0.065, one step up, in calm at full presence and full
// Brightness: 0.065 / 0.3 = 0.22. At a lower Brightness they may round to
// off. Tuned in the preview, with this in mind

// Pulse (first guesses): the field wash's level in calm (just above the
// lowest step, see above), in high (plus the groove's share), and in a
// build (plus its share at full progress, paling towards the hit colour by
// PULSE_BUILD_PALE); a low hit's burst radius as a fraction of the half
// strip, fade time and how fast it grows (reaching its radius in
// PULSE_GROW_S); mid hits' end bursts as a fraction of the strip; sparks per
// high hit at strength 0 and 1; the drop's burst fade
constexpr float SHOW_PULSE_WASH = 0.22f;
constexpr float SHOW_PULSE_GROOVE_WASH = 0.15f;
constexpr float SHOW_PULSE_BUILD_WASH = 0.2f;
constexpr float SHOW_PULSE_BUILD_PALE = 0.6f;
constexpr float SHOW_PULSE_GROW_S = 0.06f;
constexpr float SHOW_PULSE_CALM_RADIUS = 0.15f;
constexpr float SHOW_PULSE_CALM_FADE_S = 0.25f;
constexpr float SHOW_PULSE_HIGH_RADIUS = 0.3f;
constexpr float SHOW_PULSE_HIGH_RADIUS_STRENGTH = 0.5f;
constexpr float SHOW_PULSE_HIGH_FADE_S = 0.15f;
constexpr float SHOW_PULSE_BUILD_RADIUS = 0.25f;
constexpr float SHOW_PULSE_BUILD_SQUEEZE = 0.15f;
constexpr float SHOW_PULSE_BUILD_FADE_S = 0.1f;
constexpr float SHOW_PULSE_END_RADIUS = 0.12f;
constexpr float SHOW_PULSE_END_FADE_S = 0.12f;
constexpr float SHOW_PULSE_SPARKS = 3.f;
constexpr float SHOW_PULSE_SPARKS_STRENGTH = 6.f;
constexpr float SHOW_PULSE_DROP_FADE_S = 0.25f;

// Flow (first guesses): LEDs per second the stream flows in calm, what a
// full build adds, in high (at no groove, plus the groove's share); the
// drop's extra speed and how fast it settles; the stream's level (times the
// presence, see SHOW_PRESENCE_GROOVE; just above the lowest step, see there)
// and what full loudness adds; what a low hit injects at strength 0 and 1
constexpr float SHOW_FLOW_CALM_SPEED = 60.f;
constexpr float SHOW_FLOW_BUILD_SPEED = 360.f;
constexpr float SHOW_FLOW_HIGH_SPEED = 120.f;
constexpr float SHOW_FLOW_GROOVE_SPEED = 180.f;
constexpr float SHOW_FLOW_DROP_BOOST = 600.f;
constexpr float SHOW_FLOW_BOOST_S = 1.f;
constexpr float SHOW_FLOW_LEVEL = 0.22f;
constexpr float SHOW_FLOW_LOUD_LEVEL = 0.5f;
constexpr float SHOW_FLOW_HIT = 0.6f;
constexpr float SHOW_FLOW_HIT_STRENGTH = 0.4f;

// Stage (first guesses): the share of each half the bars leave to the
// wings (the field wash, and the bends), how much a full build squeezes the
// bars towards the centre, the bars' level in calm, the wings' wash level
// at no groove and full groove (halved in calm, with the bars: at full
// presence, groove 0.2, just above the lowest step, see there), the level a
// bar needs for a spark on a high hit and how many LEDs of bars get one, and
// the drop's hit-colour fade
constexpr float SHOW_STAGE_WING = 0.25f;
constexpr float SHOW_STAGE_SQUEEZE = 0.5f;
constexpr float SHOW_STAGE_CALM_LEVEL = 0.5f;
constexpr float SHOW_STAGE_WING_WASH = 0.42f;
constexpr float SHOW_STAGE_WING_GROOVE = 0.1f;
constexpr float SHOW_STAGE_SPARK_LEVEL = 0.5f;
constexpr float SHOW_STAGE_SPARK_SHARE = 0.1f;
constexpr float SHOW_STAGE_DROP_FADE_S = 0.2f;

// Sweep (first guesses): beam speeds in LEDs per second (calm, the build's
// start and end, high at no groove plus the groove's share), tail lengths in
// LEDs, levels, the drop's volley (beams from each end, spaced, faster by
// SWEEP_VOLLEY_SPEED), the crossing flash (radius, fade), and the field
// wash (just above the lowest step, see there)
constexpr float SHOW_SWEEP_CALM_SPEED = 120.f;
constexpr float SHOW_SWEEP_BUILD_SPEED = 280.f;
constexpr float SHOW_SWEEP_HIGH_SPEED = 180.f;
constexpr float SHOW_SWEEP_GROOVE_SPEED = 150.f;
constexpr float SHOW_SWEEP_CALM_TAIL = 25.f;
constexpr float SHOW_SWEEP_TAIL = 40.f;
constexpr float SHOW_SWEEP_SHORT_TAIL = 15.f;
constexpr float SHOW_SWEEP_CALM_LEVEL = 0.5f;
constexpr float SHOW_SWEEP_LEVEL = 0.8f;
constexpr size_t SHOW_SWEEP_VOLLEY = 4;
constexpr float SHOW_SWEEP_VOLLEY_GAP = 20.f;
constexpr float SHOW_SWEEP_VOLLEY_SPEED = 1.2f;
constexpr float SHOW_SWEEP_CROSS_RADIUS = 8.f;
constexpr float SHOW_SWEEP_CROSS_FADE_S = 0.1f;
constexpr float SHOW_SWEEP_WASH = 0.22f;

// Storm (first guesses): the near-black field wash (just above the lowest
// step, see there); rain sparks per high
// hit in calm and high and at the end of a build, and their level; the
// distant sheet glow of a low hit in calm; the least low hit strength that
// strikes in high; a bolt's length at strength 0 and 1, its branches, the
// afterglow and sky fade, the sky flash at full strength, and the drop's
constexpr float SHOW_STORM_WASH = 0.22f;
constexpr float SHOW_STORM_RAIN = 2.f;
constexpr float SHOW_STORM_HIGH_RAIN = 4.f;
constexpr float SHOW_STORM_BUILD_RAIN = 10.f;
constexpr float SHOW_STORM_RAIN_LEVEL = 0.4f;
constexpr float SHOW_STORM_SHEET = 0.12f;
constexpr float SHOW_STORM_STRIKE_MIN = 0.3f;
constexpr size_t SHOW_STORM_MIN_LENGTH = 40;
constexpr size_t SHOW_STORM_MAX_LENGTH = 200;
constexpr size_t SHOW_STORM_BRANCHES = 5;
constexpr float SHOW_STORM_GLOW_S = 0.14f;
constexpr float SHOW_STORM_SKY_S = 0.25f;
constexpr float SHOW_STORM_SKY_LEVEL = 0.3f;
constexpr float SHOW_STORM_DROP_SKY = 0.6f;

// What can be changed while running
typedef struct {
    // SHOW_LOOK, SHOW_SCENE: one of the looks and scenes
    show_look_t look;
    scene_t scene;
    // 0 to 1, 1 by default. Perceptual: each step looks equally brighter
    float brightness;
} show_tuning_t;

// Render state. Allocated by show_init(), the render path never allocates
typedef struct {
    size_t led_count;
    // LEDs from the centre to one end, (led_count + 1) / 2
    size_t half_led_count;
    size_t band_count;
    float hop_period_s;

    show_tuning_t tuning;

    // xorshift32 state, never 0: renders are deterministic for a seed
    uint32_t random;
    // Field and accent swapped by the last drop or lift
    bool swapped;
    // What a gap leaves of the frame, and what it keeps per hop
    float gap;
    float gap_keep;

    blocks_t blocks;
    rules_t rules;

    // Flow's stream: the colours from the centre outward, half_led_count,
    // the fraction of an LED it moved, and the drop's extra speed
    struct {
        rgb_t *history;
        float carry;
        float boost;
        float boost_keep;
    } flow;

    // Stage: what is left of the drop's hit colour, and what a hop keeps
    struct {
        float flash;
        float flash_keep;
    } stage;

    // Sweep: beams launched so far (even ones from the left end)
    struct {
        unsigned launches;
    } sweep;

    // Storm: the bolts' afterglow, led_count, the sky flash, and what each
    // keeps per hop
    struct {
        float *glow;
        float sky;
        float glow_keep;
        float sky_keep;
    } storm;
} show_t;

/**
 * band_count: length of sound_t.bands (>= 2). bend_count: LEDs on each side
 * wall, at both ends (0 if none). seed: renders are deterministic for it.
 * Starts on show_default_tuning(). False if a count is too small,
 * hop_period_s is not positive or memory runs out
 */
[[nodiscard]] bool show_init(show_t *this, size_t led_count,
                             size_t band_count, float hop_period_s,
                             size_t bend_count, uint32_t seed);

// SHOW_LOOK, SHOW_SCENE, full brightness
show_tuning_t show_default_tuning(void);

/**
 * Takes effect on the next render; another look starts clean. A field out
 * of range or NaN keeps its current value
 */
void show_tune(show_t *this, const show_tuning_t *tuning);

// The look starts clean, as after switching to it: no blocks, no state
void show_restart(show_t *this);

// Renders one frame into led_count color_ws2812_t words
void show_render(show_t *this, const sound_t *sound, uint32_t *pixels);

// Only after a successful show_init()
void show_deinit(show_t *this);

#endif
