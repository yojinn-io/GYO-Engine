# GYO Audio Editor

Turns a WAV from anywhere into a clip the engine plays, writes it into a
product's asset root and registers it in that product's catalog. Design and
Design Invariants: [docs/audio_editor/architecture.md](../../docs/audio_editor/architecture.md).

This version is command-line only (`v8-tool-audio-editor-convert`); the window
with playback, waveform, trimming and gain comes with `v8-tool-audio-editor-edit`.

## Build

```sh
cmake --preset dev -B build/target/_build/audio -DGYO_APPS= -DGYO_TOOLS=audio_editor -DBUILD_TESTING=ON
cmake --build build/target/_build/audio --target gyo_audio_editor
```

The tool is local (`release=false` in `engine/config/tools.csv`); the
executable is under `build/target/_tools/audio_editor/bin`.

## Use

```sh
gyo_audio_editor info source.wav
gyo_audio_editor convert source.wav --asset-root assets/<product> \
    --asset-catalog assets/<product>/asset_catalog.json \
    --id <product>.audio.reload --path audio/reload.wav
```

- Input: RIFF/WAVE, integer PCM of 8, 16, 24 or 32 bits or 32-bit float, 1 or 2
  channels, any sample rate.
- Output: the engine's clip format (`GYO::Audio` `kSampleRate`, PCM16), the
  source's channel count. Resampling is that of
  `docs/architecture/plans/audio/scripts/convert_wav.py`; the dither is the
  tool's own, seeded by the asset id, so one id always gets the same bytes on a
  platform.
- Registration: the `sound` entry with that id is added after the last entry or
  rewritten in place; nothing else in the catalog changes. An id registered with
  another type, or a path registered under another id, is refused. When an
  entry's path changes, the old file is reported, not deleted.
- The source is only read, and a clip path that is the source or the catalog is
  refused. Keep sources outside the asset root: every file there must be
  registered.

Exit status: 0 done, 2 usage, 3 a file could not be read or written, 4 an
invalid WAV, catalog or registration.
