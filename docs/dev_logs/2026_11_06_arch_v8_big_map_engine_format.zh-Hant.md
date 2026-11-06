# 架構 v8：v8-arch-big-map-engine-format（區塊地圖格式與標記層）

日期：2026-11-06。分支 `claude/v8-arch-big-map-engine-format`（rebase 到 master `c70e8fdc`，即 PR #161 合併後）。PR #162。
段：上段（Engine 公開介面、新 Subsystem、DI）。實作者是主會話，Opus 5.5／high。審查者是 engine-grid 會話（Opus），由架構師指定。
**狀態：** F1～F5 成立（L1 三平台對 `e756e7d9` 通過）。審查者複核找到 B2，已修（G1～G4 見「審查者複核與 B2」），等審查者再複核。
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

## L1（`e756e7d9`）

windows-x64、linux-x64、macos-arm64 三平台全部通過，CI gate 通過。F3 的「三平台待 L1」與 F5 的 L1 至此成立。

## 審查者複核與 B2（PR #162）

審查者複核 `e756e7d9`：B1、S1、N1～N5 與兩個退役突變都確認了，但 S1 帶進一個新的回歸 B2。架構師依 AGENTS §14 停下，使用者決定先修再合併，用審查者的 (a)。

- **B2：** S1 的預掃描 `KeyCountScan` 每開一層容器就存一份完整路徑，記憶體隨巢狀深度平方成長。
  - 審查者量到深度 32,000 時 1.2 s／1.6 GB（`62fe5fd3` 是 12 ms／2.6 MB）。
  - 4 MiB 內可做到約 200 萬層，`bad_alloc` 會從 `ParseMapMarkers` 漏出去。
- **修法 (a)：** 每層只存「是不是陣列、數量、最後一個鍵」，路徑只在失敗時從根組出來。回報的路徑不變，規則不變。
- **判定：** 先寫進宣告 4b 節的 B2（G1～G4），再看結果。
  - G1 是新的測試執行檔 `gyo_map_markers_alloc_tests`：替換全域 `operator new`，數 `ParseMapMarkers` 要求的位元組；深度 4,000 與 2,000 的比值要 < 3。先例是 `gyo_audio_alloc_tests`。
- **G2 Red→Green**（測試本身沒改）：

  | 形狀 | `e756e7d9`（Red） | 修正後（Green） |
  |---|---|---|
  | 陣列 | 9,401,655 → 36,802,679 位元組，比值 3.91 | 303,565 → 606,285 位元組，比值 2.00 |
  | 物件 | 4,675,725 → 17,350,765 位元組，比值 3.71 | 565,581 → 1,130,317 位元組，比值 2.00 |

- **紀錄（不作判定）：** 審查者的探針 `deep_one.cpp`，本機 Intel Mac，`-O1`：

  | 深度 | `e756e7d9` | `62fe5fd3` | 修正後 |
  |---|---|---|---|
  | 32,000 | 1,098 ms／峰值 1.6 GB | 15 ms／3.1 MB | 12 ms／5.1 MB |
  | 1,000,000 | 未跑 | 337 ms／61 MB（審查者的數字） | 393 ms／153 MB |

  修正後是線性的，但每層的 `Frame` 比 `62fe5fd3` 多，深度 100 萬時峰值約是它的 2.5 倍。極深的文件在記憶體很小的機器上仍可能 `bad_alloc`；把它轉成錯誤或加深度上限（審查者的 (b)），不在本次範圍。
- **G3 突變：**
  - 新突變 `mm-key-scan-full-path`：陣列每層開啟時又存一份完整路徑，被 G1 殺。
  - `mm-key-scan-loses-index` 原來的那一行已不存在，`find` 換成新程式中組陣列索引的那一行；守的性質、目標、期待的訊息都不變。
  - 依 D184 重跑全部 `mm-*`：**件數の突き合わせ：一致**（要求 18、結果 18：被殺 18、存活 0、過期 0）。`vmap-*`、`vreg-*`、`vm-*` 與另兩個 catalog 的對象程式沒動，沒重跑。catalog 現在 61 個。
- **G4 迴歸：** `gyo_map_markers_tests` 11/11（269 assertions）、`gyo_voxel_mesh_tests` 39/39（954 assertions）、兩條名詞守衛通過。全量建置成功，`cpu|shader` 134/134（多了 `gyo_map_markers_alloc_tests`）、`gpu` 4/4。L1 待新 head。
- **§14.4：** 只新增判定，沒改既有判定，所以沒有緩める面。
- **順手處理（審查者的參考項）：** `neutral_names_guard.py` 的 docstring 不再列出使用它的模組，刪模組時不必改它。`map-markers.md` 的 Cost 補一句：掃描的記憶體與文字長度成線性。
- **會改變的判斷：** 加這類預掃描或任何「每層的狀態」時，要另外確認它的時間和記憶體與輸入大小成線性。F2 的突變與規則測試量不到複雜度，這次的 B2 是審查者用深度探針找到的。

## 證據

主 checkout 的 `build/evidence/big-map/engine-format/`，git 不追蹤。審查修正後的兩輪在 `after_review/`（第一輪，60/62）與 `after_review_2/`（第二輪，60/60）。B2 在 `b2/`：`red_alloc_tests_e756e7d9.txt`、`green_alloc_tests.txt`、`depth32000_before.txt`、`depth32000_after.txt`、`mutations_mm.txt`、`build_all.txt`、`ctest_cpu_shader.txt`、`ctest_gpu.txt`。審查者的探針在 `review_pr162/`。核准後第一次的結果：
- 三個突變 catalog 的結果：`engine_format_mutations.txt`、`voxel_mesh_mutations.txt`、`engine_grid_mutations.txt`。
- 全量建置與測試：`build_all.txt`、`ctest_cpu_shader.txt`、`ctest_gpu.txt`。

拆批前那一輪的參考結果在 `build/evidence/big-map/engine-map/`。
