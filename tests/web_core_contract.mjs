import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import path from 'node:path';
import { build } from '../.cache/web-tools/node_modules/esbuild/lib/main.js';

// Expose bridge internals only in this in-memory test bundle; the shipped bridge has no test API.
const source = fs.readFileSync('platform/web/bridge.ts', 'utf8') +
    '\n(globalThis as any).bridgeTest = { Model, retireContext, CubismShaderManager_WebGL };';
const bundle = await build({
  stdin: { contents: source, resolveDir: path.resolve('platform/web'), loader: 'ts' },
  bundle: true, format: 'iife', target: 'es2020', write: false,
  alias: { '@cubism': path.resolve('.cache/cubism/CubismSdkForWeb-5-r.5/Framework/src') },
});

// Image decoding and GPU calls are replaced here; browser pixel rendering is a separate verification.
const context = vm.createContext({
  console, Uint8Array, Uint16Array, Uint32Array, Int8Array, Int16Array, Int32Array,
  Float32Array, Float64Array, ArrayBuffer, DataView, TextDecoder, Blob, performance,
  setTimeout, clearTimeout, atob, VideoFrame: class {}, createImageBitmap: async () => ({ close() {} }),
});
vm.runInContext(fs.readFileSync('.cache/cubism/CubismSdkForWeb-5-r.5/Core/live2dcubismcore.js', 'utf8'), context);
vm.runInContext(bundle.outputFiles[0].text, context);
const bridge = context.HuxerLive2D;
const root = '.cache/cubism/CubismSdkForNative-5-r.5/Samples/Resources/Wanko';
const entry = 'Wanko.model3.json';
const bytes = path => {
  const data = fs.readFileSync(`${root}/${path}`);
  return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
};
const parsed = bridge.manifest(bytes(entry), entry);
assert.equal(parsed.error, '');
assert.equal(parsed.value.motions.length, 5);
const malformed = bridge.manifest(new TextEncoder().encode('{}').buffer, entry);
assert.notEqual(malformed.error, '');
assert.equal(malformed.invalidPath, false);
for (const moc of ['../outside.moc3', '/absolute.moc3', 'C:/model.moc3', 'bad\\path.moc3', 'bad\0path', '']) {
  const invalid = bridge.manifest(new TextEncoder().encode(JSON.stringify({
    Version: 3, FileReferences: { Moc: moc, Textures: ['texture.png'] },
  })).buffer, entry);
  assert.equal(invalid.invalidPath, true, `Path error category was lost: ${moc}`);
}
const missingMoc = bridge.manifest(new TextEncoder().encode(JSON.stringify({
  Version: 3, FileReferences: { Textures: ['texture.png'] },
})).buffer, entry);
assert.equal(missingMoc.invalidPath, false);
const files = Object.fromEntries([entry, ...parsed.value.dependencies].map(path => [path, bytes(path)]));
const prepared = bridge.prepare(files, entry, 8192, 256 * 1024 * 1024);
const deadline = performance.now() + 10000;
while (!prepared.ready && performance.now() < deadline) await new Promise(resolve => setTimeout(resolve, 10));
assert.equal(prepared.ready, true);
assert.equal(prepared.error, '');
assert.equal(prepared.info.width, 1200);
assert.equal(prepared.info.height, 1200);
assert.equal(prepared.info.parameters.length, 25);
const { Model, retireContext, CubismShaderManager_WebGL } = context.bridgeTest;
const instance = Object.create(Model.prototype);
let elapsed = 0;
Object.assign(instance, {
  gl: { isContextLost: () => false, bindFramebuffer() {}, viewport() {}, clearColor() {}, clear() {},
        getError: () => 0, NO_ERROR: 0 },
  canvas: { width: 100, height: 100 }, evaluated: [0], current: null,
  _motionManager: { updateMotion: (_, delta) => { elapsed += delta; return true; } },
  _expressionManager: { updateMotion() {} },
  _model: { loadParameters() {}, saveParameters() {}, getParameterCount: () => 1,
            getParameterValueByIndex: () => 0, setParameterValueByIndex() {}, update() {},
            getCanvasWidth: () => 1, getCanvasHeight: () => 1 },
  _modelMatrix: { getArray: () => new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]) },
  getRenderer: () => ({ setMvpMatrix() {}, setRenderState() {}, drawModel() {} }),
});
for (let frame = 0; frame < 60; ++frame) {
  instance.render(100, 100, 8 / 60, { auto_blink: false, auto_breath: false, parameters: [] });
}
assert.ok(Math.abs(elapsed - 8) < 1e-9, 'Eightfold playback lost model time');

const manager = CubismShaderManager_WebGL.getInstance();
const survivor = { getExtension: () => ({ loseContext() {} }) };
manager.setGlContext(survivor);
for (let i = 0; i < 3; ++i) {
  let lost = false;
  const gl = { getExtension: () => ({ loseContext() { lost = true; } }) };
  manager.setGlContext(gl);
  const shader = manager.getShader(gl);
  shader._isShaderLoading = true;
  retireContext(gl);
  assert.equal(manager.getShader(gl), shader, 'Loading shaders were detached too early');
  assert.equal(lost, false);
  shader._isShaderLoading = false;
  const disposalDeadline = performance.now() + 1000;
  while (!lost && performance.now() < disposalDeadline) await new Promise(resolve => setTimeout(resolve, 10));
  assert.equal(lost, true, 'Finished shader context was not retired');
  assert.equal(manager.getShader(gl), undefined, 'Retired context remains registered');
  assert.ok(manager.getShader(survivor), 'Retiring one context removed a live context');
}
retireContext(survivor);
assert.equal(manager.getShader(survivor), undefined);
bridge.release(prepared);
bridge.release(prepared);
console.log('Web Core metadata, path errors, playback timing and deferred context release passed; no pixel rendering');
