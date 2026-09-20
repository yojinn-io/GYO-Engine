# GYO Architecture

## 1. Mission

GYO is a **Reusable C++ Game Runtime** built above SDL, SDL_image, SDL_ttf, SDL_mixer, operating-system APIs, and graphics APIs such as SDL Renderer, SDL_GPU, DX12, Vulkan, and OpenGL.

Infrastructure supplies capabilities. GYO converts those capabilities into reusable mechanisms that a game can use. A concrete game remains responsible for policy and content.

```text
Infrastructure provides capability.
GYO provides reusable game mechanisms.
Game provides policy and content.
```

GYO is intentionally not a universal editor ecosystem. It does not pre-commit to a node tree, universal ECS, visual scripting, plugin framework, full RenderGraph, PBR stack, terrain system, complete physics engine, large dependency-injection framework, or generic manager hierarchy.

## 2. Layering and dependency direction

The source dependency direction is:

```text
Concrete Game / Application composition root
                    |
                    v
          GYO public mechanisms
 Runtime / Input / Asset / Text / Render contracts
                    |
                    v
          selected outer adapters
 SDL platform / SDL input / SDL_ttf / renderer
                    |
                    v
        SDL / OS / graphics APIs
```

An optional external controller has a separate, one-way relationship:

```text
Game / Test / Debug Console / Replay / AI / Weaver
                         |
                         v
               GYO Runtime Boundary
                         |
                         v
                  GYO mechanisms
```

Normative dependency rules:

- GYO Core must not depend on a concrete game, including Object_FPS.
- GYO Core must not depend on Weaver or know which external controller is calling it.
- GYO Core must not depend on a concrete graphics backend.
- A backend implements an engine-facing contract; the application composition root selects and connects it.
- `platform/` owns host/window/event integration, not graphics implementations.
- Asset code must not create renderer or GPU resources.
- Public input, asset, text, render, and runtime-facing types must not expose SDL, SDL_ttf, Vulkan, DX12, OpenGL, or native OS pointers.
- Optional games, decoders, backends, and future modules must be removable without forcing unrelated modules to change.

There is no architectural `systems/` layer. “Runs every frame” describes execution frequency, not ownership. Each capability belongs to a responsibility-based module such as `input/`, `audio/`, `animation/`, `collision/`, or `navigation/` when that responsibility is actually implemented.

## 3. GYO standards and the Object_FPS conformance slice

Object_FPS is not a library from which GYO copies an application framework. It is the current vertical slice used to reveal missing or misplaced GYO mechanisms.

The conformance rule is:

1. Keep Object_FPS gameplay rules, campaign state, and content in `apps/object_fps` and `assets/object_fps`.
2. When the game needs a reusable runtime mechanism, define the smallest caller-neutral contract in the responsible GYO module.
3. Make Object_FPS consume that GYO contract directly.
4. Do not preserve a parallel KamataEngine input, asset, rendering, or main-loop layer merely to make the old source layout compile.
5. Do not add Object_FPS names, campaign assumptions, or game policy to GYO Core.

The standard is therefore GYO's: `IRuntimeClient` defines frame participation, `InputActionMap` defines action/axis evaluation, `AssetId`/`AssetHandle` and `AssetManager` define runtime asset ownership, `ITextRasterizer`/`TextBitmap` define neutral font raster output, `RenderQueue`/`IRenderDevice` define rendering submission, and `IRuntimePort` defines the typed observation/intent seam. Object_FPS supplies only the domain payload and policy needed to use those mechanisms.

Repository ownership follows the same rule:

```text
apps/object_fps/       Object_FPS code and composition
assets/object_fps/     Object_FPS catalog and runtime content
assets/common/         explicitly shared primitives with their own catalog/root

apps/<game_id>/        another game's code
assets/<game_id>/      that game's isolated runtime content
```

There is no shared global bucket where every game's stages, textures, or data acquire accidental cross-game ownership.

## 4. Mechanism versus policy

| Infrastructure capability | GYO mechanism | Game policy/content |
|---|---|---|
| SDL key/button/pointer events | physical input snapshot, named action/axis mapping | what Move, Fire, Reload, or Pause does |
| Filesystem and image decoder | asset source, ID, handle, catalog, cache/lifetime, loader dispatch | which texture or data asset belongs to a stage/enemy/weapon |
| Font bytes and SDL_ttf | encoded `FontAsset`, neutral text-raster request/bitmap contract | which strings, sizes, rectangles, colors, alignment, selection, and actions a screen uses |
| SDL_GPU or another graphics API | opaque render handles, resource creation, render queue execution | which world surfaces, enemies, projectiles, and overlays are submitted |
| Host clock and event pump | deterministic runtime lifecycle | state transition and gameplay update policy |
| FBX decoder | owning ModelAsset, baked clip sampling and CPU skinning | weapon clip mapping, timing, material AssetIds and placement |
| Geometric intersection math | world-positioned capsule, ray and swept-sphere queries | grid blocking, grounded state, jump gravity and attack rules |

An abstraction is admitted only when current code gives it a concrete responsibility and a real caller. “Might be useful later” is not sufficient.

## 5. Current bounded milestone

This exploration milestone builds a small but connected runtime skeleton:

- Existing `base/`, `io/`, and asset identity/cache/loading mechanisms remain in place.
- `NativeFileAssetSource` supplies catalog-resolved runtime bytes from the native filesystem.
- The optional SDL_image loader decodes PNG bytes to a CPU-side RGBA `TextureAsset`; it does not create GPU resources.
- `FontLoader` retains engine-ready TTF/OTF bytes in a backend-neutral `FontAsset` without creating a native font object.
- `GYO::Text` defines the synchronous `TextRasterRequest`/owning RGBA8 `TextBitmap` contract; the optional SDL_ttf adapter implements it without exposing SDL or SDL_ttf types.
- `RuntimeLoop` owns deterministic frame lifecycle order.
- `IRuntimePort<Snapshot, Command, Event>` supplies a minimal typed Query/Command/Event seam.
- `SdlPlatform` owns SDL process/window/event lifecycle.
- GYO input owns physical-frame and action-map semantics; `SdlInput` is only the SDL event adapter.
- GYO render owns opaque handles, primitive mesh data, `RenderQueue`, and `IRenderDevice`; concrete SDL rendering stays under `render/backend/`.
- A neutral `Renderer` prepares mesh/sprite/3D passes and delegates GPU commands to an optional SDL_GPU device. Offline common HLSL bundles support target-specific DXIL, SPIR-V and Metallib without exposing native handles or shader compilers to game-facing contracts.
- `GYO::Model` supplies skeletal clip sampling and CPU skinning; the optional `GYO::AssetUfbx` adapter converts FBX bytes to this owning format.
- `GYO::Collision` supplies capsule, ray/AABB and swept-sphere primitives. Object_FPS owns its flat-floor jumping and grid collision policy.
- `apps/object_fps` and `assets/object_fps` exercise those mechanisms as the active, separately removable concrete game vertical slice, including game-owned screen/HUD policy projected through the neutral Text and Render seams.
- `apps/runtime` remains a small standalone SDL clear/present composition root, while `apps/sandbox` keeps optional ImGui demonstration concerns separate.

`RuntimeLoop` calls an injected `IRuntimeClient` in this order:

```text
ProcessEvents(frame) -> Update(frame) -> Render(frame)
```

`FrameContext` carries the frame index and delta time. `RuntimeControl::Stop` terminates at the phase that requests it.

This milestone includes bounded text rasterization, mesh/sprite presentation, skeletal animation and primitive collision queries. General scene management, audio playback, animation graphs/blending, full physics, navigation, generalized world/entity queries, remote control, and Weaver remain outside the implemented set.

## 6. Responsibility map

Status terms used below are `implemented`, `this milestone`, and `on demand`.

### Base (`implemented`)

```yaml
Module: Engine Base
Owns:
  - fundamental Result, Error, and Span value types
Does:
  - provide small dependency-light primitives to other GYO modules
Depends On:
  - C++ standard library
Must Not Depend On:
  - Asset, Runtime, Platform, Input, Text, Render, Game, or Weaver
```

### IO (`implemented`)

```yaml
Module: Engine IO
Owns:
  - path, URI, stream, filesystem, mount, and whole-file IO mechanisms
Does:
  - expose engine-facing file access without asset or gameplay policy
Depends On:
  - Base
  - native filesystem implementation details behind its IO surface
Must Not Depend On:
  - asset types, text rasterization, render resources, scenes, concrete games, or Weaver
```

### Asset identity, catalog, cache, and dispatch (`implemented`)

```yaml
Module: Engine Asset
Owns:
  - AssetId and AssetHandle semantics
  - catalog and path resolution
  - runtime records, cache, lifetime policy, and statistics
  - loader registration and dispatch
Does:
  - locate engine-ready assets
  - request bytes through an IAssetSource
  - retain CPU-side decoded/deserialized objects
Depends On:
  - Base
  - IO-level mechanisms
  - loader contracts
Must Not Depend On:
  - renderer backends or GPU/native graphics types
  - development import tools
  - gameplay policy, Object_FPS, or Weaver
```

### Native runtime asset source (`this milestone`)

```yaml
Module: NativeFileAssetSource
Owns:
  - reading an already-resolved native file path into bytes
Does:
  - implement IAssetSource for shipped/runtime filesystem content
Depends On:
  - Asset source contract
  - native file IO
Must Not Depend On:
  - catalog selection, loader selection, cache policy, render, Game, or Weaver
```

### SDL_image texture loader (`this milestone`, optional)

```yaml
Module: GYO::AssetSdlImage
Owns:
  - SDL_image-backed decoding of supported runtime image bytes
Does:
  - produce CPU-side RGBA TextureAsset values
Depends On:
  - GYO Asset loader contract
  - SDL and SDL_image
Must Not Depend On:
  - IRenderDevice or a concrete render backend
  - GPU resource creation
  - Object_FPS policy or Weaver
```

Only PNG is enabled for the current fixture set. Broader codec support is not implied.

### Encoded font asset (`this milestone`)

```yaml
Module: FontLoader / FontAsset
Owns:
  - engine-ready encoded TTF/OTF bytes in the runtime asset cache
Does:
  - load non-empty font bytes through the ordinary Asset loader contract
  - keep native font-library objects out of AssetManager records
Depends On:
  - GYO Asset loader and Font AssetType contracts
Must Not Depend On:
  - SDL_ttf, a render device, UI layout, concrete games, or Weaver
Must Not Do:
  - glyph rasterization, text layout, GPU upload, or screen policy
```

### Neutral text raster mechanism (`this milestone`)

```yaml
Module: GYO::Text
Owns:
  - TextRasterRequest { UTF-8 run, point size }
  - owning straight-alpha RGBA8 TextBitmap values
  - the ITextRasterizer boundary
Does:
  - define how encoded font bytes are synchronously converted into a CPU bitmap
  - keep raster output consumable by any renderer through neutral pixel data
Depends On:
  - Engine Base Result/Error values
  - backend-neutral encoded font bytes supplied by the caller
Must Not Depend On:
  - SDL, SDL_ttf, SDL_GPU, a concrete render backend, Object_FPS, or Weaver
Must Not Own:
  - game strings, layout rectangles, color, selection, hit testing, or actions
```

The current request is intentionally one complete UTF-8 run at one point size. It is a real, bounded mechanism, not a claim of a universal font-layout or retained-text system.

### Closed data-driven UI standard (`this milestone`)

```yaml
Module: GYO::Ui + GYO::UiRenderer
Owns:
  - gyo.ui JSON v1 codec/validation and canonical serialization
  - RectTransform evaluation, ordered draw/clip/hit-test traversal
  - typed binding resolution, focus, pointer capture, buttons and sliders
  - bounded whole-run font texture and image-asset resolution for UiDrawList
Does:
  - emit opaque typed actions without executing game behavior
  - submit every UI image, quad and text run as CompositeLayer::Overlay
Depends On:
  - GYO Engine assets, neutral Text and Render mechanisms
Must Not Depend On:
  - Object_FPS, a concrete game screen, ImGui, SDL, or editor policy
Must Not Become:
  - a scripting host, custom-widget ABI, Flex/Grid engine, animation system,
    rich-text/shaping/localization framework, or plugin ecosystem
```

JSON colors are sRGB `#RRGGBBAA` and are decoded to linear RGB without
transforming alpha. Runtime Render colors use linear RGB and straight alpha.
See `docs/ui_toolchain.md` for the complete v1 boundary and editor workflow.

### SDL_ttf text raster adapter (`this milestone`, optional)

```yaml
Module: GYO::TextBackendSDLTTF
Owns:
  - SDL_ttf initialization and shutdown pairing
  - temporary SDL IO stream, TTF font, and surface lifetimes during Rasterize
  - conversion of SDL_ttf output to the neutral TextBitmap contract
Does:
  - implement ITextRasterizer from borrowed encoded font bytes
Depends On:
  - GYO::Text
  - SDL and SDL_ttf infrastructure
Must Not Depend On:
  - IRenderDevice, SDL_GPU, AssetManager policy, Object_FPS UI policy, or Weaver
Must Not Expose:
  - TTF_Font, SDL_Surface, SDL_Texture, SDL_GPUTexture, or other native handles
```

SDL_ttf is selected only when this optional adapter or the Object_FPS conformance application requires it. GYO Core and `GYO::Text` remain usable without SDL_ttf.

### Runtime lifecycle (`this milestone`)

```yaml
Module: RuntimeLoop / IRuntimeClient
Owns:
  - per-frame phase order
  - frame index and delta-time production
  - Continue/Stop lifecycle propagation
Does:
  - call ProcessEvents, Update, and Render on an injected client
Depends On:
  - small engine runtime value types
  - C++ steady clock
Must Not Depend On:
  - SDL polling, a renderer backend, AssetManager, concrete game policy, or Weaver
```

`IRuntimeClient` is a lifecycle port, not an all-services interface. It must not grow into a service locator.

### Typed runtime port (`this milestone`)

```yaml
Module: IRuntimePort<Snapshot, Command, Event>
Owns:
  - caller-neutral shape for Query, Submit, and Events
Does:
  - expose an immutable typed snapshot
  - accept a typed intent for implementation-controlled execution
  - expose typed facts emitted by the runtime
Depends On:
  - caller/runtime-supplied payload types
  - C++ span
Must Not Depend On:
  - backend pointers or native graphics/input types
  - a universal world schema or GenericEvent payload
  - caller identity, Object_FPS assumptions, or Weaver
```

The concrete runtime owns validation, queuing, and the lifecycle point at which submitted commands execute. `IRuntimePort` does not bypass game/runtime legality rules. `Query()` and `Events()` return borrowed current-frame views: callers must copy retained data before the next runtime advance, which may replace the snapshot and clear the event span.

### Input action mechanism (`this milestone`)

```yaml
Module: GYO::Input
Owns:
  - backend-neutral keys/buttons/pointer state for one frame
  - stable action and axis IDs
  - bindings and action-frame evaluation
Does:
  - translate physical state into named actions and axes
Depends On:
  - C++ standard library
Must Not Depend On:
  - SDL
  - Object_FPS action consequences
  - render, assets, scenes, or Weaver
```

### SDL input adapter (`this milestone`)

```yaml
Module: GYO::InputBackendSDL
Owns:
  - translation from SDL events to a PhysicalInputFrame
  - SDL relative-pointer mode and focus-edge handling
Does:
  - feed GYO input state without defining game actions
Depends On:
  - GYO::Input
  - SDL
Must Not Depend On:
  - Object_FPS controllers or gameplay state
  - renderer policy, AssetManager, or Weaver
```

SDL events may cross between concrete SDL adapters at the composition edge, but do not enter the neutral runtime port or game-policy interfaces.

### Model and skeletal animation (`this milestone`)

`GYO::Model` owns CPU `ModelAsset` data, node hierarchy, material names and linear
base colors, mesh
parts, inverse binds, four normalized skin weights per vertex, TRS clip tracks,
pose sampling and linear blend skinning. Model math uses column-major matrices
and column vectors in metres, +Y up and +Z forward. Render conversion is explicit;
no backend matrix convention leaks into Model. UV seam vertices retain the
weights of their source control vertex.
UVs use a top-left origin and retain values outside [0,1]; material presentation
chooses wrap/clamp sampling. Clamping or wrapping each vertex during import
would change interpolation across texture tile boundaries.

`GYO::AssetUfbx` is an optional `IAssetLoader` adapter pinned to ufbx v0.23.0.
It parses bytes supplied by AssetManager, normalizes coordinates/units and FBX
geometry/bind transforms, and bakes named animation clips to neutral tracks.
Already sampled animation at or above ufbx's 19.5 Hz threshold keeps its authored
poses; sparse nonlinear curves may be resampled at 60 Hz. Playback interpolates
quaternion poses. Forcing Euler subframe resampling on already-baked animation
can introduce branch flips even when adjacent authored orientations are close.
Importer IO is disabled: source texture/cache paths never bypass catalog roots.
The importer keeps the four largest skin influences and normalizes them; this
is an approximation when a source vertex has more than four influences.
Public model/animation interfaces
contain no ufbx, SDL or GPU types. Core Engine does not depend on Model or ufbx.

Loaded models are shared immutable assets. Each presentation instance owns its
pose, reusable skinning output and render handles. Sampling takes an explicit
time and does not advance a clock. Object_FPS owns clip selection, action timing,
material AssetIds and its fixed Idle-derived weapon anchor. GPU skinning,
animation graphs, crossfades, retargeting and PBR remain deferred.

### Collision primitives (`this milestone`)

`GYO::Collision` owns world-positioned upright capsules, ray/capsule and ray/AABB
queries, and swept-sphere/capsule intersection. It depends only on the standard
library and knows no map, character controller, weapon or damage rule.
Object_FPS `CombatCollision` adapts those queries to grid walls and targets.

### Neutral render mechanism (`this milestone`)

`Renderer` owns scene preparation independently of device execution.
`MeshLayer::World` is the default. `MeshLayer::ViewModel` uses its own camera and
fresh depth while retaining the world scene color. Execution order is world
meshes and Scene sprites, ViewModel meshes, scene exposure/gamma, then Overlay
sprites. Viewmodel placement/FOV is application policy; Render does not know
what a weapon or an equip action means.
Exterior triangle winding is preserved by model import and primitive generation.
Under GYO's +Z left-handed projection it reaches the render target as clockwise;
the SDL_GPU mesh pipeline uses that front-face convention. Opaque surfaces cull
backs, Sky culls fronts, and double-sided material rendering is explicit.

`IRenderDevice::UpdateMeshVertices` consumes vertices synchronously for an
existing handle with fixed vertex count and index topology. Backends may report
UnsupportedOperation; SDL_GPU implements it using cycled vertex/staging buffers
so an earlier GPU frame is not overwritten. Pose evaluation belongs to Model,
not Render. CPU skinning does not recreate GPU meshes each frame.

`Renderer` additionally offers an opt-in one-frame scene readback through the
device's neutral texture readback operation for
diagnostics. It returns owning sRGB8 RGBA pixels after World/Scene/ViewModel and
before exposure/gamma/Overlay; it is not a final swapchain screenshot. A fence
wait occurs only on requested capture frames, never on ordinary presentation.

```yaml
Module: GYO::Render
Owns:
  - opaque generational MeshHandle and TextureHandle values
  - backend-neutral mesh/image descriptions
  - FrameDescription, camera, mesh, and sprite submissions
  - MaterialDesc with a namespaced shader program ID
  - RenderQueue validation and primitive mesh generation
  - ShaderLibrary owning immutable CPU artifacts and validated bundle metadata
  - Renderer camera/matrix preparation, ordered passes and device-bound pipeline resources
  - IRenderDevice resource, acquired-frame and prepared-frame execution contract
Does:
  - describe what a game-facing presentation layer submits
  - separate CPU assets/submission from backend resource implementation
Depends On:
  - Engine Base Result/Error values and source/resolver contracts for shader bytes
  - C++ standard library
Must Not Depend On:
  - Text rasterizers or layout policy
  - SDL, SDL_ttf, SDL_GPU, DX12, Vulkan, OpenGL, or native resource types
  - concrete game content or Weaver
```

This is a bounded frame queue, not a full RenderGraph, material framework, or generic `RenderManager`.

### SDL Renderer clear/present adapter (`this milestone`)

```yaml
Module: GYO::RenderBackendSDL
Owns:
  - SDL_Renderer lifecycle for the minimal runtime/sandbox path
  - clear and present
Does:
  - support the existing small SDL composition roots
Depends On:
  - concrete SDL platform window
  - SDL
Must Not Depend On:
  - Object_FPS game policy
  - generalized scene/asset ownership
  - Weaver
```

This adapter is distinct from the neutral `IRenderDevice` contract and from the SDL_GPU backend.

### SDL_GPU render backend (`this milestone`, optional)

```yaml
Module: GYO::RenderBackendSDLGPU
Owns:
  - SDL_GPU device, swapchain, native pipelines, uploads and resource synchronization
  - translation of opaque GYO handles and PreparedFrame commands
Does:
  - implement IRenderDevice for the current bounded raster pipeline
  - consume already-compiled shader artifacts for the selected native driver
Depends On:
  - GYO::Render
  - concrete SDL platform/window integration
  - SDL_GPU
Must Not Depend On:
  - Object_FPS state or asset catalog policy
  - SDL_ttf or text-layout policy
  - an HLSL compiler or game-specific shader source paths
  - Weaver
Must Not Expose:
  - SDL_GPU, D3D12, command-buffer, descriptor, or shader handles through GYO APIs
```

Built-in HLSL belongs to `render/shaders/builtin/`, its shared data declarations
to `render/shaders/common/RasterAbi.hlsli`, and game-specific HLSL to the game.
`tools/shader_pipeline` is a separate native host toolchain pinned to DXC,
SDL_shadercross and SPIRV-Cross versions. It compiles, reflects and packages
shader programs at build time; the runtime no longer embeds HLSL or invokes
`D3DCompile`. macOS finishes offline MSL compilation with the selected Xcode's
Metal tools. Shader sources, interface versions, resource counts and byte sizes
must match the `gyo.raster.v1` contract.
The shared HLSL declares logical resources; its compilation profile supplies
SDL-specific physical bindings. Source-stage entrypoints are explicit bundle
spec fields (default `main`), while runtime manifests retain the generated
artifact's real entrypoint, including any Metal translation rename.

`ShaderLibrary` atomically appends namespaced bundles and retains immutable CPU
bytes. `Renderer` resolves a `MaterialDesc` program ID for the device's usable
format and creates device-local pipelines. Game bundles do not add game IDs to
the adapter. CPU shader ownership never includes GPU handles. Invalid or missing
programs, resources and formats fail explicitly instead of choosing a fallback
material that could hide content errors.

The frame contract is `AcquireFrame` -> `SubmitFrame` or `AbandonFrame`. An
acquired token is consumed once; minimization can return no frame. Renderer
owns ordered World/Scene, ViewModel, scene-post and Overlay passes and their
resizable targets. The adapter owns native resource lifetime and in-flight
synchronization. Renderer resources must be reset before their device dies.

`SpriteSubmission::sourceUv` uses a visual top-left origin. The SDL_GPU path converts that rectangle once through `MakeSpriteUvTransform`, including atlas sub-rectangles, because its shared XY quad reaches screen-top at `v=1`. Text rasterization and texture upload preserve row order and must not compensate for backend sprite orientation.

The same adapter routes through SDL_GPU's D3D12, Vulkan or Metal driver. This is
a portable source/API contract, not a stable cross-compiler plugin ABI. See the
paired rendering guides in [繁體中文](rendering_architecture.zh-Hant.md) and
[日本語](rendering_architecture.ja.md) for pipeline fundamentals, exact bundle
ownership, build switches and the distinction between CI and GPU acceptance.

### SDL platform adapter (`this milestone`)

```yaml
Module: GYO::PlatformSDL
Owns:
  - SDL process/video lifecycle needed by applications
  - SDL window
  - event polling and close-request detection
Does:
  - translate host lifecycle into a small platform-facing surface
Depends On:
  - Base Error/Result and RuntimeControl
  - SDL
Must Not Depend On:
  - render submission, AssetManager, game state, or Weaver
```

`NativeWindow()` is an explicit concrete-adapter escape hatch used only while composing SDL-based adapters. It is not part of GYO's neutral runtime or game-facing render contracts.

### Object_FPS code and content (`this milestone`, removable conformance consumer)

```yaml
Module: apps/object_fps + assets/object_fps
Owns:
  - FPS campaign, world, player, weapon, enemy, collision, and presentation policy
  - Object_FPS-specific Snapshot, Command, and Event payloads
  - Object_FPS catalog, CSV definitions, maps, textures, and selected UI font
  - screen/HUD wording, authored JSON layout, binding values, action mapping, and resulting game behavior
Does:
  - implement IRuntimeClient and the typed IRuntimePort specialization
  - map GYO InputActionFrame values into game commands/policy
  - load assets through GYO AssetId/AssetHandle/AssetManager
  - project immutable game snapshots into GYO RenderQueue submissions
  - load its immutable screens.json document once with no compiled fallback
  - map UiRuntime action ids to typed game commands and display-setting values
  - emit its C++ Playing HUD and JSON screens as one ordered UiDrawList
  - select concrete adapters at the application composition edge
Depends On:
  - GYO public Runtime, Input, Asset, Text, Render, Ui, and UiRenderer mechanisms
  - selected optional SDL adapters/backends in its composition root
Must Not Depend On:
  - KamataEngine
  - backend-native render resources in gameplay/domain code
  - Weaver
Must Not Be Depended On By:
  - GYO Core or reusable GYO modules
```

The current three maps are data/content fixtures. Their number, IDs, and order are campaign data, not GYO engine constants.

#### Model-derived weapon shot geometry

Object_FPS uses `LoadWeaponPresentationDefinition` for weapon placement and muzzle
calibration. It reads the model, clip mapping, fixed Idle anchor, placement and
local muzzle metadata from the weapon presentation JSON. The loader samples
Shoot at local time zero, transforms the muzzle through its model node, subtracts
the fixed Idle anchor and applies the same scale, rotation and offset used to
draw the weapon. It then stores pure numeric per-weapon `WeaponShotGeometry` in
`CampaignContent`. Gameplay consumes those values without model assets, poses,
GPU handles or a dependency on the presentation renderer. GYO supplies model
sampling and transform mechanisms; the socket choice and firing rules remain
Object_FPS content and policy.

Mark-23's muzzle is the center of the barrel's inner 18-vertex ring, expressed
in `main_j` local metres as `(0.003179880, 0.089686641, -0.213802223)`.
The barrel belongs to `main_j`; `side_j` moves the sliding part and would move
the shot origin with slide recoil. The fixed anchor is evaluated from Idle
once, while the muzzle is evaluated at Shoot zero. Animation playback keeps
that same anchor throughout the action.

The world and ViewModel cameras share aspect ratio but use vertical FOVs of
60 and 55 degrees. To preserve the muzzle's screen position when expressing it
in the world camera, multiply its camera-space x and y by
`tan(worldFov / 2) / tan(viewModelFov / 2)` and retain z. This is a projection
bridge after placement, not an extra rotation or a normalized direction.
Changing JSON placement or weapon FOV recomputes shot geometry on content load;
weapon cadence, damage and reload timing remain in the existing gameplay/CSV
data. There is no second hand-tuned muzzle offset to synchronize.

On an accepted shot, the aim and physical hit are resolved from the camera
before adding that shot's new recoil. The calibrated physical muzzle is clamped
against the world before its muzzle-to-aim query, so nearby walls can retract
the origin and real obstructions still block damage. Damage is applied once.
The cosmetic tracer is created after recoil establishes the camera used for
that rendered frame and after already-live projectiles have advanced. Its birth
frame therefore starts at the calibrated muzzle instead of moving by a full
delta immediately. Its visual path is clamped/clipped against the world and
cannot introduce another damage event or replace the resolved physical hit.

See [the muzzle calibration note](dev_logs/2026_09_15_model_muzzle_calibration.md)
for the derivation, calibration workflow and validation status. The GPU probe
compares a world-space marker with a raw bone-space muzzle point after the GPU
applies the weapon placement. This independently checks the CPU placement and
FOV bridge across three aspect ratios and seven camera cases per ratio. Gameplay
tests cover firing, recoil and tracer lifetime; the marker probe does not deal
damage or simulate a shot.

Object_FPS owns UI policy and data, while GYO owns the reusable JSON/layout/interaction/render mechanisms. `UiRuntime` owns focus, selection, pointer capture and hit testing and emits opaque actions; the Object_FPS adapter maps those ids to semantic commands. GameFlow retains transition legality and effects and no longer switches on menu indices. `UiRenderer` owns whole-run texture caching and Overlay submission, so neither game class exposes SDL_ttf or SDL_GPU types.

The `object_fps.menu_smoke` test starts the ordinary Main Menu path, presents its first frame, and requires at least one visible mesh/sprite submission. This protects against regressing to a clear-only startup frame; it is not a pixel-perfect rendering test.

### Runtime and Sandbox applications (`this milestone`)

```yaml
Module: apps/runtime
Owns:
  - minimal standalone production composition
Does:
  - connect RuntimeLoop to SDL window and clear/present adapters
Depends On:
  - GYO runtime and selected SDL adapters
Must Not Depend On:
  - Object_FPS, Sandbox, ImGui, or Weaver
```

```yaml
Module: apps/sandbox
Owns:
  - optional demonstration and experimentation composition
Does:
  - host ImGui/JSON sample behavior without making it a runtime dependency
Depends On:
  - selected GYO/SDL mechanisms and optional demo libraries
Must Not Depend On:
  - Object_FPS or Weaver
Must Not Become:
  - an editor or a requirement for ordinary games
```

```yaml
Module: tools/editor/gyo_ui_editor
Owns:
  - standalone ImGui editor chrome and in-memory edit/undo state
  - canonical atomic export of one gyo.ui JSON document
  - optional read-only mounting of an app AssetCatalog for preview validation
Depends On:
  - GYO::Ui, SDLRenderer/ImGui host, and optional asset preview adapters
Must Not Depend On:
  - Object_FPS, Input, SDL_GPU, Sandbox, or an app's native types
Must Not Do:
  - copy assets, edit a catalog, publish into an app asset root, serialize native
    paths, or create project/meta/autosave/cache/imgui.ini sidecars
```

## 7. Current target/dependency model

The intended target direction is:

```text
GYO::Engine -> nlohmann_json

GYO::AssetSdlImage (optional)
  -> GYO::Engine + SDL3_image + SDL3

GYO::Model -> GYO::Engine
GYO::AssetUfbx (optional) -> GYO::Model + private ufbx
GYO::Collision -> C++ standard library

GYO::Text -> GYO::Engine
GYO::TextBackendSDLTTF (optional)
  -> GYO::Text + SDL3_ttf + SDL3

GYO::Input
GYO::InputBackendSDL -> GYO::Input + GYO::PlatformSDL + SDL3

GYO::Render -> GYO::Engine
GYO::RenderBackendSDL -> GYO::Engine + concrete SDL platform adapter + SDL3
GYO::RenderBackendSDLGPU (optional)
  -> GYO::Render + concrete SDL platform adapter + SDL3

GYO::Ui -> GYO::Engine
GYO::UiRenderer -> GYO::Ui + GYO::Render + GYO::Text

gyo_ui_editor (optional)
  -> GYO::Ui + SDLRenderer/ImGui; optional read-only asset preview adapters

Object_FPS (optional)
  -> GYO Runtime + Input + Asset + Text + Render + Ui public mechanisms
  -> selected SDL, SDL_image, SDL_ttf, and SDL_GPU adapters at the composition root

GYO modules -X-> Object_FPS
GYO modules -X-> Weaver
```

The `GYO_BUILD_OBJECT_FPS` option controls the conformance game. Enabling it selects the SDL_image PNG loader, SDL_ttf raster adapter, and (with `GYO_RENDER_DEVICE=AUTO`) SDL_GPU device needed by this vertical slice. Disabling Object_FPS removes the concrete game without changing GYO Core. The `core` preset disables optional games, editor and adapters and selects `GYO_RENDER_DEVICE=NONE`.

`GYO_BUILD_UI_EDITOR` is on by default and independently selects its SDLRenderer/ImGui host and optional preview loaders. It does not select Object_FPS, Input, SDL_GPU, or Sandbox. Third-party source population uses the active build tree; doctest remains test-only and ImGui demo code remains Sandbox-only.

## 8. Platform and graphics backend strategy

SDL offers several capabilities, but architecture classifies adapters by responsibility:

```text
platform/sdl
  SDL process + window + host lifecycle + event pump

input/backend/sdl
  SDL input events -> GYO PhysicalInputFrame

render/backend/sdl
  minimal SDL_Renderer clear/present implementation

render/backend/sdl_gpu
  GYO IRenderDevice implementation using SDL_GPU
```

Win32 belongs under `platform/` only when a requirement cannot be met through the chosen portable platform adapter. DX12, Vulkan, OpenGL, and SDL_GPU belong under `render/backend/`.

The presence of SDL in more than one adapter does not make those adapters one module. They share infrastructure, not responsibility.

`GYO_RENDER_DEVICE=AUTO|SDL_GPU|NONE` chooses the compiled device integration.
`GYO_GPU_DRIVER=AUTO|D3D12|VULKAN|METAL` supplies the default runtime policy;
Object_FPS can override it with `--gpu-driver`. `GYO_SHADER_BUNDLE=AUTO` emits
DXIL and SPIR-V for Windows, SPIR-V for Linux, and Metallib for macOS. Explicit
format lists and forced drivers must be compatible with the target system.
CMake uses `CMAKE_SYSTEM_NAME`, not the host OS, to make target decisions. Host
shader tools are built separately; cross builds supply a native executable.

AUTO considers only fully supplied shader formats and usable SDL_GPU drivers.
An explicitly selected driver does not silently fall back. Startup diagnostics
identify the selected driver and format; packaging a format is not proof that
the target machine can run that driver. The installed program reads assets and
shader bundles relative to its executable and must reject missing deployment
content even when a source checkout exists nearby.

The Windows acceptance archive uses Release/RelWithDebInfo and app-local MSVC
redistributable DLLs discovered by `cmake/GyoMsvcRuntime.cmake` relative to the
selected compiler installation. This avoids depending on an IDE-bundled
CMake's list of known Visual Studio releases. `GYO_MSVC_REDIST_DIR` can provide
an explicit redistributable root. Missing redistributables fail release
installation, while development configuration and compilation remain available.
Debug CRT is not distributed; Windows 10+ supplies UCRT. Package validation
inspects VC DLL imports with `dumpbin` and Unix linking with `ldd`/`otool`,
preventing build-machine runtime installations or absolute build paths from
masking incomplete packages. These inspection tools are CI requirements, not
end-user runtime dependencies.

The GitHub Actions matrix builds the complete Object_FPS and UI editor graphs,
executes CPU/headless and offline shader tests, and validates isolated installed
packages on Windows x64, Linux x64 and macOS ARM64. Branch/PR/manual CI uses a
quick path: compile, install, load the real packaged catalog/campaign/model with
`--startup-smoke-test` on all three platforms, then run one shader-readback render
on Linux through Vulkan and Mesa Lavapipe under Xvfb. Release CI adds the full
CPU/shader/core/package-negative tests, gameplay `--headless-smoke-test` for
jumping, pause/resume, shooting and reload, and all eight Linux render cases.
This software rendering check is distinct from physical GPU and interactive
acceptance, which remain manual on target hardware.

`cross-platform.yml` supplies quick branch/PR/manual validation;
`prepare-release.yml` supplies the **Prepare Release** GUI with version and
prerelease inputs. Both call `build-and-validate.yml` using an exact source SHA
and a quick/release profile. Every required platform job must pass before the
release caller's write-enabled job creates a tag and Draft Release with all
three archives/checksums. The user publishes the prepared Draft after review;
tag pushes and `release.published` do not rebuild.

Exact commit, platform and smoke provenance live in `build_metadata.json` and
are rechecked before upload. Existing tags may only match the fixed commit;
same-version retries are serialized, preserve verified Draft assets and user
notes, and refuse an already-public version. Ordinary branch/PR/quick manual
runs produce Actions artifacts only. Reruns retain the original event SHA and
workflow; a new manual dispatch can select a later branch commit. See the
[Traditional Chinese](releasing.zh-Hant.md) and [Japanese](releasing.ja.md) release
guides for the GUI steps and recovery procedure.
No workflow file or compiler success is evidence of physical GPU correctness.
The current validation status lives in section 10 of the paired rendering guides.

## 9. Asset identity, runtime loading, importing, and GPU resources

These are separate responsibilities:

```text
Asset identity/lifetime
  AssetId + AssetHandle + catalog + cache + lifetime

Runtime source/loading
  resolved engine-ready path -> bytes -> loader -> CPU runtime asset

Development import
  source authoring asset -> importer/tool -> engine-ready asset

Renderer resource creation
  CPU runtime asset -> selected render backend -> opaque render handle
```

`NativeFileAssetSource` owns only the first native-file read after path resolution. The SDL_image loader owns only image decoding. `AssetManager` owns identity, lookup, records, cache/lifetime, and loader dispatch. `IRenderDevice` owns GPU resource creation from an `ImageView` or `MeshView`.

`AssetCatalog::AppendFromFile` validates an independently rooted catalog before
merging entries. Duplicate IDs and invalid paths leave existing entries and
their references unchanged. Object_FPS composes game and common catalogs with
separate restricted resolvers. Both roots ship beside the executable; once a
deployed game catalog is found, missing common/model content is an error, not a
request to silently read source-tree content. The door explicitly loads
`common.texture.white` from the supplied PNG.

The bounded FBX runtime path is bytes -> optional UfbxModelLoader -> owning
ModelAsset -> explicit-time pose/CPU skinning -> fixed-topology vertex update.
Material names map to catalog texture IDs in game presentation data. The
model remains CPU-only even after the application creates GPU resources.

Object_FPS's [3D asset structure](architecture/3d-assets.md) documents its complete
source archive, selected runtime models, AnimationSet/character definitions,
material bindings and same-source animation compatibility boundary.

Hashed public IDs use their numeric value as identity. `debugName` is diagnostic metadata only and never changes equality or hashing; invalid IDs/types use value zero. This rule also applies to input action/axis IDs so named mechanisms behave consistently across maps and frame views.

A loader must not create `SDL_Texture`, `SDL_GPUTexture`, `ID3D12Resource`, Vulkan images, or OpenGL textures. Renderer resources have separate lifetime because upload, residency, device loss, and destruction are renderer concerns.

The current text path follows the same boundary:

```text
catalog font entry
  -> FontLoader
  -> FontAsset encoded bytes
  -> ITextRasterizer
  -> owning CPU TextBitmap (straight-alpha RGBA8)
  -> IRenderDevice::CreateTexture(ImageView)
  -> opaque TextureHandle
  -> SpriteSubmission
```

The SDL_ttf adapter ends at `TextBitmap`; it neither creates GPU resources nor submits rendering. `GYO::UiRenderer` owns the bounded whole-run texture cache and alignment/render bridge. Text textures use the neutral linear RGBA upload path, while UI color is applied through `SpriteSubmission::tint` so differently colored instances can reuse a cached `{font AssetId, UTF-8, pointSize}` bitmap.

Names describe the actual pipeline. A component that parses runtime stage/map data is a Loader, Parser, or Deserializer, not an Importer. Development import tools remain absent until a real authoring pipeline requires them.

## 10. Scene, Stage, collision, and 2D/3D rules

These are architecture rules, not claims of currently implemented modules.

A Scene represents runtime lifecycle/execution state:

```text
TitleScene -> GameScene -> PauseScene -> ResultScene
```

A Stage represents gameplay content hosted by an appropriate scene:

```text
GameScene
    |
    v
StageRuntime(stage_01)
```

Scene and Stage must not independently evolve duplicate load, update, spawn, unload, and lifecycle stacks. Stage definition and gameplay runtime normally belong to Framework/Game unless multiple concrete games prove a reusable GYO-level mechanism.

GYO may share cross-cutting mechanisms between 2D and 3D: asset identity/lifetime, runtime lifecycle, input actions, audio/text mechanisms when implemented, render resource ownership rules, and the typed runtime boundary. It does not force algorithm or data-model unification. Separate `SpriteRenderer`/`MeshRenderer`, `Camera2D`/`Camera3D`, `Transform2D`/`Transform3D`, and `Physics2D`/`Physics3D` remain valid.

Collision grows from real needs. The implemented neutral capsule/ray/swept-sphere
queries do not imply a complete physics abstraction. Object_FPS retains grid
blocking, its flat-floor vertical integration and grounded/jump eligibility.
Player feet Y is authoritative; camera world Y is feet Y plus eye offset, and
the same feet Y moves the combat capsule, muzzle/aim origin and enemy target.
The single jump uses 0.6 m height and 18 m/s² gravity. XZ wall/enemy blocking
remains active in the air; elevated floors, stairs and wall-top landing are absent.

WeaponController owns Draw/Idle/Shoot/Reload/Hide/Holstered state, ammo and
timers. Its immutable WeaponPresentationSnapshot includes action, elapsed time,
duration and revision. Render samples that state without feeding animation
completion back to gameplay. Pauses/fades freeze simulation; new stages reset
player vertical state while retaining weapon action/ammo. New campaigns reset
the weapon to Draw. Object_FPS provides Space for jumping and H for holstering.

For firing, Object_FPS first acquires the camera aim point, then constrains its
model-derived muzzle along the camera-to-muzzle segment to the first world obstruction
with a 1 mm clearance. The existing muzzle-to-aim query still resolves the hit.
This prevents the calibrated muzzle from starting inside or beyond a nearby
wall while preserving genuine corner/front-wall blocking. Grid-specific segment
clamping belongs to Object_FPS; primitive ray intersection remains in Collision.

## 11. Runtime Boundary

The public boundary is caller-neutral:

```text
External Controller
  ├─ Game
  ├─ Automated Test
  ├─ Debug Console
  ├─ Replay
  ├─ AI Controller
  └─ Weaver
          |
          v
  Query / Command / Event
          |
          v
      GYO Runtime
```

Current minimum seam:

- **Query:** `IRuntimePort::Query()` returns a borrowed immutable view of the runtime's current typed snapshot.
- **Command:** `IRuntimePort::Submit()` accepts a typed intent; the runtime implementation retains validation and execution control.
- **Event:** `IRuntimePort::Events()` exposes a borrowed span of typed facts emitted by that runtime for the current advance.

The interface is neutral even though each concrete runtime supplies meaningful payload types. An Object_FPS controller sees Object_FPS state and legal intents through a GYO-owned interface; another game can use the same GYO shape with its own domain types. Borrowed query/event views are valid only until the runtime advances again, so persistent history remains the caller's responsibility. Neither caller receives `SDL_Event`, `SDL_GPUCommandBuffer`, `ID3D12Resource`, mutable `AssetRecord`, or unrestricted internal pointers.

The current seam is deliberately small. It is not a complete World/Entity/Scene query language, thread-safe remote transport, generalized subscription service, persistent event journal, command scheduler, or universal snapshot schema. Those require concrete consumers and ownership rules before they can enter GYO.

Do not create a giant `GenericEvent`, universal payload variant, complete `WorldSnapshot`, or Weaver-specific bus in anticipation. Add concrete runtime views, commands, or event delivery mechanisms only when a current Game/Test/Debug use case establishes their data and lifecycle.

## 12. Weaver relationship

The following rules are normative.

### English

> Weaver is NOT part of GYO.
>
> Weaver is an optional external high-level runtime.
>
> GYO MUST be fully functional without Weaver.
>
> Weaver MAY observe and influence GYO only through
> public runtime-facing mechanisms.
>
> GYO MUST NOT depend on Weaver.

### 繁體中文

> Weaver 不屬於 GYO。
>
> Weaver 是可選的高階因果 / 敘事 Runtime。
>
> GYO 在完全不存在 Weaver 的情況下，
> 仍必須是一個可以獨立執行普通遊戲的完整 Runtime。
>
> Weaver 未來只能透過 GYO 的公開 Runtime 邊界
> 觀察狀態、接收事件與提交意圖。
>
> GYO Core 永遠不能反向依賴 Weaver。

This milestone does not implement Weaver, a Weaver adapter, world model, causal graph, narrative graph, event-node solver, LLM integration, AI agent, or story generator. It establishes only a Weaver-compatible architecture seam and one-way dependency rule.

## 13. Growth and removal rules

- Add a module for a real new responsibility; do not add a placeholder for a roadmap label.
- Prefer adding a backend or adapter over editing unrelated Engine Core modules.
- A new module should depend on narrower, lower-level contracts and must not require a generic `EverythingManager`.
- An `EngineServices` bundle, if ever justified, is only a composition/dependency utility. It is not the Runtime Boundary and must not become a universal service locator.
- `AudioSystem` is justified only by spatial/listener/source world updates; a wrapper that only calls `AudioManager::Update()` is not a module.
- Render growth extends neutral submissions/resources and adds backend implementations; it must not expose backend-native commands or handles.
- Text growth must extend the neutral raster/pixel boundary or add an optional adapter; it must not make Render or Asset depend on SDL_ttf.
- Adding Render3D features, Physics3D, Navigation, or an external controller should mostly add code in its own module/backend/adapter.
- Removing Object_FPS, Physics, Render3D, tools, or an external controller must not break unrelated Engine, Asset, Input, or base Runtime behavior.
- Object_FPS executable/asset/shader deployment is included for native acceptance. Public SDK install/export packaging remains deferred until an external consumer stabilizes the public surface.

## 14. Deliberately deferred decisions

The following are intentionally not created or generalized in this milestone:

- Universal World/Entity/Scene snapshot or query language
- Generic runtime command scheduler, remote protocol, event subscription bus, or replay journal
- SceneManager, SceneStack, or a reusable StageRuntime
- Audio playback or spatial-audio modules
- Glyph-atlas packing, incremental atlas texture updates, per-glyph batching, SDF text, or a renderer-native text path
- A GYO-owned general shaping/layout API for wrapping, bidirectional/script/language policy, fallback fonts, caret/selection, or rich text
- A generic `TextManager`; the current concrete raster request, adapter, and game-owned cache do not justify a global text service locator
- Animation graphs/blending, navigation, and complete physics modules
- General material system, RenderGraph, GPU asset cache shared across devices, or renderer-wide `EverythingManager`
- Dedicated native DX12, Vulkan, Metal, or OpenGL adapters beyond SDL_GPU
- Universal 2D/3D transforms, renderer algorithms, or physics data models
- Development import pipeline and asset authoring tools
- Reusable game Framework policy extracted prematurely from Object_FPS
- A universal editor ecosystem, shared node tree, universal ECS, visual scripting,
  or editor/plugin ABI beyond the bounded standalone UI JSON editor
- Weaver and all causal/narrative functionality

Deferral is deliberate. The present vertical slice does not yet establish stable responsibilities for these abstractions, and GYO must not claim functionality that only exists in the architecture vision.

## 15. Architecture checks for each change

Before accepting a new runtime module, game integration, or backend, verify:

1. **Standalone:** GYO and ordinary applications configure without Weaver, Object_FPS, or development tools unless explicitly selected.
2. **GYO conformance:** a concrete game uses GYO lifecycle, input, asset, render, and runtime-facing contracts instead of preserving a parallel engine layer.
3. **External controller:** supported observation and intent use neutral public mechanisms rather than internal/backend access.
4. **Additive growth:** the feature mainly adds a module, backend, adapter, or game-owned policy/content.
5. **Removal:** disabling the game or feature does not break unrelated modules.
6. **Ownership:** Asset, Platform, Input, Text, Render, Runtime, and Game policy retain distinct responsibilities.
7. **Dependency direction:** GYO Core has no Game, Weaver, or concrete-backend dependency.
8. **Honest status:** documentation distinguishes implemented code from on-demand architecture and reports build/test results separately from design intent.
