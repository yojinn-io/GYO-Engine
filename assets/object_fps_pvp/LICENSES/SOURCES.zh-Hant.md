# Object_FPS_PVP 資產的來源

依據：移植盤點（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md`）第 6 節；授權的決定見 HANDOFF `v8-arch-gyo2-pvp-assets-license`（使用者 2026-11-23「除了骨骼動畫，都是我自己做的。」）。

| 檔 | 來源 | 授權 |
|---|---|---|
| `audio/gun_shoot.wav`、`audio/gun_reload.wav`、`audio/hit_flesh.wav`、`audio/player_footstep.wav` | 使用者自己的混音（2026-10-30，master `assets/object_fps_pvp/audio/`） | 使用者本人製作 |
| `fonts/BIZUDPGothic-Regular.ttf` | BIZ UDPGothic | SIL OFL 1.1（`BIZUDPGothic-OFL.txt`） |
| `maps/*.json`、`maps/*.voxel_mesh.json` | 產品自己的資料（voxel 地圖編輯器烘出，master `assets/object_fps_pvp/maps/`） | 產品的資料 |
| `animations/ual_mannequin/UAL1_Standard.fbx` | Universal Animation Library（master `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A.3） | CC0 |
| `characters/player/Player_V20.fbx` | 使用者在 Blender 做的（2026-10-29 交付） | 使用者本人製作 |
| `weapons/nailer_v2/viewmodel/Nailer1_FPS.fbx` 與 `textures/*.png` | 使用者交付（2026-10-29）；PNG 由交付的 JPG 與單色材質轉成 | 使用者本人製作 |
| `weapons/nailer_v2/world/Nailer2_TPS.fbx` 與 `textures/Nailer2_D.png` | 使用者在 Blender 做的（2026-10-31）；PNG 由 FBX 內嵌的 JPG 轉成 | 使用者本人製作 |
| `characters/player/*.json`、`weapons/**/*.json` | 產品自己的資料 | 產品的資料 |
| `rules/movement_rules.json`、`ui/hit_feedback.json`、`audio/sfx.json` | 產品自己的資料 | 產品的資料 |
