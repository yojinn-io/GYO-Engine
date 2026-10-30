# GYO UI editor

`gyo_ui_editor` is a design tool for versioned `gyo.ui` JSON documents. It links the engine and public UI model/codec/runtime statically; SDL/ImGui supply its GUI host. It has no dependency on a game, game assets or SDL_GPU.

From the repository root:

```sh
cmake --preset dev -DGYO_APPS= -DGYO_TOOLS=ui_editor
cmake --build --preset dev --target gyo_ui_editor
```

The runnable editor is assembled under `build/target/toolchain/bin`. Every release platform includes the GUI editor in its fixed toolchain archive, even when no games are enabled in `engine/config/projects.csv`. Editor code is not bundled into game products.

The optional standalone entry forwards to the same root graph:

```sh
cmake -S tools/ui_editor -B build/target/_build/ui-editor
cmake --build build/target/_build/ui-editor --target gyo_ui_editor
```

`-DGYO_TOOLS=ui_editor:cli` selects the `cli` variant, a local CLI validation build (also accepted by the standalone entry). This is not the GUI toolchain artifact used for release. Enable `BUILD_TESTING` to register the central engine/editor tests; editor tests and fixtures live in `tests/ui_editor`, outside this product directory.

```text
gyo_ui_editor --open working/menu.ui.json --output exports/menu.ui.json --asset-catalog assets/game/asset_catalog.json --asset-root assets/game
gyo_ui_editor --validate exports/menu.ui.json --asset-catalog assets/game/asset_catalog.json --asset-root assets/game
```

In the GUI, **File > Open Asset Folder...** picks an asset root in the operating system's folder dialog and mounts it with its `content.json`; Open, Export and the mount dialog's fields also offer the system dialogs, with typed paths as the fallback. A mounted catalog is read-only. Export writes exactly one canonical JSON file atomically and refuses destinations inside the mounted asset root. Authors copy the chosen document into `assets/<game>/`, register it in that game's catalog, and build. The build's asset assembly deploys the prepared content automatically; the editor never edits catalogs or publishes assets.

On the canvas the selected element shows Unity-style handles: drag its body to change `position`, its edges and corners to resize it (`size_delta` and `position`; the anchors stay), and the anchor and pivot handles to change those values while the element's rectangle stays where it is. Equal anchors move together when one is dragged toward the other's side and split when it is dragged away. With **Snap** on, only the coordinates a drag moves are rounded to the grid.

The editor owns authoring sessions, selection, undo, diagnostics and transient preview resources. Layout, clipping, draw order, hit testing, bindings and typed actions come from `GYO::Ui`. Future map/design editors remain sibling tools depending on public engine APIs.

Editor chrome uses an installed CJK font; canvas text deliberately uses the document's catalogued `FontAsset`. Chrome fonts are not serialized or added to the game asset catalog.

See [architecture](../../docs/architecture.md), [UI contract](../../docs/ui_toolchain.md) and [game creation](../../docs/creating_apps.md).
