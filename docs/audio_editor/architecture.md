# GYO Audio Editor

A design tool that turns a WAV from anywhere into a clip the engine can play,
writes it into a mounted product's asset root and registers it in that
product's catalog. It is one tool of the asset toolchain
([plan](../tools/plans/v8/assets.md)): the tool processes material, the
catalog registers the result, and the runtime only loads it.

Owner: `audio_editor` (`tools/audio_editor/`).

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's
agreement, however small (AGENTS.md section 3). So is changing this section.

1. The editor links no game, no `GYO::Input` and no SDL_GPU. Playback goes only
   through `GYO::Audio` and its SDL backend. Guard: the editor's dependency
   guard, a positive list of includes and targets.
2. The output follows the format the engine publishes. The sample rate is taken
   from `GYO::Audio` (`kSampleRate`, `engine/audio/include/engine/audio/AudioFormat.hpp`);
   the editor keeps no copy of that number. The sample encoding is whatever the
   engine's `SoundLoader` and `ClipBank` load. When the engine changes its rate,
   rebuilding the editor follows it; only a new encoding (float, compressed)
   needs an editor change. Guard: every converted output is loaded through
   `SoundLoader` and accepted by `ClipBank`.
3. The editor writes only what it registers: the output file and the catalog
   entry with the same id. It leaves every other entry, every other file and
   product data (such as a product's sound cue table) untouched. The catalog is
   written by atomic replace and checked against the content contract (unique
   ids, paths inside the asset root). This differs from ui_editor, whose mounted
   catalog is read-only.
4. Editing is non-destructive. The source file is only read. Edits live in the
   editor's project file, and the output is always regenerated from the source
   and those settings.
5. The project file lives outside the asset root; saving it inside the root is
   refused, because every file under the root must be registered.
6. What the preview plays is what export writes: the same function and the same
   bytes. The preview volume applies only at playback and never reaches the
   output; the gain is the setting that changes the output.
7. Conversion is reproducible: the same source, the same settings and the same
   platform give the same bytes.
8. The editor is neutral. It knows clips, not what a product means by them: no
   cue names and no branch on a product.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the editor.

- Loops, streaming or compressed formats are needed (also `GYO::Audio`'s own
  Replacement Criteria 1 and 2).
- Several tracks or a mixing timeline are needed, turning the editor into a
  sequencer.
- Registering a sound requires editing a product's own JSON.
- Formats other than WAV must be read, which needs decoders.
