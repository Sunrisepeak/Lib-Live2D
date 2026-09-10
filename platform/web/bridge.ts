import { CubismFramework } from '@cubism/live2dcubismframework';
import { CubismModelSettingJson } from '@cubism/cubismmodelsettingjson';
import { CubismUserModel } from '@cubism/model/cubismusermodel';
import { CubismMatrix44 } from '@cubism/math/cubismmatrix44';
import { CubismEyeBlink } from '@cubism/effect/cubismeyeblink';
import { CubismBreath, BreathParameterData } from '@cubism/effect/cubismbreath';
import { CubismShaderManager_WebGL } from '@cubism/rendering/cubismshader_webgl';

type Asset = {
  entry: string; files: Record<string, ArrayBuffer>; manifest: any;
  images: ImageBitmap[]; ready: boolean; error: string; released: boolean; acquired: boolean; info?: any;
};
let assetCount = 0;
let pendingShaderDisposals = 0;

function disposeRuntimeIfIdle() {
  if (assetCount === 0 && pendingShaderDisposals === 0) {
    CubismFramework.dispose();
    CubismFramework.cleanUp();
  }
}

function retireContext(gl: WebGLRenderingContext) {
  const manager = CubismShaderManager_WebGL.getInstance();
  const shader = manager.getShader(gl);
  ++pendingShaderDisposals;
  const finish = () => {
    // The SDK's asynchronous shader loader must finish before its context is destroyed.
    if (shader?._isShaderLoading) { setTimeout(finish, 10); return; }
    if (shader) {
      shader._shaderSets = shader._shaderSets.filter(set => set?.shaderProgram && gl.isProgram(set.shaderProgram));
      shader.releaseShaderProgram();
      shader._shaderSets.length = 0;
    }
    // Cubism Web 5 R5 has no public per-context removal API; detach its retained registry entry.
    const registry = manager as unknown as { _shaderMap: Map<WebGLRenderingContext, unknown> };
    registry._shaderMap.delete(gl);
    gl.getExtension('WEBGL_lose_context')?.loseContext();
    --pendingShaderDisposals;
    disposeRuntimeIfIdle();
  };
  finish();
}

class PackagePathError extends Error {}

function attempt(fn: () => any) {
  try { return { value: fn(), error: '' }; }
  catch (error) { return { value: null, error: String(error), invalidPath: error instanceof PackagePathError }; }
}

function resolve(base: string, path: string): string {
  if (typeof path !== 'string') throw Error('Expected a string model dependency');
  if (!path || /^[\\/]/.test(path) || /[:\\\0]/.test(path)) throw new PackagePathError('Invalid package path');
  const parts: string[] = [];
  for (const part of `${base}/${path}`.split('/')) {
    if (part === '..') { if (!parts.length) throw new PackagePathError('Package path escapes its root'); parts.pop(); }
    else if (part && part !== '.') parts.push(part);
  }
  if (!parts.length) throw new PackagePathError('Empty package path');
  return parts.join('/');
}

function manifest(bytes: ArrayBuffer, entry: string) {
  const json = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes));
  const refs = json.FileReferences;
  if (json.Version !== 3 || !refs || !Array.isArray(refs.Textures) || !refs.Textures.length) {
    throw Error('Expected a Cubism model3 manifest');
  }
  const base = entry.includes('/') ? entry.slice(0, entry.lastIndexOf('/')) : '';
  const dependencies = new Set<string>();
  const file = (path: string) => { const normalized = resolve(base, path); dependencies.add(normalized); return normalized; };
  const result = { moc: file(refs.Moc), textures: refs.Textures.map(file), motions: [] as any[],
                   expressions: [] as any[], dependencies: [] as string[], json };
  for (const [group, motions] of Object.entries(refs.Motions || {})) {
    if (!Array.isArray(motions)) throw Error('Invalid motion group');
    motions.forEach((motion, index) => result.motions.push({ group, index, path: file(motion.File) }));
  }
  const names = new Set<string>();
  for (const expression of refs.Expressions || []) {
    if (typeof expression.Name !== 'string' || names.has(expression.Name)) throw Error('Invalid expression name');
    names.add(expression.Name);
    result.expressions.push({ name: expression.Name, path: file(expression.File) });
  }
  for (const key of ['Physics', 'Pose', 'DisplayInfo', 'UserData']) if (refs[key]) file(refs[key]);
  result.dependencies = [...dependencies];
  return result;
}

class Model extends CubismUserModel {
  private setting: CubismModelSettingJson;
  private current: string | null = null;
  private handle: any;
  private finished: { playback: string; reason: number }[] = [];
  private evaluated: number[] | null = null;
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGLRenderingContext | null = null;
  private textures: WebGLTexture[] = [];
  private shaderDeadline = 0;
  private disposed = false;

  constructor(private asset: Asset, render: boolean) {
    super();
    try {
      const entry = asset.files[asset.entry];
      this.setting = new CubismModelSettingJson(entry, entry.byteLength);
      this.loadModel(asset.files[asset.manifest.moc], true);
      if (!this._model) throw Error('Cubism rejected the moc data');
      const layout = new Map<string, number>();
      this.setting.getLayoutMap(layout);
      this._modelMatrix.setupFromLayout(layout);
      const base = asset.entry.includes('/') ? asset.entry.slice(0, asset.entry.lastIndexOf('/')) : '';
      const refs = asset.manifest.json.FileReferences;
      if (refs.Physics) { const data = asset.files[resolve(base, refs.Physics)]; this.loadPhysics(data, data.byteLength); }
      if (refs.Pose) { const data = asset.files[resolve(base, refs.Pose)]; this.loadPose(data, data.byteLength); }
      if (this.setting.getEyeBlinkParameterCount()) this._eyeBlink = CubismEyeBlink.create(this.setting);
      const breathId = CubismFramework.getIdManager().getId('ParamBreath');
      for (let i = 0; i < this._model.getParameterCount(); ++i) {
        if (this._model.getParameterId(i) === breathId) {
          this._breath = CubismBreath.create();
          const inputs = new Array<BreathParameterData>();
          inputs.push(new BreathParameterData(breathId, 0.5, 0.5, 3.2345, 1));
          this._breath.setParameters(inputs);
        }
      }
      this._model.saveParameters();
      if (render) {
        if (typeof VideoFrame === 'undefined') throw Error('WebCodecs VideoFrame is unavailable');
        this.canvas = document.createElement('canvas');
        this.gl = this.canvas.getContext('webgl', { alpha: true, premultipliedAlpha: true, preserveDrawingBuffer: true });
        if (!this.gl) throw Error('WebGL is unavailable');
        this.createRenderer(1, 1);
        this.getRenderer().startUp(this.gl);
        this.getRenderer().setIsPremultipliedAlpha(true);
        this.getRenderer().loadShaders('live2d-shaders/');
        this.shaderDeadline = performance.now() + 10000;
        asset.images.forEach((image, index) => {
          const gl = this.gl!;
          const texture = gl.createTexture();
          if (!texture) throw Error('Cannot allocate a WebGL texture');
          this.textures.push(texture);
          gl.bindTexture(gl.TEXTURE_2D, texture);
          gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, true);
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
          this.getRenderer().bindTexture(index, texture);
        });
      }
    } catch (error) { this.dispose(); throw error; }
  }

  info() {
    const model = this._model;
    return { width: model.getCanvasWidth() * model.getPixelsPerUnit(),
      height: model.getCanvasHeight() * model.getPixelsPerUnit(),
      parameters: Array.from({ length: model.getParameterCount() }, (_, i) => ({
        id: model.getParameterId(i).getString(), minimum: model.getParameterMinimumValue(i),
        maximum: model.getParameterMaximumValue(i), default_value: model.getParameterDefaultValue(i) })),
      hit_areas: Array.from({ length: this.setting.getHitAreasCount() }, (_, i) => this.setting.getHitAreaName(i)) };
  }

  prepare() {
    if (this.gl!.isContextLost()) throw Error('WebGL context was lost');
    if (CubismShaderManager_WebGL.getInstance().getShader(this.gl!)._isShaderLoaded) return 0;
    if (performance.now() > this.shaderDeadline) throw Error('Cubism WebGL shaders failed to load');
    return 2;
  }

  play(id: string, group: string, index: number, priority: number, force: boolean, loop: boolean) {
    const file = this.asset.manifest.motions.find((motion: any) => motion.group === group && motion.index === index);
    if (!file) return 3;
    if (!force && !this._motionManager.reserveMotion(priority)) return 5;
    const bytes = this.asset.files[file.path];
    const motion = this.loadMotion(bytes, bytes.byteLength, file.path, undefined, undefined, this.setting, group, index, true);
    if (!motion) { this._motionManager.setReservePriority(0); return 7; }
    motion.setLoop(loop);
    const blink = new Array(), lip = new Array();
    for (let i = 0; i < this.setting.getEyeBlinkParameterCount(); ++i) blink.push(this.setting.getEyeBlinkParameterId(i));
    for (let i = 0; i < this.setting.getLipSyncParameterCount(); ++i) lip.push(this.setting.getLipSyncParameterId(i));
    motion.setEffectIds(blink, lip);
    if (this.current) this.finished.push({ playback: this.current, reason: 1 });
    this.handle = this._motionManager.startMotionPriority(motion, true, priority);
    this.current = id;
    this.evaluated = null;
    return 0;
  }

  stop() {
    this._motionManager.stopAllMotions();
    if (this.current) this.finished.push({ playback: this.current, reason: 2 });
    this.current = null;
    return 0;
  }
  expression(name: string) {
    const file = this.asset.manifest.expressions.find((expression: any) => expression.name === name);
    if (!file) return 4;
    const data = this.asset.files[file.path];
    const expression = this.loadExpression(data, data.byteLength, name);
    if (!expression) return 7;
    this._expressionManager.startMotion(expression, true);
    this.evaluated = null;
    return 0;
  }
  clear() { this._expressionManager.stopAllMotions(); this.evaluated = null; return 0; }
  takeFinished() { return this.finished.splice(0); }
  animating() { return !this._motionManager.isFinished() || !this._expressionManager.isFinished(); }

  render(width: number, height: number, delta: number, options: any) {
    const gl = this.gl!, canvas = this.canvas!, model = this._model;
    if (gl.isContextLost()) throw Error('WebGL context was lost');
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width; canvas.height = height; this.setRenderTargetSize(width, height);
    }
    if (delta > 0 || !this.evaluated) {
      model.loadParameters();
      const motion = this._motionManager.updateMotion(model, delta);
      model.saveParameters();
      if (!motion && options.auto_blink && this._eyeBlink) this._eyeBlink.updateParameters(model, delta);
      this._expressionManager.updateMotion(model, delta);
      if (options.auto_breath && this._breath) this._breath.updateParameters(model, delta);
      if (this._physics && delta > 0) this._physics.evaluate(model, delta);
      if (this._pose) this._pose.updateParameters(model, delta);
      this.evaluated = Array.from({ length: model.getParameterCount() }, (_, i) => model.getParameterValueByIndex(i));
      if (this.current && this._motionManager.isFinishedByHandle(this.handle)) {
        this.finished.push({ playback: this.current, reason: 0 }); this.current = null;
      }
    }
    this.evaluated.forEach((value, i) => model.setParameterValueByIndex(i, value));
    for (const input of options.parameters) {
      const i = model.getParameterIndex(CubismFramework.getIdManager().getId(input.id));
      const current = model.getParameterValueByIndex(i);
      const value = input.blend === 1 ? current + input.value * input.weight
                                     : current * (1 - input.weight) + input.value * input.weight;
      model.setParameterValueByIndex(i, Math.min(model.getParameterMaximumValue(i), Math.max(model.getParameterMinimumValue(i), value)));
    }
    model.update();
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, width, height);
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    const projection = new CubismMatrix44();
    projection.scale(model.getCanvasHeight() / model.getCanvasWidth(), 1);
    projection.multiplyByMatrix(this._modelMatrix);
    this.getRenderer().setMvpMatrix(projection);
    this.getRenderer().setRenderState(null, [0, 0, width, height]);
    this.getRenderer().drawModel();
    if (gl.getError() !== gl.NO_ERROR) throw Error('WebGL rendering failed');
    return new VideoFrame(canvas, { timestamp: Math.round(performance.now() * 1000), alpha: 'keep' });
  }

  hit(x: number, y: number) {
    x = (2 * x - 1) * this._model.getCanvasWidth() / this._model.getCanvasHeight();
    y = 1 - 2 * y;
    for (let i = 0; i < this.setting.getHitAreasCount(); ++i) {
      if (this.isHit(this.setting.getHitAreaId(i), x, y)) return this.setting.getHitAreaName(i);
    }
    return '';
  }
  dispose() {
    if (this.disposed) return;
    this.disposed = true;
    this.release();
    this.setting?.release();
    if (this.gl) for (const texture of this.textures) this.gl.deleteTexture(texture);
    this.textures = [];
    if (this.gl) retireContext(this.gl);
  }
}

function prepare(files: Record<string, ArrayBuffer>, entry: string, maxDimension: number, maxBytes: number) {
  const asset: Asset = { files, entry, manifest: null, images: [], ready: false, error: '', released: false, acquired: false };
  (async () => {
    try {
      asset.manifest = manifest(files[entry], entry);
      const deadline = performance.now() + 5000;
      for (;;) {
        if (asset.released) return;
        try { if (Live2DCubismCore.Version.csmGetVersion() > 0) break; } catch {}
        if (performance.now() >= deadline) throw Error('Cubism Web Core did not initialize');
        await new Promise(resolve => setTimeout(resolve, 10));
      }
      if (assetCount === 0 && pendingShaderDisposals === 0) {
        if (CubismFramework.isStarted()) throw Error('Another Cubism Web provider is already initialized');
        CubismFramework.startUp(); CubismFramework.initialize();
      }
      ++assetCount;
      asset.acquired = true;
      let total = 0;
      for (const path of asset.manifest.textures) {
        const bytes = new Uint8Array(files[path]);
        const header = new DataView(bytes.buffer);
        if (bytes.byteLength < 24 || header.getUint32(0) !== 0x89504e47 || header.getUint32(4) !== 0x0d0a1a0a) throw Error('Expected a PNG texture');
        const width = header.getUint32(16), height = header.getUint32(20);
        total += width * height * 4;
        if (!width || !height || width > maxDimension || height > maxDimension || total > maxBytes) throw Error('Texture budget exceeded');
        const image = await createImageBitmap(new Blob([files[path]], { type: 'image/png' }),
                                              { premultiplyAlpha: 'premultiply', colorSpaceConversion: 'none' });
        if (asset.released) { image.close(); return; }
        asset.images.push(image);
      }
      const model = new Model(asset, false);
      try { asset.info = model.info(); } finally { model.dispose(); }
    } catch (error) { asset.error = String(error); }
    asset.ready = true;
  })();
  return asset;
}

(globalThis as any).HuxerLive2D = {
  manifest: (bytes: ArrayBuffer, entry: string) => attempt(() => manifest(bytes, entry)),
  prepare,
  create: (asset: Asset) => attempt(() => new Model(asset, true)),
  invoke: (model: Model, method: string, args: any[]) => attempt(() => (model as any)[method](...args)),
  release: (asset: Asset) => {
    if (asset.released) return;
    asset.released = true;
    asset.images.forEach(image => image.close());
    asset.images = [];
    if (asset.acquired) { --assetCount; disposeRuntimeIfIdle(); }
  },
};
