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
- The optional Windows/MSVC SDL_GPU backend exercises mesh, texture, sprite, and 3D submission without leaking native handles into game-facing contracts.
- `apps/object_fps` and `assets/object_fps` exercise those mechanisms as the active, separately removable concrete game vertical slice, including game-owned screen/HUD policy projected through the neutral Text and Render seams.
- `apps/runtime` remains a small standalone SDL clear/present composition root, while `apps/sandbox` keeps optional ImGui demonstration concerns separate.

`RuntimeLoop` calls an injected `IRuntimeClient` in this order:

```text
ProcessEvents(frame) -> Update(frame) -> Render(frame)
```

`FrameContext` carries the frame index and delta time. `RuntimeControl::Stop` terminates at the phase that requests it.

This milestone does **not** claim a complete ordinary-game engine. It includes only bounded whole-run text rasterization and sprite presentation, not a general text-layout ecosystem. Scene management, audio playback, animation, physics, navigation, generalized world/entity queries, remote control, and Weaver remain outside the implemented set.

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

### Neutral render mechanism (`this milestone`)

```yaml
Module: GYO::Render
Owns:
  - opaque generational MeshHandle and TextureHandle values
  - backend-neutral mesh/image descriptions
  - FrameDescription, camera, mesh, and sprite submissions
  - RenderQueue validation and primitive mesh generation
  - IRenderDevice resource and frame-execution contract
Does:
  - describe what a game-facing presentation layer submits
  - separate CPU assets/submission from backend resource implementation
Depends On:
  - Engine Base Result/Error values
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

### SDL_GPU render backend (`this milestone`, optional Windows/MSVC)

```yaml
Module: GYO::RenderBackendSDLGPU
Owns:
  - SDL_GPU device, swapchain, pipelines, uploads, and backend resources
  - translation of opaque GYO handles and RenderQueue submissions
Does:
  - implement IRenderDevice for the current mesh/sprite/3D vertical slice
  - compile backend-owned standalone HLSL sources into the current DXBC pipeline
Depends On:
  - GYO::Render
  - concrete SDL platform/window integration
  - SDL_GPU and private Windows shader/backend details
Must Not Depend On:
  - Object_FPS state or asset catalog policy
  - SDL_ttf or text-layout policy
  - Weaver
Must Not Expose:
  - SDL_GPU, D3D12, command-buffer, descriptor, or shader handles through GYO APIs
```

The HLSL files under `render/backend/sdl_gpu/shaders/` are the only shader sources. CMake reads them into a generated, backend-private build-tree header so static-library consumers do not depend on a working directory or runtime file deployment; `D3DCompile` remains a private SDL_GPU initialization detail. Editing either HLSL file triggers CMake regeneration and recompilation of the backend.

`SpriteSubmission::sourceUv` uses a visual top-left origin. The SDL_GPU path converts that rectangle once through `MakeSpriteUvTransform`, including atlas sub-rectangles, because its shared XY quad reaches screen-top at `v=1`. Text rasterization and texture upload preserve row order and must not compensate for backend sprite orientation.

Its Windows/MSVC limitation is an implementation constraint of this optional backend, not a platform requirement of GYO Core.

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

The `GYO_BUILD_OBJECT_FPS` option controls the conformance game. Enabling it selects the current SDL_image PNG loader, SDL_ttf raster adapter, and Windows/MSVC SDL_GPU implementation needed by this vertical slice. Disabling Object_FPS removes the concrete game without changing GYO Core.

`GYO_BUILD_UI_EDITOR` is off by default and independently selects its SDLRenderer/ImGui host and optional preview loaders. It does not select Object_FPS, Input, SDL_GPU, or Sandbox. Third-party source population uses the active build tree; doctest remains test-only and ImGui demo code remains Sandbox-only.

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

Collision grows from real needs. AABB, circle, hitbox, trigger, and grid tests do not justify an empty complete-physics abstraction. Bodies, response, integration, and queries belong to Physics only when those responsibilities exist. Object_FPS collision remains game policy until reuse proves a neutral engine mechanism.

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
- Public install/export packaging can be added after a real external consumer stabilizes the public surface; no empty packaging facade is required now.

## 14. Deliberately deferred decisions

The following are intentionally not created or generalized in this milestone:

- Universal World/Entity/Scene snapshot or query language
- Generic runtime command scheduler, remote protocol, event subscription bus, or replay journal
- SceneManager, SceneStack, or a reusable StageRuntime
- Audio playback or spatial-audio modules
- Glyph-atlas packing, incremental atlas texture updates, per-glyph batching, SDF text, or a renderer-native text path
- A GYO-owned general shaping/layout API for wrapping, bidirectional/script/language policy, fallback fonts, caret/selection, or rich text
- A generic `TextManager`; the current concrete raster request, adapter, and game-owned cache do not justify a global text service locator
- Animation, navigation, or physics modules
- General material system, RenderGraph, GPU asset cache shared across devices, or renderer-wide `EverythingManager`
- Non-Windows SDL_GPU support and dedicated DX12, Vulkan, or OpenGL backends
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
