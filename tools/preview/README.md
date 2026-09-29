# Preview

A page that runs the firmware's own analysis and effects on a demo signal, an audio file or the microphone, and shows every mode as 300 LEDs, with the menu's settings as controls. The C code is compiled to WebAssembly, so what you see is what the code computes, not a re-implementation. It is not measured on the board: timing and the LEDs' real look are still the strip's to show.

## Build and open

Needs [Emscripten](https://emscripten.org) (`brew install emscripten`) and Node.

```bash
tools/preview/build.sh
```

This writes `build-preview/preview.html`, one file with everything in it. Open it in a browser (Chrome is the target; it has been tried in the app's built-in browser, not yet in Chrome itself). For the microphone, or if a browser refuses `file://`, serve it:

```bash
python3 -m http.server 8765 --directory build-preview
```

then open `http://localhost:8765/preview.html`.

## Using it

- **Source:** the demo signal, an audio file, or the microphone. The demo is a 12 s loop: 10 s of a 120 BPM kick and snare, hi-hats, a bass line, a chord pad and a lead, all with harmonics, then 2 s of silence to watch the modes fall off. Stopping a source makes the strip fall dark by itself: the engine keeps running on silence.
- **Input level:** the board's microphones hear a room, far quieter than a song at full volume, and the quiet floor depends on absolute level. Lower it until the strip behaves like the board would (dark in quiet, alive in music). It starts at -18 dB for the demo and an audio file, and at 0 dB for the microphone.
- **Microphone:** the browser's echo cancelling, noise suppression and automatic gain are turned off, so the analysis hears the raw signal. The microphone path has only been exercised with a refused permission and a mocked stream, not with a real microphone.
- **Settings:** the same ranges and steps as the board's menu, taken from the firmware's `settings` module, so a new setting shows up on its own.
- **All modes:** tick "show all at once" to render every mode and compare them; click a tile to select it.
- The browser's audio rate (usually 48 000 Hz) differs a little from the firmware's 48 828 Hz, so timing is about 2 % off.

## Checks

```bash
tools/preview/check-golden.sh   # the golden test, compiled to WebAssembly: same pixels as the host build
tools/preview/check.sh          # builds the engine and runs its 7 Node tests
ctest --test-dir build-tests    # test_preview: the engine's pixels equal the visualizer's
```

## Layout

- `preview_api.c/.h` - the engine, also built natively for the host tests
- `common.sh`, `build.sh`, `check.sh`, `check-golden.sh`, `build-page.mjs` - the build
- `page/` - the page (`index.html`, `style.css`, `app.js`)
- `test/engine.test.mjs` - Node tests of the WebAssembly engine
