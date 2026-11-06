# 工具 v8：v8-arch-big-map-tool-chunked（區塊化的烘焙、greedy、差分重烘、標記）

日期：2026-11-06。分支 `claude/v8-arch-big-map-tool-chunked`（從 master `915ddc62`）。上段。
- 實作：主會話（Opus 5.5／high）。
- 審查：子 Agent（Opus／high，唯讀）。
- 事前宣告：[voxel-map-tool-chunked.md](../tools/plans/v8/voxel-map-tool-chunked.md)，2026-11-06 使用者核准（「推奨どおり承認」）。

**狀態：** `tc-bake`、`tc-measure`、`tc-markers` 完成。DI 1 守衛的放寬（§14.4）與 DI 2 字句的追加，2026-11-06 使用者核准（「両方承認」）。L1 三平台通過（PR #163）：`tc-bake` 在 run 42188382428；`tc-markers` 在 run 42195132825（head `137085ae`）。之後的 `16a28c62` 只改文件與突變目錄，L1 照規則 skip，CI gate 通過。

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
| T9 | 成立。<br>- 本機：全量建置成功；`cpu|shader` 135/135。<br>- L1（PR #163，head `585f599b`，run 42188382428）：windows-x64、linux-x64、macos-arm64 全部通過。<br>- 第一次 L1 的 windows-x64 編譯失敗（測試變數 `far` 被 `windef.h` 的巨集吃掉，C2660），改名 `farSide` 後通過，判定不變 | `ctest-cpu-shader-round2.txt`、CI |

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

## 和宣告的差異（2026-11-06 使用者確認：「確認、(a) 記録のみ」）

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

## 使用者的決定

**2026-11-06：(a) 只記錄。** architecture.md「The chunked map」的 Range 段落就是紀錄。

原本的問題：

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

## tc-markers（2026-11-06，PR #160 合併後）

PR #160（`gyo.marker_kinds` 的契約）合併後，把 master（`15257faf`）merge 進本分支（`929c7664`）。用 merge 不用 rebase，是為了不對已推送的 PR #163 做 force push。

### 做了什麼

- **Engine**（`GYO::MapMarkers` 公開介面的追加，架構師指定）：
  - `ParseMarkerKinds`：規則 Q1～Q5，錯誤碼 `InvalidMarkerKinds`；
  - `UnknownMarkerKinds`：回傳 `markers[i].kind` 的路徑，依 markers 順序；
  - 規則測試每條 Q 一個 case，另有 `UnknownMarkerKinds` 的測試，都在 `tests/common/map_markers/MapMarkersTests.cpp`；
  - Engine 的 DI 2（Leaf）沒有變：連結不變，標頭只多 `<span>`。
- **工具**（`MarkersFile.hpp`／`.cpp`）：
  - `ReadMarkers`、`ReadMarkerKinds`：直接用 Engine 的解析器；
  - `WriteMarkers`：寫出、讀回、逐欄比較；
  - CLI `check-markers <markers> --marker-kinds <kinds>`；
  - 錯誤碼 `InvalidMarkers`、`UnknownKinds`；
  - core 連 `GYO::MapMarkers`。
- **守衛**：依賴守衛允許 `GYO::MapMarkers`；自我測試加「其他 Engine 模組仍然失敗」。
- **文件**：
  - architecture.md：「Markers」節、命令表、DI 1、DI 2；
  - map-markers.md：Status 行，以及「Relation to a markers file」段落寫明路徑形式 `markers[i].kind`；DI 節沒動。

### 判定

件數の突き合わせ：一致（突變：要求 48、結果 48）。

| 代號 | 結果 |
|---|---|
| T6 | 成立：<br>- Q1～Q5 各一個 case；<br>- `UnknownMarkerKinds` 的路徑與順序；<br>- golden 逐位元相同、寫兩次相同、讀回相等（含 `"`、`\`、yaw 的位元）；<br>- 非 UTF-8 的名字讀回不同時被拒；<br>- CLI 的 `check_markers*` 六條；<br>- Engine 的 `neutral_names` 與 `gyo_expect_map_markers_links` 通過 |
| T7 | 成立：整份目錄 48/48 被殺（`mutations-all-round3.txt`） |
| T8 | 成立：依賴守衛與自我測試通過；`tc-guard-*` 兩條被殺 |
| T9 | 成立：本機全量建置成功，`cpu|shader` 135/135（`ctest-cpu-shader-markers-final.txt`）；L1 三平台通過（run 42195132825，head `137085ae`） |

- **突變的經過**：
  - 第一輪 9/10。`tc-markers-not-read-back` 是 wrong_failure：原本只拿掉 parse 的檢查，程式碰到失敗的結果就崩潰，沒有到斷言。
  - 改成「直接回傳不讀回」之後被殺（`mutations-markers-round2.txt`）。
  - 這是突變定義的錯誤，不是實作的問題。

### 審查（子 Agent，Opus／high）

沒有 blocker。兩個 should-fix 都修了：

1. `WriteMarkers` 遇到非 UTF-8 的位元組會拋例外，而不是回傳錯誤（nlohmann 的嚴格模式）。改為丟掉那些位元組，之後由讀回的比較拒收。
2. 讀回的比較沒有測試走到；把比較拿掉的突變會存活。用 1 的情況（id `m\xff0`）測到它，並加突變 `tc-markers-read-back-not-compared`。

nit：
- **處理了**：
  - golden 加了反斜線；
  - 標頭寫明「每個逗號後有空格，約 3 MiB 的緊湊路徑檔寫回後可能超過 4 MiB 而被拒」；
  - map-markers.md 的 Status 改為「工具讀，程式庫寫」。
- **記錄，不處理**：
  - kinds 的解析沒有用 `KeyCountScan`：64 KiB 內最壞 128 ms（-O2），上限有界。要改是 Engine 的事，交架構師。
  - `check_markers_unknown` 沒有專屬突變：runner 不能突變 CLI 測試的判斷；這條路徑由 `mm-unknown-kinds-none` 間接守住。

### §14.4（DI 1 的守衛）

驗證者是另一個子 Agent（Opus／high），用舊、新兩版守衛跑了 21 個合成樹與實際的樹，結果記在下面。

1. **守的性質**：編輯器不連遊戲、不提產品；不用 SDL、ImGui、render、platform、`*Native`；只用清單裡的 Engine 模組。驗證者確認。
   - 補充：「只用清單的模組」只對 target 成立；include 本來就允許任何 `engine/` 開頭的路徑。
2. **放寬的面**：工具的 CMake 文字裡**任何地方**出現 `GYO::MapMarkers` 這個名字都會通過，以前會失敗。
   - 範圍不只連結，也包括註解、`set()`、生成式。這點是驗證者指正的。
   - 21 個樹中，新守衛的發現都是舊守衛發現的子集；消失的只有 `names GYO::MapMarkers` 這一行。
3. **收緊的面**：無。
   - 新加的自我測試在舊守衛上也得到 3 個發現，所以不是收緊。
4. **緩的面以外仍受守護**：
   - `GYO::Collision`、`GYO::Render`、`SDL3::SDL3`、`imgui::core`、遊戲的 plain target、`GYO::MapMarkers.extra`、`GYO::MapMarkersNative` 都仍然失敗；
   - `GYO::MapMarkers` 本身只連 Base、Math、JSON，由 Engine 的 `gyo_expect_map_markers_links` 守（configure 時，只看直接連結；Base 與 Math 沒有其他邊）。
5. **證據**：
   - 自我測試 7 條通過；
   - 突變 `tc-guard-allows-any-engine-module`、`tc-guard-core-links-collision` 被殺；
   - 驗證者的比較腳本與輸出在本會話的 scratchpad，未保存。

**要使用者核准的事（2026-11-06 核准：「両方承認」）：**
- 上面的放寬；
- DI 2 字句的追加（`ParseMarkerKinds`、`UnknownMarkerKinds`）。宣告只核准了「markers 經 `ParseMapMarkers`」。這次加的只會收緊，但照 AGENTS §3，改 DI 節要同意。

**另案（交架構師）**：map-markers.md 的 DI 3 還寫著「the kinds rules and their mutants arrive with the parser」，已經過時（突變在工具的目錄裡）。改 DI 節要使用者同意，所以本批不動。

### 架構師轉達的使用者定案（2026-11-06，PR #163 內處理）

1. **kinds 的突變搬到 Engine 的目錄。**
   - 從 `voxel_map_editor_mutations.json` 移到 `engine_format_mutations.json`，改名 `mm-kinds-*`、`mm-unknown-kinds-none`；規則與 find 不變。
   - 理由：它們守的是 Engine 模組的 DI。放在工具的目錄裡，刪掉編輯器時守衛會跟著消失（AGENTS §7）。
   - 工具的目錄從 48 條減為 41 條。
   - **engine_format 目錄重跑：68/68 被殺**（原有 61、新加 7；件數の突き合わせ：一致，要求 68、結果 68）。
   - 證據：`mutations-engine-format-with-kinds.txt`。
2. **map-markers.md 的 DI 3**：把「the kinds rules and their mutants arrive with the parser」換成「the kinds rules are tested in the same file, mutants mm-kinds-* and mm-unknown-kinds-none in the same catalog」。這是使用者同意的 DI 節變更。
3. **`ParseMarkerKinds` 不加鍵數預掃**（使用者同意）。map-markers.md 的「Cost」段末補一句：64 KiB 的上限把最壞情況限制在約 130 ms，只在載入時發生。
