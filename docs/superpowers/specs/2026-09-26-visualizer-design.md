# Visualizer design

> Historical record: paths and names are from before the lib/, platform/, app/ restructuring, see the README for the current layout.

Date: 2026-09-26
Status: approved in discussion, pending spec review

## Goal

Replace the current pixel mapping in `main.c` with a music visualizer that looks good in demos. The current mapping has four problems:
- 256 linear FFT bins are stretched across the strip, so the bass gets about 3 LEDs and 40 % of the strip sits above the microphone's 15 kHz roll-off.
- Hue is driven by magnitude at full brightness, so silence shows a bright red strip.
- There is no smoothing.
- There is no gamma correction.

## Constraints

- **Hardware:** 300 × WS2812B on a 5 V 40 A supply, so no brightness or current cap is needed (worst case about 18 A).
- **Input per hop:** 256 FFT bins (`AUDIO_FFT_SIZE` 512, `AUDIO_FFT_HOP` 256, fs = 48828.125 Hz), which is 95.4 Hz per bin and a new analysis every 5.24 ms.
- **Microphone:** INMP441, usable roughly 60 Hz – 15 kHz (datasheet −3 dB points), summed to mono.
- **Language:** plain C17 with no SDK dependencies in the new modules, so both are host-testable.
- **Mode and palette:** chosen by compile-time constants for now, but stored in runtime state so a future LCD menu can change them.

## Structure

The pipeline has two new modules:

```
FFT bins ──► features/ ──features_t──► effects/ ──► uint32_t pixels[300] ──► LED swapchain
```

- **`features/`** turns the bins into a compact description of the sound.
- **`effects/`** turns that description into pixels.
- **`main.c`** calls `features_update` then `effects_render` once per hop. `visualizer_map_frequency_bins_to_pixels` and `magnitude_to_color` are removed.

## features/

```c
typedef struct {
    const float *bands;   // FEATURES_BAND_COUNT levels, 0..1, smoothed
    float loudness;       // 0..1
    float centroid;       // 0..1, weighted band position
    bool beat;            // true on the hop a beat is detected
    float beat_strength;  // 0..1
} features_t;

bool features_init(features_state_t *this, size_t bin_count, float bin_hz,
                   float hop_seconds);
const features_t *features_update(features_state_t *this, const float *bins);
void features_deinit(features_state_t *this);
```

| Constant | Default | Meaning |
|---|---|---|
| `FEATURES_BAND_COUNT` | 32 | log-spaced bands |
| `FEATURES_LOW_HZ` / `FEATURES_HIGH_HZ` | 60 / 12000 | band range |
| `FEATURES_RANGE_DB` | 45 | floor = ceiling − range |
| `FEATURES_CEILING_FALL_DB_PER_S` | 6 | auto-gain release |
| `FEATURES_MIN_CEILING_DB` | −32 | lowest auto-gain ceiling (band power dB of the normalized bins). Band power is per bin: the INMP441 noise floor of −87 dBFS plus the ×12 input gain is −65 dB in total, spread over N/2 = 256 bins (−24 dB), so a quiet room is about −88 dB per bin. The floor, −32 − 45 = −77 dB, keeps it dark with 11 dB of margin. Tune on hardware so that a quiet room reads dark |
| `FEATURES_ATTACK_MS` / `FEATURES_DECAY_MS` | 10 / 120 | per-band smoothing |
| `FEATURES_BEAT_SMOOTH_MS` | 30 | smoothing of the bass energy before detection |
| `FEATURES_BEAT_THRESHOLD` | 2.8 | smoothed bass energy vs its average |
| `FEATURES_BEAT_AVERAGE_MS` | 1000 | time constant of that average |
| `FEATURES_BEAT_MIN_LEVEL` | 0.3 | mean smoothed level of the bass bands for a beat |
| `FEATURES_BEAT_REFRACTORY_MS` | 150 | minimum time between beats |
| `FEATURES_BEAT_MAX_HZ` | 150 | bands counted as bass |

### Bands

Edges are geometric from `LOW_HZ` to `HIGH_HZ`.
- **Band power:** a band's power is the mean of the squared bin magnitudes within its edges.
- **Narrow bands:** a band narrower than a bin (the lowest ~5 bands) takes the power interpolated linearly at its centre frequency instead. These bands are correlated by nature; that is the resolution/latency trade-off already chosen.

### Level

- **Conversion:** `dB = 10·log10(power + ε)`.
- **Auto-gain:** a shared ceiling follows the maximum band dB instantly upwards and falls at `CEILING_FALL_DB_PER_S`, never below `MIN_CEILING_DB`. That minimum keeps silence dark: noise must not be auto-gained up to full scale.
- **Normalisation:** level = `(dB − (ceiling − RANGE_DB)) / RANGE_DB`, clamped to 0..1.
- **Smoothing:** `level += (target − level) · k`, with k from `ATTACK_MS` when rising and `DECAY_MS` when falling, converted with `hop_seconds` (`k = 1 − exp(−hop/τ)`).
- **Loudness** is the mean smoothed level.
- **Centroid** is `Σ level·i / Σ level / (BAND_COUNT − 1)`, or 0 for silence.

### Beat

- **Bass energy:** the mean linear power of the bands up to `BEAT_MAX_HZ`. These bands all interpolate the lowest ~2 bins, so per hop the energy is a single, very noisy draw (for steady noise it exceeds 1.4× its mean in about a quarter of the hops).
- **Smoothing:** the energy is smoothed with a `BEAT_SMOOTH_MS` time constant (about 6 hops). Measured through the real audio chain, smoothed steady white noise exceeds 2.6× its average in only 0.01 % of hops, while 120 BPM kicks peak about 6.5× above theirs.
- **Average:** an exponential moving average of the smoothed energy with a `BEAT_AVERAGE_MS` time constant, updated after the comparison so that a kick does not raise its own bar.
- **Trigger:** a beat fires when the smoothed energy exceeds `average × BEAT_THRESHOLD`, all of:
  - the refractory time has passed since the last beat;
  - the smoothed energy has fallen back below the trigger since the last beat, so a sustained rise gives one beat, not one per refractory time;
  - the bass is audible: the mean smoothed level of the bass bands is above `BEAT_MIN_LEVEL`. A dB comparison with the auto-gain floor would always pass for a real microphone, whose self-noise is above it.
- **Strength:** `(smoothed energy / (average × threshold) − 1)`, clamped to 0..1.
- **Not variance adaptive:** a `mean + k·σ` trigger was tried. A kick train is sparse, so its own σ is about 1.7× its mean and `mean + 3σ` sits at the smoothed kick peak: it missed three kicks in four.
- **Measured** (white noise as I²S words through audio, features and effects, 300 s after 2 s settling): none at −87 to −70 dBFS, where the bass level stays under `BEAT_MIN_LEVEL`, and 0.01 beats/s at −50 and −30 dBFS. 120 BPM kicks, alone or with white noise 20 dB below them, give 40 beats for 40 kicks, all within 15 ms of the kick. A steady 50 or 80 Hz tone and silence give none.

## effects/

```c
bool effects_init(effects_t *this, size_t led_count, size_t band_count,
                  float hop_seconds, uint32_t seed);
void effects_set_mode(effects_t *this, effects_mode_t mode);
void effects_set_palette(effects_t *this, effects_palette_t palette);
void effects_render(effects_t *this, const features_t *features,
                    uint32_t *pixels);   // color_neopixel_t values
void effects_deinit(effects_t *this);
```

### Modes (`VISUALIZER_MODE` in main.c, default `EFFECTS_RIVER`)

| Mode | Behaviour |
|---|---|
| `EFFECTS_SPECTRUM` | band levels interpolated along the strip, low → high. Colour = palette(position), brightness = level |
| `EFFECTS_SPECTRUM_MIRRORED` | same, bass in the centre, treble towards both ends |
| `EFFECTS_RIVER` | each frame a pixel = palette(centroid) × loudness enters at the centre; the history flows outward at `EFFECTS_RIVER_SPEED` LEDs per frame (default 1) |
| `EFFECTS_RIPPLES` | each beat launches a pulse from the centre (max 8, additive), width and brightness from beat strength, colour steps through the palette per beat, speed `EFFECTS_RIPPLE_SPEED`. Treble level spawns sparkles |
| `EFFECTS_VU` | twin meters filling from both ends with loudness, palette gradient, peak dots that hold for `EFFECTS_PEAK_HOLD_MS` then fall |
| `EFFECTS_GLOW` | the whole strip in palette(centroid) × bass level, plus treble sparkles |

### Colour layer (all modes)

- **Palettes** (`VISUALIZER_PALETTE` in main.c, default `PALETTE_SYNTHWAVE`): Rainbow, Synthwave, Fire, Ocean. Each is a list of RGB stops interpolated linearly. Rainbow wraps around; the others reflect at their ends when shifted.
- **Drift:** the palette position is offset by `t / EFFECTS_DRIFT_PERIOD_S` (default 60 s; 0 disables it). `t` wraps every two periods, where the drift repeats for wrapping and reflecting palettes alike, so it never loses float precision.
- **Warmth:** the position is offset towards the hot end by `loudness × EFFECTS_WARMTH` (default 0.25; 0 disables it).
- **Beat flash:** on a beat, white × strength × `EFFECTS_FLASH_LEVEL` (default 0.35) is added to every pixel, fading exponentially over `EFFECTS_FLASH_MS` (default 80 ms). It uses the saturating add in `util/color.h`.
- **Gamma:** 2.2, through a 256-entry lookup table applied last to each channel.
- **Sparkles:** a seeded xorshift32 pseudo-random generator, so renders are deterministic for a given seed.

## Timing and resources

- **Timing:** one render per hop (about 190/s). The LED driver sends only the newest frame (about 150 frames/s at 300 LEDs), so some frames are dropped by design.
- **Memory:** the river history is 300 × 3 floats, plus the ripple state and a 256-byte gamma table (about 5 KB in total).
- **Allocation:** everything is allocated at init. The render path does no allocation.

## Testing (host, CTest)

- **features:**
  - A 1 kHz tone peaks in the band containing 1 kHz.
  - Silence stays at 0. Microphone self-noise (Rayleigh distributed bins at −88 dB per bin) keeps every level and the loudness below 0.05, which the gamma table maps to 0, and gives no beats.
  - Synthetic kicks at 120 BPM give one beat per kick with no extra beats, also with noise 20 dB below them, and a steady tone gives none.
  - Steady noise (Rayleigh distributed bins) at any level gives fewer than 0.2 beats/s.
  - A −20 dB signal refills the range once the auto-gain settles.
  - Attack is faster than decay, per the constants.
- **effects:**
  - Silence gives an all-dark strip in every mode (flash off).
  - One loud band lights the matching position in both spectrum modes.
  - The river moves outward by `RIVER_SPEED` per frame.
  - A beat launches a ripple at the centre that is at the expected LED after n frames.
  - The VU length is proportional to loudness, and the peak holds, then falls.
  - Palette endpoints and wrap/reflect behave as specified, and the gamma table has fixed endpoints (0 → 0, 255 → 255).
  - Renders are deterministic for a given seed.
- **firmware:** builds with `-Werror`. A check on hardware follows once the hardware is available.

## Out of scope

- The LCD menu. The runtime setters are the hook for it.
- Stereo effects.
- Stop and restart of I²S sampling. The L/R parity issue is noted in the driver.
