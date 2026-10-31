# PvP v8：素材標準文件（v8-product-asset-layout-doc）

日期：2026-10-31。分支 `claude/jolly-heisenberg-78148d`（從 master `3d5fe1a5`）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。

**狀態：完成。和 `v8-product-asset-layout-guard` 放在同一個 PR（只改 docs 的推送會跳過 L1）。**

## 為什麼

- 支撐的 Feature：hitbox 資料與之後的 v8 素材需要唯一的落點。
- PvP 的人物模型在 `characters/player/`，人物定義卻在 `players/`；定義檔有兩種命名；只有測試在用的 buns 跟著出貨；原始檔沒有固定位置；新素材該放哪裡沒有寫下來，也沒有檢查。

## 做了什麼

- 新增 [PvP 素材標準](../object_fps_pvp/assets.zh-Hant.md)：結構、各種檔案的位置、命名、AssetId 與 type、依路徑載入的檔、只有測試用的素材、來源與授權、例外的政策、Design Invariants（DI-1～DI-6）與 Replacement Criteria。
- 新增 [asset-layout 計畫](../object_fps_pvp/plans/v8/asset-layout.md)：13 項決定、批次表、協調、現況。v8 README 的連結行由 v8 規劃會話在它的分支加。
- 新增來源記錄 `tests/object_fps_pvp/asset_sources.json`：catalog 裡 16 個不是 text 的項目各一筆（source、license、record）。守衛在 guard 批加入。
- 沒有改素材、catalog、程式與測試。

## 決定與核准

- 架構會話的 13 項建議：使用者核准「素材結構的決定照你的建議做」。其中依 10-31 現況調整的 4 項，開工時再確認，使用者回覆「照建議」。
- Design Invariants 條文與「例外只能減少」政策（`v8-product-asset-invariants-approve`）：使用者回覆「照草案核准」。和架構會話的草案相比有兩處不同：
  - DI-1 加上「出貨的素材要被產品程式用到，只被測試引用的不算」。只算 `apps/object_fps_pvp/` 時，被抓出來的正好是 buns 的 3 個 id（`hair.buns`、`model.hair.buns`、`texture.character.female.hair`），沒有別的。
  - DI-3 適用於所有 text 項目，不只角色和武器的定義。
- 命名、id 與 type、來源記錄也由守衛檢查，但不列為 Design Invariants，因為它們是可以隨 Feature 調整的細節。

## 來源記錄的依據

| 項目 | 依據 |
|---|---|
| 4 個音效 | 使用者自己混剪（[sfx 的 dev_log](2026_10_30_pvp_v8_product_sfx.zh-Hant.md) 記錄的使用者原話） |
| `UAL1_Standard.fbx` | FBX 內嵌的原檔路徑寫著 `Universal Animation Libraries\UAL1_Standard.blend`；pack 授權 CC0 記在 [3d-assets.md](../architecture/3d-assets.md) |
| `Hair_Buns.fbx` | FBX 內嵌的原檔路徑寫著 `Universal Base Characters\Hairstyles\Hairstyles_Female.blend`；同上 CC0 |
| `T_Hair_2_BaseColor.png` | 和 `object_fps` 的 superhero_female 貼圖是同一個 blob（`bda8d959`），3d-assets.md 記載那些貼圖是 Universal Base Characters 的複本；CC0 |
| 字型 | 字型檔的 name table：Copyright 2022 The BIZ UDGothic Project Authors；SIL Open Font License 1.1 |
| Player_V20、Nailer1（模型與 4 張貼圖）、Nailer2（模型與貼圖） | 交付的 commit 與 FBX metadata；授權沒有資料，8 筆寫 `unrecorded`，已向使用者詢問 |

另外查到：4 個音效的原檔在 master 的歷史裡（commit `30b36bdb` 的 `assets/object_fps_pvp/audio/source/`，同日移出）。工具線的交接文件寫「原檔已找不到」，之後通知工具線。

## 驗證

- 文件核對腳本（抽出兩份文件裡的連結、反引號中的路徑、id 與測試名稱，對照 repo 與 catalog）：連結 8 個全部有效；repo 路徑 17 個、素材路徑 53 個、id 3 個、測試名稱 13 個；有 36 項要人工判讀：
  - 9 個測試名稱：guard 批才加入，在 guard 批重跑確認。
  - 27 項逐一看過，都是刻意的：標準裡預定的資料夾與檔名（`maps/`、`rules/`、`ui/textures/`、`hitbox.json`、`viewmodel.json` 等）、副檔名表、CTest 名稱 `object_fps_pvp.asset_catalog`、分支名、歷史裡的路徑、主 checkout 的證據資料夾、原本預設的 `~/gyo-art-source/`。
- 證據：腳本與輸出在主 checkout 的 `build/evidence/asset-layout-20261031/`（`doc_check.py`、`doc-check-doc-batch.txt`，git 不追蹤）。

## 範圍的限制

- 8 筆授權仍是 `unrecorded`，等使用者提供。
- repo 外美術儲存的位置還沒定，在 `v8-product-art-source-out` 前請使用者選。
