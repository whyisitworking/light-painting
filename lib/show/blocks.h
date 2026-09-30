#ifndef BLOCKS_H
#define BLOCKS_H

/**
 * The shared drawing blocks, each with one musical meaning in every look:
 *
 *   wash   a soft background colour over a stretch: the song part
 *   burst  a flash growing from a point and fading: a low or mid hit
 *   spark  single LEDs blinking and fading: high hits, the fine rhythm
 *   beam   a comet with a tail: motion, its speed from the groove
 *
 * They add into one frame of linear RGB (the rules then keep it in range).
 * Bursts and beams live in fixed pools: when one is full, the dimmest burst
 * gives way (in practice the oldest), or the beam that travelled furthest
 * (beams fade only in the bends, so the dimmest may be a fresh one).
 * Nothing is allocated after blocks_init(). Positions are LED indexes,
 * fractional, 0 at the left end
 */

#include "color.h"

#include <stddef.h>

constexpr size_t BLOCKS_MAX_BURSTS = 16;
constexpr size_t BLOCKS_MAX_BEAMS = 16;

// Below this a level is dark on the strip (1e-3 ^ 2.2 of full after gamma)
// and goes to exactly 0, so that a fade ends at black
constexpr float BLOCKS_DARK = 1e-3f;

// A beam heading into a bend (the LEDs on a side wall, at the end it moves
// towards) fades with this time constant, so it turns the corner and dies
// away. First guess
constexpr float BLOCKS_BEND_FADE_S = 0.06f;

typedef struct {
    float centre;
    // Now and at most, in LEDs, and how fast it grows, LEDs per second
    float radius;
    float max_radius;
    float growth;
    // Brightness, and what a hop keeps of it
    float level;
    float keep;
    rgb_t color;
    bool active;
} blocks_burst_t;

typedef struct {
    float head;
    // LEDs per second, the sign is the direction
    float velocity;
    // Tail length in LEDs, and how far it went so far
    float tail;
    float travelled;
    float level;
    rgb_t color;
    bool active;
} blocks_beam_t;

typedef struct {
    size_t led_count;
    float hop_period_s;
    // LEDs on each side wall, at both ends: beams fade there
    size_t bend_count;
    // What a hop keeps of a beam in a bend
    float bend_keep;

    // The frame being drawn and the sparks' levels, led_count each
    rgb_t *frame;
    float *sparks;

    blocks_burst_t bursts[BLOCKS_MAX_BURSTS];
    blocks_beam_t beams[BLOCKS_MAX_BEAMS];
} blocks_t;

// False if led_count < 2, hop_period_s is not positive or memory runs out
[[nodiscard]] bool blocks_init(blocks_t *this, size_t led_count,
                               float hop_period_s, size_t bend_count);

// The frame to black, before a look draws
void blocks_clear(blocks_t *this);

// No bursts, beams or sparks: a look starts clean
void blocks_reset(blocks_t *this);

// Adds color to the LEDs from from up to (not including) to
void blocks_wash(blocks_t *this, size_t from, size_t to, rgb_t color);

/**
 * Starts a burst at centre, growing at growth LEDs per second up to
 * max_radius, at level, fading with the time constant fade_s
 */
void blocks_burst(blocks_t *this, float centre, float max_radius,
                  float growth, float level, float fade_s, rgb_t color);

// Lights a spark at an LED to at least level
void blocks_spark(blocks_t *this, size_t index, float level);

// Starts a beam at head moving at velocity LEDs per second, not 0 (a beam
// standing still is refused)
void blocks_beam(blocks_t *this, float head, float velocity, float tail,
                 float level, rgb_t color);

/**
 * Draws the bursts, beams and sparks into the frame and moves them one hop
 * on: bursts grow and fade, beams move (and fade in the bends, and end once
 * their tail left the strip; one still heading onto it, launched off an
 * end, goes on), sparks fade with the time constant fade_s and
 * show in spark_color
 */
void blocks_draw(blocks_t *this, rgb_t spark_color, float spark_fade_s);

void blocks_deinit(blocks_t *this);

#endif
