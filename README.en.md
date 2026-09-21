[日本語](README.md) | [繁體中文](README.zh-Hant.md) | [English](README.en.md)

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
├─ README.md                                      Japanese entry; en/zh-Hant companions
├─ config/engine/projects.csv                     app selection; human metadata only
├─ docs/
│  └─ architecture.md                            [this milestone]
│
├─ third_party/                                  dependency wrappers
│  ├─ sdl3/                                      [implemented]
│  ├─ sdl3_image/                                [this milestone; optional PNG decode]
│  ├─ sdl3_ttf/                                  [this milestone; optional font rasterization]
│  ├─ nlohmann_json/                             [implemented; asset catalogs]
│  ├─ ufbx/                                      [this milestone; optional FBX loader]
│  ├─ imgui/                                     [implemented; editor chrome]
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

Requirements are CMake 3.30 or newer and a C++20 compiler. The first configure
may fetch enabled third-party sources into the selected build tree. New Visual
Studio generators may require newer CMake; an existing CLion/Ninja MSVC profile
can retain its compiler and CMake selection. Native presets require Ninja and,
on Windows, an x64 MSVC developer shell.

[config/engine/projects.csv](config/engine/projects.csv) is the single app
selection list for local builds and CI. Its columns are
`name,description,version,enabled,windows,linux,macos`. A name resolves to
`apps/<name>` from the repository root. Description and version are personal
notes: they never change build logic, package names, or Release tags. Boolean
columns accept `1/0` and `true/false`; an app builds only when both `enabled` and
its **target platform** are enabled. See the
[build design](docs/architecture.md#build-project-management) for the complete
CSV and dependency contract.

Build the apps selected by the CSV together with the independent UI editor:

```sh
cmake --preset dev
cmake --build --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage
```

Build backend-neutral modules and tests without apps, tools or optional adapters:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

To narrow a build, pass `-DGYO_APPS=object_fps` or quote a semicolon list such as
`"-DGYO_APPS=object_fps;runtime"`. This is a subset selector: first enable the
requested entries and target platforms in the CSV. It cannot bypass a disabled
entry. For an editor-only build:

```sh
cmake -S . -B build-ui-editor -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build build-ui-editor --config Debug --target gyo_ui_editor
```

Adding or removing an app using existing engine capabilities changes only its
code directory, private assets and the CSV. Its `CMakeLists.txt` declares requirements, targets,
assets and ordinary installation. Quality and package acceptance are separate adapters. Common modules and CI do not keep
an app-name list. CSV edits trigger reconfiguration and recompute requirements.
The former per-app build options are removed; existing caches receive migration
instructions. Use a fresh build tree or remove the reported legacy cache entries,
then use the CSV and `GYO_APPS`.

New apps can start from scratch or by manually copying an existing
`apps/<name>` plus its private `assets/<name>`. The [app creation guide](docs/creating_apps.md)
provides both procedures. `gyo_app_project()` derives identity from the CSV/directory;
`OUT_TARGET` handles avoid hard-coded target names, and private `gyo/AppConfig.hpp`
provides deployment paths without source roots. Shared helpers handle normal target
settings and content deployment; internal asset/shader IDs and gameplay remain app-owned.
There is no project creation/clone tool or additional project registry.

Ordinary `dev` builds default to `BUILD_TESTING=OFF` and
`GYO_ENABLE_PACKAGING=OFF`. Product code must configure, build, run and install
with all testing, CI and acceptance files absent. `MAIN` installs its executable
and runtime dependencies without any startup check or package manifest. Quality
and project/CI management consume product targets through optional adapters:
`tests/Tests.cmake` and `packaging/Package.cmake`. Enable quality separately:

```sh
cmake --preset test
cmake --build --preset test
ctest --preset test
```

| Option | Default | Effect |
|---|---:|---|
| `GYO_APPS` | `AUTO` | Select CSV-enabled apps for the target; an empty value selects no apps; a semicolon list narrows the selection. |
| `GYO_BUILD_UI_EDITOR` | `ON` | Build the independent SDLRenderer/ImGui UI authoring executable; `GYO_APPS=` selects an editor-only graph. |
| `GYO_BUILD_SDL_GPU_BACKEND` | `OFF` | Explicitly request the optional SDL_GPU implementation of `IRenderDevice`; an app can also require it. |
| `GYO_RENDER_DEVICE` | `AUTO` | `AUTO`, `SDL_GPU`, or `NONE`; selecting a GPU-requiring app with `NONE` is a configuration error. |
| `GYO_GPU_DRIVER` | `AUTO` | Runtime driver policy: `AUTO`, `D3D12`, `VULKAN`, or `METAL`; incompatible target choices fail configuration. |
| `GYO_SHADER_BUNDLE` | `AUTO` | Target shader formats; a quoted semicolon list such as `"DXIL;SPIRV"` restricts packaged drivers. |
| `GYO_SHADER_TOOL_EXECUTABLE` | unset | Absolute path to a native host shader tool; required for cross compilation. |
| `GYO_MSVC_REDIST_DIR` | unset | Optional MSVC redistributable root; otherwise discovered from the selected compiler installation. |
| `GYO_BUILD_SDL_IMAGE_LOADER` | `OFF` | Explicitly request the optional PNG loader. |
| `GYO_BUILD_SDL_TTF_ADAPTER` | `OFF` | Explicitly request the optional SDL_ttf text rasterizer. |
| `GYO_BUILD_UFBX_LOADER` | `OFF` | Explicitly request the optional ufbx model loader; neutral Model/Collision remain independent. |
| `BUILD_TESTING` | `OFF` | Opt into external quality adapters, doctest and CTest; the `test` preset enables it. |
| `GYO_ENABLE_PACKAGING` | `OFF` | Opt into package manifests and strict installed acceptance; CI enables it explicitly. |

App requirements are combined with explicit adapter choices without rewriting
user cache options. Neutral Engine, Input, Model, Collision, Text, Render and UI
remain independent of any concrete game. With apps, tools and optional adapters
disabled, their build does not require SDL, SDL_image, SDL_ttf or ImGui.

### Rendering and native development

GPU apps use shared HLSL compiled offline to DXIL and SPIR-V on Windows, SPIR-V
on Linux and Metallib on macOS. The runtime has no HLSL compiler. Native shader
tools build separately inside the build tree; macOS requires the selected
Xcode's Metal tools. The `test` preset runs CPU/headless and shader tests;
GPU tests require a usable display and GPU and run separately. `ci-windows`,
`ci-linux`, and `ci-macos` select native CI toolchains.

In CLion, retain the MSVC profile, reload CMake, and select the desired app target.
The child shader-tool build uses that profile's compiler and Ninja paths.
Missing optional redistributables do not block development configuration or
compilation. macOS uses deployment target 13.3 for both applications and
Metallib, set before `project()`. A deployment target does not establish SDK
library API availability: game CSV decimal parsing uses an explicit decimal
grammar and the classic C++ locale with float range checks because Xcode 16.4
lacks floating-point `std::from_chars`.

Ubuntu needs `libxtst-dev` for SDL XTest support. The offline host tool disables
SDL video and dialogs and enables `SDL_UNIX_CONSOLE_BUILD=ON`, avoiding missing
Cocoa symbols on macOS and deliberate no-video rejection on Linux. These host
tool settings do not change the application's SDL video configuration.

### CI, packages and releases

[GitHub Actions](.github/workflows/cross-platform.yml) always builds and tests
engine modules and the UI editor on Windows x64/MSVC, Linux x64/GCC 14 and macOS
ARM64/Xcode 16.4. Independently, the fixed source commit's CSV generates an
app × platform matrix. Each combination has an isolated build tree and install
stage containing that app and its required dependencies; Editor is not in app
packages. Apps without GPU/shader requirements do not run those steps.

CI explicitly enables quality and packaging. Each app selected for that packaging
operation supplies a package adapter with an installed startup test. The common runner uses
the generated `share/gyo/apps/<app>/manifest.json` and executes from outside the
installed package. App-specific acceptance commands select quick/release,
platform and GPU requirements. Failure, timeout or missing required evidence
blocks packaging. Object_FPS retains startup, gameplay, missing-content and
Linux Lavapipe checks; see its [acceptance guide](apps/object_fps/docs/acceptance.ja.md).
A platform with no selected app still runs engine/tool checks. An entirely empty
list is valid for normal CI; Prepare Release rejects it during preflight.

Each selected combination produces `gyo-<name>-<platform>.tar.gz` plus
`.tar.gz.sha256`, with one `gyo-<name>` archive root. Release derives the expected
set from the same source commit's CSV and checks app identity, platform, source
SHA, release profile, required files, complete acceptance evidence and checksums.
Quick evidence cannot be substituted for release evidence. Archive safety,
relative library paths and dependency checks remain release gates.

Use **Actions → Prepare Release → Run workflow**, select a source branch, enter
a version such as `v1.0.1`, and select prerelease when appropriate. This fixes a
source SHA and runs the full profile. After every required check succeeds, the
write-enabled job creates the tag and Draft Release with the computed package
set. Review the Draft link, notes and assets in the Actions Summary, then press
**Publish release**. Publication and tag pushes do not trigger another build.
CSV versions are personal records and do not supply this Release version.

Ordinary push/PR/manual quick runs produce Actions artifacts only. Same-version
Draft retries preserve verified assets and user notes, require any existing tag
to match the exact source SHA, and refuse to replace a public version. Rerun the
original execution to retain its SHA; a new dispatch may select a newer commit.
Merge workflows into the default branch to expose the manual entry; old runs
retain their original definitions. Full GUI and recovery instructions:
[繁體中文](docs/releasing.zh-Hant.md) · [日本語](docs/releasing.ja.md).

App-owned content is installed relative to the executable; non-system libraries
use `bin` on Windows and `lib` on Linux/macOS. Windows Release/RelWithDebInfo
installs matching app-local MSVC runtime DLLs discovered by
`cmake/GyoMsvcRuntime.cmake`. Set `GYO_MSVC_REDIST_DIR` when discovery cannot find
them: release installation fails until they are available, while development
builds remain possible. Debug CRT is not distributed; Windows 10+ provides UCRT.
End users need no compiler or SDK. CI checks VC imports and Unix linking so
libraries on a developer machine cannot hide an incomplete package.

### Previous validation evidence

The validation counts in the rendering guides and dated development notes are
historical evidence for their stated commits and build configurations, not
proof that this project-management refactor or new matrix passed hosted CI.
[Rendering status](docs/rendering_architecture.ja.md#r10) preserves the Windows
CPU/GPU/deployment results, the earlier Linux Lavapipe quick result, the macOS
Metallib/CPU/deployment result, failed hosted runs and unverified physical GPU
acceptance. The old GPU-disabled Object_FPS configuration is historical; the
current declared GPU requirement rejects `GYO_RENDER_DEVICE=NONE` for that app.
Use the new build's logs and Actions Summary for its actual validation status.

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

The conformance integration is not a source-to-source KamataEngine port. Its rule is:

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

The `test` preset or `GYO_ENABLE_PACKAGING=ON` attaches optional acceptance diagnostics. Ordinary `dev` builds retain startup health checks and interactive preview; other smoke/validation flags report that diagnostics are unavailable. Their sources live under `apps/object_fps/tests/diagnostics/` and are not product build inputs.

- `--startup-smoke-test` verifies real installed content loading without creating a window or GPU. Quick and release CI execute this installed binary from an unrelated working directory on all three platforms.
- `--headless-smoke-test` also verifies gameplay startup, jumping, pause/resume, shooting and reload timing without a window or GPU; release CI runs this heavier scenario.
- `object_fps.smoke` enters Playing and verifies a world frame can be submitted and presented.
- `object_fps.menu_smoke` uses ordinary MainMenu startup and fails if the first menu frame has no visible UI submission.
- `object_fps.viewmodel_smoke` presents each Mark-23 action at its start, middle and end. Use `--viewmodel-smoke-test --capture-dir <directory>` to save diagnostic scene frames, optionally with `--preview-4x3` or `--preview-21x9`.
- `object_fps.reload_smoke` submits the complete Reload at 60 Hz, including the brief wrist-motion intervals between original frames. Use `--reload-smoke-test --capture-dir <directory>` for a full diagnostic sequence. See [the reload and muzzle fix](docs/dev_logs/2026_09_15_reload_muzzle_fix.md).
- `object_fps.muzzle_smoke`, `object_fps.muzzle_smoke_4x3`, and `object_fps.muzzle_smoke_21x9` compare GPU muzzle marker positions in seven camera cases. Use `--muzzle-smoke-test --capture-dir <directory>` for 16:9, adding `--preview-4x3` or `--preview-21x9` for the other ratios. The saved model review image marks the calibrated barrel opening in cyan; [the calibration note](docs/dev_logs/2026_09_15_model_muzzle_calibration.md) records commands, measured error and evidence.

The historical muzzle-calibration validation completed all 18 integrated checks, including a
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
