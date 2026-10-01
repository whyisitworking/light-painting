#include "song.h"

#include <math.h>

constexpr float TAU = 6.28318530718f;

// Amplitudes, 24-bit sample units
constexpr float PAD_CALM = 30000.f;
constexpr float PAD_DROP = 60000.f;
constexpr float KICK_CALM = 100000.f;
constexpr float KICK_DROP = 600000.f;
constexpr float BASS = 120000.f;
constexpr float SNARE_DROP = 250000.f;
constexpr float HAT = 80000.f;
// The build: the snare roll and the riser noise grow from the first to the
// second value
constexpr float ROLL_START = 60000.f;
constexpr float ROLL_END = 300000.f;
constexpr float RISER_START = 5000.f;
constexpr float RISER_END = 80000.f;

// Decay time constants of the drums, in seconds
constexpr float KICK_DECAY_S = 0.04f;
constexpr float SNARE_DECAY_S = 0.06f;
constexpr float HAT_DECAY_S = 0.015f;

// The roll's gap halves every ROLL_STEP_S, from half a second, at most
// ROLL_HALVINGS times
constexpr float ROLL_STEP_S = 3.f;
constexpr uint32_t ROLL_HALVINGS = 3;

// Samples between phasor renormalisations: a rotation by multiplication
// drifts off the unit circle by rounding
constexpr uint32_t RENORMALISE_EVERY = 256;

static song_osc_t osc(float hz, float sample_rate) {
    float step = TAU * hz / sample_rate;

    return (song_osc_t){.c = 1.f, .s = 0.f, .step_c = cosf(step),
                        .step_s = sinf(step)};
}

// The oscillator's sine, then one sample on
static float osc_next(song_osc_t *osc) {
    float s = osc->s, c = osc->c;

    osc->c = c * osc->step_c - s * osc->step_s;
    osc->s = s * osc->step_c + c * osc->step_s;

    return s;
}

static void osc_renormalise(song_osc_t *osc) {
    // One Newton step towards 1 / |(c, s)|, enough for a small drift
    float k = 1.5f - 0.5f * (osc->c * osc->c + osc->s * osc->s);

    osc->c *= k;
    osc->s *= k;
}

static void osc_restart(song_osc_t *osc) {
    osc->c = 1.f;
    osc->s = 0.f;
}

static float env_next(song_env_t *env) {
    float level = env->level;

    env->level *= env->keep;

    return level;
}

// -1 to 1, xorshift32
static float noise(song_t *this) {
    uint32_t x = this->noise;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    this->noise = x;

    return (float)(int32_t)x / 2147483648.f;
}

static uint32_t samples(const song_t *this, float seconds) {
    return (uint32_t)lroundf(seconds * this->sample_rate);
}

bool song_init(song_t *this, float sample_rate, uint32_t seed) {
    if (!(sample_rate > 0.f))
        return false;

    *this = (song_t){
        .sample_rate = sample_rate,
        .noise = seed != 0 ? seed : 1,
        .pad_low = osc(220.f, sample_rate),
        .pad_high = osc(330.f, sample_rate),
        .bass = osc(82.5f, sample_rate),
        .kick = osc(55.f, sample_rate),
        .snare_body = osc(190.f, sample_rate),
        .kick_env = {.keep = expf(-1.f / (KICK_DECAY_S * sample_rate))},
        .snare_env = {.keep = expf(-1.f / (SNARE_DECAY_S * sample_rate))},
        .hat_env = {.keep = expf(-1.f / (HAT_DECAY_S * sample_rate))},
    };
    this->loop_samples = samples(this, SONG_LENGTH_S);

    return true;
}

// One sample of the song at this->sample
static float next_sample(song_t *this) {
    uint32_t n = this->sample, second = samples(this, 1.f),
             half = samples(this, 0.5f), quarter = samples(this, 0.25f);
    uint32_t build = samples(this, SONG_CALM_END_S),
             gap = samples(this, SONG_BUILD_END_S),
             drop = samples(this, SONG_GAP_END_S),
             calm = samples(this, SONG_DROP_END_S);
    float pad = 0.f, out = 0.f, hiss;

    if (n >= gap && n < drop) {
        // Silence, and nothing rings on into the drop
        this->kick_env.level = 0.f;
        this->snare_env.level = 0.f;
        this->hat_env.level = 0.f;
        osc_next(&this->pad_low);
        osc_next(&this->pad_high);
        return 0.f;
    }

    if (n >= drop && n < calm) {
        uint32_t t = n - drop;

        if (t % half == 0) {
            osc_restart(&this->kick);
            this->kick_env.level = KICK_DROP;
        }
        if (t % second == half) {
            osc_restart(&this->snare_body);
            this->snare_env.level = SNARE_DROP;
        }
        if (t % quarter == quarter / 2)
            this->hat_env.level = HAT;

        pad = PAD_DROP;
        out += BASS * osc_next(&this->bass);
    } else {
        if (n % second == 0) {
            osc_restart(&this->kick);
            this->kick_env.level = KICK_CALM;
        }

        pad = PAD_CALM;

        if (n >= build && n < gap) {
            uint32_t t = n - build;
            uint32_t step = t / samples(this, ROLL_STEP_S);
            uint32_t gap_samples =
                half >> (step < ROLL_HALVINGS ? step : ROLL_HALVINGS);
            float progress = (float)t / (float)(gap - build);

            if (t % gap_samples == 0) {
                osc_restart(&this->snare_body);
                this->snare_env.level =
                    ROLL_START + (ROLL_END - ROLL_START) * progress;
            }
            out += (RISER_START + (RISER_END - RISER_START) * progress) *
                   noise(this);
        }
    }

    hiss = noise(this);
    out += pad * (osc_next(&this->pad_low) + osc_next(&this->pad_high));
    out += env_next(&this->kick_env) * osc_next(&this->kick);
    out += env_next(&this->snare_env) *
           (0.7f * hiss + 0.3f * osc_next(&this->snare_body));
    out += env_next(&this->hat_env) * noise(this);

    return out;
}

void song_fill(song_t *this, int32_t *frames, size_t count) {
    for (size_t i = 0; i < count; i++) {
        float sample = next_sample(this);
        int32_t word;

        if (sample > 8388607.f)
            sample = 8388607.f;
        else if (sample < -8388608.f)
            sample = -8388608.f;

        // As tests/signals.h: 24 bits, shifted up by 7
        word = (int32_t)((uint32_t)(int32_t)lroundf(sample) << 7);
        frames[2 * i] = word;
        frames[2 * i + 1] = word;

        if (++this->sample == this->loop_samples)
            this->sample = 0;

        if (this->sample % RENORMALISE_EVERY == 0) {
            osc_renormalise(&this->pad_low);
            osc_renormalise(&this->pad_high);
            osc_renormalise(&this->bass);
            osc_renormalise(&this->kick);
            osc_renormalise(&this->snare_body);
        }
    }
}
