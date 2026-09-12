# Lib-Live2D on mcpp

The library is an [mcpp](https://github.com/mcpp-community/mcpp) package beside its CMake build. Nothing here changes the CMake build; delete `mcpp.toml`, `build.mcpp`, `mcpp/`, `modules/` and `examples/preview/{mcpp.toml,build.mcpp,src/main.cpp}` and the repository is CMake-only again.

## Layout

| Path | What it is |
|---|---|
| `mcpp.toml` | The library package `huxerui.live2d`: six platforms, the per-platform sources, the payloads (`xim:cubism-sdk-native`, `xim:cubism-sdk-web`, `xim:esbuild`, `xim:glew`). |
| `build.mcpp` | Its build program: sources through `huxerui.rules`, the Cubism headers, the embedded GLSL/HLSL, and on the Web the esbuild bundle of `platform/web/bridge.ts` plus the WebGL shaders deployed as `live2d-shaders/`. |
| `modules/live2d.cppm` | The module interface. `import huxerui.live2d;` names the same entities as `#include <huxerui/live2d.h>`. |
| `mcpp/cubism/` | The package `huxerui.live2d-cubism`: the Cubism SDK for Native compiled from its payload — no sources of its own, `build.mcpp` selects the Framework's for the row's renderer, links the Core slice, compiles GLEW on Linux and the 477 Metal shader libraries on macOS/iOS. Separate because the Framework's Objective-C++ is MRC and this library's is ARC. |
| `examples/preview/` | The preview: `mcpp.toml`, `build.mcpp`, `src/main.cpp` beside the CMake project, compiling the same `src/app.cpp` and `resources/`. |

## Use

```sh
mcpp build                                        # the library, this machine
cd examples/preview
mcpp run                                          # the preview, this machine
mcpp pack --format appimage                       # Linux; msi on Windows, app on macOS
mcpp pack --target wasm32-emscripten --format web
mcpp run  --target aarch64-ios-sim --format app
mcpp run  --target x86_64-linux-android --format apk
```

Every payload — the Cubism SDKs, esbuild, GLEW, the NDK, emsdk, the JDK — is installed by mcpp on first use.

In an application:

```toml
[dependencies]
huxerui.live2d = { git = "https://github.com/HuxerUI/Lib-Live2D.git", tag = "v0.1.0" }
```

```cpp
import huxerui;
import huxerui.live2d;
```

No `live2d_configure_app()`, no SDK path, no `npm install`; the shaders are deployed by the library and every `mcpp pack --format` carries them.

## Not on mcpp yet

The library's own string catalogue (`resources/strings`), and runnable macOS/iOS bundles (mcpp does not stage Mach-O programs yet, so `--format app` carries the executable only). The CMake build serves both. On Windows the Cubism D3D11 renderer is compiled from a clang-adjusted copy (`mcpp/cubism/build.mcpp` says how).
