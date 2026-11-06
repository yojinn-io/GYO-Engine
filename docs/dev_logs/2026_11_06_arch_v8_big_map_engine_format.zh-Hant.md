# 架構 v8：v8-arch-big-map-engine-format（區塊地圖格式與標記層）

日期：2026-11-06。分支 `claude/v8-arch-big-map-engine-format`（rebase 到 master `c70e8fdc`，即 PR #161 合併後）。PR #162。
段：上段（Engine 公開介面、新 Subsystem、DI）。實作者是主會話，Opus 5.5／high。審查者是 engine-grid 會話（Opus），由架構師指定。
**狀態：** 受理判定在本機全部成立；L1 與審查待 PR。
宣告：[engine-format](../architecture/plans/big-map/engine-format.zh-Hant.md)，使用者 2026-11-06 核准（「照建議核准」）。

相關文件：
- [計畫 README](../architecture/plans/big-map/README.md)
- [HANDOFF](../architecture/plans/big-map/HANDOFF.md)（「現況：engine-format」）
- [voxel-mesh 契約](../architecture/voxel-mesh.md)
- [map-markers 契約](../architecture/map-markers.md)
- [collision 契約](../architecture/collision.md)

## 做了什麼

- `GYO::VoxelMesh` 加了區塊地圖的兩種格式：`gyo.voxel_map` v1（索引）和 `gyo.voxel_region` v1（region 檔）。
  - 解析與驗證：`ParseVoxelMap`、`ParseVoxelRegion`、`ValidateVoxelChunk`。`ValidateVoxelChunk` 和 Parse 共用 `CheckChunk` 這一份規則實作。
  - 展開與座標：`ExpandVoxelChunk`、`ExpandVoxelPalette`、`RegionOf`、`ChunkWorldOrigin`、`MapUnitsToWorld`。
  - quad 與 box 的規則（`CheckQuad`、`CheckBox`），v1 和區塊共用同一份。
- `gyo.voxel_mesh` v1 保留，標成 deprecated。刪除放在 `-product-arena-v3`：遷移兩張地圖的同一批，以「`ParseVoxelMesh` 的呼叫為 0 件」確認。
- 新 Subsystem `GYO::MapMarkers`（`gyo.map_markers` v1）：點加 yaw、盒、路徑三種幾何，id、kind、tags 都是不透明的名字。
  - 嚴格解析用第一批合併的 `StrictJson.hpp`，以 `ordered_json` instantiate，這樣才看得到 tags 在文字裡的鍵順序。
  - 守衛：依賴守衛，以及 Python 的玩法名詞守衛。
- 文件：
  - `voxel-mesh.md` 升版：DI 6、7，RC 5～7，RC 3 標為已處置。
  - `map-markers.md` 新建。
  - `docs/architecture.md` 的模組表與目錄樹。
  - `collision.md` 加 RC 4、5，來自 PR #161 審查者的 N4、U1。
- 突變 catalog `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`，共 55 個。

## 經過

1. 實作是在拆批前，照使用者核准的 engine-map 宣告寫的。拆批後保留在本機分支。
2. PR #161 合併後，原工作樹 `bold-mahavira-68c717` 已被刪除，所以改開工作樹 `.claude/worktrees/engine-format`。
   把實作提交 rebase 到 `c70e8fdc`，沒有衝突。
3. 宣告草稿改成正式版，以 draft 開出 PR #162；使用者核准第 7 節。
4. 照新的「突變存活的處理」規則，拆批前存活的 `mm-two-shapes-accepted` 先補了測試，沒有自己認定成等價。
   - 突變把「恰好一種幾何」的檢查改成「至少一種」。兩種幾何的標記仍會被拒，但理由變成「unknown field」。
   - 原本 K4 的測試只檢查路徑，所以抓不到。現在 K4 也檢查訊息「exactly one of point, box and path」，突變被殺。
5. 規則測試是在實作後才寫的。Red 的證據是突變（F2）。

## 受理判定

件數の突き合わせ：一致。突變每個都有結果（被殺／存活／過期），沒有漏跑；測試全部執行完。

| 判定 | 結果 | 成立 |
|---|---|---|
| F1 規則 | `gyo.voxel_map` M1～M10、`gyo.voxel_region` R1～R5／C1～C5、`gyo.map_markers` K1～K10，各有自己的 test case。C1～C5 另有 `ValidateVoxelChunk` 的反例 | 是 |
| F2 突變 | `engine_format_mutations.json` 55/55（之後新增的 `vm-guard-gameplay-word` 也被殺，共 56）、`voxel_mesh_mutations.json` 20/20、`engine_grid_mutations.json` 16/16 被殺 | 是 |
| F3 黃金值 | 區塊展開的頂點、uv、盒；`ChunkWorldOrigin`；`MapUnitsToWorld` 與字面值 bits 一致，含 ±2²⁰ 兩端與負區塊鍵。「v1 單文件與區塊展開位元相同」也通過。三平台待 L1 | 本機是 |
| F4 守衛 | 依賴守衛的突變（`vm-links-collision`、`mm-links-voxel-mesh`、`grid-links-voxel-mesh`）與名詞守衛的突變（`mm-guard-gameplay-word`）都被殺 | 是 |
| F5 迴歸 | 全量建置成功，`ctest -L "cpu\|shader"` 132/132、`-L gpu` 4/4。L1 待 PR | 本機是 |

## DI 1 的中性說法（`v8-arch-big-map-neutral-wording`）

使用者在架構師會話定案。本會話原本的建議是照原文保留。

- `voxel-mesh.md` 的 DI 1 原文以否定句寫了「spawn points」，改成「placement data」。守的性質不變：這個模組不知道放置資料。
- 名詞守衛 `neutral_names_guard.py` 原本只掃 MapMarkers，擴大到 `engine/voxel_mesh`、`tests/common/voxel_mesh`、`docs/architecture/voxel-mesh.md`。
- 加突變 `vm-guard-gameplay-word`：把 DI 1 改回「spawn points」，守衛要失敗。它和 `mm-guard-gameplay-word` 都被殺（`mutations_neutral_wording.txt`）。
- 改完後再跑一次 `cpu|shader`，132/132 通過。

這是 Design Invariants 的守衛判定的變更，照 §14.4 寫明 1～5：

1. **守的性質：** Engine 的地圖格式模組、它的測試與契約，不出現玩法名詞（MapMarkers DI 1、VoxelMesh DI 1）。
2. **緩的面：** 沒有。掃描的檔案只增不減，拒絕的詞也沒變。
3. **嚴的面：** `GYO::VoxelMesh` 的程式、測試、契約裡出現玩法名詞時，以前通過，現在失敗。
4. 不適用（沒有緩的面）。
5. **根據：**
   - 舊守衛會失敗的突變 `mm-guard-gameplay-word`，在新守衛下仍被殺；新突變 `vm-guard-gameplay-word` 也被殺。
   - 證據在主 checkout `build/evidence/big-map/engine-format/mutations_neutral_wording.txt`。
   - DI 的文字變更由使用者同意（經架構師會話轉達）。

## 審查後變更（PR #162）

審查者（engine-grid 會話）提了 B1、S1、N1～N5，使用者決定全部在本 PR 修，而且都是收緊。
內容與 §14.4 的 1～5 在宣告的 4b 節；用一句話說明如下：

- **B1：** `ParseVoxelRegion` 多收 `expected`，也就是讀的是哪個索引項。檔案裡裝了別的 region 的文字，在 `at` 失敗。
- **S1：** MapMarkers 建 DOM 之前，以規則會回報的路徑，拒絕鍵超過 32 個的物件。
- **N1：** M8 拒絕結尾是 `.` 的段名與 Windows 裝置名。
- **N2：** M4 每軸 |16 × origin| ≤ 2²⁰。
- **N3：** 改正舊代號。
- **N4：** 突變 `mm-tags-plain-json`。
- **N5：** 名詞守衛移到共用位置，各模組自己登記。

**第一輪 F2 不成立（AGENTS §14）：**
- 修正提交 `757ba5df` 上重跑，engine-format 60/62，有兩個存活：
  - `vmap-region-file-dot-segments`：N1 的「結尾 `.`」已涵蓋 `.`、`..`。
  - `mm-tags-unbounded`：S1 的預掃描先以同一路徑拒絕。
- 先停下報告，使用者決定刪掉兩處多餘的檢查並退役這兩個突變（「刪多餘檢查（建議）」）。
- 第二輪（`e566fd1a`）：**件數の突き合わせ：一致**（要求 96、結果 96、存活 0），60/60、20/20、16/16。全量建置成功，`cpu|shader` 133/133、`gpu` 4/4。F2 成立。
- 第一輪的失敗保留在 `after_review/`，沒有被這次的成功覆蓋。
- 會改變的判斷：加新規則時，要先查舊的檢查會不會被新規則蓋過。

## 證據

主 checkout 的 `build/evidence/big-map/engine-format/`，git 不追蹤。審查修正後的兩輪在 `after_review/`（第一輪，60/62）與 `after_review_2/`（第二輪，60/60）。核准後第一次的結果：
- 三個突變 catalog 的結果：`engine_format_mutations.txt`、`voxel_mesh_mutations.txt`、`engine_grid_mutations.txt`。
- 全量建置與測試：`build_all.txt`、`ctest_cpu_shader.txt`、`ctest_gpu.txt`。

拆批前那一輪的參考結果在 `build/evidence/big-map/engine-map/`。
