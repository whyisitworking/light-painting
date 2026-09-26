# Light Painting

Simple and ultra fast music visualizer using RP2350 (Raspberry Pi Pico 2). It uses the trustworthy MEMS microphone i2s and outputs the visualization into an RGB addressable LED strip WS2812

## Architecture

```
I2S mic ─► platform/i2s ─frames─► lib/visualizer ─pixels─► platform/neopixel ─► WS2812
                                   audio ─bins─► features ─features_t─► effects
```

Once per hop (256 samples, about 5 ms) the main loop waits for new audio, analyzes it and renders a frame:

```c
frames = i2s_wait_buffer();
visualizer_analyze(&visualizer, frames);
visualizer_render(&visualizer, neopixel_frame());
neopixel_submit();
```

| Directory | What | Depends on |
|---|---|---|
| `lib/` | Portable C17, no Pico SDK, unit tested on the host | `lib/` only |
| `platform/` | Pico drivers: PIO + DMA, interrupts, the critical sections | `lib/swapchain`, Pico SDK |
| `app/` | `main.c`, the build time configuration (`config.h`) and the opt-in statistics (`perf.c`) | everything |

Dependencies point one way only, `app → platform → lib`. Anything that can run without hardware belongs in `lib/`, so the host tests can cover it.

| Module | Role |
|---|---|
| `lib/fft` | Radix-2 complex FFT and a real input FFT built on it |
| `lib/audio` | I2S words to mono samples, the sliding window, gain and FFT |
| `lib/features` | Log bands, auto-gain, smoothing, loudness, centroid and beats |
| `lib/effects` | Features to pixels: one renderer per mode, palettes, flash, gamma |
| `lib/color` | Linear RGB, gamma, the WS2812 word |
| `lib/swapchain` | Triple buffer handing data between an interrupt and the main loop |
| `lib/visualizer` | audio, features and effects put together |
| `platform/i2s` | INMP441 input, DMA into a ring, published per hop |
| `platform/neopixel` | WS2812 output, sends the newest frame |

Each module is defined once, with `lp_add_module()` from `cmake/modules.cmake`, and built the same way by the firmware and the tests.

### Adding an effect mode

1. Add a value to `effects_mode_t` in `lib/effects/effects.h`.
2. Write its renderer in a new `lib/effects/mode_<name>.c`, using the helpers in `effects_internal.h`, and declare it there.
3. Add it to the `renderers` table in `effects.c`, and the file to `lib/effects/CMakeLists.txt`.
4. Test it in `tests/test_effects.c`, and record the golden hashes again (see below).

Select it with `VISUALIZER_MODE` in `app/config.h`.

## Building

The firmware, with the Pico SDK (the VS Code Pico extension sets it up):

```bash
cmake -S . -B build -G Ninja
```

```bash
ninja -C build
```

Options: `-DPERF_STATS=ON` prints stage timings and driver counters once per second over USB. `-DWAIT_FOR_USB_HOST=ON` waits up to 2 s at startup for a serial host.

## Testing

The host tests build `lib/` with the native compiler:

```bash
cmake -S tests -B build-tests
```

```bash
cmake --build build-tests
```

```bash
ctest --test-dir build-tests --output-on-failure
```

`test_golden` hashes the pixels of every mode for a fixed input, and fails if a change alters them. When a change is meant to alter the look, check the other tests still pass, then record the new hashes from `build-tests/test_golden --print` into `tests/test_golden.c`.
