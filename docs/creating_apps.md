# Creating an app in this repository

[Architecture and selection](architecture.md#build-project-management) · [Package contract](architecture.md#app-package-contract) · [日本語 README](../README.md) · [繁體中文 README](../README.zh-Hant.md) · [English README](../README.en.md)

Choose either [a new app from scratch](#from-scratch) or [a manual copy of an existing app](#manual-copy). Both use the same root build and helpers; there is no app creation/clone command, additional project JSON, standalone app build, or separate-repository workflow.

The ownership boundary is fixed:

```text
config/engine/projects.csv      selection and human notes only
apps/<name>/                   product CMake, source and shaders
apps/<name>/tests/             optional quality implementation and Tests.cmake
apps/<name>/packaging/         optional Package.cmake acceptance adapter
assets/<name>/                 that app's independently editable runtime content
assets/common/                 intentionally shared engine-neutral content
```

Product, quality and project/CI management are separate layers. A product must
configure, build, run and install even when test, CI and acceptance files are
physically absent. `BUILD_TESTING=OFF` and `GYO_ENABLE_PACKAGING=OFF` are the
ordinary defaults. They express that dependency direction; guarding an include
while still requiring its source list or script would not satisfy it. Quality
and package management consume product targets from the outside.

A CSV name must match its app directory and use lowercase letters, digits and underscores, starting with a letter. It is the app identity; the helper obtains it from the selected directory's CMake context. Do not declare the name again in the app CMake file. `description` and `version` remain human notes. Add or edit only the intended CSV rows, keeping other rows, versions, active settings and unrelated local projects unchanged. Enabling the app and target platform in the CSV is required even when using an explicit `GYO_APPS` subset.

<a id="from-scratch"></a>
## 1. Start a new app from scratch

For the example identity `hello_game`, create `apps/hello_game/CMakeLists.txt` and `apps/hello_game/main.cpp`. Add a row to `config/engine/projects.csv`, preserving the existing header and rows:

```csv
hello_game,My first app,,1,1,1,1
```

Use the following CMake file. Requirements and the discovery return must precede `gyo_app_project()` and every target, test or install operation. Configure from the repository root.

```cmake
gyo_app_requirements(COMPONENTS SDL_RENDERER)
if(GYO_APP_DISCOVERY)
    return()
endif()

gyo_app_project()

gyo_app_add_executable(main MAIN WIN32
    OUT_TARGET app
    SOURCES main.cpp
    LIBRARIES GYO::Engine
    COMPONENTS SDL_PLATFORM SDL_RENDERER)

```

This complete `main.cpp` opens a window, uses the ordinary runtime loop and renders a clear frame. It runs until the window closes; renderer/window creation and frame errors return failure. No test or acceptance flag is needed.

```cpp
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "gyo/AppConfig.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl/SdlRenderer.hpp"

using Engine::Platform::Sdl::SdlPlatform;
using Engine::Platform::Sdl::SdlPlatformOptions;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::RuntimeControl;

class Client final : public Engine::Runtime::IRuntimeClient {
public:
    Client(SdlPlatform& platform, SdlRenderer& renderer)
        : platform_(platform), renderer_(renderer) {}

    RuntimeControl ProcessEvents(const FrameContext&) override {
        return platform_.PumpEvents();
    }
    RuntimeControl Update(const FrameContext&) override {
        return RuntimeControl::Continue;
    }
    RuntimeControl Render(const FrameContext&) override {
        if (!renderer_.Clear({20, 20, 22, 255}) || !renderer_.Present()) {
            failed = true;
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    bool failed = false;

private:
    SdlPlatform& platform_;
    SdlRenderer& renderer_;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SdlPlatformOptions options;
    options.title = Gyo::AppConfig::DisplayName;
    auto platformResult = SdlPlatform::Create(options);
    if (!platformResult) return 1;
    auto platform = std::move(platformResult).value();
    auto rendererResult = SdlRenderer::Create(*platform);
    if (!rendererResult) return 1;
    auto renderer = std::move(rendererResult).value();
    Client client(*platform, *renderer);
    Engine::Runtime::RuntimeLoop loop(client);
    loop.Run();
    return client.failed ? 1 : 0;
}
```

Use Ninja and the native C++20 compiler (an x64 MSVC developer shell on Windows):

```sh
cmake -S . -B build/hello-game -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGYO_APPS=hello_game -DGYO_BUILD_UI_EDITOR=OFF
cmake --build build/hello-game --target gyo_hello_game
./build/hello-game/apps/hello_game/bin/gyo_hello_game
cmake --install build/hello-game --prefix /absolute/path/to/hello-stage
```

Use a fresh installation prefix for every app. The program is `bin/gyo_hello_game` (`.exe` on Windows), and `MAIN` automatically installs it with its runtime dependencies. Ordinary installation requires no package manifest, startup check, Python validator, CTest or test source files. The example uses no file content, so it does not request `ASSETS` or need an empty catalog. Add `assets/hello_game/` when the app actually has runtime content; the [content helper](#content) then requires a real `asset_catalog.json`. The CSV version does not change the executable or package name.

<a id="target-helpers"></a>
## 2. Keep targets and identity local to the app

`gyo_app_project([DISPLAY_NAME text] [ASSET_ROOT path])` initializes one app directory. Display name defaults to the CSV identity; set `DISPLAY_NAME` only when the window or UI needs another human-facing name. A relative `ASSET_ROOT` is resolved from that app's source directory; an absolute override is also accepted. The default is `<repository>/assets/<name>`. An override changes the source location, not the deployed namespace.

| Helper | Contract |
|---|---|
| `gyo_app_add_library(role OUT_TARGET variable SOURCES ...)` | Static library; optional `PUBLIC_LIBRARIES`, `PRIVATE_LIBRARIES`, `PUBLIC_COMPONENTS`, `PRIVATE_COMPONENTS`. |
| `gyo_app_add_executable(role [MAIN] [WIN32] OUT_TARGET variable SOURCES ...)` | Executable; optional `LIBRARIES` and `COMPONENTS`. Only the main executable uses the app's base target name. |
| `gyo_app_link_components(target PUBLIC\|PRIVATE\|INTERFACE ...)` | Link declared optional capabilities to a target owned by this app. It does not discover new requirements after the dependency phase. |

Executables build into `<app-binary-dir>/bin` (with a configuration subdirectory
for multi-configuration generators). This keeps compiled shader output separate
from the executable-relative staging directory.

Every target uses C++20, disabled C++ extensions, common compiler warnings, UTF-8/permissive settings on MSVC and `NOMINMAX` on Windows. The app's `include/` directory is added when present. Keep explicit `SOURCES`; adding a source file is an intentional app-local CMake change.

For identity `hello_game`, a `MAIN` executable is `gyo_hello_game`; a `domain` library or non-main executable is `gyo_hello_game-domain`. Role names use lowercase letters, digits and underscores and must be unique within the app. Inside product CMake, link and deploy through returned `OUT_TARGET` variables rather than spelling these names or adding app-specific aliases. Optional quality/package adapters retrieve targets through `gyo_app_get_target` instead of depending on temporary product variables. Target names are useful for the developer's build command, not a second source of project identity.

For a larger app, split a domain library from its executable without changing shared build files:

```cmake
gyo_app_add_library(domain OUT_TARGET domain
    SOURCES src/Game.cpp src/World.cpp
    PUBLIC_LIBRARIES GYO::Engine)

gyo_app_add_executable(main MAIN OUT_TARGET app
    SOURCES main.cpp
    LIBRARIES ${domain}
    COMPONENTS SDL_PLATFORM SDL_RENDERER)

# Equivalent explicit component linking when it belongs in a separate statement:
# gyo_app_link_components(${app} PRIVATE SDL_PLATFORM SDL_RENDERER)
```

Declare each optional capability at the beginning with `gyo_app_requirements`. `SDL_PLATFORM` is also made available by a declared `SDL_RENDERER`, `SDL_GPU` or `SDL_INPUT` requirement. Neutral libraries such as `GYO::Engine`, `GYO::Ui` and `GYO::Collision` remain ordinary `LIBRARIES` entries. Gameplay policy, data formats and individual domain source lists stay in the app; the shared helper has no Object_FPS-specific concepts.

Each helper-created target privately receives the generated `gyo/AppConfig.hpp` include directory. Read it inside app code:

```cpp
#include "gyo/AppConfig.hpp"
// Gyo::AppConfig::Id             -> "hello_game"
// Gyo::AppConfig::DisplayName    -> "hello_game" unless overridden
// Gyo::AppConfig::Assets         -> "assets/hello_game"
// Gyo::AppConfig::Shaders        -> "shaders/hello_game"
// Gyo::AppConfig::CommonAssets   -> "assets/common"
// Gyo::AppConfig::BuiltinShaders -> "shaders/builtin"
```

The header lives at the app binary directory's `generated/gyo/AppConfig.hpp`; do not edit, copy or commit it. It contains package-relative strings and no source-root path. Resolve these strings from the executable directory when opening deployed content. Do not put this app-private header in public engine APIs or replace it with hard-coded `assets/object_fps` paths. Identity selects deployment locations, not the meaning of game data.

<a id="content"></a>
## 3. Add app-owned assets and shaders

For an app with an asset catalog and SDL_GPU shader bundle, first declare all required components before the discovery guard, including `SDL_GPU` and any loaders. After creating `${app}`, use:

```cmake
gyo_app_deploy_content(TARGET ${app}
    ASSETS
    COMMON_ASSETS
    BUILTIN_SHADERS
    SHADER_SPEC shaders/bundle.json)
```

No quality or package registration is required. Omit any content flag the app does not use. `ASSETS` requires `<GYO_APP_ASSET_ROOT>/asset_catalog.json`; missing content fails configure instead of generating a broken package. `COMMON_ASSETS` uses the repository's existing `assets/common` root. The helper stages content when the executable is built and installs the same content under `bin/`:

| Input | Build output / executable-relative deployment | Installed content entry point |
|---|---|---|
| `assets/<name>/` or `ASSET_ROOT` | `assets/<name>/` | `bin/assets/<name>/asset_catalog.json` |
| `assets/common/` | `assets/common/` | `bin/assets/common/asset_catalog.json` |
| App `SHADER_SPEC` | Build: `<app-binary-dir>/shaders/`; deployment: `shaders/<name>/` | `bin/shaders/<name>/manifest.json` |
| Engine builtin shaders | Build: `<build>/shaders/builtin/`; deployment: `shaders/builtin/` | `bin/shaders/builtin/manifest.json` |

In a root build, `<app-binary-dir>` is normally `<build>/apps/<name>`; do not write game shader output to `<build>/shaders/<old-app>`. `SHADER_SPEC` resolves from the app's source directory. Shared builtin HLSL and its ABI stay in Render. Identical common content may be shared by multiple apps; conflicting content mapped to one destination is rejected.

CMake declares whole content roots and the shader compilation specification. The
catalog owns individual asset filenames, types and relative paths; app/test code
selects assets by `AssetId` through the catalog and `AssetManager`. Do not add a
model-specific filename or source-root compile definition to a CMake target when
adding or changing a model. The standard catalog and shader manifests remain
package entry points, not a second list of individual game assets.

The helper groups ordinary compiler/target/content work. It does not invent
catalogs, shader source, gameplay or validators. Runtime loading uses the deployed
content independently of any acceptance implementation. Optional output variables
such as `OUT_REQUIRED_FILES` and `OUT_STAGE_TARGET` are available to integrations;
a product need not request them for quality or packaging to discover its metadata.

<a id="manual-copy"></a>
## 4. Start by manually copying an existing app

An independent app copy is a normal source/content copy inside this repository. For example, use the file manager to copy:

```text
apps/object_fps/   -> apps/my_game/
assets/object_fps/ -> assets/my_game/
```

Copy ordinary files, not links to the original content. Do not copy build directories, generated headers, caches or install stages. For the Object_FPS product, copy `CMakeLists.txt`, `sources.cmake`, `main.cpp`, `include/`, `src/` and `shaders/`. The `tests/`, `packaging/` and documentation directories are optional; copy them only if the new project needs their quality/acceptance procedures. The large local `art_source/` and authoring `tools/` directories are not required to build or run the app. Leave `assets/common`, `render/shaders/builtin` and public GYO modules shared. Keep product shader sources with the code. Ordinary startup health checks and interactive preview remain runtime features. Automated gameplay, package and GPU probes live in `tests/diagnostics/`; the optional quality/release adapters attach them to the executable, without requiring doctest for release-only configuration. An app without assets needs only its code directory until it requests `ASSETS`.

Then add the intended CSV row, preserving existing rows:

```csv
my_game,Independent game experiment,,1,1,1,1
```

Keep the copied helper-based `CMakeLists.txt` unchanged unless the new app has different product source files, component requirements or content. The CSV/directory identity automatically changes the main executable, other targets, test-name prefix, generated config, asset/shader deployment paths and package identity. An inherited explicit `DISPLAY_NAME` may be changed locally; an inherited `ASSET_ROOT` override should be removed for the normal `assets/my_game` layout, or deliberately pointed at this copy's own content.

Do not perform a global text replacement of `object_fps`. C++ domain namespaces, catalog AssetIds such as `object_fps.*`, UI action identifiers and shader IDs such as `game/object_fps/channel_swap` are internal data contracts. They can stay unchanged in an independent app. Their files are resolved against the new app's root, and each process loads its own catalogs/bundles. Rename internal IDs only as a separate, coordinated code-and-data migration. The helper changes build/deployment identity; it does not rewrite game semantics.

Editing `assets/my_game` or `apps/my_game/shaders` changes only the copy. Editing intentionally shared `assets/common` or builtin shaders affects their consumers. If a previously shared resource must diverge, put the divergent data in the copy's asset/shader root and update its catalog/material references rather than silently altering the common root.

Build both apps together after enabling both rows and the target platform:

```sh
cmake -S . -B build/two-apps -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DGYO_APPS=object_fps;my_game" -DGYO_BUILD_UI_EDITOR=OFF
cmake --build build/two-apps
```

This confirms product target names do not collide. Use a separate app-only build and fresh stage for an isolated ordinary installation:

```sh
cmake -S . -B build/my-game -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGYO_APPS=my_game -DGYO_BUILD_UI_EDITOR=OFF
cmake --build build/my-game
cmake --install build/my-game --prefix /absolute/path/to/my-game-stage
```

The resulting executable is `bin/gyo_my_game`, content roots are `bin/assets/my_game` and `bin/shaders/my_game`, without a quality manifest or acceptance scripts. Run the installed executable from outside that stage; it must not rely on the original game's private assets or source tree. Explicit packaging and its manifest-declared checks are a separate, optional operation below.

To remove an app, remove or disable its CSV row before removing its code and private assets. Reconfigure to recompute capabilities. No shared CMake, preset or workflow app-name table needs editing. Removal of private content does not imply removal of shared common assets.

<a id="quality"></a>
## 5. Add quality and packaging only when needed

The engine-managed integration discovers optional app-local adapters after the
product targets exist. Product `CMakeLists.txt` and `sources.cmake` do not include
or enumerate test sources, test registration, acceptance scripts or packaging
rules. Missing quality files are valid for ordinary product configuration.

| Layer | Opt-in | Registration / ownership |
|---|---|---|
| Product | ordinary `dev` build | Product `CMakeLists.txt`, `sources.cmake`, runtime assets and normal install. |
| Quality | `BUILD_TESTING=ON`; `test` preset | Optional `tests/Tests.cmake`; separate `tests/sources.cmake` when grouped source lists are useful. |
| Packaging / CI management | `GYO_ENABLE_PACKAGING=ON` explicitly | Optional `packaging/Package.cmake`; strict acceptance is required only for this requested operation. |

The adapters execute in isolated functions within the app source/binary directory.
They query engine-managed product metadata, not the product CMake file's local
variables. Paths inside the adapters remain relative to the app root, for example
`tests/DomainTests.cpp`. To add a domain test in `tests/Tests.cmake`:

```cmake
gyo_app_get_target(domain OUT_TARGET domain)
gyo_app_add_executable(domain_tests OUT_TARGET tests
    SOURCES tests/DomainTests.cpp
    LIBRARIES ${domain} doctest::doctest)
gyo_app_add_test(NAME domain COMMAND ${tests} TIMEOUT 30)
```

This assumes the product actually defines a `domain` role and that the new test
file supplies the doctest entry point. `gyo_app_add_test` prefixes names with the
app ID, defaults to the `cpu` label and accepts `LABELS`, `TIMEOUT`, `ENVIRONMENT`,
`WORKING_DIRECTORY` and `RUN_SERIAL`. Domain assertions may know the game's rules;
that knowledge stays in quality code and does not reverse the dependency.

Asset tests consume the same staged, executable-relative `Gyo::AppConfig` roots
as the product. When the main executable declares content, retrieve its stage
through product metadata:

```cmake
gyo_app_get_target(main OUT_TARGET app)
get_target_property(content_stage ${app} GYO_APP_CONTENT_STAGE_TARGET)
if(content_stage)
    add_dependencies(${tests} ${content_stage})
endif()
```

Helper-created test executables share the app's binary directory. Their content
dependency does not require compiling the main game. Asset tests normally use
`AssetId`, catalog and `AssetManager`; truncated-byte or native-loader reference
tests may read bytes through catalog lookup and `IAssetSource`. Neither path
receives a specific model filename or source-root compiler definition.

Run quality separately from ordinary development:

```sh
cmake --preset test
cmake --build --preset test
ctest --preset test
```

For a product that exposes a suitable finite startup mode, its optional
`packaging/Package.cmake` can register acceptance without repeating the main target
or content list:

```cmake
gyo_register_app_package(STARTUP_ARGS --startup-smoke-test STARTUP_TIMEOUT 30)
```

`TARGET` may still be supplied for a legacy registration. The main target and
required content are otherwise obtained from product metadata. Target/runtime
installation is idempotent; ordinary `MAIN` installation does not depend on this
registration. Add `gyo_add_app_package_check` declarations only in the packaging
adapter, using the existing [package contract](architecture.md#app-package-contract).
The minimal `hello_game` above deliberately has no startup flag or package adapter;
adding automated acceptance is a later product/quality decision, not a prerequisite
for creating, running or installing the app.

With a package adapter and its quality implementation available, request packaging
explicitly, choosing a fresh build and stage:

```sh
cmake -S . -B build/my-game-package -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGYO_APPS=my_game -DGYO_BUILD_UI_EDITOR=OFF -DGYO_ENABLE_PACKAGING=ON
cmake --build build/my-game-package
cmake --install build/my-game-package --prefix /absolute/path/to/my-game-package-stage
```

This axis generates `share/gyo/apps/my_game/manifest.json` and installs the declared
acceptance tools. A missing package adapter, startup contract or required evidence
fails the explicitly requested packaging operation; it does not make an ordinary
product invalid. `BUILD_TESTING` and `GYO_ENABLE_PACKAGING` are independent axes.

Object_FPS's optional package-check implementation lives in `tests/package/`;
its registration is `packaging/Package.cmake`. Copy `package_info.py` with those
validators when opting into their use. It discovers the sole
`share/gyo/apps/*/manifest.json` and obtains the executable from it. Ordinary
products and installs do not import or install these Python helpers.

## 6. Project/CI management consumes those layers

CI explicitly opts into the testing/packaging operations it needs; product builds
do not infer them from running under CI. Three-platform baseline jobs check the
engine/tools and helper contracts. Selected apps receive independent jobs from
the CSV without a workflow table of game names.

The Windows release baseline additionally runs
[app_copy_integration.py](../tools/ci/tests/app_copy_integration.py), a regression
fixture rather than an app creation utility. It verifies independent copied
code/assets, combined target names and isolated installs/packages in a scratch
tree. Product independence must also hold with quality/CI/acceptance files removed,
not merely with tests skipped. The fixture retains diagnostic logs, preserves
the developer's registry, and does not upload temporary copied-app archives as
release artifacts.

A local regression run requires an initialized native Windows/MSVC environment
and an unused scratch path. An existing cache supplies only selected native tools
and dependency sources, not its app selection or binaries:

```sh
python tools/ci/tests/app_copy_integration.py --workdir /absolute/path/to/new-copy-check --reuse-cache /absolute/path/to/existing-build/CMakeCache.txt --parallel 1
```

These commands state verification procedures, not successful results. Use the
run's logs, `integration-result.json` and CI summary as evidence. Existing unrelated
apps, local ignore rules and CSV versions/activation settings remain user-owned.
