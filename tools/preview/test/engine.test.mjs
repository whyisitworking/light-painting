// The WebAssembly engine, driven from Node as the page drives it.
// Run through tools/preview/check.sh, which builds it first.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const build = process.env.PREVIEW_BUILD ?? join(root, 'build-preview');
const createEngine = createRequire(import.meta.url)(join(build, 'engine.js'));

const RATE = 48828.125;
const LEDS = 300;

async function engine() {
  const m = await createEngine();
  assert.equal(m._preview_init(RATE), 1);
  return m;
}

// Feeds count samples of a signal through the input buffer, in blocks
function feed(m, count, sample) {
  const input = m._preview_input() >> 2;
  for (let done = 0; done < count; ) {
    const n = Math.min(512, count - done);
    const block = m.HEAPF32.subarray(input, input + n);
    for (let i = 0; i < n; i++) block[i] = sample(done + i);
    m._preview_push(n);
    done += n;
  }
}

const words = (m, mode) =>
  new Uint32Array(m.HEAPU8.buffer, m._preview_pixels(mode), LEDS);
const lit = (m, mode) => words(m, mode).filter((w) => w).length;

// 120 BPM kicks
const kick = (i) => {
  const t = (i / RATE) % 0.5;
  return 0.8 * Math.exp(-t / 0.04) * Math.sin(2 * Math.PI * 55 * t);
};
// A tone with kicks: every look has something to draw
const music = (i) => 0.2 * Math.sin((2 * Math.PI * 1000 * i) / RATE) + kick(i);

test('names and counts', async () => {
  const m = await engine();
  assert.equal(m._preview_look_count(), 5);
  assert.equal(m.UTF8ToString(m._preview_look_name(0)), 'Pulse');
  assert.equal(m.UTF8ToString(m._preview_look_name(4)), 'Storm');
  assert.equal(m._preview_scene_count(), 5);
  assert.equal(m.UTF8ToString(m._preview_scene_name(0)), 'Neon Noir');
  assert.equal(m.UTF8ToString(m._preview_part_name(1)), 'Build');
  assert.equal(m._preview_setting_count(), 8);
});

test('ranges and snapping', async () => {
  const m = await engine();
  const at = m._preview_setting_range(2) >> 1;
  assert.deepEqual(Array.from(m.HEAP16.subarray(at, at + 6)), [10, 100, 5, 100, 100, 0]);
  assert.equal(m._preview_setting_range(999), 0);
  assert.equal(m._preview_set(2, 47), 45);
  assert.equal(m._preview_get(2), 45);
  m._preview_reset();
  assert.equal(m._preview_get(2), 100);
});

test('silence is dark, music lights every look in the gallery', async () => {
  const m = await engine();
  m._preview_set_gallery(1);
  feed(m, 20000, () => 0);
  for (let look = 0; look < 5; look++) assert.equal(lit(m, look), 0, `look ${look}`);
  feed(m, 80000, music);
  for (let look = 0; look < 5; look++) assert.ok(lit(m, look) > 0, `look ${look}`);
  assert.ok(m._preview_loudness() > 0.01);
});

test('without the gallery only the selected look renders', async () => {
  const m = await engine();
  m._preview_set(0, 3);
  feed(m, 80000, music);
  assert.ok(lit(m, 3) > 0);
  assert.equal(lit(m, 0), 0);
  assert.equal(lit(m, 4), 0);
});

test('a trim far below any level is silence', async () => {
  const m = await engine();
  m._preview_set_input_trim_db(-200);
  feed(m, 40000, music);
  assert.equal(lit(m, m._preview_get(0)), 0);
  assert.equal(m._preview_loudness(), 0);
});

test('hops, hits and starting again', async () => {
  const m = await engine();
  const input = m._preview_input() >> 2;
  m.HEAPF32.fill(0, input, input + 256);
  assert.equal(m._preview_push(100), 0);
  assert.equal(m._preview_push(156), 1);
  assert.equal(m._preview_hops(), 1);
  feed(m, RATE * 6, kick);
  assert.ok(m._preview_hits() >= 8, `hits ${m._preview_hits()}`);
  assert.equal(m._preview_init(44100), 1);
  assert.equal(m._preview_hops(), 0);
  assert.equal(m._preview_hits(), 0);
});

test('bands are readable', async () => {
  const m = await engine();
  feed(m, 40000, music);
  const at = m._preview_bands() >> 2;
  const bands = m.HEAPF32.subarray(at, at + 32);
  assert.ok(bands.some((level) => level > 0.1));
  assert.ok(bands.every((level) => level >= 0 && level <= 1));
});

test('the song part is readable', async () => {
  const m = await engine();
  feed(m, 40000, music);
  const part = m._preview_part();
  assert.ok(part >= 0 && part < 4, `part ${part}`);
  assert.equal(m._preview_drops(), 0);
});
