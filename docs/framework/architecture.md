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
    file access on user paths (`FileService`), the read-only catalog mount
    (`ReadOnlyAssetCatalog`), registering a catalog entry as structure
    (`CatalogPlan`: the catalog-level rules and the registration policy), the
    old text registration (`CatalogRegistration`, until the tools move), and
    the neutral types of the system path dialogs (`PathDialog`).
  - `GYO::ToolFrameworkWindow`, built where Engine's SDL platform is: the
    windowed shell (`RunWindowShell`), through Engine's neutral platform
    target only.
  - The ImGui binding `GYO::ToolFrameworkImGui` (`framework/bindings/imgui/`),
    built where the windowed shell, the SDL renderer and ImGui's SDL backends
    are: ImGui on Engine's SDL renderer, run by the windowed shell
    (`RunImGuiShell`).
  - The JSON binding `GYO::ToolFrameworkJson` (`framework/bindings/json/`),
    built with the core: catalog v1 text (`ReadCatalog`, `RegisterAsset`).
    Whether a text is a catalog v1 is Engine's `CatalogParser`'s to decide.
  - `GYO::ToolFrameworkGui`, the old ImGui and SDL renderer host
    (`ImGuiSdlHost`) and the system path dialogs through SDL
    (`SdlPathDialogs`), until the tools move to the shells and to Engine's
    path dialogs (removed in `v8-framework-invariants`).
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
   it (so far: when at least one tool is selected), so removing every product
   that uses it leaves the engine and the other products building.
   `framework` is a reserved name for apps and tools. Guard: the framework's
   dependency guard and the registry's reserved names.
2. Dependencies point one way: game or tool → framework → engine. The framework
   includes nothing from `apps/` or `tools/`, and the engine never names it; it
   links no game and no SDL_GPU, so a tool's own Design Invariant 1 (no game,
   no SDL_GPU) still holds through it. Engine's capabilities are open to the
   framework as to a game or tool, input included
   (`v8-tool-ui-editor-ime-composition-source`, 2026-11-01).
   The core (everything outside `framework/bindings/`) includes no `SDL3/`,
   `imgui`, `backends/`, `nlohmann/` or Engine `*Native` header, names no
   `SDL_`, `ImGui` or `nlohmann` symbol, and links no SDL3, imgui,
   `nlohmann_json` or `GYO::*Native` target and no binding: it reaches SDL only
   through Engine's neutral targets. A binding is its own target under
   `framework/bindings/<name>/` with its own positive list, and the core never
   names it. Only the ImGui binding links `GYO::*Native`, PRIVATE; no framework
   target names `SDL3::SDL3` or an `SDL_` symbol. The ImGui binding reads typed
   text and the input method's composition through Engine's input backend
   (`GYO::InputBackendSDL`), not from SDL's events. Guards: the dependency guard
   (core rules, one positive list per binding, and an allowlist of existing
   core sites whose counts must match and only shrink, empty after
   `v8-framework-invariants`) and the configure-time link check in
   `tests/framework/CMakeLists.txt`.
3. The framework is neutral. Its code names no tool, no product and no document
   format (UI JSON, sound, and so on): window titles and asset types come from
   the caller. Registration policy is chosen by the caller's options, never by
   which product calls. Guard: the dependency guard fails when framework code
   names an id registered in `engine/config/tools.csv` or
   `engine/config/projects.csv`.
4. The framework writes only what its caller asks for. The host writes no side
   file (`imgui.ini`, ImGui's log). The catalog mount is read-only and has no
   write API. Writing a catalog changes only the entry with the caller's id,
   leaves every other byte of the text as it was, replaces the file
   atomically and checks the result against catalog v1 of the content
   contract.
5. The core is the product shell: it puts Engine capabilities together and
   hands control to the product (a headless and a windowed shell, content
   mounting, registration policy). It holds no product's UI, document format or
   rules, and names no concrete UI or data-format library; such a library
   enters only as a binding. A capability Engine lacks is added to Engine, not
   to the framework. Otherwise only what two or more products share enters. The
   game part enters only on the condition above. The windowed shell has one
   user (ui_editor) when it is added; the user accepted that on 2026-11-01
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

## Using it

- A tool links `GYO::ToolFramework` (and `GYO::ToolFrameworkImGui` for an
  ImGui window, or `GYO::ToolFrameworkGui` until it moves) and includes
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
- The old host runs one client: `RunImGuiSdlHost(options, factory)` creates the
  window, the renderer and ImGui, then the factory's client. Every frame it
  calls `BeginFrame` (before ImGui's frame), `Draw` (inside it), checks
  `WantsExit`, renders and calls `EndFrame`.
- A tool registers a catalog entry with the JSON binding's
  `RegisterAsset(text, entry, policy)`: the entry is Engine's
  `RawCatalogEntry` (id, type, path); the policy says whether the id may get
  another type and whether the path may already belong to another id (both
  refused by default; neither is a rule of the content contract). The core's
  `CheckCatalogEntries` holds the catalog-level rules of the content
  contract for one catalog, with Engine's `IsRelativeContentPath` as the path
  rule; across catalogs and against shader bundles `ContentManifest` decides
  at load. The tool writes the returned text back by atomic replace.
- A path dialog request carries a kind (open a file, save a file, open a
  folder), the filters, a default location and an opaque tag. The answer comes
  back later with the same tag; the tool maps the tag to its own purpose.
