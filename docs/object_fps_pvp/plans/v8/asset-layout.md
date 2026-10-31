# PvP v8：素材結構標準化（asset-layout）

更新：2026-10-31。Owner：`object_fps_pvp`（另含 Engine 層的 `v8-engine-assets-common-retire`）。**狀態：`v8-product-asset-layout-doc`、`v8-product-asset-layout-guard` 完成，分支推送為 `claude/v8-asset-layout`；接著 `v8-product-asset-player-folder`。**
本文件是 asset-layout 線的計畫與記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。標準本身寫在 [PvP 素材標準](../../assets.zh-Hant.md)。

## 代號對照

| 代號 | 白話 |
|---|---|
| asset-layout | 這條 PR 線：PvP 的素材資料夾照同一套標準擺放 |
| `v8-product-asset-layout-doc` | 寫 PvP 素材標準、例外的政策、Design Invariants、來源記錄 |
| `v8-product-asset-layout-guard` | 在既有的 catalog 測試加結構檢查（只締不緩） |
| `v8-product-asset-player-folder` | `players/` 的 3 個定義檔併進 `characters/player/` |
| `v8-product-asset-buns-fixture` | buns 髮型與頭髮貼圖改成測試專用的 fixture，不再出貨 |
| `v8-product-art-source-out` | 主 checkout 的 5 個交付檔移到 repo 外 |
| `v8-engine-assets-common-retire` | 刪掉沒有 owner 的 `assets/common` |
| `v8-product-asset-path-loaded-move` | ［延後］arena 搬進 `maps/`，`movement_rules.json` 搬進 `rules/` |

## 決定（2026-10-31）

架構會話以 origin/master `3d5fe1a5` 核對後提出 13 項建議，使用者核准：「素材結構的決定照你的建議做」。其中標「調整」的 4 項，是依 10-31 現況改過的建議，本會話開工時再用白話確認一次，使用者回覆：「照建議」。

| 代號 | 決定 |
|---|---|
| `asset-std-scope` | 標準只寫在 PvP（[素材標準](../../assets.zh-Hant.md)＋`tests/object_fps_pvp/test_asset_catalog.py`），刪 PvP 時一起刪。共通層不出現 PvP 的路徑（AGENTS §7、§9）。 |
| `asset-std-invariants` | 素材標準加 Design Invariants 節與 Replacement Criteria。「依路徑載入的檔放哪裡」不寫成固定清單：hitbox 由誰、怎麼讀還沒定。條文先給使用者看，同意後才提交。 |
| `asset-source-home` | 原始檔（DCC 原檔、交付包、原始錄音）不進 repo，放 repo 外的儲存。 |
| `asset-source-record`（調整） | 來源與授權記錄併進 doc 批（原本要併進 `v8-product-sfx`，但 sfx 已合併、沒有做）。pack 的授權沿用 [3d-assets.md](../../../architecture/3d-assets.md) 已有的紀錄，不在兩處維護；沒有資料的寫 unrecorded，不猜。做成守衛會讀的檔。 |
| `assets-common-retire` | 刪 `assets/common`。 |
| `asset-buns-fixture` | buns 與頭髮貼圖改成 `PlayerPresentationTests` 專用的測試 fixture，不再出貨。 |
| `asset-id-check` | 守衛只檢查 id 的語法，以及 type 和副檔名是否對得上；不檢查 kind 段（否則 1/3 的既有 id 會成為永久例外）。 |
| `asset-version-naming` | 換版本時照現行慣例新增 id（`player_v19` → `player_v20`）。 |
| `asset-match-only-data`（調整） | 新增的 Match 規則資料放 `rules/<topic>.json`，列進同一份 catalog。已在根目錄、依路徑載入的 `movement_rules.json` 列為例外，由 `v8-product-asset-path-loaded-move` 搬。`rules/` 的名稱與位置先和負責新規則的會話對齊。 |
| `asset-arena-maps`（調整：範圍擴大） | 地圖變多或 voxel 地圖出現時，arena 一組平放進 `maps/`，`movement_rules.json` 同批搬進 `rules/`。代號改成 `v8-product-asset-path-loaded-move`。搬時要改 C++、測試、6 個驗收 runner，還有驗收常數 `content_deep_path`（AGENTS §14.4，要核准）。 |
| `asset-sfx-script-home` | 音效等生成腳本放 `apps/object_fps_pvp/<子目錄>/`，腳本裡不寫 AssetId。 |
| `asset-layout-pr-line` | 從 master 切一條小線，盡量一個 PR；v8 各線動到素材前先同步 master。計畫登記在本文件；v8 README 的連結行由 v8 規劃會話在它的分支加。 |
| `asset-untracked-intake`（調整） | 第三人稱 nailer 已由 PR #120 放進 `world/`，`v8-product-asset-nailer-world` 取消；4 個原始 WAV 已不在主 checkout。剩 5 個交付檔移到 repo 外（是移，不是刪）。位置待定：原本預設的 `~/gyo-art-source/` 在 home 底下，和「不在 home 放東西」的指示衝突，`v8-product-art-source-out` 前請使用者選。 |
| `v8-product-asset-invariants-approve` | （doc 批）核准 [素材標準](../../assets.zh-Hant.md) 的 Design Invariants DI-1～DI-6、Replacement Criteria，以及「例外只能減少」政策：新增例外要核准；刪除已解除的例外事先核准，事後在 dev_log 報告 AGENTS §14.4 的 1～5 項。使用者原話：「照草案核准」。 |

## 批次

```text
asset-layout
├── v8-product-asset-layout-doc        medium｜PvP 素材標準、例外政策、Design Invariants、來源記錄
├── v8-product-asset-layout-guard      high  ｜catalog 測試加結構檢查（只締不緩）
├── v8-product-asset-player-folder     medium｜players/ 的 3 個檔併進 characters/player/
├── v8-product-asset-buns-fixture      medium｜buns＋頭髮貼圖改成測試 fixture，不再出貨
├── v8-engine-assets-common-retire     low   ｜刪 assets/common
├── v8-product-art-source-out          low   ｜主 checkout 的 5 個交付檔移到 repo 外（先確認位置）
└── v8-product-asset-path-loaded-move  high  ｜［延後］arena→maps/、movement_rules→rules/
```

| 代號 | 估時 | 支撐的 Feature | 驗收 | 狀態 |
|---|---|---|---|---|
| `v8-product-asset-layout-doc` | 2～3 h | hitbox 資料與之後的 v8 素材需要唯一的落點 | 和 guard 同一個 PR（只改 docs 的推送會跳過 L1）；用腳本核對文件裡的路徑與 id | 完成 |
| `v8-product-asset-layout-guard` | 3～4.5 h（含突變；超過 5 h 停） | 同上：新素材的落點在 CI 上自動檢查 | CI L1 的 `object_fps_pvp.asset_catalog`；自我檢查（注入壞資料夾、跨資料夾貼圖、type 不符、只差大小寫、過期的例外，每種都要被抓到）＋少量突變 | 完成 |
| `v8-product-asset-player-folder` | 1～1.5 h | hitbox 資料（hitbox 和模型放在一起） | CI L1；apps、tests、build 裡 `git grep 'players/'` 為 0（含 `mutations.json` 的突變 `11-limit-uncalibrated` 與 catalog 測試的假路徑）；after-pv8 合併後重跑它的 Match 測試 | 未開始 |
| `v8-product-asset-buns-fixture` | 1.5～2.5 h（開工時重估） | 出貨內容只放產品用到的素材（AGENTS §7） | CI L1；catalog 不再有 buns 的 3 個 id；`PlayerPresentationTests` 用 fixture 通過 | 未開始 |
| `v8-engine-assets-common-retire` | 0.5 h | 沒有（使用者同意的清理） | CI 全量 | 未開始 |
| `v8-product-art-source-out` | 0.25 h | 主 checkout 能跑 ctest | 主 checkout 的 `git status` 沒有未追蹤的素材 | 未開始（位置待定） |
| `v8-product-asset-path-loaded-move` | 4～6 h | 地圖增加、voxel 地圖 | arena digest 搬移前後相同；驗收常數先宣告並經核准 | 延後 |

- 順序：doc＋guard 先做（同一 PR），接著 player-folder、buns-fixture、common-retire，每做完一批就刪掉對應的例外（只締）。art-source-out 不進 PR，在主 checkout 操作。
- 合計估時（不含延後批）：8.25～12.25 h。單批超過估時 1.5 倍、或單一問題累計超過 2 小時就停下回報。

## 協調

- v8 規劃會話（分支 `claude/pvp-v8-after-pv8`）：
  - `mutations.json`：after-pv8 在檔尾加了約 510 行；本線新增的突變不放檔尾。player-folder 要改的那一行（突變 `11-limit-uncalibrated` 的路徑）不在 after-pv8 改動的段落內。
  - v8 README 的連結行由它加。
  - after-pv8 的 Match 以 AssetId 讀 `object_fps_pvp.player.presentation`，所以 player-folder 只改 catalog 的 path；那條線合併後重跑它的 Match 測試。
- 3D model 會話：doc 批合併後告訴它標準。
- 工具線：audio_editor 會寫 catalog；通知它 PvP 的落點由 PvP 守衛檢查，不要求改程式。`docs/tools/plans/v8/assets.md` 用 `players/character.json` 當例子，player-folder 之後通知工具線。

## 現況（每次停下來時更新）

- 2026-10-31：開工前核對 origin/master `3d5fe1a5`，素材與 catalog（31 筆）、主 checkout 的 6 個未追蹤檔、after-pv8 分支都和架構會話的說明相符。另外查到：
  - `tests/object_fps_pvp/mutations.json` 的突變 `11-limit-uncalibrated` 寫了 `players/presentation.json` 的路徑。
  - 4 個音效的原檔在 master 的歷史裡（commit `30b36bdb` 的 `assets/object_fps_pvp/audio/source/`，之後移出）。
  - 證據依 PR #122 的條文：原始輸出放主 checkout 的 `build/evidence/asset-layout-20261031/`（git 不追蹤），數字寫進 dev_log。
- 2026-10-31：使用者確認 4 項調整（「照建議」）。`v8-product-asset-layout-doc` 開始。
- 2026-10-31：素材標準、來源記錄（`tests/object_fps_pvp/asset_sources.json`，16 筆，其中 8 筆授權 unrecorded）的草稿寫好，未提交。停在 Design Invariants 條文與「例外只能減少」政策的核准；人物、nailer 的授權向使用者要。
- 2026-10-31：使用者核准 Design Invariants 與例外政策（`v8-product-asset-invariants-approve`）。人物、nailer 的 8 筆授權尚未提供，維持 unrecorded。
- 2026-10-31：`v8-product-asset-layout-doc` 完成（文件核對：連結 8 個有效，人工判讀 36 項，9 個測試名稱留待 guard 批）。`v8-product-asset-layout-guard` 開始。
- 2026-10-31：`v8-product-asset-layout-guard` 完成：檢查 9 個、自我檢查 10 個、初始例外 13 筆；突變 12／12 killed；`used_ids` 改寫成等價的逐詞比對（測試 12.7 秒 → 1.1 秒）。
- 2026-10-31：推送後合併 master `ebfe3225`（PR #121～#124）。PR #123 新增最上層 `framework/`，DI-6 的掃描改成「最上層除了排除的資料夾以外全部掃」，加一個自我檢查與一個突變（13／13 killed）。

## dev_log

- [2026_10_31_pvp_v8_asset_layout_doc](../../../dev_logs/2026_10_31_pvp_v8_asset_layout_doc.zh-Hant.md)：`v8-product-asset-layout-doc`
- [2026_10_31_pvp_v8_asset_layout_guard](../../../dev_logs/2026_10_31_pvp_v8_asset_layout_guard.zh-Hant.md)：`v8-product-asset-layout-guard`
