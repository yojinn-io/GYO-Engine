# GYO authoring tools

`gyo_ui_editor` is an independent executable for authoring versioned
`gyo.ui` JSON documents. It links the public `GYO::Ui` model/codec/runtime and
the engine's asset/text adapters; it never links an application such as
Object_FPS or an SDL_GPU backend.

Typical workflow:

```text
gyo_ui_editor --open working/menu.ui.json --output exports/menu.ui.json --asset-catalog path/to/app/asset_catalog.json --asset-root path/to/app/assets
gyo_ui_editor --validate exports/menu.ui.json --asset-catalog path/to/app/asset_catalog.json --asset-root path/to/app/assets
```

The catalog mount is read-only and supplies AssetId pickers plus preview data.
Export writes exactly one canonical JSON file, atomically, and refuses every
destination inside the mounted asset root. The author explicitly copies that
JSON into the application assets and registers it in the application's catalog;
the editor never copies assets, edits a catalog, or publishes application data.

The executable owns only authoring concerns (windows, selection, transactions,
gizmos, diagnostics, transient SDL preview textures). Layout evaluation,
clipping, draw order, hit testing, typed actions, and preview binding semantics
come from `GYO::Ui`. A future map editor should be a sibling executable with its
own document/session/UI modules and depend on public engine APIs in the same
direction, rather than adding map concepts to this UI editor core.

Editor chrome loads an installed system CJK font (with a Japanese fallback on
Windows) so UTF-8 document text remains readable in ImGui fields and logs. This
font is never serialized. Canvas preview deliberately continues to use the
document's catalogued `FontAsset`; a missing glyph inside the Canvas therefore
means the app-selected font does not contain that character.
