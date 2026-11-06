# 工具 v8：v8-arch-big-map-tool-chunked（區塊化的烘焙、greedy、差分重烘）

日期：2026-11-06。分支 `claude/v8-arch-big-map-tool-chunked`（從 master `915ddc62`）。上段。
- 實作：主會話（Opus 5.5／high）。
- 審查：子 Agent（Opus／high，唯讀）。
- 事前宣告：[voxel-map-tool-chunked.md](../tools/plans/v8/voxel-map-tool-chunked.md)，2026-11-06 使用者核准（「推奨どおり承認」）。

**狀態：** `tc-bake` 與 `tc-measure` 完成；`tc-markers` 等 PR #160（`marker_kinds.json` 的契約）合併後再做。L1 待 PR。

相關文件：
- [voxel_map_editor/architecture.md](../voxel_map_editor/architecture.md)：「The chunked map」節，DI 2～5、RC 3、RC 5、RC 6
- [voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md)

## 做了什麼

- **區塊烘焙**（`src/MapBuild.cpp`，介面 `ChunkedMap.hpp`）：
  - `BakeMap`：每個 16³ 區塊各自做，cullface 讀跨區塊的鄰格；
  - greedy：區塊內，依（方向、平面、調色盤）分組，座標壓縮到各組自己的邊，掃描順序固定，只用整數；
  - 碰撞盒：`MergeBoxes` 在每個區塊各做一次；
  - 調色盤：格子的全部方塊，依格子順序；
  - 每個區塊交給 Engine 的 `ValidateVoxelChunk`，索引用 `ParseVoxelMap` 讀回。
- **差分重烘**：
  - `ChunksTouchedBy`：格子所在的區塊，加上它在區塊邊界時，面相鄰的區塊；
  - `RebakeChunks`：只重烘這些區塊，邊界或 region 變了才重建索引；
  - 調色盤、形狀庫、方塊的形狀、origin、cell size 有變時，全部重烘。
- **寫出與存檔**：
  - `WriteMap`：寫 `map.json` 與 `regions/r_<x>_<z>_<雜湊前 16 位>.json`，以 `ParseVoxelMap`／`ParseVoxelRegion` 讀回，逐欄和烘焙的內容比較；
  - `SaveMap`：先寫 region，再原子替換 `map.json`，最後刪除舊索引列過、新索引沒列的 region 檔。
- **CLI**：`build-map`、`verify-map`、`inspect-map`、`measure-map`。
- **共用部件**：cullface、面的四邊形、亮度、整數輸出，從 `Build.cpp` 移到 `src/BuildParts.hpp`。v1 的輸出逐位元不變（golden 測試通過）。
- **v1 不動**：`build`、`verify`、`inspect` 與 v1 的 golden、突變都不變（`v8-tool-chunked-plan` 第 2 點）。
- **測試**：
  - 新執行檔 `gyo_voxel_map_editor_chunked_tests`，17 個 case；
  - CLI 測試加了區塊化的段落；
  - golden 放在 `tests/voxel_map_editor/fixtures/sample_map/`，位元組由既有的 `.gitattributes` 固定。
- **突變**：`voxel_map_editor_mutations.json` 加了 `tc-*` 16 條。
- **文件**：architecture.md 的「The chunked map」、命令表、DI 2～5、RC 3、RC 5、RC 6。DI 1 的 `GYO::MapMarkers` 留到 `tc-markers`。

## 判定

件數の突き合わせ：一致（T5：要求 768 區塊、量到 768、失敗 0；突變：要求 37、結果 37）。

| 代號 | 結果 | 證據 |
|---|---|---|
| T1 | 成立：Engine 不收就不回傳、不寫檔。<br>測試：區塊（`ValidateVoxelChunk`，`BakeMap` 與 `RebakeChunks` 各一）、索引（M6）、region（R5，16 MiB）；CLI 的 `build_map_refused` 與 `build_map_empty` | chunked tests |
| T2 | 成立：<br>- golden 逐位元相同；<br>- 同一格子建兩次相同；<br>- 差分＝全部：預設形狀 200 組 × 4 步，隨機形狀 100 組 × 4 步 | 同上 |
| T3 | 成立：<br>- greedy 與 v1 的面在 8 單位格點上逐點相等、不重疊（200 組）；<br>- 隨機形狀在 1 單位上逐點相等（100 組）；<br>- 每區塊碰撞盒不重疊、聯集相等（200 組）；<br>- 跨區塊 cullface | 同上 |
| T4 | 成立：存到索引之前停下時，舊地圖仍讀得回；只刪舊索引列過的檔；同一張圖存兩次不改檔 | 同上 |
| T5 | **成立**：像建築的合成圖每區塊中位數的 p95 **1.11 ms** ≤ 16 ms（p50 0.86 ms，max 1.45 ms） | 主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/measure/` |
| T6 | 未做（`tc-markers`） | — |
| T7 | 成立：突變 37/37 被殺（`tc-*` 16、既有 21）。第一輪 13/14 的紀錄見下 | `mutations-tc-round1.txt`、`mutations-all-round2.txt` |
| T8 | 成立：依賴守衛與自我測試通過（本批沒有新的依賴，守衛不改） | ctest |
| T9 | 本機：全量建置成功；`cpu|shader` 135/135。L1 待 PR | `ctest-cpu-shader-round2.txt` |

T5 是對審查前的程式碼量的。審查修正後，用同一方法再量一次，只當確認：p95 1.12 ms（`measure-after-review/`）。

## 量測（描述用，不設門檻）

| 地圖 | 格數 | v1：quads／colliders／大小 | 區塊化：區塊／region／quads／colliders／大小 | T-junction 頂點 |
|---|---|---|---|---|
| 像建築的合成圖（256×48×256，grid sha `60e3aa3c…`） | 721,279 | **Engine 拒收**：文字超過 16 MiB | 768／4／80,324／16,916／3.99 MB | 39,869 |
| 棋盤格一區塊 | 2,048 | 12,288／2,048／522 KB | 1／1／12,288／2,048／580 KB | 0 |
| `pvp_corners` | 260 | 696／5／23 KB | 4／1／38／9／2.9 KB | 8 |

- **時間**：
  - 合成圖：全部烘焙 578 ms、寫出 408 ms；
  - 棋盤格一區塊的重烘是 10.2 ms（沒有剔除，也沒有合併，是最壞情況）。
- **碰撞盒變多**：區塊內合併讓盒子比 v1 多（`pvp_corners` 5 → 9）。這是決定 `v8-arch-big-map-greedy` 預期的代價；Engine 的 `StaticAabbGrid` 承擔查詢。
- **T-junction**：頂點數只記錄。目視依決定 5，移到 window 批。

## 審查（子 Agent，Opus／high）

沒有 blocker。審查者自己寫了 probe（隨機形狀、偏離格點、24 種朝向），greedy、cullface、`ChunksTouchedBy`、差分重烘都沒有不一致。

**should-fix（全部修了）**
1. `RebakeChunks` 只比調色盤，不比形狀庫與方塊的形狀；改形狀後只重烘指定的區塊，會留下過期的區塊。改為這兩者有變時全部重烘：`BakedMap` 記下烘焙時的形狀；新增測試「a shape edit or a block's new shape rebakes everything」與突變 `tc-shape-change-ignored`。
2. T1 不完整：沒有 `ValidateVoxelChunk` 拒收的案例，CLI 也沒有 Engine 拒收的案例；DI 2 的守衛清單把工具自己的 `EmptyMap` 檢查算進去。改為：
   - 新增「a chunk Engine refuses is not kept」：記憶體中的格子，形狀盒超出格子（C4）；
   - 新增 CLI 的 `build_map_refused`（M6）；
   - DI 2 的守衛清單改寫。
3. M6 的邊界：格子接受的範圍和地圖格式的範圍不一致（見下面「要使用者決定的事」）。這次只寫進 architecture.md。

**nit（處理了的）**
- 讀回時也比較 region 的 file、sha256、數量，以及 `source_sha256`、`build_version`。
- 加 `static_assert`，把工具的區塊大小和單位綁到 Engine 的值。
- `measure-map` 的 CSV 改為經 engine/io 原子替換，寫進命令表與 DI 5。
- 照審查者的 probe，加了隨機形狀的測試（1 單位解析度）。

**nit（記錄，不處理）**
- `CoversSide` 對超出 [0, 16] 的形狀盒會越界。v1 也一樣，是既有的問題；格子檔不會有這種盒子，但 window 批要守住這個前提。

## 和宣告的差異（請確認）

1. **刪除舊 region 檔的範圍變窄。**
   - 宣告：刪除「符合命名形式、新索引沒列的檔」。
   - 實作：刪除「舊 `map.json` 列過、新的沒列、且符合命名形式的檔」。
   - 原因：engine/io 的 `NativeFileSystem::List` 不支援（`canList = false`），列不出目錄。擴充 OS／IO 層超出本批範圍。
   - 結果：只會刪更少的檔。代價是「寫完 region、在換索引前停下的存檔」留下的檔不會被清掉。
   - DI 5 的字句已照實作寫。
2. **`tc-measure` 併在同一個變更。** 量測命令在執行檔（`MapMeasure.cpp`，不在 core）。
3. **突變的第一輪是 13/14。**
   - 存活的是 `tc-chunk-not-validated`（拿掉 `BakeMap` 裡的 `ValidateVoxelChunk`）。
   - 審查者逐條核對 C1～C5，確認：從格子檔讀進來的格子，烘焙出的區塊 Engine 一定接受。所以對檔案輸入，它是等價突變。
   - 審查者另外找到反例：記憶體中的格子，形狀盒超出格子。
   - 處置：把反例做成測試（上面 should-fix 2），重跑後被殺。同性質的 `RebakeChunks` 也加了突變 `tc-rebake-chunk-not-validated`。
   - 這個呼叫的實際作用是保證 `ExpandVoxelChunk` 的前提（window 批的預覽會直接展開）。
4. **合成圖的 v1 數字是「拒收」。** v1 單檔超過 16 MiB，Engine 不收。這不是量測失敗；區塊化的數字都量到了。

## 要使用者決定的事

- **M6 的邊界**（審查 should-fix 3）。格子接受「origin＋格」在 ±2^16 以內；但 origin 的 x 或 z 不是 16 的倍數時，最邊緣的區塊會超出 Engine 的 ±2^20 單位，`build-map` 會拒收（`build` 仍可）。選項：
  - (a) 只記錄（目前的做法）；
  - (b) 收緊格子的範圍規則，讓格子不能放到區塊會超界的位置。這是格式規則的變更，會讓現有的格子變成無效（現有兩張地圖都不在邊緣，不受影響）。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/`（Intel Mac，RelWithDebInfo）：

| 檔案 | 內容 |
|---|---|
| `measure/` | T5 的判定試行：summary、CSV、合成圖的格子檔、環境 |
| `measure-after-review/` | 審查後的確認，不是判定 |
| `mutations-tc-round1.txt` | 13/14 |
| `mutations-all-round2.txt` | 37/37 |
| `ctest-cpu-shader-round1.txt` | 只留了尾部，合否行被截掉，不當證據 |
| `ctest-cpu-shader-round2.txt` | 135/135 |

審查者的 probe 在本會話的 scratchpad，未保存。
