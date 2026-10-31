# PvP v8：玩家定義併進 characters/player/（v8-product-asset-player-folder）

日期：2026-10-31。分支 `claude/v8-asset-layout`（PR [#125](https://github.com/yojinn-io/GYO-Engine/pull/125)）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。標準：[PvP 素材標準](../object_fps_pvp/assets.zh-Hant.md)。

**狀態：完成。**

## 為什麼

- 支撐的 Feature：hitbox 資料。hitbox 綁在人物骨架上，要和模型、定義放在同一個資料夾（`characters/player/hitbox.json`）。
- 人物模型在 `characters/player/`，人物定義卻在 `players/`，違反標準的「依對象分資料夾」（DI-2）與「自足引用」（DI-3）。

## 做了什麼

- `players/presentation.json`、`players/character.json`、`players/animations.animset.json` 以 `git mv` 搬到 `characters/player/`，內容不變；刪掉留下的空資料夾。
- `asset_catalog.json`：這 3 筆只改 `path`，`id` 不變（DI-4）。C++ 以 AssetId 讀取，所以程式不用改。
- `mutations.json`：突變 `11-limit-uncalibrated` 的 `file` 改成新路徑。
- `test_asset_catalog.py`：刪掉 4 筆例外（`players/` 的 3 筆 first-folder，加上 `players/character.json` 的 own-files）與不再使用的常數；`test_the_check_finds_an_orphan` 的假路徑改成新路徑（假資料，判定不變）。

## 判定變更的報告（AGENTS §14.4，刪除例外，已事先核准）

1. 守的性質：第一段資料夾只能是標準列出的種類（DI-2）；text 項目引用的模型在自己的資料夾（DI-3）。
2. 緩和的地方：沒有。
3. 收緊的地方：`players/` 底下的檔、以及 `characters/player/character.json` 以外位置的人物定義引用 `Player_V20.fbx`，從「例外放行」變成失敗。
4. 沒有緩和，不適用。
5. 根據：`test_every_exception_is_still_needed` 在搬移後要求刪掉這 4 筆（不刪會失敗）；刪掉後 `test_paths_follow_the_layout`、`test_definitions_reference_their_own_files` 通過。

## 驗證

- 搬移後 `git grep 'players/'` 在 `apps/`、`tests/`、`build/` 為 0 筆。
- catalog 的 diff 只有 3 行 `path` 改變，`id` 相同（DI-4）。
- test preset 建置 `gyo_object_fps_pvp_presentation_tests`：組裝後的產品素材只有 `characters/player/presentation.json`，沒有留下 `players/`。
- CTest：`object_fps_pvp.presentation_cpu`、`object_fps_pvp.asset_catalog`、`object_fps_pvp.mutation_tool` 通過。
- 突變：路徑改了的 `11-limit-uncalibrated` 重跑，1／1 killed。
- 證據：主 checkout 的 `build/evidence/asset-layout-20261031/mutations-player-folder.json`（git 不追蹤）。

## 範圍的限制

- after-pv8 分支（`claude/pvp-v8-after-pv8`）的 Match 以 AssetId 讀 `object_fps_pvp.player.presentation`，不受路徑影響；那條線合併後要重跑它的 Match 測試。
- `docs/tools/plans/v8/assets.md` 拿舊路徑當例子，屬工具線，已通知。舊計畫（v5、v6）與 dev_log 裡的 `players/` 是當時的紀錄，不改寫。
