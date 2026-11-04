# GYO Framework

The layer games and tools share, between the engine and the products that use
it (`v8-tool-framework-top-level`). The engine supplies mechanisms that do not
decide how a product is put together and stays pure: it holds no tool or
integration feature (`v8-tool-framework-principle`). The framework holds what
several products share on top of those mechanisms.

Owner: `framework` (`framework/`). It is not a product.

It has two parts. Only the first exists so far:

- **The tool part** (this batch, `v8-tool-framework-shell`): what two or more
  tools use, and the integration between a tool and the asset contract.
  Namespace `Gyo::Tools::Framework`, includes `gyo/framework/…`.
  - `GYO::ToolFramework`, no SDL: the headless shell (`RunHeadlessShell`),
    the read-only catalog mount (`ReadOnlyAssetCatalog`), the absolute form
    of a path that content mounting and the tools remember (`AbsolutePath`),
    registering a catalog entry as structure (`CatalogPlan`: the
    catalog-level rules and the registration policy), and the text input
    mapping the products share (`ToUiTextInput`: Engine's input entries as
    Ui's text input; `v8-input-ui-adapter`). File access on user
    paths is Engine's (`engine/io`), and so are the system path dialogs
    (`docs/architecture/platform-sdl.md`).
  - `GYO::ToolFrameworkWindow`, built where Engine's SDL platform is: the
    windowed shell (`RunWindowShell`), through Engine's neutral platform
    target only.
  - The ImGui binding `GYO::ToolFrameworkImGui` (`framework/bindings/imgui/`),
    built where the windowed shell, the SDL renderer, Engine's SDL input and
    ImGui's SDL backends are: ImGui on Engine's SDL renderer, run by the
    windowed shell (`RunImGuiShell`), which hands the client the window's
    input (`ImGuiShellInput`).
  - The JSON binding `GYO::ToolFrameworkJson` (`framework/bindings/json/`),
    built with the core: catalog v1 text (`ReadCatalog`, `RegisterAsset`).
    Whether a text is a catalog v1 is Engine's `CatalogParser`'s to decide.
- **The game part** (not built): how games are put together on the engine,
  such as the skeleton, the role arrangement, the client prediction loop and
  the authoritative host, as the
  [v7 README](../object_fps_pvp/plans/v7/README.md) defines the framework
  layer. It is extracted only when a second game puts the engine's mechanisms
  together in the same way, from those two instances. A game that does not use
  it must still build and run.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's
agreement, however small (AGENTS.md section 3). So is changing this section.

1. The framework is not a product. It has no row in `engine/config/tools.csv`
   or `engine/config/projects.csv`, no `project.json`, no executable and no
   install or package target. It is built only while a selected product uses
   it: while a tool is selected, or while a selected game declares the
   component `FRAMEWORK` in its `project.json` (which links
   `GYO::ToolFramework`), so removing every product that uses it leaves the
   engine and the other products building.
   `framework` is a reserved name for apps and tools. Guard: the framework's
   dependency guard and the registry's reserved names.
2. Dependencies point one way: game or tool → framework → engine. The framework
   includes nothing from `apps/` or `tools/`, links no game and names no
   product, and the engine never names it. It may use any Engine module, input
   included. It reaches SDL only as a game does: through Engine's SDL backend
   targets, which keep SDL private, or, where an Engine contract allows it and
   only inside a binding, an Engine `*Native` target.
   The core (everything outside `framework/bindings/`) includes no `SDL3/`,
   `imgui`, `backends/`, `nlohmann/` or Engine `*Native` header, names no
   `SDL_`, `ImGui` or `nlohmann` symbol, and links no SDL3, imgui,
   `nlohmann_json` or `GYO::*Native` target and no binding: it reaches SDL only
   through Engine's SDL backend targets. A binding is its own target under
   `framework/bindings/<name>/` with its own positive list, and the core never
   names it. Only the ImGui binding links `GYO::*Native`, PRIVATE; no framework
   target names `SDL3::SDL3` or an `SDL_` symbol. The ImGui binding reads typed
   text and the input method's composition through Engine's input backend
   (`GYO::InputBackendSDL`), not from SDL's events. Adding an Engine target
   that is not `*Native` to a positive list follows this item, though the
   guard change still follows AGENTS.md section 14.4; adding an SDL, `*Native`
   or third-party target changes this item. There are no exceptions: the
   allowlist of the core's old sites emptied and was removed in
   `v8-framework-invariants`. Guards: the dependency guard (core rules and one
   positive list per binding; the removed allowlist coming back fails) and the
   configure-time link check in `tests/framework/CMakeLists.txt`.
3. The framework is neutral. Its code names no tool and no product and knows
   no product's meaning: window titles and asset types come from the caller,
   and registration policy is chosen by the caller's options, never by which
   product calls. It owns no product format and no Engine asset format (UI
   JSON, a sound clip, a catalog: whether a text is one of those is Engine's to
   decide). It may own a composition format: a document that says how Engine
   assets combine (for example which sound plays at a clip's moment, and from
   where in the sound), in which every reference is an Engine asset id or an
   Engine value (seconds, gains) and no name carries product meaning; the
   facts such a document references (an id exists, a moment lies within its
   clip) are validated by calling Engine. Which of the three kinds a format is
   follows the layer table in `docs/architecture.md` ("Layers: admission and
   format ownership", user, 2026-11-04). Guards: the dependency guard fails
   when framework code names an id registered in `engine/config/tools.csv` or
   `engine/config/projects.csv`; a composition format has rule tests and
   mutants like the catalog's (C1–C7), one group per rule.
4. The framework writes no file of its own. The ImGui binding writes no side
   file (`imgui.ini`, ImGui's log). The catalog mount is read-only and has no
   write API. The JSON binding's `RegisterAsset` returns the new catalog text
   and writes nothing; the caller replaces the file atomically (through
   `engine/io`). The new text changes only the entry with the caller's id (on
   an update, only that entry's own `type` and `path` values), leaves every
   other byte as it was, and is read again and checked as data against
   catalog v1 before it is returned. The catalog-level rules follow the
   catalog-level cases of `tests/common/fixtures/asset_contract/cases.json`,
   with Engine's path rule (`IsRelativeContentPath`); across catalogs and
   against shader bundles, `ContentManifest` decides at load. Guards: the
   ImGui shell's side-file test (I2), the catalog structure tests (C1–C7,
   E1–E4) and the fw-cat-* and fw-extra-* mutants.
5. The core is the product shell: it puts Engine capabilities together and
   hands control to the product (a headless and a windowed shell, content
   mounting, registration policy). It holds no product's UI, document format or
   rules, and names no concrete UI or data-format library; such a library
   enters only as a binding. A capability Engine lacks is added to Engine, not
   to the framework. What two or more products use (a game and a tool, or two
   tools) enters the framework first, even where Engine has the capability:
   the framework calls Engine and the products call the framework. What one
   product alone needs, it takes from Engine directly. The game part, a game's
   own composition (its loop, its scenes), enters only on the condition above;
   a capability that a game and a tool both call is shared and enters by this
   rule (user, 2026-11-01). The windowed shell has one user (ui_editor) when
   it is added; the user accepted that on 2026-11-01
   (framework-rework D-8). Replacing ImGui means replacing its binding and the
   tool's own drawing code; the core shell and Engine stay.
6. **Shell lifecycle.** RunHeadlessShell hands its body the command line as
   Engine's CommandLineUtf8 gives it (UTF-8, program name first) and returns
   the body's or the failure function's value unchanged. RunWindowShell
   creates the platform before its client and destroys the client before the
   platform, also while an exception unwinds; it owns the one RuntimeLoop,
   passes every event of a pump to OnEvent before that pump's Update, returns
   1 for its own failures and otherwise the client's code, and sets no
   live-frame handler and no log handler. The ImGui binding's renderer is its
   own, lent to the product, and lives until the product is destroyed. No
   shell catches an exception, and shells log only through Base::Log. Guards:
   the headless, window-shell and ImGui-shell tests, the configure-time check
   that the shells are built where their Engine targets are, and the
   fw-shell-* and fw-imgui-* mutants.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the component.

- To serve a product, the framework has to know what that product means, for
  example a branch on which game or tool is calling.
- Each new tool needs a change to the core's public interface (adding a shared
  piece does not count).
- The core needs a concrete UI or JSON library to serve a caller.
- A binding has to know which product calls it.
- The windowed shell has to create or choose a render device for its client.
- The framework copies an Engine rule that no shared fixture checks.
- Serving one product needs a callback or flag in a shell that no other
  product uses, or a client has to pump events or run a RuntimeLoop itself.
  Handing every client of a binding a neutral capability that only the
  binding may reach under Design Invariant 2, such as the window's input
  frame and text input (`ImGuiShellInput`, `v8-input-ui-adapter`), does not
  count, however many products use it so far.

## Using it

- A game declares the component `FRAMEWORK` in its `project.json` and
  links it with `PRIVATE_COMPONENTS FRAMEWORK` (`GYO::ToolFramework`); that
  is what adds the framework to a build without tools.
- A tool links `GYO::ToolFramework` (and `GYO::ToolFrameworkImGui` for an
  ImGui window) and includes
  `gyo/framework/…`. The tool's own dependency guard lists them. An ImGui
  window needs the components `SDL_RENDERER`, `SDL_INPUT` and `IMGUI` in the
  tool's `project.json`.
- The ImGui binding draws the input method's composition at ImGui's text
  cursor, from Engine's input backend: on macOS the program has to draw it;
  on Windows the input method keeps drawing it unless the platform options
  ask otherwise. It covers what follows the cursor, as ImGui cannot insert it.
- `RunHeadlessShell(argc, argv, body, failure)` is the program's main: it
  hands the body the UTF-8 command line and returns its exit code. A window
  shell may run inside the body.
- `RunWindowShell(options, factory)` creates the platform from Engine's
  `SdlPlatformOptions` as given and runs the factory's client: `OnEvent` for
  each event (queries only), then `Update` and `Render` each frame, and
  `ExitCode` once at the end. The client creates its own render device from
  the platform.
- `RunImGuiShell(options, factory)` runs an ImGui window on the windowed
  shell. The factory gets the platform and the binding's renderer before
  ImGui exists; `Initialize` runs once ImGui is ready; every frame it calls
  `BeginFrame` (before ImGui's frame), `Draw` (inside it), checks
  `WantsExit`, renders and calls `EndFrame`. ImGui is shut down before the
  client is destroyed, and the renderer after it.
- A tool registers a catalog entry with the JSON binding's
  `RegisterAsset(text, entry, policy)`: the entry is Engine's
  `RawCatalogEntry` (id, type, path); the policy says whether the id may get
  another type and whether the path may already belong to another id (both
  refused by default; neither is a rule of the content contract). The core's
  `CheckCatalogEntries` holds the catalog-level rules of the content
  contract for one catalog, with Engine's `IsRelativeContentPath` as the path
  rule; across catalogs and against shader bundles `ContentManifest` decides
  at load. The tool writes the returned text back by atomic replace.
- `ToUiTextInput(events, shortcut, clipboard)` turns a frame's text input entries
  (`PhysicalInputFrame::text`) into Ui's text input for the field being
  edited: the one mapping of which key means which edit that the products
  calling the framework share (Ui Design Invariant 7 leaves the mapping to the
  product; a product that wants another keeps its own). `shortcut` is the
  modifier of select-all and paste (`Ctrl`, or `Gui` where the platform's
  text fields take Command); `clipboard` is called for the paste only.
- An ImGui client reads the window's input from `context.input`: `Frame()` is
  the shell frame's `PhysicalInputFrame` (the same snapshot the composition
  overlay reads, from `BeginFrame` to `EndFrame`; ImGui still gets every
  event). While a field of the client's own is edited, it calls
  `RequestTextInput(area, cursorPixels)` every frame: `area` in window
  coordinates, `cursorPixels` in pixels from its left edge (as Engine's
  `SdlInput::SetTextInputArea`).
  The binding applies it after ImGui's frame, so within a frame it wins over
  an ImGui text field; when the client stops asking, text input goes back to
  the ImGui field, or off when none wants it.
- `AbsolutePath(path)` gives the absolute, lexically normal form of a path
  through `engine/os` and never throws; a tool remembers paths in that form.
- System path dialogs are Engine's (`ShowPathDialog` and
  `TakePathDialogAnswers` on the SDL platform,
  `docs/architecture/platform-sdl.md`): the tool maps each answer's tag to its
  own purpose.
