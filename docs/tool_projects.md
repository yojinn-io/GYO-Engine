# Registering and building design tools

Root `tools/` contains design support. Compiler, integration and release support belong to `build/`; shader compilation belongs to `engine/render/shaders/pipeline`. A tool edits its own data contract and consumes public engine/game interfaces. It does not install editor code into a game.

## Select a tool

`engine/config/tools.csv` is independent of the game registry:

```csv
name,description,version,enabled,default,release,windows,linux,macos
ui_editor,UI design editor,,1,1,1,1,1,1
object_fps_preview,Object FPS local preview,,1,0,0,1,1,1
```

The description/version fields are notes. `enabled` and platform flags permit selection; `default` selects a tool in `AUTO`; `release` selects its default variant for that platform's toolchain. Release selection must be nonempty on all three supported platforms. The CMake parser is authoritative for both local builds and CI; directory discovery and a second Python CSV parser are not selection mechanisms.

```sh
# Games only; no tool sources are required.
cmake --preset dev -DGYO_APPS=object_fps -DGYO_TOOLS=
# GUI editor; no game is needed.
cmake --preset dev -DGYO_APPS= -DGYO_TOOLS=ui_editor
# Local CLI-only mode.
cmake --preset dev -DGYO_APPS= -DGYO_TOOLS=ui_editor:cli
# A game-specific preview requires that game to be selected separately.
cmake --preset dev -DGYO_APPS=object_fps -DGYO_TOOLS=object_fps_preview
```

An explicit list uses semicolons, for example `-DGYO_TOOLS=ui_editor;object_fps_preview` (quote the entire argument in the shell). `GYO_TOOLS=AUTO` selects enabled default tools on the current platform. `dev` and `core` default to no tools, while `test` selects `AUTO`. Use separate build trees and output roots for concurrent configurations.

Packageable tools go to `build/target/toolchain/bin`; local-only tools go to `build/target/_tools/<owner>/bin`. Object_FPS preview depends on game libraries and content, so it remains local. It cannot simultaneously be a fixed game-independent release tool. UI Editor's standalone `cmake -S tools/ui_editor -B build/target/_build/ui-editor` entry forwards to the same graph and accepts `-DGYO_TOOLS=ui_editor:cli`.

## Add a tool by hand

1. Add `tools/<id>/CMakeLists.txt`, sources and `project.json`. IDs match `[a-z][a-z0-9_]*`, cannot be `common` or `toolchain`, and cannot collide with registered game owners.
2. Declare the default variant and its capabilities once in `project.json`. For example:

   ```json
   {
     "version": 1,
     "display_name": "Example design tool",
     "default_variant": "native",
     "requires_apps": {},
     "variants": {
       "native": { "components": [], "packageable": true }
     }
   }
   ```

3. Build the executable in that tool's CMake and register its role. The helper uses the selected variant's components for deployment:

   ```cmake
   add_executable(example_tool main.cpp)
   target_link_libraries(example_tool PRIVATE GYO::Engine)
   gyo_tool_install_target(TARGET example_tool ROLE main)
   ```

   For multiple executables with different native requirements, the variant may add `"role_components": {"cli": []}` alongside its `components`. Each role's array must be a subset of that variant's components; an empty array declares no optional components. Unspecified roles inherit the variant's full component list. For example, a variant with `"components": ["SDL_RENDERER"]` and `"role_components": {"cli": []}` deploys SDL for `ROLE main`, while `ROLE cli` has no SDL requirement. Keep these differences in the descriptor rather than repeating component literals in CMake.

4. Add an enabled CSV row with the intended platform/default/release flags. A local tool sets `release=0` and `packageable=false`. Release tools must be packageable and have no app dependencies.
5. Add owner tests under `tests/<id>` and release checks under `build/acceptance/<id>/checks.json`. Every release tool must execute one of its registered executable or probe roles in both `quick` and `release` on each selected release platform. Missing coverage fails configuration.
6. Build, run from the assembled directory, install into a separate stage, and run declared acceptance. Only after the manual sequence is clear should a script automate those same operations.

Game-specific local tools declare dependencies as aliases, such as `"requires_apps": {"game": "object_fps"}`. Tool code resolves the alias with `gyo_tool_app_dependency(game app)` and uses the existing app target/config helpers. A missing selected app is an error, not a request to silently enable it. No shared Build, runner or workflow edits are needed for a new tool.

## Acceptance and package roles

Checks use `@EXECUTABLE:main@`, `@PROBE:<role>@` and `@CHECK_ROOT@`, resolved within their declaring owner. Choose suite arguments in the owner's check data. Shared execution does not infer an executable from array order, a probe from its filename, or a game's diagnostic suite from the CI profile. Evidence and log names are `owner.name`, so separate tools can both declare `startup`.

The product manifest records the nonempty build `configuration`, installed executable identities and each executable's native dependencies. Native deployment records exactly which runtime files it installs. Acceptance context is generated under `<build>/packages/<product>/<configuration>/acceptance-context.json`; pass it using `--context`. Its configuration must match the manifest; an unspecified CMake build type is recorded as `Unspecified`. The context binds that manifest/configuration to owner script roots and actual registered probe paths. It stays outside the product and cannot change the manifest's commands or check selection.

## Migrating an old build tree

The former `GYO_BUILD_UI_EDITOR`, `GYO_UI_EDITOR_BUILD_GUI` and `GYO_BUILD_OBJECT_FPS_PREVIEW` variables are removed. Use `GYO_TOOLS=ui_editor`, `ui_editor:cli`, or `object_fps_preview`. Do not retain both interfaces in presets or local IDE options. Remove the three obsolete cache entries, or configure a fresh build tree with the new selection; reloading an old cache does not remove saved variables automatically. Never delete all of `build/`: maintained development support lives there.

Schema 2 product manifests and old probe-directory invocation are superseded. Reconfigure, build and reinstall to regenerate schema 3 manifests and matching acceptance contexts before running current acceptance scripts. Local outputs and old reports are not evidence for the new contract.
