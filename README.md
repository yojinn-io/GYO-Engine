# GYO-Engine

GYO is a **Reusable C++ Game Runtime**. It turns infrastructure capabilities from SDL, operating-system APIs, and graphics APIs into reusable game mechanisms. A game supplies policy and content; optional external controllers may observe or influence the runtime only through neutral public seams.

GYO is not a monolithic editor ecosystem and is not intended to become a small Godot, Unity, or Unreal. Bounded tools are independent executables built around concrete runtime data standards; the repository still grows from demonstrated game needs rather than speculative framework completeness.

`apps/object_fps` is the current in-repository conformance vertical slice. It is used to check whether GYO's runtime skeleton is practical: the game package follows GYO input actions, asset identity/loading, render submission/device contracts, lifecycle, and typed runtime boundary directly. GYO is the standard being verified; GYO never depends on Object_FPS-specific policy or data.

See [docs/architecture.md](docs/architecture.md) for ownership and dependency rules.
The rendering pipeline, common HLSL, shader ABI, deployment and native acceptance
guide is available in [繁體中文](docs/rendering_architecture.zh-Hant.md) and
[日本語](docs/rendering_architecture.ja.md).

## Status legend

- `[implemented]`: real code that predates the current conformance milestone.
- `[this milestone]`: real, bounded code introduced or corrected while using Object_FPS to exercise the skeleton.
- `[on demand]`: an architectural direction only. The directory or type may not exist and must not be created until a concrete responsibility requires it.

The tree below deliberately retains the project's **status-labelled architecture vision**. It is not a claim that every displayed directory or feature already exists.

## Architecture vision

```text
GYO-Engine/
├─ CMakeLists.txt
├─ README.md                                      [this milestone]
├─ docs/
│  └─ architecture.md                            [this milestone]
│
├─ third_party/                                  dependency wrappers
│  ├─ sdl3/                                      [implemented]
│  ├─ sdl3_image/                                [this milestone; optional PNG decode]
│  ├─ sdl3_ttf/                                  [this milestone; optional font rasterization]
│  ├─ nlohmann_json/                             [implemented; asset catalogs]
│  ├─ ufbx/                                      [this milestone; optional FBX loader]
│  ├─ imgui/                                     [implemented; Sandbox/editor chrome]
│  └─ doctest/                                   [implemented; tests only]
│
├─ engine/                                       reusable, backend-neutral mechanisms
│  ├─ include/engine/
│  │  ├─ base/                                   [implemented]
│  │  ├─ io/                                     [implemented]
│  │  ├─ asset/                                  [implemented + this milestone]
│  │  │  ├─ catalog/                             identity/path metadata
│  │  │  ├─ core/                                records, handles, cache, lifetime
│  │  │  ├─ loading/
│  │  │  │  ├─ IAssetSource.hpp
│  │  │  │  ├─ NativeFileAssetSource.hpp         [this milestone]
│  │  │  │  ├─ LoaderRegistry.hpp
│  │  │  │  └─ AssetPipeline.hpp
│  │  │  └─ loaders/
│  │  │     ├─ FontAsset.hpp                     [this milestone; encoded font bytes]
│  │  │     ├─ FontLoader.hpp                    [implemented; bytes -> FontAsset]
│  │  │     ├─ TextureAsset.hpp                  CPU-side decoded pixels
│  │  │     └─ sdl_image/                        [this milestone; optional loader]
│  │  └─ runtime/                                [this milestone]
│  │     ├─ FrameContext.hpp
│  │     ├─ IRuntimeClient.hpp
│  │     ├─ IRuntimePort.hpp                     typed Query/Command/Event seam
│  │     ├─ RuntimeControl.hpp
│  │     └─ RuntimeLoop.hpp
│  └─ src/
│     ├─ io/                                     [implemented]
│     ├─ asset/                                  [implemented + this milestone]
│     └─ runtime/RuntimeLoop.cpp                  [this milestone]
│
├─ platform/                                     OS/window/event adapters
│  ├─ sdl/                                       [this milestone]
│  └─ win32/                                     [on demand; not created]
│
├─ input/                                        input mechanisms, not a systems/ bucket
│  ├─ include/engine/input/                      [this milestone]
│  │  ├─ PhysicalInputFrame.hpp
│  │  └─ InputActionMap.hpp
│  ├─ backend/sdl/                               [this milestone]
│  │  └─ SdlInput                                SDL events -> physical input
│  └─ tests/                                     [this milestone]
│
├─ text/                                         minimal font/text mechanism
│  ├─ include/text/                              [this milestone; backend-neutral]
│  │  ├─ ITextRasterizer.hpp                     encoded font + UTF-8 run -> RGBA8 bitmap
│  │  ├─ TextTypes.hpp                          request and owning CPU bitmap
│  │  └─ TextError.hpp
│  └─ backend/sdl_ttf/                           [this milestone; optional adapter]
│
├─ model/                                        [this milestone; CPU model/animation/skinning]
│  └─ backend/ufbx/                              [optional; FBX bytes -> ModelAsset]
├─ collision/                                    [this milestone; capsule/ray/sphere queries]
├─ render/                                       renderer contract and implementations
│  ├─ include/render/                            [this milestone; backend-neutral]
│  │  ├─ RenderQueue.hpp
│  │  ├─ IRenderDevice.hpp
│  │  ├─ RenderHandle.hpp
│  │  └─ RenderTypes.hpp
│  ├─ backend/
│  │  ├─ sdl/                                    [this milestone; clear/present adapter]
│  │  ├─ sdl_gpu/                                [this milestone; optional SDL_GPU device]
│  │  ├─ dx12/                                   [on demand; not created]
│  │  ├─ vulkan/                                 [on demand; not created]
│  │  └─ opengl/                                 [on demand; not created]
│  ├─ shaders/                                   shared HLSL ABI + built-in bundle
│  └─ tests/                                     [this milestone]
│
├─ ui/                                           [this milestone; closed JSON UI v1]
│  ├─ include/ui/                                document/codec/runtime/renderer seams
│  ├─ src/                                       layout, binding, focus, draw-list bridge
│  └─ tests/                                     codec/runtime/renderer coverage
│
├─ framework/                                    reusable game-domain policy [on demand]
│  ├─ stage/
│  ├─ combat/
│  └─ ai/
│
├─ apps/                                         composition roots and concrete games
│  ├─ runtime/                                   [this milestone] minimal clear/present app
│  ├─ sandbox/                                   [this milestone; optional ImGui demo]
│  └─ object_fps/                                [this milestone; active conformance slice]
│     ├─ include/RetroFPS/                       Object_FPS policy/domain types
│     ├─ src/                                    adapters use GYO contracts directly
│     │  └─ App/ObjectFpsUi.cpp                  game bindings/actions + C++ HUD policy
│     └─ tests/                                  headless game-policy and UI-command tests
│
├─ assets/                                       game-owned runtime content
│  ├─ common/                                    [this milestone; separately rooted shared primitives]
│  ├─ object_fps/                                [this milestone]
│  │  ├─ asset_catalog.json
│  │  ├─ data/                                   campaign/enemy/weapon definitions
│  │  ├─ fonts/                                  game-selected UI font and license
│  │  ├─ maps/                                   current conformance fixtures
│  │  └─ textures/
│  └─ <game_id>/                                 [on demand; one isolated root per game]
│
├─ tools/                                        independent, optional executables
│  └─ editor/                                    [this milestone; JSON UI authoring tool]
│
└─ tests/
   ├─ engine_tests/                              [implemented + this milestone]
   └─ backend integration tests/                 [on demand]
```

There is deliberately no `systems/` catch-all. Input belongs to `input/`, model animation to `model/`, and geometric queries to `collision/`. Future audio, full physics and navigation mechanisms enter their own modules only when implemented.

`apps/<game_id>` and `assets/<game_id>` form a pair. Object_FPS therefore uses `apps/object_fps` and `assets/object_fps`; its catalog and content do not enter a shared global game-asset bucket.

## Build the current skeleton

Requirements are CMake 3.30 or newer and a C++20 compiler. The first configure may fetch the enabled third-party dependencies into the selected build tree.
The CMake version must support the selected generator and compiler. Using a
new Visual Studio generator may require a newer CMake; a working CLion/Ninja
MSVC profile can keep its existing CMake and compiler selection.

Build the standalone runtime and tests without the Object_FPS conformance game:

```sh
cmake -S . -B build -DGYO_BUILD_RUNTIME=ON -DGYO_BUILD_OBJECT_FPS=OFF -DGYO_BUILD_SANDBOX=OFF
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Build the Object_FPS conformance slice in a separate build tree:

```sh
cmake -S . -B build-object-fps -DGYO_BUILD_OBJECT_FPS=ON -DGYO_BUILD_RUNTIME=OFF -DGYO_BUILD_SANDBOX=OFF
cmake --build build-object-fps --config Debug
ctest --test-dir build-object-fps -C Debug --output-on-failure
```

Object_FPS selects the SDL_GPU device plus the optional SDL_image PNG decoder
and SDL_ttf text rasterizer. Shared HLSL is compiled offline into platform shader
bundles: DXIL and SPIR-V on Windows, SPIR-V on Linux, and Metallib on macOS.
The runtime contains no HLSL compiler. Native shader tools are built separately
inside the build tree; macOS also requires the selected Xcode's Metal tools.

The repository currently defaults `GYO_BUILD_OBJECT_FPS` to `ON` because the MVP is the active conformance consumer. Set it to `OFF` for a core-only, standalone-runtime-only, or backend-module build.

Build switches are responsibility-based:

| Option | Default | Effect |
|---|---:|---|
| `GYO_BUILD_RUNTIME` | `OFF` | Builds the standalone SDL window/clear-present composition root. |
| `GYO_BUILD_SANDBOX` | `OFF` | Adds the optional ImGui/JSON demonstration app. |
| `GYO_BUILD_OBJECT_FPS` | `ON` | Adds the Object_FPS GYO-conformance game, its isolated assets, and required optional adapters. |
| `GYO_BUILD_UI_EDITOR` | `ON` | Adds the independent SDLRenderer/ImGui JSON UI editor target. For an editor-only graph, also set the default-on `GYO_BUILD_OBJECT_FPS=OFF`; the editor itself never links ObjectFPS, Input, Sandbox, or SDL_GPU. |
| `GYO_BUILD_SDL_GPU_BACKEND` | `OFF` | Explicitly enables the optional SDL_GPU implementation of `IRenderDevice`; Object_FPS selects it through `AUTO`. |
| `GYO_RENDER_DEVICE` | `AUTO` | Selects `AUTO`, `SDL_GPU`, or `NONE` at the composition root. |
| `GYO_GPU_DRIVER` | `AUTO` | Default runtime driver policy: `AUTO`, `D3D12`, `VULKAN`, or `METAL`. Incompatible target choices fail during configuration. |
| `GYO_SHADER_BUNDLE` | `AUTO` | Target shader formats; an explicit semicolon list such as `"DXIL;SPIRV"` limits the packaged drivers. |
| `GYO_SHADER_TOOL_EXECUTABLE` | unset | Optional absolute path to an already-built native host shader tool; required for cross compilation. |
| `GYO_MSVC_REDIST_DIR` | unset | Optional MSVC redistributable root for release packaging; otherwise discovered from the selected compiler installation. |
| `GYO_BUILD_SDL_IMAGE_LOADER` | `OFF` | Builds the SDL_image-backed runtime PNG texture loader. |
| `GYO_BUILD_SDL_TTF_ADAPTER` | `OFF` | Builds the SDL_ttf implementation of the neutral text-rasterizer contract; Object_FPS also selects it. |
| `GYO_BUILD_UFBX_LOADER` | `OFF` | Builds the optional ufbx 0.23.0 model loader; Object_FPS also selects it. Neutral Model/Collision remain available without it. |
| `BUILD_TESTING` | `ON` | Adds doctest targets registered with CTest. |

With all application and optional adapter/decoder options disabled, the backend-neutral Engine, `GYO::Text` contract, and their tests do not require SDL, SDL_image, SDL_ttf, ImGui, or Object_FPS. Optional decoders and backends are selected at the outer build/composition layer.

### Native presets and cross-platform feedback

With Ninja and a native C++20 toolchain available (an x64 MSVC developer shell
on Windows):

```sh
cmake --preset object-fps
cmake --build --preset object-fps
ctest --preset object-fps
cmake --install build/object-fps --prefix /absolute/path/to/stage
```

The `object-fps` test preset runs CPU/headless and shader tests, including offline
shader validation. GPU tests run separately on a machine with a working display
and GPU. The `core` configure/build/test presets disable games, tools and optional
adapters; `ci-windows`, `ci-linux`, and `ci-macos` select the native CI toolchains.

In CLion, keep the existing MSVC CMake profile, run **Reload CMake Project**,
then select and run the `gyo_object_fps` target. The native shader-tool build
uses the compiler and Ninja paths selected by that profile. Missing optional
redistributable files do not block development configuration or compilation.

[The Actions workflow](.github/workflows/cross-platform.yml) builds Object_FPS,
the UI editor and shader bundles on Windows x64/MSVC, Ubuntu 24.04 x64/GCC 14,
and macOS 15 ARM64/Xcode 16.4. It tests isolated deployment, uploads diagnostics
and native archives, and explicitly leaves GPU acceptance to local machines.
The macOS package targets 13.3 or newer for the game's floating-point
`std::from_chars`; the application and Metallib use the same deployment target.
Adding the workflow does not mean that a hosted run or Linux/macOS GPU test has
already passed. See the [acceptance guide](docs/rendering_architecture.zh-Hant.md#r09)
for downloadable-package checks and manual smoke commands.

Installed content lives next to the executable under `stage/bin/assets` and
`stage/bin/shaders`; non-system shared libraries use `bin` on Windows and `lib`
on Linux/macOS. Windows Release/RelWithDebInfo packages include the matching
redistributable MSVC runtime DLLs beside the executable. `GyoMsvcRuntime.cmake`
finds them relative to the selected compiler installation, independent of the
CMake release's built-in Visual Studio version list. If they are unavailable,
release installation fails with instructions to set `GYO_MSVC_REDIST_DIR` or
install the redistributables; development builds remain available. Debug CRT
is not packaged, and Windows 10+ provides UCRT. End users do not need Visual Studio,
shader compilers or an SDK. CI checks imported VC DLLs and Unix library paths
so a developer machine's installed libraries cannot hide incomplete packaging.
`--validate-package` verifies the installed assets and shader
bundles without creating a window or GPU. `--gpu-driver d3d12|vulkan|metal`
forces a runtime driver and reports failure if that driver or its shader format
is unavailable.

Current local Windows evidence for this rendering iteration: the optional-free
core configuration passes 6/6 CTest entries, and the native shader host passes
3/3 tests in the CLion profile (including dependency/rebuild behavior and native
toolchain forwarding). Automatic host-tool build
also succeeds. The full RelWithDebInfo build passes 12/12 CPU/headless tests.
Both explicit D3D12/DXIL and Vulkan/SPIR-V pass the engine numeric GPU smoke
and all eight Object_FPS GPU cases. Isolated package validation, missing-content
rejection and app-local CRT dependency checks pass. The `NONE` configuration
also builds and runs headless/render tests without any shader compiler.
GitHub Actions and native Linux/macOS execution remain unverified; detailed
evidence and log paths are recorded in the rendering guides.

The existing CLion MSVC profile also passes with its bundled CMake 4.1.2,
Ninja and VS18 cl 14.51: Object_FPS and UI editor build, four regression tests,
three D3D12/DXIL GPU smoke tests, release installation and isolated package validation.

## Responsibility at a glance

```text
Infrastructure provides capability.
GYO provides reusable game mechanisms.
Game provides policy and content.
```

Current examples:

- SDL reports keys, buttons, pointer motion, and focus; `SdlInput` translates them into a GYO physical frame; `InputActionMap` produces named actions and axes; Object_FPS decides how those actions move, aim, fire, reload, pause, or navigate menus.
- `NativeFileAssetSource` reads catalog-resolved runtime files; the optional SDL_image loader decodes image bytes into a CPU-side `TextureAsset`; `AssetManager` owns identity, handles, cache/lifetime, and loader dispatch; the selected render device alone creates GPU textures.
- `FontLoader` keeps encoded font bytes in a backend-neutral `FontAsset`; `ITextRasterizer` converts a borrowed font byte span and one UTF-8 text run into an owning CPU-side RGBA8 `TextBitmap`; the optional SDL_ttf adapter implements that contract without exposing `TTF_Font`, `SDL_Surface`, or `SDL_Texture`. The render device uploads the bitmap and the existing sprite submission path draws it.
- Object_FPS projects its immutable game snapshot into GYO `RenderQueue` submissions. `Renderer` owns camera/matrix preparation and world/viewmodel/post/HUD passes; `IRenderDevice` owns opaque GPU handles and executes the prepared frame. `ShaderLibrary` keeps immutable CPU shader artifacts independently of each device's resources. Native SDL_GPU, D3D12, Vulkan and Metal handles remain private to the adapter.

`GYO::Ui` is now a deliberately closed v1 standard: JSON codec/validation, RectTransform layout, typed bindings/actions, focus/hit testing, buttons, sliders, fixed-step lists and an ordered draw list. `GYO::UiRenderer` resolves font/texture assets, keeps a bounded whole-run text cache, and submits only Overlay sprites. Rich text, shaping, localization, Flex/Grid, scripts and a widget/plugin ABI remain out of scope. See [docs/ui_toolchain.md](docs/ui_toolchain.md).

## Object_FPS is a conformance consumer

The migration is not a source-to-source KamataEngine port. Its rule is:

```text
Object_FPS game policy/content
        |
        v
GYO lifecycle + input actions + assets + render contracts + runtime port
        |
        v
selected SDL platform / SDL_GPU / SDL_image infrastructure
```

When the old game exposed a reusable need, the neutral mechanism was added to GYO first and Object_FPS then consumed it. Object_FPS does not carry a parallel input framework, resource manager, renderer contract, or application loop, and its game-specific names and state remain outside GYO Core.

The current maps and CSV files are conformance fixtures, not hard-coded engine knowledge. Asset IDs are catalogued under the `object_fps.*` namespace and the campaign data determines which content is used.

### Visible MVP flow

Object_FPS owns the content, authored layout data, binding values and action consequences for:

- MainMenu: Start Game, Controls, and Quit.
- Controls: visible input instructions and Back.
- Pause: Gamma, Exposure, Resume, Main Menu, and Quit over the game view.
- Results: campaign outcome, room results, and return to Main Menu.
- Playing HUD: crosshair, HP, magazine/reserve ammunition, reload state, and current stage status.

### Jumping and Mark-23

- **Space** performs one grounded jump. Feet position, hit capsule, camera and
  shot origin move together; holding Space does not repeat. Defaults are 0.6 m
  height and 18 m/s² gravity. Wall/enemy grid blocking also applies in the air.
- **Left mouse** fires the semi-automatic Mark-23; **R** reloads; **H** holsters
  or draws. The new campaign starts with Draw. Actions use native animation
  times: Shoot 0.333 s, Reload 3.733 s, Draw 0.833 s and Hide 0.367 s. Magazine
  and reserve start at 12/48; ammo transfers only when reload completes.
- The model includes animated hands, slide and magazine, using three diffuse
  textures. Its independent right-lower camera preserves self-occlusion while
  nearby walls do not clip the weapon. It participates in scene exposure/gamma.
- `assets/object_fps/data/mark23_viewmodel.json` defines model/material/clip IDs,
  the fixed Idle anchor, repeat sampler, camera-relative offset, rotation, scale
  and weapon FOV. The calibrated offset is `(0.12, -0.18, 0.55)` m with a 55°
  vertical FOV. It also defines the barrel muzzle in `main_j` local coordinates.
  The shared loader derives per-weapon shot geometry from Shoot at time zero
  using that placement; the 55° weapon / 60° world FOV bridge preserves its
  screen position. Edit, build to stage assets, and restart to recalibrate both
  placement and muzzle. CSV action timings remain unchanged.
- Hits resolve against the reticle before the shot's new recoil. The cosmetic
  tracer starts at the muzzle in the rendered recoil frame, after older
  projectiles advance; world obstruction checks still apply and the tracer
  cannot deal damage again. GPU projection checks pass at three aspect ratios; see
  [the calibration note](docs/dev_logs/2026_09_15_model_muzzle_calibration.md).
- Doors use the actual `assets/common/white1x1.png` via a separate catalog.
  Deployment includes both asset roots and does not allow catalog path escapes.

See [the iteration note](docs/dev_logs/2026_09_15_mark23_jump.md) for the model
analysis, ownership boundaries, validation and current limitations.

The four non-playing screens load once from `assets/object_fps/ui/screens.json`; there is no compiled fallback, live link or hot reload. `UiRuntime` owns selection, focus, pointer capture and hit testing. Object_FPS maps opaque action ids to its typed commands, while Playing HUD policy remains C++ and emits the same `UiDrawList`. GYO still does not know what an Object_FPS menu action means.

Executable smoke paths cover different boundaries:

- `object_fps.smoke` enters Playing and verifies a world frame can be submitted and presented.
- `object_fps.menu_smoke` uses ordinary MainMenu startup and fails if the first menu frame has no visible UI submission.
- `object_fps.viewmodel_smoke` presents each Mark-23 action at its start, middle and end. Use `--viewmodel-smoke-test --capture-dir <directory>` to save diagnostic scene frames, optionally with `--preview-4x3` or `--preview-21x9`.
- `object_fps.reload_smoke` submits the complete Reload at 60 Hz, including the brief wrist-motion intervals between original frames. Use `--reload-smoke-test --capture-dir <directory>` for a full diagnostic sequence. See [the reload and muzzle fix](docs/dev_logs/2026_09_15_reload_muzzle_fix.md).
- `object_fps.muzzle_smoke`, `object_fps.muzzle_smoke_4x3`, and `object_fps.muzzle_smoke_21x9` compare GPU muzzle marker positions in seven camera cases. Use `--muzzle-smoke-test --capture-dir <directory>` for 16:9, adding `--preview-4x3` or `--preview-21x9` for the other ratios. The saved model review image marks the calibrated barrel opening in cyan; [the calibration note](docs/dev_logs/2026_09_15_model_muzzle_calibration.md) records commands, measured error and evidence.

Muzzle calibration validation completed all 18 integrated checks, including a
rebuilt headless test rerun with 1,169 assertions. The three GPU aspect-ratio
checks measured 0.0000 pixel marker-pair error and at most 0.3823 pixel error
against the theoretical projection.

Headless `ObjectFpsUi` tests parse the real JSON asset and verify Japanese labels, all four canvases, Results bindings, C++ HUD composition, action-id behavior after JSON reordering, and same-frame slider updates without SDL_ttf or a render backend.

## Runtime Boundary and Weaver

The caller-neutral boundary is:

```text
Game / Test / Debug Console / Replay / AI / Weaver
                         |
                         v
               GYO Runtime Boundary
              Query / Command / Event
                         |
                         v
                 GYO mechanisms
```

`IRuntimePort<Snapshot, Command, Event>` is the current minimum typed seam: a controller can query the current immutable snapshot, submit a legal typed intent, and inspect typed facts emitted by that runtime. `Query()` and `Events()` are borrowed views valid only until the runtime advances again; a controller copies anything it must retain. Concrete payloads still belong to the runtime/game domain that defines their meaning; the interface shape, ownership rule, and lack of backend pointers are GYO standards.

This is not a complete world/entity API, remote protocol, command scheduler, or generic event bus. Those abstractions remain deferred until a current consumer establishes their responsibility.

Weaver is not implemented here. It is an optional external high-level runtime and may eventually use the same public mechanisms as a game, test, or debug tool. GYO must build and run ordinary games without Weaver, and GYO Core must never depend on Weaver.

## Growth rule

A feature is added where its responsibility belongs. A new text backend, Render3D, Physics3D, Navigation, or external-controller integration should normally add a module, backend, or adapter rather than rewrite unrelated Asset, Input, Render, Runtime, or game code.

Do not pre-create editor ecosystems, node trees, universal ECS layers, visual scripting, plugin frameworks, full RenderGraphs, complete physics engines, large DI containers, generic managers, or empty interfaces. An abstraction enters the repository only when current code gives it a concrete responsibility and a real caller.

## Naming

| Kind | Rule | Example |
|---|---|---|
| Directory | lowercase with `_` when separation is needed | `render/backend/sdl_gpu` |
| C++ namespace | PascalCase or engine-rooted | `Engine::Asset` |
| C++ source/header | PascalCase | `RuntimeLoop.hpp` |
| Per-game asset root | lowercase game identifier | `assets/object_fps/` |
| Data file | lowercase | `asset_catalog.json`, `levels.csv` |
