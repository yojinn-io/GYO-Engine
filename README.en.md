[日本語](README.md) · [繁體中文](README.zh-Hant.md)

# GYO-Engine

GYO is a C++20 game engine with explicit runtime, asset, input, collision, model, text, rendering and UI boundaries. Engine libraries are linked statically into games and design tools. Games own their rules and content; the engine does not depend on a concrete game.

## Repository

| Directory | Responsibility |
|---|---|
| `apps/` | Game source and minimal project declarations |
| `assets/<game>/` | Prepared runtime content, catalogs and `content.json` |
| `engine/` | All reusable engine modules, adapters and `config/projects.csv` |
| `tools/` | Design support; currently the UI editor |
| `tests/common`, `tests/<project>` | Common engine and project-specific validation |
| `build/cmake`, `build/ci` | Build, integration, packaging and acceptance support |
| `build/target/` | Ignored build trees, runnable products and reports |
| `docs/`, `third_party/` | Design documents and pinned dependency wrappers |

`engine/render/shaders/pipeline` owns the offline shader compiler. Root `tools/` is for authoring tools. Do not remove the entire `build/` directory: it contains maintained source support as well as ignored generated output.

## Build a game

Use CMake 3.30+, a C++20 compiler and Ninja. Windows builds need an initialized x64 MSVC environment. Asset assembly uses Python 3; shader builds use the engine-owned native host compiler. Dependencies are fetched into the build tree; existing source caches can be reused explicitly.

Enable the intended game and OS columns in `engine/config/projects.csv`, then run from the repository root:

```sh
cmake --preset dev -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev --target gyo_object_fps
```

Build state is under `build/target/_build/dev`; the runnable game is assembled under `build/target/object_fps/bin`. The game reads only executable-relative `assets/object_fps`, including builtin and game shader bundles. No source checkout is needed to run the assembled product.

The normal product build has testing and CI/package acceptance disabled. It does not depend on tests, CI code or the editor. To check the backend-neutral engine:

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

Use the `test` preset for selected-game tests. GPU tests require a suitable graphics environment and are reported separately from CPU/CLI checks. Historical logs are not evidence for a new revision.

## Design tools and content

```sh
cmake --preset dev -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build --preset dev --target gyo_ui_editor
```

The GUI editor is assembled under `build/target/toolchain/bin`. It authors `gyo.ui` JSON using a read-only asset catalog. Authors prepare/copy chosen source content into a game's asset tree and register it in the catalog. A generic build hook automatically assembles `content.json` and compiles its declared shaders during local and CI builds. Asset policy is not encoded in game CMake files.

Manual variants copy only `apps/<game>` and `assets/<game>`, then add a CSV row. No CI, tests, editor code or source-art archive travels with the game. See [creating and copying games](docs/creating_apps.md).

## Integration and releases

The registry determines the selected games; directory discovery does not. A selected game failure fails engine integration. Every supported release platform has a fixed toolchain archive containing the GUI UI editor linked with the engine; enabled games have separate archives. Zero selected games is a valid release. These products are runnable binaries and dependencies, not an engine SDK or source-code package.

`Prepare Release` fixes a commit, builds and validates the complete product set, and prepares the tag and Draft Release. Publishing the Draft is a separate user action. Game packages contain only their game, assets and runtime dependencies; tests and acceptance tools run outside them.

## Design reference

- [Architecture and responsibility boundaries](docs/architecture.md)
- [UI standard and authoring workflow](docs/ui_toolchain.md)
- [Rendering and shader contract](docs/rendering_architecture.zh-Hant.md)
- [3D asset ownership and animation limits](docs/architecture/3d-assets.md)
- [Release procedure](docs/releasing.zh-Hant.md)

`Object_FPS` is a removable consumer demonstrating campaign flow, UI, grid combat, CPU-skinned first-person animation and SDL_GPU rendering. It does not define a generic game framework. General scene management, full physics, GPU skinning, PBR, scripting and a dynamic plugin ABI are outside the current implemented scope.
