# Object_FPS_PVP 資產的來源

依據：移植盤點（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md`）第 6 節；授權的決定見 HANDOFF `v8-arch-gyo2-pvp-assets-license`（使用者 2026-11-23「除了骨骼動畫，都是我自己做的。」）。

| 檔 | 來源 | 授權 |
|---|---|---|
| `audio/gun_shoot.wav`、`audio/gun_reload.wav`、`audio/hit_flesh.wav`、`audio/player_footstep.wav` | 使用者自己的混音（2026-10-30，master `assets/object_fps_pvp/audio/`） | 使用者本人製作 |
| `fonts/BIZUDPGothic-Regular.ttf` | BIZ UDPGothic | SIL OFL 1.1（`BIZUDPGothic-OFL.txt`） |
| `maps/*.json`、`maps/*.voxel_mesh.json` | 產品自己的資料（voxel 地圖編輯器烘出，master `assets/object_fps_pvp/maps/`） | 產品的資料 |
| `characters/stand_in.voxel_mesh.json` | 移植時手寫的替身方塊（`v8-product-pvp-port` 第 2 段；`build_version` 寫 `hand 1`，`source_sha256` 沒有工具的來源，填 0） | 產品的資料 |
| `rules/movement_rules.json`、`ui/hit_feedback.json`、`audio/sfx.json` | 產品自己的資料 | 產品的資料 |
