# GYO architecture

This document describes the current repository boundaries. The engine supplies reusable C++ mechanisms; games own gameplay and content; design tools consume the same public engine APIs. Engine libraries are linked statically into game and tool executables. Native third-party runtime libraries are deployed beside those products where required.

## Repository ownership

```text
GYO-Engine/
  apps/<game>/                 game source, project metadata, source lists
  assets/<game>/               prepared runtime content and content.json
  engine/
    base/, io/                foundational values and filesystem/stream IO
    runtime/, asset/          frame lifecycle and CPU asset management
    collision/                geometric queries
    config/projects.csv       integrated game selection
    input/                    neutral input and adapters
    model/                    owning models, animation, FBX adapter
    platform/                 SDL host/window/event lifetime
    render/                   rendering contracts and adapters
      shaders/pipeline/       offline shader compiler and its host dependencies
    text/                     neutral raster contract and font adapter
    ui/                       JSON UI, layout, actions, render bridge
  tools/ui_editor/            design support; independent GUI/CLI executable
  tests/
    common/                   engine, build-contract and common integration tests
    <project>/                project-specific unit and component tests
  build/
    cmake/                    shared build composition and generic product hooks
    ci/                       integration, packaging and release support
    acceptance/               common/project acceptance tools and diagnostic executables
    target/                   ignored build trees, runnable products and reports
  docs/                       current design and explicitly historical dev logs
  third_party/                pinned dependency wrappers
```

`build/` contains maintained development support. Only `build/target/` and older local generated output are disposable output; do not delete all of `build/`. Existing local caches are not migrated or committed. Fresh CMake configurations use `build/target/_build/<preset>`.

`apps/` contains game code. It does not carry test runners, package scripts, editor code, vendor source archives or CI definitions. Tests consume game libraries from outside the game. Root `tools/` is reserved for design support such as UI and map editing. Compilation, integration and release orchestration belong to `build/`. Shader compilation is a rendering responsibility and its implementation stays with `engine/render/shaders/pipeline/`.

## Workflow evolution

Every workflow progresses through **manual operation → script → tool → platform**. First make the inputs, ordered operations, outputs and failure conditions understandable and repeatable by hand. A script then performs those same operations; a design tool edits their stable data contracts. A platform is justified only after the tool and workflow have matured through actual use.

This refactor stops at the maturity each existing workflow supports. Runtime assembly automates prepared-file copying and validation; it does not create an asset-authoring platform. The UI editor remains a separate design product that edits/export data. It never embeds its own implementation into a game. JSON/CSV move project choices into explicit, editable data; each fact still has one owner: CSV selects games, project metadata selects capabilities, and asset metadata selects content.

## Runtime boundaries

| Owner | Responsibility | Dependencies |
|---|---|---|
| `GYO::Engine` | base values, IO, runtime lifecycle, CPU asset identity/cache/loading | C++ library; JSON parser privately |
| `GYO::Collision` | capsule, ray/AABB, swept-sphere queries | neutral geometry values |
| `GYO::Input` | physical input and action/axis evaluation | neutral input types |
| `GYO::Model` | owning meshes, skeletons, clips, sampling and CPU skinning | Engine |
| `GYO::PlatformSDL` | SDL process, window and event ownership | Engine and SDL |
| `GYO::Text` | encoded-font to owning RGBA bitmap contract | Engine |
| `GYO::Render` | opaque resources, queue, shader library, frame/pass preparation | Engine |
| `GYO::Ui` | JSON codec, validation, layout, focus, binding and typed actions | Engine |
| `GYO::UiRenderer` | UI draw list to renderer and text/texture resources | Ui, Render, Text |
| Game | campaign, movement/combat rules, screen policy and presentation composition | selected public engine APIs |
| UI editor | authoring session, selection, undo, export and preview | public Engine/Ui APIs and selected adapters |

Optional SDL input, SDL_image, SDL_ttf, SDL renderer/SDL_GPU and ufbx adapters remain explicit choices. Backend-neutral core builds do not fetch SDL merely because another application uses it. Public neutral contracts do not expose native SDL/GPU pointers; concrete adapter APIs may expose their own native interoperability surface.

The engine must not include game headers or understand Object_FPS asset names. Tests and game-specific design adapters may depend on game libraries; the game and engine runtime must not depend on those support layers. No new umbrella framework, scene hierarchy, ECS, plugin ABI or service locator is introduced by this organization.

`RuntimeLoop` calls `ProcessEvents(frame)`, `Update(frame)` and `Render(frame)` in order. A phase returning `RuntimeControl::Stop` ends the loop immediately. `IRuntimePort<Snapshot, Command, Event>` is a typed observation/intent boundary; each game retains ownership of its payloads and legality rules. Borrowed observations remain valid only until the runtime advances.

## Build and project management

<a id="build-project-management"></a>

The repository root is the supported game build entry. `engine/config/projects.csv` determines which games may participate:

```csv
name,description,version,enabled,windows,linux,macos
```

`GYO_APPS=AUTO` selects enabled games supported by the target OS. An empty value selects none. An explicit semicolon-separated subset must obey the same CSV policy. App directories are never enumerated as a substitute for the registry. A selected missing or broken app fails the integration; an unselected directory has no effect.

Each product has a small `project.json` declaring engine components and an optional display name. The root reads that data before building dependencies, then adds each selected product once. Game CMake files declare only game targets, sources and links. They do not implement requirement-discovery passes, asset recipes, test registration or release contracts.

The normal `dev` product configuration disables `BUILD_TESTING`, `GYO_ENABLE_PACKAGING` and the UI editor. A single selected game can therefore build and run without `tests/` or CI files. The `test`/CI configurations opt into the outer validation layer. `tools/ui_editor` also offers a thin standalone entry that forwards to this same root graph; it does not rebuild an independent copy of the engine composition rules.

Game executables and their complete runnable content are assembled under `build/target/<game>/bin`. Tool executables are assembled under `build/target/toolchain/bin`. CMake cache, object files, host tools and transient validation state remain under `build/target/_build/` or other generated target subdirectories. The generated output root may be overridden for isolated validation.

See [creating and copying games](creating_apps.md) for commands and the minimum project files.

## Asset preparation and assembly

Source authoring and runtime deployment are distinct operations:

```text
DCC/editor output and vendor source material
  -> author prepares selected runtime files and catalog entries
  -> assets/<game>/content.json describes the content and shader bundles
  -> build automatically invokes asset assembly
  -> build/target/<game>/bin/assets/<game>/
  -> executable-relative runtime loading
```

Authors decide which source material becomes runtime content. Source archives and working DCC projects remain outside `apps/` and the runtime content tree. Design tools do not silently publish content. Local builds and CI both invoke the same automated assembly through a generic CMake hook. CMake owns build scheduling and toolchain parameters; asset manifests and the asset pipeline own file selection, catalog validation and assembly.

`assets/<game>/content.json` lists catalogs and shader bundles. Build-only shader `source` fields identify input specs. The deployed content descriptor contains only relative runtime paths; build-only shader source fields are omitted. Runtime loading does not require the source tree. A game owns all of its deployed resources, including copies of reusable content and compiled builtin shaders. Runtime loading uses only `bin/assets/<game>/`; there is no separate shared mount or source-tree fallback.

The current Object_FPS runtime layout includes `asset_catalog.json`, gameplay data, models, textures, UI/font assets and `shaders/builtin`. Custom shader validation belongs to `tests/common`, using its own channel-swap fixture; the game does not ship or require it. Its white texture retains the internal ID `common.texture.white` but is a game-owned file. Existing AssetIds and shader IDs are content contracts, not deployment directory names; a manual game copy does not need global identifier replacement.

Python assembly and package validation share the pure data rules in `build/content_contract.py`. Runtime and Editor use native C++ parsing, checked against the same valid/invalid fixtures. Every catalog requires integer `version: 1`, an `assets` array, and object entries with nonempty string `id`, `type` and `path`. IDs must be unique across the entire content collection. Normalized relative paths, reserved names and shader-directory overlaps use the same rules. Custom types, additional fields and distinct IDs referring to one file remain supported.

Every selected game's content is synchronized on build and install. A missing source asset directory means empty content and removes only that product's previous deployed assets. An existing directory with a missing or invalid descriptor fails and preserves the last successful output. The shared hook watches asset-root and descriptor additions/removals so an incremental build reconfigures when needed.

Editor full-content mounting parses `content.json` and its catalogs once. Validation, asset lists and previews use that same snapshot; a failed replacement keeps the previous mount. Mount changes and text-cache eviction occur at frame boundaries before drawing commands borrow textures. See [UI toolchain](ui_toolchain.md) for explicit full-content and single-catalog modes.

`AssetManager` owns IDs/handles, catalog lookup, records, cache/lifetime and loader dispatch. Loaders create CPU values only. `NativeFileAssetSource` reads catalog-resolved bytes. `IRenderDevice` owns GPU upload, residency, synchronization and disposal. Shader compilers run at build time, never at game startup. See [rendering](rendering_architecture.zh-Hant.md) and [3D assets](architecture/3d-assets.md) for the concrete data contracts.

### Asset requests and logical IO position

An asynchronous reload reserves a new generation when queued. Its returned handle is `Loading` and unreadable while the published handle remains readable. Success makes that same candidate `Ready` and invalidates the old handle. Failure makes the candidate `Failed`: `KeepOldIfAny` preserves the published handle, while the other policy invalidates it. Synchronous fallback retains the existing behavior of returning the old handle on a failed replacement. Issued generations are never reused after failure or eviction; releases of stale handles still balance their references.

There is at most one pending request per AssetId. Equal requests coalesce; a synchronous request can complete the pending work immediately without a second load during `Update()`. Conflicting resolved path, mode, fallback, type or tag returns `RequestInProgress`. Sync mode and pin are not content differences. Ordinary `Auto` cache hits may still return the published version during reload, and pending work prevents eviction. Only the current candidate and most recent failed candidate retain query state.

The public priority and TTL-override fields remain reserved. Non-default `priority` or `keepAliveFramesOverride` returns `UnsupportedRequest` before IO, queueing, reference changes or pin changes. Supported pin requests apply to both initial loads and cache hits. Exhausting the generation counter returns `GenerationExhausted` rather than reusing handles; existing cache hits and pending requests remain usable. Queue, synchronous and watcher completion share the same publication/failure rules; no background scheduler is introduced.

Buffered IO exposes one logical read position. Relative seeks account for unread prefetch, preserve buffered bytes when the underlying seek fails and reject offset overflow. `StreamReader` consumes leftover line-buffer bytes before all binary, integer and whole-content reads; mixing these operations never skips prefetched input.

## Tests and release products

Unit and engine capability tests live under root `tests/`. `tests/common` uses engine-owned fixtures, and `tests/<game>` contains game tests. Separate acceptance executables and their checks belong to `build/acceptance/<game>`. Product executables do not receive injected test source files or diagnostics compile definitions. UI editor tests live in `tests/ui_editor` and are registered only when that product is selected and testing is enabled.

Release integration is engine-centered. Each supported platform always produces a toolchain archive containing the GUI UI editor linked with the engine and its required runtime libraries. CSV-selected games produce additional isolated game archives. An empty registry, or no app source directory, still permits a successful engine/toolchain release. One failed selected game or required platform blocks the release as a whole.

There is no engine SDK or source-code archive in this release contract. Toolchain archives do not include game source/assets. Game archives contain the runnable game and its own assets/dependencies; CI scripts, diagnostic executables, tests, build tools and source art are excluded. Acceptance tools run from outside the product archive, with common and project-specific contracts owned by `build/acceptance/`.

The release workflow fixes a source SHA, derives the complete expected product/platform set, builds and validates it, then prepares the existing tag/Draft process. Linux toolchain integration always runs common engine GPU rendering tests under Xvfb/Lavapipe, including when no games are selected. Game jobs additionally run the checks declared for their platform and profile. Acceptance evidence binds the actual product file and link contents; archive verification independently checks that inventory along with product membership and source identity. Quick validation is not full release evidence. See the [release guide](releasing.zh-Hant.md).

## Architecture changes and verification

The reorganization resolves observed ownership conflicts: duplicated root/editor dependency composition, asset deployment policy embedded in CMake, tests injected into game binaries, and CI framed around individual apps. Module aliases and neutral C++ interfaces are retained while their physical directories move below `engine/`.

The contract repair addresses three additional observed pressures: divergent Python content validators, Editor re-reading catalogs independently of validation, and a test shader required by normal gameplay. Validation rules now have one Python owner with shared C++ fixtures, Editor consumes one parsed snapshot, and shader validation belongs to common tests. No top-level subsystem or reverse engine-to-game dependency is added; runtime does not depend on Python.

Structural verification checks one-way dependencies, absence of test/CI code in products, CSV-only game selection, single-app and zero-app builds, manual-copy isolation, and self-contained executable-relative content. Functional verification includes engine tests, editor validation, asset/shader failure cases, game diagnostics and installed-product execution. A successful build does not prove physical GPU support on an untested machine.

Historical results in `docs/dev_logs/` and explicitly dated sections describe their original commits and paths. They are not evidence that a later refactor or release passed. Current build logs, CTest output and Actions artifacts are the evidence for a particular revision.
