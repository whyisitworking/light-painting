'use strict';

// The page: drives the WebAssembly engine from the Web Audio API and draws
// what it renders. The engine is the firmware's own code (see preview_api.h).

const LED_COUNT = 300;
const INPUT_CAPACITY = 8192;
const FIRMWARE_RATE = 48828.125;
const BAND_COUNT = 32;

// The menu's settings in settings_id_t order (lib/settings/settings.h). A
// setting the engine has beyond this table still gets a slider
const SETTINGS = [
  { name: 'Mode', kind: 'mode' },
  { name: 'Palette', kind: 'palette' },
  { name: 'Brightness', group: 'Look', show: (raw) => `${raw} %` },
  { name: 'Gain', group: 'Sound', show: (raw, real) => `${real.toFixed(1)}x` },
  { name: 'Beat threshold', group: 'Sound', show: (raw, real) => `${real.toFixed(1)}x` },
  { name: 'Quiet floor', group: 'Sound', show: (raw) => `${raw} dB` },
  { name: 'Attack', group: 'Sound', show: (raw) => `${raw} ms` },
  { name: 'Decay', group: 'Sound', show: (raw) => `${raw} ms` },
  { name: 'Palette drift', group: 'Effects', show: (raw) => (raw === 0 ? 'Off' : `${raw} s`) },
  { name: 'Warmth', group: 'Effects', show: (raw) => `${raw} %` },
  { name: 'Beat flash', group: 'Effects', show: (raw) => `${raw} %` },
  { name: 'Sparkles', group: 'Effects', show: (raw, real) => `${(real * 100).toFixed(1)} %` },
  { name: 'River speed', group: 'Effects', show: (raw) => `${raw}` },
  { name: 'Ripple speed', group: 'Effects', show: (raw, real) => real.toFixed(1) },
  { name: 'VU peak hold', group: 'Effects', show: (raw) => `${raw} ms` },
  { name: 'Screen', hidden: true }, // the LCD backlight: nothing to see here
  { name: 'Trails', group: 'Layers', show: (raw) => (raw === 0 ? 'Off' : `${raw} ms`) },
  { name: 'Diffuse', group: 'Layers', show: (raw) => (raw === 0 ? 'Off' : `${raw} %`) },
  { name: 'Symmetry', group: 'Layers', show: (raw) => (raw <= 1 ? 'Off' : `${raw}`) },
  { name: 'Chase', group: 'Layers', show: (raw) => (raw === 0 ? 'Off' : `${raw > 0 ? '+' : ''}${raw} /s`) },
];
const SETTING_MODE = 0;
const SETTING_PALETTE = 1;
const GROUP_ORDER = ['Look', 'Sound', 'Effects', 'Layers', 'Other'];

const $ = (selector) => document.querySelector(selector);

// The LEDs' PWM words are gamma-corrected for the strip, whose light is linear
// in them; a screen applies its own gamma to a byte, so undo the strip's
const SCREEN = Uint8ClampedArray.from({ length: 256 }, (_, i) => Math.round(255 * Math.pow(i / 255, 1 / 2.2)));

function note(message) {
  const element = $('#note');
  element.hidden = !message;
  element.textContent = message ?? '';
}

async function main() {
  const engine = await createEngine();
  const inputAt = () => engine._preview_input() >> 2;
  const names = (count, name) => Array.from({ length: count() }, (_, i) => engine.UTF8ToString(name(i)));
  const modeNames = names(() => engine._preview_mode_count(), (i) => engine._preview_mode_name(i));
  const paletteNames = names(() => engine._preview_palette_count(), (i) => engine._preview_palette_name(i));
  const settingCount = engine._preview_setting_count();

  // What the page has set, replayed when the engine starts over at another
  // sample rate
  const state = { values: new Map(), trim: -18, gallery: false, rate: FIRMWARE_RATE };

  function start(rate) {
    if (!engine._preview_init(rate)) throw new Error('the engine could not start');
    state.rate = rate;
    for (const [id, value] of state.values) engine._preview_set(id, value);
    engine._preview_set_input_trim_db(state.trim);
    engine._preview_set_gallery(state.gallery ? 1 : 0);
  }
  start(FIRMWARE_RATE);

  const setSetting = (id, value) => {
    const stored = engine._preview_set(id, value);
    state.values.set(id, stored);
    return stored;
  };
  const selectedMode = () => engine._preview_get(SETTING_MODE);

  // ---- Settings controls, from the engine's own ranges
  const refreshers = [];

  function addRow(parent, label, control, output) {
    const row = document.createElement('div');
    row.className = 'row';
    const text = document.createElement('label');
    text.textContent = label;
    text.htmlFor = control.id;
    row.append(text, control);
    if (output) row.append(output);
    parent.append(row);
  }

  function addSelect(parent, id, label, options) {
    const select = document.createElement('select');
    select.id = `setting-${id}`;
    options.forEach((option, i) => select.append(new Option(option, i)));
    select.value = engine._preview_get(id);
    select.addEventListener('change', () => setSetting(id, Number(select.value)));
    refreshers.push(() => { select.value = engine._preview_get(id); });
    addRow(parent, label, select);
  }

  function addSlider(parent, id, info) {
    const at = engine._preview_setting_range(id) >> 1;
    const [min, max, step, , divisor] = Array.from(engine.HEAP16.subarray(at, at + 6));
    const slider = document.createElement('input');
    const output = document.createElement('output');
    slider.type = 'range';
    slider.id = `setting-${id}`;
    Object.assign(slider, { min, max, step });
    const show = () => {
      const raw = engine._preview_get(id);
      slider.value = raw;
      output.textContent = info.show ? info.show(raw, raw / divisor) : `${raw / divisor}`;
    };
    slider.addEventListener('input', () => {
      setSetting(id, Number(slider.value));
      show();
    });
    refreshers.push(show);
    show();
    addRow(parent, info.name, slider, output);
  }

  function buildControls() {
    const groups = new Map(GROUP_ORDER.map((group) => [group, []]));
    for (let id = 0; id < settingCount; id++) {
      const info = SETTINGS[id] ?? { name: `Setting ${id}`, group: 'Other' };
      if (info.hidden || info.kind) continue;
      groups.get(info.group ?? 'Other').push([id, info]);
    }
    const controls = $('#controls');
    const look = document.createElement('div');
    look.append(Object.assign(document.createElement('h3'), { textContent: 'Look' }));
    addSelect(look, SETTING_MODE, 'Mode', modeNames);
    addSelect(look, SETTING_PALETTE, 'Palette', paletteNames);
    for (const [id, info] of groups.get('Look')) addSlider(look, id, info);
    controls.append(look);
    for (const group of GROUP_ORDER.slice(1)) {
      if (!groups.get(group).length) continue;
      const column = document.createElement('div');
      column.append(Object.assign(document.createElement('h3'), { textContent: group }));
      for (const [id, info] of groups.get(group)) addSlider(column, id, info);
      controls.append(column);
    }
  }
  buildControls();

  $('#reset').addEventListener('click', () => {
    engine._preview_reset();
    state.values.clear();
    refreshers.forEach((refresh) => refresh());
    markSelected();
  });

  // ---- Drawing
  const strips = [];
  function stripContext(canvas) {
    const context = canvas.getContext('2d');
    return { context, image: context.createImageData(LED_COUNT, 1) };
  }
  const bigStrip = stripContext($('#strip'));
  const glow = stripContext($('#glow'));

  function paint(target, mode) {
    const words = new Uint32Array(engine.HEAPU8.buffer, engine._preview_pixels(mode), LED_COUNT);
    const data = target.image.data;
    for (let i = 0; i < LED_COUNT; i++) {
      const word = words[i];
      data[4 * i] = SCREEN[(word >>> 16) & 255];
      data[4 * i + 1] = SCREEN[(word >>> 24) & 255];
      data[4 * i + 2] = SCREEN[(word >>> 8) & 255];
      data[4 * i + 3] = 255;
    }
    target.context.putImageData(target.image, 0, 0);
  }

  const gallery = $('#gallery');
  modeNames.forEach((name, mode) => {
    const tile = document.createElement('div');
    tile.className = 'tile';
    tile.tabIndex = 0;
    tile.setAttribute('role', 'button');
    const label = document.createElement('span');
    label.textContent = name;
    const canvas = document.createElement('canvas');
    canvas.width = LED_COUNT;
    canvas.height = 1;
    tile.append(label, canvas);
    const choose = () => {
      setSetting(SETTING_MODE, mode);
      refreshers.forEach((refresh) => refresh());
      markSelected();
    };
    tile.addEventListener('click', choose);
    tile.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        choose();
      }
    });
    gallery.append(tile);
    strips.push({ tile, ...stripContext(canvas) });
  });

  function markSelected() {
    const mode = selectedMode();
    strips.forEach((entry, i) => entry.tile.setAttribute('aria-current', i === mode ? 'true' : 'false'));
    $('#mode-name').textContent = modeNames[mode];
  }
  $('#mode-name').textContent = modeNames[selectedMode()];
  markSelected();
  // The mode select changes the selected mode too
  $('#setting-0').addEventListener('change', markSelected);

  $('#gallery-toggle').addEventListener('change', (event) => {
    state.gallery = event.target.checked;
    engine._preview_set_gallery(state.gallery ? 1 : 0);
  });

  const bandsContext = $('#bands').getContext('2d');
  let lastBeats = 0;
  let beatAt = 0;
  let lastHud = 0;

  function frame(now) {
    const mode = selectedMode();
    paint(bigStrip, mode);
    paint(glow, mode);
    if (state.gallery) strips.forEach((entry, i) => paint(entry, i));

    const beats = engine._preview_beats();
    if (beats !== lastBeats) {
      lastBeats = beats;
      beatAt = now;
    }
    $('#beat').classList.toggle('on', now - beatAt < 120);

    if (now - lastHud > 50) {
      lastHud = now;
      $('#loudness i').style.width = `${Math.min(1, engine._preview_loudness()) * 100}%`;
      $('#hops').textContent = `${engine._preview_hops()} hops`;
      const at = engine._preview_bands() >> 2;
      const bands = engine.HEAPF32.subarray(at, at + BAND_COUNT);
      bandsContext.clearRect(0, 0, BAND_COUNT, 24);
      bandsContext.fillStyle = '#7c8cff';
      for (let b = 0; b < BAND_COUNT; b++) {
        const height = Math.round(bands[b] * 24);
        bandsContext.fillRect(b, 24 - height, 1, height);
      }
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ---- Audio: sources feed a tap, which hands the samples to the engine
  const WORKLET = `
    class Tap extends AudioWorkletProcessor {
      process(inputs) {
        const input = inputs[0];
        // With no source connected the input is empty: post silence, so the
        // engine keeps advancing and the modes fall off
        const mono = new Float32Array(input.length ? input[0].length : 128);
        for (const channel of input) for (let i = 0; i < mono.length; i++) mono[i] += channel[i];
        if (input.length > 1) for (let i = 0; i < mono.length; i++) mono[i] /= input.length;
        this.port.postMessage(mono, [mono.buffer]);
        return true;
      }
    }
    registerProcessor('tap', Tap);`;

  let graph = null;

  function feed(samples) {
    for (let offset = 0; offset < samples.length; offset += INPUT_CAPACITY) {
      const chunk = samples.subarray(offset, offset + INPUT_CAPACITY);
      engine.HEAPF32.set(chunk, inputAt());
      engine._preview_push(chunk.length);
    }
  }

  async function ensureGraph() {
    if (graph) return graph;
    const ctx = new AudioContext({ latencyHint: 'interactive' });
    await ctx.audioWorklet.addModule(URL.createObjectURL(new Blob([WORKLET], { type: 'text/javascript' })));
    const tap = new AudioWorkletNode(ctx, 'tap');
    tap.port.onmessage = (event) => feed(event.data);
    // The tap outputs silence; connecting it keeps the browser pulling it
    tap.connect(ctx.destination);
    // The browser's rate, not the firmware's: the engine starts over at it
    start(ctx.sampleRate);
    graph = { ctx, tap };
    return graph;
  }

  const SOURCES = {};
  let current = null;

  async function choose(name) {
    if (current) {
      SOURCES[current].stop();
      current = null;
    }
    note(null);
    $('#source-extra').replaceChildren();
    for (const button of document.querySelectorAll('#source-buttons button'))
      button.setAttribute('aria-pressed', String(button.dataset.source === name));
    if (!name) return;
    try {
      const g = await ensureGraph();
      await g.ctx.resume();
      setTrim(SOURCES[name].trim);
      await SOURCES[name].start(g, $('#source-extra'));
      current = name;
    } catch (error) {
      note(`${SOURCES[name].label}: ${error.message ?? error}`);
      for (const button of document.querySelectorAll('#source-buttons button'))
        button.setAttribute('aria-pressed', 'false');
    }
  }

  function setTrim(db) {
    state.trim = db;
    $('#trim').value = db;
    $('#trim-out').textContent = `${db} dB`;
    engine._preview_set_input_trim_db(db);
  }
  $('#trim').addEventListener('input', (event) => setTrim(Number(event.target.value)));
  setTrim(state.trim);

  function addSource(name, source) {
    SOURCES[name] = source;
    const button = document.createElement('button');
    button.type = 'button';
    button.dataset.source = name;
    button.textContent = source.label;
    button.setAttribute('aria-pressed', 'false');
    button.addEventListener('click', () => choose(current === name ? null : name));
    $('#source-buttons').append(button);
  }

  // A demo signal with the spread of real music, not a lone tone: 10 s of a
  // 120 BPM kick, a snare on the backbeat, hi-hats, a bass line, a chord pad
  // and a lead, all with harmonics, then 2 s of silence to watch the modes
  // fall off. Looped. A thin signal (one tone) lights a few bands and looks
  // dim on the board too
  function demoBuffer(ctx) {
    const seconds = 12;
    const rate = ctx.sampleRate;
    const buffer = ctx.createBuffer(1, Math.floor(seconds * rate), rate);
    const data = buffer.getChannelData(0);
    const TAU = 2 * Math.PI;
    let seed = 1;
    const noise = () => {
      seed ^= seed << 13;
      seed ^= seed >>> 17;
      seed ^= seed << 5;
      return ((seed >>> 0) / 4294967296) * 2 - 1;
    };
    // Harmonics 1..count with 1/h weights: a bright, saw-like tone
    const saw = (frequency, t, count) => {
      let sum = 0;
      for (let h = 1; h <= count; h++) sum += Math.sin(TAU * frequency * h * t) / h;
      return sum;
    };
    const bass = [55, 55, 82.4, 65.4];
    const chord = [220, 261.6, 329.6];
    const lead = [880, 784, 659.3, 587.3, 659.3, 784, 987.8, 784];
    for (let i = 0; i < data.length; i++) {
      const t = i / rate;
      if (t >= 10) continue;
      const beat = t % 0.5;
      const bar = Math.floor(t / 2);
      const kick = 0.55 * Math.exp(-beat / 0.05) * Math.sin(TAU * (48 + 60 * Math.exp(-beat / 0.02)) * beat);
      const backbeat = (t % 1) - 0.5;
      const snare = backbeat >= 0 ? Math.exp(-backbeat / 0.07) * (0.22 * noise() + 0.12 * Math.sin(TAU * 190 * backbeat)) : 0;
      const offBeat = (t + 0.25) % 0.5;
      const hat = (offBeat < 0.08 ? 0.16 : 0.03) * noise() * Math.exp(-offBeat / 0.02);
      const bassLine = 0.16 * saw(bass[bar % bass.length], t, 6) * (0.6 + 0.4 * Math.exp(-beat / 0.3));
      const pad = 0.035 * chord.reduce((sum, f) => sum + saw(f, t, 8), 0) * (0.7 + 0.3 * Math.sin(TAU * 0.25 * t));
      const step = Math.floor(t / 0.25);
      const noteT = t % 0.25;
      const melody = 0.07 * saw(lead[step % lead.length], t, 10) * Math.exp(-noteT / 0.18);
      data[i] = 0.75 * (kick + snare + hat + bassLine + pad + melody);
    }
    return buffer;
  }

  let demo = null;
  addSource('demo', {
    label: 'Demo signal',
    trim: -18,
    async start(g) {
      demo = g.ctx.createBufferSource();
      demo.buffer = demoBuffer(g.ctx);
      demo.loop = true;
      demo.connect(g.ctx.destination);
      demo.connect(g.tap);
      demo.start();
    },
    stop() {
      demo?.stop();
      demo?.disconnect();
      demo = null;
    },
  });

  // An audio file: played by an audio element (seeking, volume), heard and
  // tapped
  let fileNode = null;
  addSource('file', {
    label: 'Audio file',
    trim: -18,
    async start(g, extra) {
      const picker = document.createElement('input');
      picker.type = 'file';
      picker.accept = 'audio/*';
      picker.setAttribute('aria-label', 'Choose an audio file');
      const player = document.createElement('audio');
      player.controls = true;
      player.loop = true;
      player.addEventListener('error', () => note('That file could not be played.'));
      player.addEventListener('play', () => g.ctx.resume());
      picker.addEventListener('change', () => {
        const [file] = picker.files;
        if (!file) return;
        note(null);
        player.src = URL.createObjectURL(file);
        player.play().catch(() => {});
      });
      extra.append(picker, player);
      // One node per element for good: rebuilt with the element each time
      fileNode = g.ctx.createMediaElementSource(player);
      fileNode.connect(g.ctx.destination);
      fileNode.connect(g.tap);
    },
    stop() {
      fileNode?.disconnect();
      fileNode = null;
      $('#source-extra audio')?.pause();
    },
  });

  // The microphone: tapped, not heard (that would feed back). The browser's
  // own gain control, echo cancelling and noise suppression are off: they
  // would reshape the sound before the analysis
  let micStream = null;
  let micNode = null;
  addSource('mic', {
    label: 'Microphone',
    trim: 0,
    async start(g) {
      micStream = await navigator.mediaDevices.getUserMedia({
        audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1 },
      });
      micNode = g.ctx.createMediaStreamSource(micStream);
      micNode.connect(g.tap);
    },
    stop() {
      micNode?.disconnect();
      micStream?.getTracks().forEach((track) => track.stop());
      micNode = null;
      micStream = null;
    },
  });

  window.preview = { engine, SOURCES, addSource, choose, note, state };
}

main().catch((error) => {
  note(`The preview could not start: ${error.message ?? error}`);
  console.error(error);
});
