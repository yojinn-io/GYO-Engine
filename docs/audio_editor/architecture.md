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

1. The editor links no game and names no other product. It may use any
   Engine module, as an editor depends on the engine; what a game and a tool,
   or two tools, both need comes through the framework first, even where
   Engine has it, and the framework calls Engine. Playback goes through
   `GYO::Audio` and its SDL backend; the editor itself includes no `SDL3/`
   header, calls no `SDL_` function and names no SDL or `*Native` target.
   Guard: the editor's dependency guard, a positive list of targets and
   include prefixes.
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
   written by atomic replace and checked against the content contract (ids
   unique within the catalog, paths inside the asset root); duplicates across
   catalogs and overlaps with shader bundles are decided at load by Engine's
   `ContentManifest`. This differs from ui_editor, whose mounted catalog is
   read-only.
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

## Editing and preview

`gyo_audio_editor edit [<project.json> | <source.wav>]` opens the window
(`v8-tool-audio-editor-edit`); `info` and `convert` stay command-line only.

- **Settings** (`Edit.hpp`): a trim in the source's own frames (start, and an
  end or "to the source's end") and a gain in dB, from -60 to +24. A trim that
  keeps no frame, before or after resampling, is refused (`InvalidSettings`),
  so export never writes a clip the engine's `ClipBank` refuses.
- **One render** (`RenderClip`): trim, gain (`x * 10^(dB / 20)`), resample,
  quantize. Export and the preview both call it with the same dither seed (the
  id's), so the preview's samples are export's data chunk (Design Invariant 6).
  The default settings give the bytes `convert` gave before.
- **Preview** (`PreviewPlayer.hpp`): plays on a `GYO::Audio` mixer opened through
  `SdlAudioOutput`, one clip at a time, at gain 1; the preview volume is the
  mixer's master gain. The mixer has two clip slots and no schedule delay, so a
  stop and an unload take effect in the block that takes them and a new clip
  that finds no free slot starts after the next block. The position shown is
  the mixer's `PlayheadOf`, for display only (`GYO::Audio` Design Invariant 4).
  Without an audio device the editor still edits and exports; nothing is heard.
- **Waveform** (`Waveform.hpp`): the extremes of each column's samples, drawn
  with the gain applied so clipping shows; outside the trim is dimmed.
- **Window**: the framework's ImGui shell (`RunImGuiShell`); its context lends
  Engine's platform, whose path dialogs open WAVs, projects, the asset root and
  the catalog. The editor reaches `GYO::PlatformSDL` only through that binding's
  public interface and does not name the target. Only the latest dialog's answer counts; a Failed answer does not
  end the wait (platform-sdl Design Invariant 10).

## Project file

The editor's own format (Design Invariants 4 and 5); the game never reads it,
and only the editor writes it, so it is not a Data Contract with the runtime.
A UTF-8 JSON object with exactly these fields:

| Field | Value |
|---|---|
| `format` | `"gyo.audio_editor.project"` |
| `version` | `1`; another version is refused |
| `source` | the source WAV, as it was opened (UTF-8) |
| `asset_root`, `asset_catalog` | where export writes, as for `convert` |
| `id`, `path` | the catalog entry |
| `trim_start` | first source frame kept |
| `trim_end` | one past the last frame kept, or `null` for the source's end |
| `gain_db` | the gain, written with every digit it needs |

The same project always gives the same text. A file with other fields, a field
of another type or another `format` is refused (`InvalidProject`); saving it
inside its own asset root is refused too. The settings are checked against the
source when it is opened and again on export.
