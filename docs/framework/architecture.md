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
  - `GYO::ToolFramework`, no SDL: file access on user paths (`FileService`),
    the read-only catalog mount (`ReadOnlyAssetCatalog`), writing one catalog
    entry (`CatalogRegistration`), and the neutral types of the system path
    dialogs (`PathDialog`).
  - `GYO::ToolFrameworkGui`, built only when the SDL platform, the SDL
    renderer and ImGui's SDL backends are in the build: the ImGui and SDL
    renderer host (`ImGuiSdlHost`) and the system path dialogs through SDL
    (`SdlPathDialogs`).
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
2. Dependencies point one way: game or tool → framework → engine. The
   framework includes nothing from `apps/` or `tools/`, and the engine never
   names the framework. It links no game, no `GYO::Input` and no SDL_GPU, so a
   tool's own Design Invariant 1 (no game, no `GYO::Input`, no SDL_GPU) still
   holds through it. Guard: the framework's dependency guard, a positive list
   of includes and targets.
3. The framework is neutral. Its code names no tool, no product and no
   document format (UI JSON, sound, and so on): window titles, dialog filters
   and asset types come from the caller. Guard: the dependency guard fails when
   framework code names an id registered in `engine/config/tools.csv` or
   `engine/config/projects.csv`.
4. The framework writes only what its caller asks for. The host writes no side
   file (`imgui.ini`, ImGui's log). The catalog mount is read-only and has no
   write API. Writing a catalog changes only the entry with the caller's id,
   leaves every other byte of the text as it was, replaces the file
   atomically and checks the result against catalog v1 of the content
   contract.
5. Only what two or more products share enters the framework, or the
   integration between a tool and the asset contract
   (`v8-tool-framework-principle`). Code that only one product uses stays in
   that product. The game part enters only on the condition above, not in
   advance.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the component.

- To serve a product, the framework has to know what that product means, for
  example a branch on which game or tool is calling.
- A tool needs a GUI stack other than ImGui on the SDL renderer.
- Each new tool needs a change to the framework's public interface (adding a
  shared piece does not count).

## Using it

- A tool links `GYO::ToolFramework` (and `GYO::ToolFrameworkGui` for a window)
  and includes `gyo/framework/…`. The tool's own dependency guard lists both.
- The host runs one client: `RunImGuiSdlHost(options, factory)` creates the
  window, the renderer and ImGui, then the factory's client. Every frame it
  calls `BeginFrame` (before ImGui's frame), `Draw` (inside it), checks
  `WantsExit`, renders and calls `EndFrame`.
- A path dialog request carries a kind (open a file, save a file, open a
  folder), the filters, a default location and an opaque tag. The answer comes
  back later with the same tag; the tool maps the tag to its own purpose.
