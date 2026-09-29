# Preview

A page that runs the firmware's own analysis and effects on a demo signal, an audio file, the computer's audio or a microphone, and shows every mode as 300 LEDs, with the menu's settings as controls. The C code is compiled to WebAssembly, so what you see is what the code computes, not a re-implementation. It is not measured on the board: timing and the LEDs' real look are still the strip's to show.

## Build and open

Needs [Emscripten](https://emscripten.org) (`brew install emscripten`) and Node.

```bash
tools/preview/build.sh
```

This writes `build-preview/preview.html`, one file with everything in it. Open it in a browser (Chrome is the target; it has been tried in the app's built-in browser, not yet in Chrome itself). The audio tap is loaded from a `data:` URL: opened from disk in Chrome, the page failed with "Failed to load worklet module script: blob:null/..." when the tap came from a `blob:` URL. That failure was reported from Chrome and not reproduced in the built-in browser, so the `data:` URL is not yet confirmed in Chrome opened from disk. For the microphone, or if a browser refuses `file://`, serve it:

```bash
python3 -m http.server 8765 --directory build-preview
```

then open `http://localhost:8765/preview.html`.

## Using it

- **Source:** the demo signal, an audio file, computer audio, or the microphone. The demo is a 12 s loop: 10 s of a 120 BPM kick and snare, hi-hats, a bass line, a chord pad and a lead, all with harmonics, then 2 s of silence to watch the modes fall off. Stopping a source makes the strip fall dark by itself: the engine keeps running on silence.
- **Input level:** the board's microphones hear a room, far quieter than a song at full volume, and the quiet floor depends on absolute level. Lower it until the strip behaves like the board would (dark in quiet, alive in music). It starts at -18 dB for the demo, an audio file and computer audio, and at 0 dB for the microphone (and a virtual input picked on it).
- **Microphone:** the browser's echo cancelling, noise suppression and automatic gain are turned off, so the analysis hears the raw signal. The microphone path has only been exercised with a refused permission and a mocked stream, not with a real microphone.
- **Computer audio:** click it and Chrome's share picker opens. Pick a Chrome tab and tick "Also share tab audio" (tab audio on Chrome for macOS is documented by third-party sources; not tried here). Newer Chrome and macOS may also offer system audio: Chrome 141 and macOS 14.2 or later were reported (by a third-party article, not confirmed against Chrome's documentation, and not tried here). The shared video is switched off and never shown, and the audio is only analysed, not played again: the tab keeps playing on its own. Chrome's own "Stop sharing" stops the source.
- **Microphone input device:** once the microphone is running, an "Input device" list appears; choosing another device restarts the microphone on it. It should work for any app whose sound the virtual device can capture; it has not been tried. With [BlackHole](https://github.com/ExistentialAudio/BlackHole) (GPL-3.0; steps from its README, not tried here):
  1. `brew install blackhole-2ch` (needs admin rights and prompts for a restart).
  2. In Audio MIDI Setup create a Multi-Output Device with your speakers or headphones and "BlackHole 2ch".
  3. Make it the system output. macOS cannot change the volume of a Multi-Output Device, so use the volume of the app that plays.
  4. In the page choose Microphone, then the input device "BlackHole 2ch".
  5. If the loudness strip is pinned, lower "Input level".
- Neither capture route has been run: the code builds and passes a syntax check, but it has not been tried with a real share picker, a real device or even a mocked stream. Nothing is measured on the LED board.
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
