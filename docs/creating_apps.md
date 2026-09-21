# Creating and copying games

Games are source projects inside the engine repository. Build them through the repository root; no external SDK or separate engine installation is required. `apps/<name>` owns game code, `assets/<name>` owns its prepared runtime content, and `engine/config/projects.csv` controls integration. Tests and release support live outside both directories.

## Minimal game project

Use a lowercase name matching `[a-z][a-z0-9_]*`. The names `common`, `toolchain` and `ui_editor` are reserved for engine test/tool/product ownership and cannot appear as game IDs, even in disabled CSV rows. A small game needs:

```text
apps/my_game/
  CMakeLists.txt
  project.json
  main.cpp
  include/ and src/              when the game needs them
assets/my_game/                  when the game has runtime content
  content.json
  asset_catalog.json
  ...prepared runtime files...
```

For an SDL renderer game, `project.json` declares the optional engine components:

```json
{
  "version": 1,
  "display_name": "My Game",
  "components": ["SDL_RENDERER"]
}
```

The root reads metadata before composing the engine. Keep the game CMake file limited to targets and links:

```cmake
gyo_app_project()
gyo_app_add_executable(main MAIN OUT_TARGET app
    SOURCES main.cpp
    LIBRARIES GYO::Engine
    COMPONENTS SDL_RENDERER SDL_PLATFORM)
```

Larger games can use `gyo_app_add_library` for game-owned source groups and an explicit `sources.cmake`. Use returned target variables rather than hard-coded target names. An SDL renderer component implies SDL platform availability; other optional adapters must be declared in metadata. `GYO::Engine`, `GYO::Model`, `GYO::Collision`, `GYO::Input`, `GYO::Render`, `GYO::Text` and `GYO::Ui` remain public engine target names.

Do not add asset copy commands, install recipes, CI callbacks, tests, diagnostic compilation or requirement-discovery guards to the game CMake file. Shared build helpers provide the ordinary runtime assembly and native dependency deployment.

## Select and build

Add the game to `engine/config/projects.csv`:

```csv
name,description,version,enabled,windows,linux,macos
my_game,My game,,1,1,1,1
```

`description` and `version` are notes. Only `enabled` and the target platform flag select a game. `AUTO` selects all enabled games for the target; a supplied list selects a permitted subset. Unknown, disabled or platform-disabled names are errors. A directory alone never opts a game into CI.

```sh
cmake --preset dev -DGYO_APPS=my_game -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev --target gyo_my_game
```

The generated build tree is `build/target/_build/dev`. The runnable product is `build/target/my_game/bin`, including required native libraries and `assets/my_game`. On Windows the executable has an `.exe` suffix. It can start from any working directory; runtime content resolution is relative to the executable.

A normal game build does not require CI, tests or design tools. Enable tests separately with the `test` preset. Use separate build trees/output roots for simultaneous configurations of the same product so their generated content does not overwrite each other.

## Prepare assets, then let the build assemble them

Authors manually prepare engine-ready content from source art or editor exports and register each file in the catalog. Keep vendor archives and working DCC files outside game code and deployed runtime roots. The UI editor exports to a working location; authors explicitly copy the chosen document into the game assets and update the catalog.

`assets/my_game/content.json` is the deployment declaration. A game with catalogs only can use:

```json
{
  "version": 1,
  "catalogs": ["asset_catalog.json"],
  "shader_bundles": []
}
```

An SDL_GPU game additionally lists its compiled bundles. Include a `game` bundle only when the game actually uses custom shaders; Object_FPS currently needs only `builtin`. A game with custom shaders can use:

```json
{
  "version": 1,
  "catalogs": ["asset_catalog.json"],
  "shader_bundles": [
    {
      "name": "builtin",
      "source": "../../engine/render/shaders/builtin/bundle.json",
      "path": "shaders/builtin"
    },
    {
      "name": "game",
      "source": "shaders/source/bundle.json",
      "path": "shaders/game"
    }
  ]
}
```

Sources are relative to the source content manifest. Shader bundle specs retain their own relative source/include paths. The build compiles requested formats and assembles catalogs, content and compiled shaders automatically. Deployed metadata omits build-only `source` paths. Nothing in the game CMake file enumerates individual assets or shader files.

The runtime root is exclusively `bin/assets/<game>/`, with the declared compiled shaders underneath. A game that needs reusable content owns its deployed copy; it does not mount `assets/common` beside its private root. Asset IDs remain logical content identifiers, so a texture can retain `common.texture.white` while being stored in the game's own catalog/root.

Each catalog must contain integer `version: 1` and an `assets` array. Entries are objects with nonempty string `id`, `type` and `path`; IDs cannot repeat within or across catalogs. Paths must be normalized relative deployment paths and cannot collide with metadata or declared shader directories. Custom types, additional fields and two IDs sharing one file are allowed. Assembly, package validation and native runtime parsing enforce these same rules.

No asset directory means empty content. Incremental builds detect asset-root or descriptor additions/removals, and build/install synchronization removes only that game's obsolete deployed content. If the directory exists but `content.json` is missing or invalid, the operation fails and preserves the last successful content. Keep `build/content_contract.py` with `build/assemble_runtime.py` when making a minimal source copy; ordinary builds do not require the CI directory.

## Reproduce assembly by hand, then use the script

The order is manual operation, script, tool, then platform when the workflow is mature. Asset creation and export remain author-controlled. To understand the existing assembly operation, start with a compiled Windows game and a prepared catalog whose only asset is `marker.txt`. Use a separate generated directory:

```powershell
$stage = "build/target/manual/my_game"
New-Item -ItemType Directory -Force "$stage/bin/assets/my_game" | Out-Null
Copy-Item "build/target/my_game/bin/gyo_my_game.exe" "$stage/bin/"
Copy-Item "build/target/my_game/bin/*.dll" "$stage/bin/"
Copy-Item "assets/my_game/content.json", "assets/my_game/asset_catalog.json", "assets/my_game/marker.txt" "$stage/bin/assets/my_game/"
```

Copy the catalog-listed files only. If shader bundles are declared, also copy each compiled `manifest.json` and its referenced shader binaries into the declared bundle path; omit compiler depfiles. The deployed `content.json` retains `version`, `catalogs`, and each bundle's `name`/`path`; omit build-only `source`. Native libraries come from the selected build toolchain, with Unix libraries in `lib/`. Verify the result from an unrelated working directory before automating it.

The equivalent content operation is already scripted:

```sh
python build/assemble_runtime.py --kind app --product my_game --stage build/target/manual/my_game --assets assets/my_game --executable build/target/my_game/bin/gyo_my_game.exe
```

For shader-enabled content, supply the declared compiled bundles, for example `--shader builtin=build/target/_build/dev/apps/my_game/shaders/builtin --shader game=build/target/_build/dev/apps/my_game/shaders/game`. The helper validates sources, synchronizes catalog files and compiled objects, removes stale managed content and emits the runtime descriptor. Native-library scheduling and synchronization remain in the shared build helper. Ordinary builds and `cmake --install` already invoke these steps; games do not repeat this recipe in their own CMake files. A future editor should edit these same descriptors instead of generating a second assembly policy.

## Manual copy

<a id="manual-copy"></a>

1. Copy `apps/object_fps` to `apps/my_variant`.
2. Copy `assets/object_fps` to `assets/my_variant` so content changes are independent.
3. Change `display_name` in the new `project.json` if desired. Review copied component requirements and relative shader sources.
4. Add and enable `my_variant` in `engine/config/projects.csv` for the intended platforms.
5. Build with `-DGYO_APPS=my_variant`; the product identity and runtime asset root derive from the new directory name.

Do not globally rename C++ namespaces, AssetIds, shader IDs, clip names or definition references merely to change deployment identity. They can remain the same until the variant's gameplay/content actually needs different values. The copied game does not carry tests, CI, acceptance scripts, source-art archives or editor tools.

To integrate variant-specific validation later, add an outer adapter under `tests/my_variant` or `build/acceptance/my_variant`. This is optional for ordinary local builds. Generic product packaging remains engine-owned. New games do not require names to be added to workflow YAML.

## Validation boundaries

Engine tests belong to `tests/common`, game unit/component tests to `tests/<game>`, and separate acceptance executables to `build/acceptance/<game>`. Tests may link the game's reusable libraries. They must not inject testing sources or macros into the shipped game executable.

Release always includes the GUI UI editor toolchain for each supported platform, then adds CSV-selected games. A selected game failing to configure, build or pass required validation fails the complete integration. With no selected games, the engine/toolchain release still succeeds. See [architecture](architecture.md) and [release procedure](releasing.zh-Hant.md).
