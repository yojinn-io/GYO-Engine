# GYO architecture

> **Legacy** (current-state record; `v8-arch-docs-legacy`, 2026-11-12). Design authority is [`docs/architecture/target/`](architecture/target/README.md). This document takes only two kinds of updates: the current state of code that still exists, and Design Invariants that tests still guard. Existing code is bound by this document's Design Invariants; new code (from `engine/kernel` on) by target. Each component's current document is deleted and its target version moved here when that component's migration batch completes (the batch is set by `v8-arch-migration-plan`).

This document describes the current repository boundaries. The engine supplies reusable C++ mechanisms; games own gameplay and content; design tools consume the same public engine APIs. Engine libraries are linked statically into game and tool executables. Native third-party runtime libraries are deployed beside those products where required.

This document owns engine-wide architecture, dependency directions and shared rules. Detailed game architecture, gameplay policies and product data contracts belong in `docs/<game>/`. Engine subsystem documents describe reusable mechanisms; dated development logs record implementation history and validation evidence.

## Repository ownership

```text
GYO-Engine/
  apps/<game>/                 game source, project metadata, source lists
  assets/<game>/               prepared runtime content and content.json
  engine/
    base/, io/                foundational values and filesystem/stream IO
    runtime/, asset/          frame lifecycle and CPU asset management
    math/                     value types, pure geometry and coordinate conventions
    time/                     engine time base, deadline-or-notify waits, late-wake statistics
    threads/                  named role threads with cooperative stop
    trace/                    bounded multi-producer line sink with a notified writer thread
    net/                      GYOP datagram header codec (byte transport only)
    collision/                collision raycasts, sweeps, overlaps, contacts and the static broadphase
    config/projects.csv       integrated game selection
    config/tools.csv          design-tool default/release/platform selection
    input/                    neutral input and adapters
    model/                    owning models, animation, FBX adapter
    voxel_mesh/               baked grid-built maps (index, regions, chunks)
    map_markers/              named points, boxes and paths placed on a map
    platform/                 SDL host/window/event lifetime
    render/                   rendering contracts and adapters
      shaders/pipeline/       offline shader compiler and its host dependencies
    text/                     neutral raster contract and font adapter
    ui/                       JSON UI, layout, actions, render bridge
  framework/                 the layer games and tools share above the engine; only its tool part exists
  tools/<tool>/              design support; independent or game-specific local tools
  services/gyo_gateway/      reusable Go HTTP, session and transport mechanisms
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

`apps/` contains game code. It does not carry test runners, package scripts, editor code, vendor source archives or CI definitions. Tests consume game libraries from outside the game. Root `tools/` is reserved for design support such as UI and map editing. Root `framework/` holds what several products share above the engine: dependencies point game or tool → framework → engine, the engine never names it, and it is built only while a selected product uses it ([framework](framework/architecture.md)). Compilation, integration and release orchestration belong to `build/`. Shader compilation is a rendering responsibility and its implementation stays with `engine/render/shaders/pipeline/`.

A rule that several components must follow because it is a direct consequence of one Engine or framework contract has a single owner, recorded in [cross-component rules](cross-component-rules.md); the other places cite that row instead of restating the rule.

## Layers: admission and format ownership

> 2026-11-13 (`v8-arch-a6-revision`): for new code the three layers are Engine, plugins, and products and tools (`architecture/target/runtime-composition.zh-Hant.md` section 4); the Framework row, checks 2 and 4 and the framework dependency guard below describe the running code until the plugins batches land and are superseded then. Check 1 (an Engine function, type or format names no product, gameplay concept or meaning of a name) does not retire: its target clause is `architecture/target/naming.zh-Hant.md` Design Invariant 4.

Three layers, each admitting a thing by questions that can be answered yes or no from the change alone, not by
rank. "The engine is the most general layer" orders the layers; it does not say where a new piece goes. The same
questions decide which layer owns a data format (AGENTS.md section 8): the dependency rules above place code and
said nothing about data, and three candidate homes for one format in 2026-11 (Engine, framework, product) each
matched one layer. Adopted with the user on 2026-11-04; the framework's Design Invariants 3 and 5
([framework](framework/architecture.md)) apply it to that layer.

| Layer | Admits code when | Owns a format when | Never holds |
|---|---|---|---|
| Engine (`engine/`) | it is a capability: an OS or hardware difference, a mechanism, or the reading and validation of one asset, stated in Engine vocabulary (clips, seconds, gains, ids, scancodes) and carrying no product meaning | the format describes **one asset**: a model, a sound clip, a catalog, the moments on one model's clips (`gyo.anim_events`) | product names, gameplay constants, what a name means to a product, how a product is put together |
| Framework (`framework/`) | two or more products **compose or adapt** Engine capabilities in the same way: an adapter between Engine modules (`ToUiTextInput`), a composition of Engine assets, a shell. Two products calling the same single Engine capability is not composition; they call Engine directly | the format describes **how Engine assets combine** (for example which sound plays at a clip's moment, and from where in the sound), every reference in it is an Engine asset id or an Engine value, and it names no product | a product's meaning, a branch on which product calls, a façade over one Engine call |
| Product (`apps/<game>/`, `tools/<tool>/`) | everything else, including what looks general while one product uses it | the format is the product's own definition (a sound cue table, a weapon presentation, a character) | nothing it needs is forbidden; where it bypasses Engine, the bypass is recorded with the Engine capability whose arrival ends it |

Checks for a change:

1. A new Engine function, type or format: does its signature, its fields or its documentation name a product, a
   gameplay concept or the meaning of a name? Yes: it is not Engine.
2. A new framework piece: name the two products that use it and the Engine capabilities it joins or adapts. One
   product: that product takes Engine directly. One capability passed through: the products call Engine, and the
   framework does not wrap it.
3. A new format: does it describe one asset (Engine), how assets combine (framework), or what a product means
   (product)? A document that needs two of these answers is two documents. A composition read by one product is that
   product's until a second product reads it.
4. A framework format's fields: every reference is an Engine asset id or an Engine value (seconds, gains, counts);
   no field carries a product name, and no field decides a product's behaviour by what a name means. The framework
   validates the facts a document references (an id exists, a moment lies within its clip) by calling Engine.
5. A product's bypass of Engine (a `*Native` target, a direct SDL or OS call): recorded in that product's Design
   Invariants together with the Engine capability that ends it (today: ui_editor's native renderer, until Engine has
   a texture API).

Guards: review against this table for every Architecture Delta (AGENTS.md section 3); the framework dependency guard
for product names (item 4); the periodic checkup ([procedure](checkup/architecture/README.md)) for items 2, 3 and 5.

## Workflow evolution

Every workflow progresses through **manual operation → script → tool → platform**. First make the inputs, ordered operations, outputs and failure conditions understandable and repeatable by hand. A script then performs those same operations; a design tool edits their stable data contracts. A platform is justified only after the tool and workflow have matured through actual use.

This refactor stops at the maturity each existing workflow supports. Runtime assembly automates prepared-file copying and validation; it does not create an asset-authoring platform. The UI editor remains a separate design product that edits/exports data. It never embeds its own implementation into a game. JSON/CSV move project choices into explicit, editable data; each fact still has one owner: registries select games and tools, project metadata selects capabilities, and asset metadata selects content.

### Plan records and identifiers

Each plan folder (for example `docs/object_fps_pvp/plans/v7.1/`) keeps a HANDOFF with a decision log, and batches leave dev_logs in `docs/dev_logs/`. Two identifier schemes exist; read an identifier by the scheme of the record it appears in.

- **Until 2026-10-29: running numbers.** Decisions were `D1`, `D2`, … continuing across product versions (v6 → v7 → v7.1). Inventory and batch codes were short codes such as `B6`, `A4`, `E3`, `F7`. A `Dnnn` in a dev_log, commit message or HANDOFF means the decision of that record's own plan folder: a v7.1 dev_log's `D243` is v7.1 HANDOFF's `D243`. The same number can mean different things in different folders: v7.1 HANDOFF's `D243`～`D245` (worker-wake-phase) are unrelated to v8's former `D243`～`D254`.
- **From 2026-10-30 (v8 onward): `<version>-<area>-<topic>`**, like a commit message scope, in ASCII lowercase with hyphens, so the identifier says what the item is about. Areas: `v8-product-…` (product tasks and decisions), `v8-engine-…`, `v8-tool-…`, `v8-docs-…`, and `pv8-<topic>-…` for the network protocol (for example `pv8-action-…`, `pv8-item-…`). Batches and decisions use the same scheme.
- v8 started with running numbers; its `D243`～`D254` were renamed (for example `D243` → `v8-product-scope`). The mapping table is in the v8 HANDOFF's 代號對照. Older plan folders keep their running numbers.
- dev_logs and commit messages are historical records and are not rewritten to the new identifiers; resolve an old number through the mapping table of its plan folder.

## Runtime boundaries

Which of these results must reproduce across platforms bit for bit, which only on the same platform, and what guards each, is the [determinism contract](architecture/determinism.md) (proposal, 2026-11-11).

| Owner | Responsibility | Dependencies |
|---|---|---|
| `GYO::Base` | failure handling: `GYO_ASSERT` for Programmer Errors, `Result`/`Error` for Runtime Errors ([contract](architecture/error-handling.md)); the log facade (`Log.hpp`, one replaceable handler); the strict JSON parse for modules that own a JSON data contract (`StrictJson.hpp`: a template that rejects repeated keys without nlohmann's quadratic parser callback; it names the parser only through its template parameter, so the module that instantiates it links the parser privately); other domain-free helpers | C++ standard library only |
| `GYO::Math` | vectors, matrices, quaternions, geometric primitives and queries, coordinate conventions ([contract](architecture/math.md)) | Base |
| `GYO::EntityStore` | entity columns: one typed `std::vector` per component kept in strictly ascending order of a caller-given `EntityId` (binary-search lookup, insert at the ordered position, order-preserving erase); no id allotment, kinds, schedule or world ([contract](architecture/entity-store.md)) | Base |
| `GYO::Os` | the OS differences behind neutral APIs: files with UTF-8 paths (reads, writes, rename, resolve, compare), process facts, the console and shutdown requests, thread names and priority hints ([contract](architecture/os.md)) | Base; OS system libraries |
| `GYO::Time` | the engine time base (`MonotonicClock`), the deadline-or-notify `Waiter` and `LateWakeStats` ([contract](architecture/time.md)) | Base; the operating system (kqueue, Win32 waitable timers, or a condition variable) |
| `GYO::Threads` | named role threads with a cooperative stop through the role's `Waiter` and a priority hint; no pool or scheduler ([contract](architecture/threads.md)) | Time; Os privately, for the thread name and priority |
| `GYO::Trace` | one file of pre-serialized lines from many producer threads: a bounded queue that drops and counts when full, written and flushed by one role thread woken only by notification ([contract](architecture/trace.md)) | Threads; Os privately, for the file |
| `GYO::Audio` | the audio mixer: 48 kHz PCM16 clips (an immutable bank, plus clips each submitter loads and unloads while output runs), bounded SPSC submitters, plays mapped from Engine timestamps to sample positions per output block and stopped by handle the same way, an integer mix to 48 kHz stereo PCM16 with counted drops, late starts and saturation, and a playhead per sound for display ([contract](architecture/audio.md)) | Base, Time |
| `GYO::AudioBackendSDL` | plays a `GYO::Audio` mixer on the default playback device through an SDL audio stream; the `SDL_AUDIO` component; SDL stays PRIVATE, with no native target ([contract](architecture/audio.md)) | Audio; SDL3 privately |
| `GYO::Net` | the GYOP 24-byte datagram header codec and size limits; message kinds, payloads and protocol versions belong to products | Base |
| `GYO::Engine` | IO, runtime lifecycle, CPU asset identity/cache/loading | Base, Math, Time; C++ library; JSON parser privately |
| `GYO::Collision` | collision algorithms (raycasts, sweeps, overlaps, contacts) over Math primitives, plus the character `VerticalCapsule`; `StaticAabbGrid`, a static broadphase whose every query equals a linear scan of all boxes ([contract](architecture/collision.md)) | Math, Base |
| `GYO::Input` | physical input (full keyboard scancodes, mouse, modifiers), window interaction events (focus, move, resize, minimize, restore) and action/axis evaluation | Base (private) |
| `GYO::Model` | owning meshes, skeletons, clips, sampling and CPU skinning; asset `Transform` (TRS); named moments on clips, the `gyo.anim_events` data contract ([contract](architecture/animation-events.md)) | Engine, Math; the JSON parser privately |
| `GYO::VoxelMesh` | the data contracts of baked grid-built maps: `gyo.voxel_map` (an index) with `gyo.voxel_region` (files of chunks), and the deprecated single-file `gyo.voxel_mesh`; validating them (faces, per-direction colours, collision boxes in 1/16-cell integers) and expanding a chunk to world-space vertices and boxes, and a map's palette image; no culling, merging or shading ([contract](architecture/voxel-mesh.md)) | Base, Math; the JSON parser privately |
| `GYO::MapMarkers` | the `gyo.map_markers` data contract: named points, boxes and paths on a map, with opaque ids, kinds and tags; form only, meaning belongs to products ([contract](architecture/map-markers.md)) | Base, Math; the JSON parser privately |
| `GYO::ModelRenderer` | shared model/material GPU resources and independent skinned mesh instances; consumes an already evaluated pose | Model, Render |
| `GYO::PlatformSDL` | SDL process, window and event ownership | Engine and SDL |
| `GYO::Text` | encoded-font to owning RGBA bitmap contract | Engine |
| `GYO::Render` | opaque resources, queue, shader library, frame/pass preparation; GPU ABI and render submission contracts over Math values | Engine, Math |
| `GYO::Ui` | JSON codec, validation, layout, focus, binding and typed actions; UI geometry uses Math values | Engine, Math |
| `GYO::UiRenderer` | UI draw list to renderer and text/texture resources | Ui, Render, Text |
| Game | campaign, movement/combat rules, screen policy and presentation composition | selected public engine APIs |
| UI editor | authoring session, selection, undo, export and preview | public Engine/Ui/Math APIs, the framework's tool part and selected adapters |
| Framework, tool part (`GYO::ToolFramework`, `GYO::ToolFrameworkWindow`, the bindings `GYO::ToolFrameworkImGui` and `GYO::ToolFrameworkJson`) | the product shell (headless and windowed shells) and the content and catalog integration the tools share: the read-only catalog mount, the absolute form of remembered paths (kept in the core by framework-rework D-2), registering a catalog entry; the composition format `gyo.event_sounds` (which sound plays at an animation event, its rules, its binding and the one schedule offset; [contract](framework/event-sounds.md)); the ImGui binding runs ImGui on the SDL renderer, the JSON binding reads and rewrites catalog text and reads event-sounds text ([contract](framework/architecture.md)) | Engine, Os, Model, Audio; the windowed shell the neutral SDL platform; the ImGui binding the SDL renderer and ImGui, with the native targets privately; the JSON binding its JSON library |

Optional SDL input, SDL_image, SDL_ttf, SDL renderer/SDL_GPU and ufbx adapters remain explicit choices. Backend-neutral core builds do not fetch SDL merely because another application uses it. Public neutral contracts do not expose native SDL/GPU pointers; concrete adapter APIs may expose their own native interoperability surface.

The engine must not include game headers or understand any game's asset names. Tests and game-specific design adapters may depend on game libraries; the game and engine runtime must not depend on those support layers. *(Amended 2026-11-11, user decision `v8-arch-kernel-delta-approved`, spec K.3 item 7.)* The Kernel enters the Engine: World, System contract, Registry, Scheduler and Executor (`architecture/plans/kernel/spec.zh-Hant.md` v0.3; target documents in `architecture/target/`). A plugin ABI and dynamic loading stay forbidden; no scene hierarchy and no service locator. Composition and third-party adaptation will live in the plugins layer (`plugins/`, target: `architecture/target/runtime-composition.zh-Hant.md` section 4), not in an umbrella framework; the framework concept retires (`v8-arch-framework-retire`). Until the migration batches land, the three-layer table above (Engine, Framework, Product) describes the running code. Of an ECS, the Engine today has the entity store ([contract](architecture/entity-store.md)); its columns become the World's storage (`architecture/determinism.md` section 4.7). The framework layer ([framework](framework/architecture.md)) is not an umbrella: no product has to go through it, a product that does not use it builds and runs without it, and its game part is extracted only when a second game puts the engine together in the same way.

`RuntimeLoop` calls `ProcessEvents(frame)`, `Update(frame)` and `Render(frame)` in order. A phase returning `RuntimeControl::Stop` ends the loop immediately. The former typed observation/intent boundary `IRuntimePort` was removed on 2026-11-12 (`v8-arch-remove-object-fps`) together with its only user, a removed game.

### Fixed simulation time and product hosts

`Engine::Runtime::FixedTickRuntime` owns the conversion of caller-supplied elapsed
time into fixed simulation steps. `TickSettings` selects the rate and bounded
catch-up; `TickContext` carries the simulated tick ID and constant delta. The
runtime owns its accumulator and tick ID. `Advance(elapsed, callback)` returns
the number of executed steps, discarded excess time and time until the next
step. IDs start at 1 and count executed steps only. Excess whole-step debt is
discarded after the catch-up limit; fractional time is retained.

The fixed-step core does not read a clock, sleep, own threads, expose `Run()`,
or depend on network or gameplay. `GYO::Time` provides the clock (the engine
time base), the wait (a `Waiter` that wakes at a deadline or on a notification,
never early) and late-wake statistics; `GYO::Threads` provides named role
threads. The product host owns process lifetime, its loops and deadlines,
cancellation and the decision to reset a match.

Object_FPS_PVP's `MatchRuntimeHost::Start()` runs this engine mechanism at 60 Hz
on a named role thread (`GYO::Threads`), waiting on absolute tick deadlines with
the engine `Waiter`. The host processes ordered join/leave controls, consumes
each player's latest input, updates `PvpMatch` and publishes a full state every
authority tick. Publication is coalesced after one `Advance` call. IPC exchanges owning
values through bounded handoff; it does not invoke world updates or advance the
simulation clock. Local tests drive the same host via `Advance` without IPC.

The PvP client runs its simulation (input sampling, prediction, sending) on a
separate fixed-step role thread (`ClientSimulationRole`); the main thread owns
the window, view and presentation. Client tick IDs
and authority tick IDs are **not synchronized clocks**; no direct subtraction,
execution-time mapping or latency estimate is valid. Presentation continues to
use `RuntimeLoop` frames. Object_FPS v1 and v2, which stayed on variable-frame
gameplay paths in this change, were removed on 2026-11-12
(`v8-arch-remove-object-fps`).

### Network infrastructure and Object_FPS_PVP ownership

```text
PvP client → client protocol → PvP Go composition / ObjectFPS adapter
                                        ↓ runtime protocol over local TCP
                                  PvP C++ IPC host
                                        ↓ owning commands / latest input
                                  MatchRuntimeHost → PvpMatch
                                        ↓              ↓
                              GYO FixedTickRuntime   GYO Collision
```

`services/gyo_gateway` is a Go module providing HTTP server configuration,
session credentials/endpoint checks, framed TCP connections with deadlines and
error returns, and transport framing/ordering mechanisms.
It does not import product code or define Room, Match, player or FPS rules.
The PvP Go module owns the executable, one-room policy, capacity, HTTP routes,
Session → PlayerId mapping, runtime link lifecycle and adapter. The engine does
not depend on this service module, Go, sockets or either wire protocol. The one
shared transport fact is the GYOP datagram header: `GYO::Net` is its C++ codec
and `services/gyo_gateway/framing` its Go codec, both checked against the same
synthetic vectors (`tests/common/net/fixtures/gyop_datagram_vectors.json`);
message kinds and protocol versions stay with the product.

`match_domain` contains the product's pure movement/collision policy and owning
world values. It does not link client-only code (`client_simulation`, `app_support`), camera,
mouse settings, SDL, UI or the renderer. Map-to-geometry conversion remains separate from
the numeric collision functions. The client and runtime protocols are distinct,
versioned Object_FPS_PVP contracts; neither is a universal GYO protocol.

The new service top-level directory, fixed-step engine API, product host/domain
split and native `match` executable role are explicit architecture deltas. The
implementation pressure is a world that must run without a window or network,
plus a gateway that must translate inputs without owning gameplay. Putting a
timer in IPC, reusing the entire client controller or placing Room in common
transport would violate these boundaries. Network dependencies are selected by
the product; ordinary engine-only builds do not require them. Gateway output is
staged separately from native product archives. See the
[PvP network design](object_fps_pvp/network-architecture.zh-Hant.md) for contracts,
startup and validation procedures.

### Model, collision and rendering boundaries

`Model` owns animation sampling, caller-driven playback and pose blending.
Compatible-skeleton clip transfer takes explicit node bindings and a displacement
scale, corrects reference frames and preserves target geometry and skin bindings.
It validates mapped hierarchy and supported scale constraints; applications own
node-name selection and the decision to enable cross-source binding.
It also owns `gyo.anim_events`, the data contract for named moments on a model's
clips: it parses and binds the files but keeps the names opaque; what an event
triggers, and when that is in Engine time, belongs to the product
([animation events](architecture/animation-events.md)).
`ModelRenderer`, inside `engine/render/model`, depends on Model and Render to
manage shared model/material resources and independent skinned mesh instances.
It consumes an evaluated pose without advancing an animation clock. Model and
Render do not depend on each other; product state and content semantics remain
outside this bridge.

`Collision` takes Math primitives and returns collision results (world-space
ray distances, sweep fractions and contacts); tolerance-free geometric queries
belong to Math. Render's wire generators also accept Math values, without a
Collision dependency. `WorldOverlay` provides world-camera drawing without depth testing
or writing; an empty layer adds no pass. Application-specific collision policies,
controls and diagnostic meanings belong to their product owners. Collision's
`StaticAabbGrid` only chooses which boxes a query tests: each query equals a
linear scan of all boxes with a tie rule the caller picks, so a product keeps
its policy and swaps only where the boxes come from
([collision](architecture/collision.md)). Where things are on a map, and what
they are, are separate: `GYO::MapMarkers` carries named places with opaque
kinds, and a product gives the kinds meaning
([map markers](architecture/map-markers.md)). See the
[rendering architecture](rendering_architecture.zh-Hant.md) for pass and resource contracts.

Coordinate, matrix, Euler and clip-space conventions are owned by the
[Math contract](architecture/math.md). Model's asset `Transform` composes
through `Math::ComposeTRS`; Render's `Transform3D` uses `Math::ComposeEulerXYZ`
and uploads Math matrices to `ShaderAbi` without a transpose; Collision queries
take Math primitives under Collision's own tolerances. Modules and products
link this contract instead of restating it.

## Build and project management

<a id="build-project-management"></a>

The repository root is the supported game build entry. `engine/config/projects.csv` determines which games may participate:

```csv
name,description,version,enabled,windows,linux,macos
```

`GYO_APPS=AUTO` selects enabled games supported by the target OS. An empty value selects none. An explicit semicolon-separated subset must obey the same CSV policy. App directories are never enumerated as a substitute for the registry. A selected missing or broken app fails the integration; an unselected directory has no effect.

Each game has a small `project.json` declaring engine components and an optional display name. Tool descriptors declare a default variant, each variant's components/packageability, and any named app dependencies. `engine/config/tools.csv` owns enabled/default/release/platform selection. `GYO_TOOLS=` selects none, `AUTO` selects the platform's enabled default tools, and an explicit semicolon-separated `id[:variant]` list selects permitted tools. A tool's required app must already be selected by `GYO_APPS`; tools never silently enable games. The root resolves game, tool and common-test requirements before composing the engine, then adds each selected owner once. Game CMake files declare only game targets, sources and links. They do not implement requirement-discovery passes, asset recipes, test registration or release contracts.

The normal `dev` product configuration disables `BUILD_TESTING`, `GYO_ENABLE_PACKAGING` and design tools. A single selected game can therefore build and run without `tests/`, tool sources or CI files. The `test` configuration selects default tools. The pull request merge gate (L1) builds the `ci-<os>` presets without extra selection arguments, covering every registry-enabled game and the default tools; packaging CI explicitly selects the release tools exported from the same CMake registry parser. `tools/ui_editor` also offers a thin standalone entry that forwards to this same root graph; it does not rebuild an independent copy of the engine composition rules.

Game executables and their complete runnable content are assembled under `build/target/<game>/bin`. Packageable design tools are assembled under `build/target/toolchain/bin`. Non-packageable variants and game-dependent tools are local tools under `build/target/_tools/<owner>/bin` and never join a release archive. CMake cache, object files, host tools and transient validation state remain under `build/target/_build/` or other generated target subdirectories. The generated output root may be overridden for isolated validation.

Game support obtains generated app configuration through `gyo_use_app_config(target app)`, privately for exactly one app per target. It does not construct another owner's generated directory or publish every game's same-named `gyo/AppConfig.hpp` to consumers. See [tool registration](tool_projects.md) for data and manual steps.

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

`assets/<game>/content.json` lists catalogs and shader bundles. Build-only shader `source` fields identify input specs. The deployed content descriptor contains only relative runtime paths; build-only shader source fields are omitted. Runtime loading does not require the source tree. A content path inside the root follows one rule, `Engine::Asset::IsRelativeContentPath` (`ContentManifest.hpp`), which tools that write catalogs share. A game owns all of its deployed resources, including copies of reusable content and compiled builtin shaders. Runtime loading uses only `bin/assets/<game>/`; there is no separate shared mount or source-tree fallback.

The current `object_fps_pvp` runtime layout includes `asset_catalog.json`, gameplay rules, models, textures, UI/font assets and `shaders/builtin`. Custom shader validation belongs to `tests/common`, using its own channel-swap fixture; the game does not ship or require it. Existing AssetIds and shader IDs are content contracts, not deployment directory names; a manual game copy does not need global identifier replacement.

Python assembly and package validation share the pure data rules in `build/content_contract.py`. Runtime and Editor use native C++ parsing, checked against the same valid/invalid fixtures. Every catalog requires integer `version: 1`, an `assets` array, and object entries with nonempty string `id`, `type` and `path`. IDs must be unique across the entire content collection. Normalized relative paths, reserved names and shader-directory overlaps use the same rules. Custom types, additional fields and distinct IDs referring to one file remain supported.

Every selected game's content is synchronized on build and install. A missing source asset directory means empty content and removes only that product's previous deployed assets. An existing directory with a missing or invalid descriptor fails and preserves the last successful output. The shared hook watches asset-root and descriptor additions/removals so an incremental build reconfigures when needed.

Editor full-content mounting parses `content.json` and its catalogs once. Validation, asset lists and previews use that same snapshot; a failed replacement keeps the previous mount. Mount changes and text-cache eviction occur at frame boundaries before drawing commands borrow textures. See [UI toolchain](ui_toolchain.md) for explicit full-content and single-catalog modes.

`AssetManager` owns IDs/handles, catalog lookup, records, cache/lifetime and loader dispatch. Loaders create CPU values only. `NativeFileAssetSource` reads catalog-resolved bytes. `IRenderDevice` owns GPU upload, residency, synchronization and disposal. Shader compilers run at build time, never at game startup. See [rendering](rendering_architecture.zh-Hant.md) for the concrete data contracts.

### Asset requests and logical IO position

An asynchronous reload reserves a new generation when queued. Its returned handle is `Loading` and unreadable while the published handle remains readable. Success makes that same candidate `Ready` and invalidates the old handle. Failure makes the candidate `Failed`: `KeepOldIfAny` preserves the published handle, while the other policy invalidates it. Synchronous fallback retains the existing behavior of returning the old handle on a failed replacement. Issued generations are never reused after failure or eviction; releases of stale handles still balance their references.

There is at most one pending request per AssetId. Equal requests coalesce; a synchronous request can complete the pending work immediately without a second load during `Update()`. Conflicting resolved path, mode, fallback, type or tag returns `RequestInProgress`. Sync mode and pin are not content differences. Ordinary `Auto` cache hits may still return the published version during reload, and pending work prevents eviction. Only the current candidate and most recent failed candidate retain query state.

The public priority and TTL-override fields remain reserved. Passing a non-default `priority` or `keepAliveFramesOverride` is API misuse and fails a `GYO_ASSERT` before IO, queueing, reference changes or pin changes. Supported pin requests apply to both initial loads and cache hits. Exhausting the generation counter returns `GenerationExhausted` rather than reusing handles; existing cache hits and pending requests remain usable. Queue, synchronous and watcher completion share the same publication/failure rules; no background scheduler is introduced.

Buffered IO exposes one logical read position. Relative seeks account for unread prefetch, preserve buffered bytes when the underlying seek fails and reject offset overflow. `StreamReader` consumes leftover line-buffer bytes before all binary, integer and whole-content reads; mixing these operations never skips prefetched input.

## Tests and release products

Unit and engine capability tests live under root `tests/`. `tests/common` uses engine-owned fixtures, and `tests/<game>` contains game tests. Separate acceptance executables and their checks belong to `build/acceptance/<game>`. Product executables do not receive injected test source files or diagnostics compile definitions. UI editor tests live in `tests/ui_editor` and are registered only when that product is selected and testing is enabled.

Real-machine sessions with players collect a common set of data for every product before the product's own items ([field sessions](architecture/field-session.md)).

These central source directories apply to C++ tests and cross-process acceptance.
Go unit tests follow Go's package-local `*_test.go` convention in
`services/gyo_gateway` and the PvP Go packages. They retain the enclosing module's
ownership, do not join production binaries and run through each module's
`go test ./...`; this exception does not move acceptance orchestration into apps.

Release integration is engine-centered. Each supported platform always produces a toolchain archive containing the release tools selected by `tools.csv`, currently the UI editor's GUI variant. Every release tool must have executable acceptance for quick and release profiles on its selected platforms. Local tools and app-dependent previews cannot be release tools. CSV-selected games produce additional isolated game archives. An empty game registry, or no app source directory, still permits an engine/toolchain release; an empty release-tool selection fails. One failed selected game, tool or required platform fails the default-branch integration run, which then publishes no snapshot for any train. Formal releases are prepared per train (`tools` or one game): each builds the toolchain baseline plus that train's products, so one game's failure does not block another train.

There is no engine SDK or source-code archive in this release contract. Toolchain archives do not include game source/assets. Game archives contain the runnable game and its own assets/dependencies; CI scripts, diagnostic executables, tests, build tools and source art are excluded. Acceptance tools run from outside the product archive, with common and project-specific contracts owned by `build/acceptance/`.

The release workflow fixes a source SHA, derives the selected train's complete expected product/platform and tool-owner sets, builds and validates them with the toolchain baseline, then prepares that train's tag and Draft. Linux toolchain integration always runs common engine GPU rendering tests under Xvfb/Lavapipe, including when no games are selected. Every product, including toolchain, also runs its declared GPU checks independently. Acceptance evidence binds the actual product file and link contents; archive verification checks that inventory, exact tool membership and source identity. Quick validation is not full release evidence. See the [release guide](releasing.zh-Hant.md).

Product manifest schema 3 records a nonempty build `configuration`, executable owner/role/path and per-executable native dependencies, deployed native files, content requirements and owner-scoped checks. The native dependency union schedules copying only; it does not require an SDL-free executable to link SDL. Package validation accepts only declared product files, regardless of filename prefixes. CMake generates an external acceptance context whose configuration must match the product manifest; it resolves owner script roots and registered probes without changing commands or check selection. Checks, evidence and log names use `owner.name`. The context and probes are not installed.

## Architecture changes and verification

The reorganization resolves observed ownership conflicts: duplicated root/editor dependency composition, asset deployment policy embedded in CMake, tests injected into game binaries, and CI framed around individual apps. Module aliases and neutral C++ interfaces are retained while their physical directories move below `engine/`.

The contract repair addresses three additional observed pressures: divergent Python content validators, Editor re-reading catalogs independently of validation, and a test shader required by normal gameplay. Validation rules now have one Python owner with shared C++ fixtures, Editor consumes one parsed snapshot, and shader validation belongs to common tests. No top-level subsystem or reverse engine-to-game dependency is added; runtime does not depend on Python.

The build-boundary repair addresses repeated concrete-tool branches in composition, packaging and CI, inferred acceptance target names, and generated-header path leakage. Selection and requirements move to owner data; reusable build code composes those declarations. Packaging/acceptance remain development support and games still own their rules. Fixing only the preview conditional would leave the same special cases in component resolution, package checks and common tests. This changes the build graph and support interfaces, adds no top-level subsystem, and introduces no engine-to-game dependency or plugin framework.

At the user's request, Math was made the engine foundation (a requested refactoring). It resolves observed duplication: four isomorphic float3 types and three float2 types across Render, Model, Collision, Ui and a product; two `Matrix4` types with opposite storage order, default value and `Multiply` argument order; repeated converters (`ToCollision`, `ConvertRect`, brace copies); and divergent helpers (hypot or sqrt lengths, guarded or unguarded normalization, three sRGB copies). `GYO::Math` is a new header-only leaf below every engine module. It owns value types, pure geometry and the coordinate conventions previously stated in Render and Model comments. Engine, Collision, Model, Render and Ui link it publicly (Engine since the min/max/clamp unification); products and tools link it where they use Math values. Math links nothing but `GYO::Base` (added later for `GYO_ASSERT`), Collision still does not depend on `GYO::Engine`, and Model and Render stay independent. Semantic types, the GPU ABI, wire and file formats and product types (for example a ground-plane point) stay with their owners. All GYO code builds without floating-point contraction so x64 platforms round alike, and MSVC compiles it with its standard-conforming preprocessor (`/Zc:preprocessor`) so macros expand alike. Old names were removed without aliases, so inactive products need the recorded migration before they are re-enabled. Placing the types in `engine/base` would make Collision depend on `GYO::Engine`, adopting Render's types would make every module depend on Render, and adding converters would keep the duplication; see the [math-foundation plan](architecture/plans/math-foundation/README.md).

Structural verification checks one-way dependencies, absence of test/CI code in products, CSV-only game selection, single-app and zero-app builds, manual-copy isolation, and self-contained executable-relative content. Functional verification includes engine tests, editor validation, asset/shader failure cases, game diagnostics and installed-product execution. A successful build does not prove physical GPU support on an untested machine.

At the user's request, failure handling was unified on "Assert = Programmer Error, Result = Runtime Error" (a requested refactoring; [error-handling contract](architecture/error-handling.md), [plan](architecture/plans/result-unification/README.md)). The mechanisms already existed in `engine/base` since early 2026 but were undocumented, so later modules reported failures with exceptions, string errors, hand-written result structs and out-parameters, and the engine had no assertion facility (Math used `<cassert>`, which the RelWithDebInfo presets compile out). `engine/base` was an include directory of `GYO::Engine`, so Math and Collision could not use it without depending on the engine library. It is now `GYO::Base`, a header-only leaf below Math that depends only on the C++ standard library; Engine links it publicly, Math through its interface, Collision privately, and test helpers add a test-only `gyo_test_support`. No cycle is introduced, products keep their dependency edges, and the host shader tool keeps including only `Sha256.hpp` by path. The same refactoring made MSVC compile all GYO code with its standard-conforming preprocessor (`/Zc:preprocessor` in `build/cmake/GyoBuild.cmake`; the host shader tool sets it on its own target), so macros such as `GYO_ASSERT` expand alike on every compiler.

The engine time base and role threads (2026-10-08) answer observed pressure from a networked game's inventory of clocks, waits and threads ([time-threads-trace plan](architecture/plans/time-threads-trace/README.md)). The engine used three clocks (`RuntimeLoop` the steady clock, the SDL GPU backend `SDL_GetTicksNS`, the asset watcher the system clock) and had no wait primitive or thread facility, so consumers and acceptance loops each paired a clock with a sleep or condition variable; on macOS those waits are coalesced late by about a quarter to a half of their interval, and nothing recorded late wakes. Thread owners each started and stopped threads their own way, and the consumer's build carried a compiler flag for `std::jthread`. `GYO::Time` is a new leaf above `GYO::Base` (OS libraries only, no SDL): `MonotonicClock` is the steady clock, now the engine time base used by `RuntimeLoop` (the clock source changed, not the loop's semantics) and by the SDL GPU slow-acquire log; `Waiter` arms relative OS timers (kqueue with `NOTE_CRITICAL`, high-resolution waitable timers, or a condition variable) and re-reads the time base after every wake. `GYO::Threads` sits above Time. `GYO::Engine` links Time publicly; Net, Collision and PlatformSDL edges are unchanged, so roles without a window still link no SDL. Placing Time inside `GYO::Engine` would have left Base-level modules without it, keeping the wait in each consumer would have kept the platform knowledge there, and a thread pool or scheduler has no consumer. File times stay on the wall clock.

The trace transport and log facade (2026-10-20, TT-2 of the same plan) followed the same pressure: a networked game rebuilds its key measurements offline from per-record trace files, which it wrote with its own writer polling every 20 ms, and the Engine's own diagnostic line (the SDL GPU slow-acquire log) used another clock and went straight to SDL logging. `GYO::Trace` is a new subsystem above `GYO::Threads` and links nothing else (no SDL, no `GYO::Engine`); a producer never does file I/O or waits for the writer, and the writer has no polling timer. Engine owns only the transport (queue, writer thread, flush, drop counts); record kinds, schemas and file names stay with the consumer. The log facade lives in `GYO::Base` so every module can use it without a new dependency edge; the SDL GPU slow-acquire line now goes through it. Its Design Invariants and Replacement Criteria are in the [trace contract](architecture/trace.md).

The audio mixer (2026-10-30, v8-engine-audio-core of the [audio plan](architecture/plans/audio/README.md)) answers a game's need to play sounds at the moments its events happened: events carry Engine timestamps, but at 30 FPS a frame handles them up to 33 ms later, so playing them "now" would put every onset on the frame grid. `GYO::Audio` is a new module above `GYO::Time` (Base and Time only, checked at configure time): a clip bank that copies 48 kHz PCM16 clips and a sample-domain core that starts voices at exact sample positions and mixes in integer arithmetic, so the output does not depend on block sizes or platforms. The public `Mixer` (v8-engine-audio-schedule) adds bounded SPSC submitters and maps each request's timestamp to a sample position from an anchor the output backend reads once per block, so the rate difference between the device clock and the Engine clock never accumulates; the SDL backend (v8-engine-audio-sdl) reads that anchor once per callback, owns the mixer and SDL's AUDIO subsystem reference, and keeps SDL entirely behind its neutral header (no native target; a new `SDL_AUDIO` component, which needs SDL but not the SDL platform). The existing `SoundLoader` stays in `GYO::Engine`; the mixer takes its samples by value instead of linking the asset library. Putting the mixer in a product would make real-time safety and the time mapping product knowledge, and putting it in `GYO::PlatformSDL` would tie its offline tests to SDL.

The audio control layer (2026-10-31, v8-engine-audio-control of the same plan) answers a tool's need to preview clips: stop a sound it sent, play a newly converted clip without reopening the output, and show where a sound is. The mixer's design premises (clips fixed while output runs, one-shot plays) met two of its Replacement Criteria, and the user chose to rebuild the control layer while keeping the sample-domain core and the timestamp mapping. Stops and unloads travel in the same per-submitter ring as plays, so they are ordered after the plays they name; a play returns a never-reused handle; a submitter loads clips into its own slots on its own thread, the audio thread counts the voices reading each slot and hands it back only after the unload and the last voice, and the owner frees the samples then, so the audio thread still allocates and frees nothing; the audio thread publishes each voice's sound and position to a board of atomics that any thread reads wait-free, and never reads it back. The dependency edges are unchanged; the public interface changed (`Play` returns a `SoundHandle`, `ClipId` is 64-bit, new `Stop`, `StopAll`, `LoadClip`, `UnloadClip`, `PlayheadOf` and counters). The clip conversion and the tool's project knowledge stay in the tool.

The entity store (2026-11-06, v8-arch-entity-store of the [entity-store plan](architecture/plans/entity-store/README.md)) moves the container part of a networked game's ECS data layout into the Engine (a requested refactoring with one consumer). The game already kept one typed column per component in ascending entity-id order, with ordered insert, order-preserving erase and binary-search lookup, so its iteration order depends on ids alone; nothing in that code depended on the game except one transition field name. `GYO::EntityStore` is a new header-only leaf above `GYO::Base` (it links nothing else, checked at configure time): the key type, how a key is read and the column functions. It allots no id, knows no range or kind, runs no system and keeps no change tick; allotment, kinds, the frozen phases, the single erase entry and digests stay with the game. A reference survey of Bevy ECS and EnTT is the basis of its Design Invariants and Replacement Criteria, which are in the [entity-store contract](architecture/entity-store.md). The store adds nothing its one consumer does not use; moving the game's headers as they were would have carried its field names and id semantics into the Engine.

Historical results in `docs/dev_logs/` and explicitly dated sections describe their original commits and paths. They are not evidence that a later refactor or release passed. Current build logs, CTest output and Actions artifacts are the evidence for a particular revision.
