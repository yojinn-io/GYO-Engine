# object_fps_pvp 的音效

| 檔案 | 用途 | 原檔（`source/`） |
|---|---|---|
| `gun_shoot.wav` | 射擊 | 44.1 kHz、32 位元整數、立體聲，0.320 s |
| `hit_flesh.wav` | 受擊 | 同上，0.365 s |
| `gun_reload.wav` | 換彈 | 同上，0.958 s |
| `player_footstep.wav` | 腳步 | 同上，0.179 s |

- 來源：2026-10-30 使用者提供（放在主 checkout 的 `assets/object_fps_pvp/audio/`）。授權：未記錄，待使用者補上。
- 本資料夾的 `*.wav` 是 `GYO::Audio` 的片段格式（48 kHz、PCM16、原聲道數），由下面的命令從 `source/` 轉出，可重現（同一輸入得到同樣的位元組）：

```bash
python3 docs/architecture/plans/audio/scripts/convert_wav.py assets/object_fps_pvp/audio/source/gun_shoot.wav assets/object_fps_pvp/audio/gun_shoot.wav assets/object_fps_pvp/audio/source/hit_flesh.wav assets/object_fps_pvp/audio/hit_flesh.wav assets/object_fps_pvp/audio/source/gun_reload.wav assets/object_fps_pvp/audio/gun_reload.wav assets/object_fps_pvp/audio/source/player_footstep.wav assets/object_fps_pvp/audio/player_footstep.wav
```

- `source/` 不登錄在 catalog，所以不會被打包。之後匯入與轉換由 assets editor 接手（`v8-tool-assets-editor` 的需求）。
