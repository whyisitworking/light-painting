// Inlines the engine, the script and the style into the page template:
// OUTDIR/preview.html, one file with everything in it.
// Usage: node build-page.mjs OUTDIR (OUTDIR holds engine.js)
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const out = process.argv[2];
if (!out) throw new Error('usage: node build-page.mjs OUTDIR');

const read = (...parts) => readFileSync(join(...parts), 'utf8');
// A script must not contain the end of its own element
const inlineScript = (text) => text.replaceAll('</script', '<\\/script');

// Replacer functions: the engine holds "$" sequences a replacement string
// would interpret
const html = read(here, 'page', 'index.html')
  .replace('/*STYLE*/', () => read(here, 'page', 'style.css'))
  .replace('/*ENGINE*/', () => inlineScript(read(out, 'engine.js')))
  .replace('/*APP*/', () => inlineScript(read(here, 'page', 'app.js')));

writeFileSync(join(out, 'preview.html'), html);
console.log(`page: ${join(out, 'preview.html')}`);
