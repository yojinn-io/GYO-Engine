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

The canvas frames the document's design canvas (`design_canvas.size`) and labels its size; elements outside the frame are clipped, as in the game. The size belongs to the document: **Inspector > Document > Design size** shows it locked, and unchecking **Locked** lets you edit it (editor state only, never saved). On the canvas the selected element shows Unity-style handles: drag its body to change `position`, its edges and corners to resize it (`size_delta` and `position`; the anchors stay), and the anchor and pivot handles to change those values while the element's rectangle stays where it is. Equal anchors move together when one is dragged toward the other's side and split when it is dragged away. With **Snap** on, only the coordinates a drag moves are rounded to the grid. Hold Alt while dragging an edge or corner to resize around the centre, and Shift on a corner to keep the aspect ratio. While the canvas has focus, the arrow keys move the selected element by one design pixel (Shift: ten). A `text_field` (gyo.ui version 2; adding one upgrades the document) shows its max length and placeholder in the Inspector; in **Preview interaction** mode, Enter or a click starts editing it, typing and the editing keys change it, Enter submits (the action log shows the text and the preview binding keeps it) and Escape cancels. The input method's composition is drawn by the operating system there. **Edit > Copy / Paste / Duplicate Element** (Ctrl+C / Ctrl+V / Ctrl+D) copy an element with its children; a paste goes right after the selected element, renames every id it brings (`<id>_copy_<n>`) and adds its focusable elements to the canvas focus order.

The editor owns authoring sessions, selection, undo, diagnostics and transient preview resources. Layout, clipping, draw order, hit testing, bindings and typed actions come from `GYO::Ui`. Future map/design editors remain sibling tools depending on public engine APIs.

Editor chrome uses an installed CJK font; canvas text deliberately uses the document's catalogued `FontAsset`. Chrome fonts are not serialized or added to the game asset catalog.

See [architecture](../../docs/architecture.md), [UI contract](../../docs/ui_toolchain.md) and [game creation](../../docs/creating_apps.md).
