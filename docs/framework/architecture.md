# GYO Tool Framework

The shared layer of the design tools: what two or more tools use, and the
integration between a tool and the asset contract. The engine stays pure: it
holds no tool or integration feature; the framework provides it
(`v8-tool-framework-principle`, [plan](../tools/plans/v8/assets.md)).

Owner: `framework` (`tools/framework/`). It is not a tool.

Two libraries:

- `GYO::ToolFramework`, no SDL: file access on user paths (`FileService`), the
  read-only catalog mount (`ReadOnlyAssetCatalog`), writing one catalog entry
  (`CatalogRegistration`), and the neutral types of the system path dialogs
  (`PathDialog`).
- `GYO::ToolFrameworkGui`, built only when the SDL platform, the SDL renderer
  and ImGui's SDL backends are in the build: the ImGui and SDL renderer host
  (`ImGuiSdlHost`) and the system path dialogs through SDL (`SdlPathDialogs`).

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's
agreement, however small (AGENTS.md section 3). So is changing this section.

1. The framework is not a tool. It has no row in `engine/config/tools.csv`, no
   `project.json`, no executable and no install or package target. It is built
   only when at least one tool is selected, so removing every tool leaves the
   engine and the products building. `framework` is a reserved name for apps
   and tools. Guard: the framework's dependency guard and the registry's
   reserved names.
2. Dependencies point one way: tool → framework → engine. The framework
   includes nothing from `tools/<id>/` or `apps/`, and the engine never names
   the framework. It links no game, no `GYO::Input` and no SDL_GPU, so a tool's
   own Design Invariant 1 (no game, no `GYO::Input`, no SDL_GPU) still holds
   through it. Guard: the framework's dependency guard, a positive list of
   includes and targets.
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
5. Only what two or more tools use, or the integration between a tool and the
   asset contract (`v8-tool-framework-principle`), enters the framework. Code
   that only one tool uses stays in that tool.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the component.

- To serve a tool, the framework has to know what that tool means, for
  example a branch on which tool is calling.
- A tool needs a GUI stack other than ImGui on the SDL renderer.
- Each new tool needs a change to the framework's public interface (adding a
  shared piece does not count).

## Using it

- A tool links `GYO::ToolFramework` (and `GYO::ToolFrameworkGui` for a window)
  and includes `gyo/framework/…`; namespace `Gyo::Tools::Framework`. The tool's
  own dependency guard lists both.
- The host runs one client: `RunImGuiSdlHost(options, factory)` creates the
  window, the renderer and ImGui, then the factory's client. Every frame it
  calls `BeginFrame` (before ImGui's frame), `Draw` (inside it), checks
  `WantsExit`, renders and calls `EndFrame`.
- A path dialog request carries a kind (open a file, save a file, open a
  folder), the filters, a default location and an opaque tag. The answer comes
  back later with the same tag; the tool maps the tag to its own purpose.
