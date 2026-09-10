import { build } from '../.cache/web-tools/node_modules/esbuild/lib/main.js';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const project = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const [sdk, output] = process.argv.slice(2);
if (!sdk || !output) throw Error('Usage: node tools/build_web.mjs <Cubism Web SDK> <output.js>');
await build({
  entryPoints: [path.join(project, 'platform/web/bridge.ts')],
  bundle: true, format: 'iife', target: 'es2020', outfile: output,
  alias: { '@cubism': path.resolve(sdk, 'Framework/src') },
});
