# v8 工具：voxel 地圖編輯器的區塊化（`v8-arch-big-map-tool-chunked`）事前宣告

更新：2026-11-06。Owner：voxel 地圖編輯器線（本會話）。上層計畫：[大地圖](../../../architecture/plans/big-map/README.md)；
本線的計畫與紀錄：[voxel-map-editor.md](voxel-map-editor.md)、[HANDOFF](voxel-map-editor-HANDOFF.md)。
**狀態：2026-11-06 使用者核准（原話「推奨どおり承認」：宣告、DI 與 RC 的變更、要決定的事 3～5 全部照建議）。`tc-bake`、`tc-measure` 完成（[dev_log](../../../dev_logs/2026_11_06_tool_v8_voxel_map_tool_chunked.zh-Hant.md)，T5 p95 1.11 ms 成立）；和宣告的差異 4 項使用者已確認，M6 邊界只記錄。`tc-markers` 完成（PR #160 合併後），DI 1 守衛的放寬與 DI 2 字句的追加，使用者已核准（§14.4，dev_log）。**

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map-tool-chunked` | 本批：編輯器輸出 Engine 的區塊化地圖（`gyo.voxel_map`＋`gyo.voxel_region`），只重烘改過的區塊，加 greedy，標記檔的讀寫 |
| `tc-declare` | 本宣告 |
| `tc-bake` | 區塊烘焙、greedy、區塊內碰撞合併、map／region 的寫出與讀回、CLI |
| `tc-markers` | Engine 的 kinds 解析與檢查（兩個函式）、`gyo.map_markers` 的讀寫（經 Engine）、CLI |
| `tc-measure` | 事前宣告的量測（`pvp_corners`、像建築的合成圖） |
| `v8-tool-chunked-plan` | 使用者 2026-11-06 對計畫案的五個判斷（「推奨どおり決める」） |

## 已定的前提

- 大地圖的決定（big-map HANDOFF「決定」）：區塊 16³、region 8×8 柱、只 JSON、greedy 與區塊內碰撞合併在工具、固定掃描順序、無 LOD、kinds 由產品的 `marker_kinds.json` 給。
- `v8-tool-chunked-plan`（使用者，2026-11-06）：
  1. 從 master（`915ddc62`）分支 `claude/v8-arch-big-map-tool-chunked`；window 批（PR #158）之後 rebase 到本批上，重新宣告。
  2. v1 單文件輸出（`build`）保留到 product-arena-v3 刪除為止；新格式用新命令。v1 不加 greedy，v1 的 golden 不動。
  3. 標記的讀寫核心在本批；放置的 UI 在 window 批。
  4. `marker_kinds.json` 的形式由架構師寫進 `docs/architecture/map-markers.md` 的「Kinds」節（已請架構師）；工具只收路徑、不解釋名字。
  5. 重烘時間的判定值在本宣告定（候選 p95 ≤ 16 ms）。
- 工具的格子已經以 16³ 區塊存檔，區塊座標以 `origin` 為原點；地圖的 `origin` 用格子的 `origin`，所以格子的區塊座標＝Engine 的區塊鍵。

## 範圍

**做**
- `tc-bake`：
  - 區塊烘焙：一個區塊的 quads 與 colliders，以區塊最小角為原點、1/16 格單位（0～256）。
  - 面剔除照 v1（cullface），鄰格跨區塊時讀鄰區塊的格子。
  - greedy：區塊內，依（方向、平面座標、調色盤）分組，把剔除後的面在 1/16 單位的平面上合併成長方形。掃描順序固定（先 v 後 u，先延伸寬再延伸高），只用整數。
  - 碰撞：現有的 `MergeBoxes` 改為每個區塊各做一次（格子的盒子不跨區塊）。
  - 調色盤：格子的全部方塊，依格子裡的順序（v1 只放用到的方塊）。這樣只有改調色盤時才要全部重烘；改調色盤本來就是整份文件的變更。
  - 每個區塊交給 `ValidateVoxelChunk`；Engine 不收就不回傳。
  - 差分重烘：`ChunksTouchedBy(cells)` 給出「格子所在區塊＋在區塊邊界時，面相鄰的區塊」；`RebakeChunks` 只重烘這些。
  - 寫出：`map.json`＋`regions/*.json`，文字用 Engine 的鍵順序；寫之前以 `ParseVoxelMap`／`ParseVoxelRegion` 讀回，和烘焙的內容逐欄比較。
  - 存檔順序（中途失敗時舊地圖仍可讀）：region 檔名帶內容雜湊的前 16 位（`regions/r_<x>_<z>_<hash16>.json`），先寫新的 region，最後以原子替換寫 `map.json`，之後才刪除新索引沒有列出、且符合這個命名形式的 region 檔。
  - CLI：`build-map <grid> <map-dir>`、`verify-map <grid> <map-dir>`（重烘並逐位元比較）、`inspect-map <map-dir>`（區塊數、region 數、quads、colliders）。
  - 空的格子（沒有方塊）不產生地圖（Engine 的 M8 要求至少一個 region）：`build-map` 以輸入錯誤結束，不寫任何檔案。
- `tc-markers`：
  - **Engine 公開介面的追加**（架構師 2026-11-06 指定，決定 `v8-arch-big-map-marker-kinds-format`）：在 `GYO::MapMarkers` 實作
    - `ParseMarkerKinds(text)`：回傳文字順序的 kinds；錯誤碼 `MapMarkersErrorCode::InvalidMarkerKinds`，detail 是路徑（如 `kinds[3]`）；
    - `UnknownMarkerKinds(markers, kinds)`：回傳 kind 不在清單裡的 marker 的路徑，依 markers 順序，空表示沒有；產品載入與編輯器共用。
  - 規則照 `map-markers.md`「Marker kinds (gyo.marker_kinds)」節的 Q1～Q5（PR #160，`29c79fbf`）：
    - Q1：三個欄位，鍵不重複；
    - Q2：schema 與 version 1；
    - Q3：kinds 1～256 個，嚴格遞增；
    - Q4：每個符合 K5；
    - Q5：文字 ≤ 64 KiB。
  - 規則測試每條 Q 一個 case，突變進本線的 catalog；kinds 範例只用中性名字（名詞守衛已涵蓋）。`ParseMapMarkers` 不變。
  - 工具：`gyo.map_markers` 的讀寫只經 `ParseMapMarkers`；寫出的 tags 依鍵順序，寫後讀回比較。kinds 由 `--marker-kinds <path>` 讀，只經 `ParseMarkerKinds`，不解釋名字。CLI：`check-markers <markers> --marker-kinds <path>`（用 `UnknownMarkerKinds`）。
  - **前提：PR #160 合併後開工**（契約在那裡）；之前先做 `tc-bake`。
- `tc-measure`：見下面「量測」。

**不做**
- 不改 v1 的 `build`、`verify`、其 golden 與突變；不改 framework、產品。Engine 只做 `tc-markers` 的兩個函式（Engine 的 DI 與其他介面不動）。
- 不做 window（預覽、編輯工作階段的髒區塊追蹤、標記的放置 UI）：window 批重新宣告。
- 不做區塊之間的 greedy、LOD、region 代理、二進位快取。

## Architecture Delta（AGENTS §3）

1. **需要的 Feature**：大地圖的編輯（big-map 線；`v8-arch-big-map-tool-chunked`）。
2. **現在的問題**：工具只輸出單一文件（v1，Engine 已標為 deprecated）；一處修改就要重烘整張圖。工具 RC 3 成立。
3. **變化的邊界**：工具的輸出契約多了 Engine 的區塊化地圖；Editor→Data Contract→Application 的方向不變。
4. **受影響**：`tools/voxel_map_editor`（core、CLI）、`tests/voxel_map_editor`、`docs/voxel_map_editor/architecture.md`、本線的計畫文件。產品到 arena-v3 之前不受影響。
5. **依賴方向**：core 新增連結 `GYO::MapMarkers`（Engine，只依賴 Base、Math），DI 1 的清單和守衛加一項。沒有反向依賴。Engine 公開介面追加 `ParseMarkerKinds`、`UnknownMarkerKinds`（架構師指定；形式屬 Engine，內容屬產品）。
6. **Ownership**：不變。區塊格式與驗證＝Engine；烘焙、greedy、合併＝工具；標記的形式＝Engine、意義＝產品、編輯＝工具。
7. **為何不能更小**：Engine 的契約已經以區塊為驗證與展開的單位；在 v1 上加差分烘焙，存檔時仍要寫整份文件，問題不會解決。

## Design Invariants 的變更（請一併核准；寫進 `docs/voxel_map_editor/architecture.md`）

- **DI 1（依賴）**：core 的 Engine 清單加上 `GYO::MapMarkers`。其他不變。
- **DI 2（單一驗證者）改寫**：
  > Whether baked output is valid is decided by Engine alone: `ValidateVoxelChunk` for every chunk the editor bakes, `ParseVoxelMap` and `ParseVoxelRegion` for the map text it writes, `ParseMapMarkers` for a markers text, and `ParseVoxelMesh` for the deprecated single file. The editor keeps no copy of these rules: it reads every text it writes back, compares what it read with what it baked, and returns nothing Engine refuses.
  守衛：既有的 `build_refused` 與突變，加上區塊版的「Engine 不收就不回傳」測試、讀回比較的測試與其突變。
- **DI 3（決定性）擴充**：區塊化的輸出也逐位元固定。守衛加：區塊化的 golden（`tests/voxel_map_editor/fixtures/`）、建兩次相同、**差分重烘＝全部重烘**（200 組隨機編輯序列，比較每個區塊與所有輸出文字的位元組）。
- **DI 4（無損）擴充**：
  > The collision boxes of each chunk are disjoint and their union is the union of that chunk's block boxes. Greedy merging keeps the covered area: for every direction, plane and palette entry, the union of the merged quads is the union of the faces left after culling, and no two quads overlap.
  守衛加：200 組隨機區塊的碰撞合併、200 組隨機格子的 greedy 面積等價（以 1/16 單位的格點逐點比較）、跨區塊邊界的 cullface 測試。v1 的守衛照舊。
- **DI 5（只寫自己做的）擴充**：`build-map` 寫命令列指定的地圖目錄裡的 `map.json` 與 `regions/` 下的 region 檔，並只刪除該目錄 `regions/` 下符合 `r_<x>_<z>_<hash16>.json`、且新索引沒有列出的檔；標記檔只寫命令列指定的路徑。
- **RC 3**：改為「（2026-11-06 成立並處理）由本批的區塊輸出、差分重烘與 greedy 回應」。
- **新 RC**：
  - RC 5：單一區塊的重烘時間超過本批宣告的值（見判定 T5），或差分重烘要追的區塊超出面相鄰。
  - RC 6：greedy 造成可見的 T-junction 裂縫，要跨區塊處理或改 Render。

DI 1、3、4 的守衛變更照 AGENTS §14.4：下面「§14.4 的 1～5（草稿）」。

## 介面（草案；實作時要改，會在審查前回報）

```cpp
// tools/voxel_map_editor/include/gyo/voxel_map_editor/VoxelMap.hpp（core）
inline constexpr std::string_view MapBuildVersion = "voxel_map_editor 2";
struct BakedMap final {
    Engine::VoxelMesh::VoxelMapDocument index;   // regions 在寫出時填
    std::map<Vec3i, Engine::VoxelMesh::VoxelChunkDocument> chunks;  // 空區塊不放
};
[[nodiscard]] Result<BakedMap> BakeMap(const GridDocument& grid);
[[nodiscard]] Result<void> RebakeChunks(const GridDocument& grid, BakedMap& map, std::span<const Vec3i> chunks);
[[nodiscard]] std::set<Vec3i> ChunksTouchedBy(std::span<const Vec3i> cells);
struct MapText final { std::string index; std::vector<std::pair<std::string, std::string>> regions; };  // (file, text)
[[nodiscard]] Result<MapText> WriteMap(const BakedMap& map, std::span<const std::byte> sourceBytes);  // 讀回比較
[[nodiscard]] Result<void> SaveMap(const std::string& directory, const MapText& text);
```

錯誤碼：沿用 `BuildRefused`（Engine 不收）、`ReadFailed`、`WriteFailed`，新增 `EmptyMap`。

## 量測（`tc-measure`；方法現在固定，看結果後不改）

- **機器**：本機 MacBook Pro（Intel Core i9-9980HK、64 GB、macOS 26.7.1）；RelWithDebInfo 的 test 建置樹。
- **地圖**：
  - `pvp_corners`：主 checkout `build/art-source/object_fps_pvp/maps/pvp_corners.voxel_grid.json`。
  - 像建築的合成圖：`minstd_rand(20261106)`，256×48×256 格（16×3×16 區塊）。每層 8 格高、1 格厚的樓板，每 16 格一道牆（隨機開門洞），每層隨機放樓梯與半磚。產生器放在 tool 的 measure 命令裡，產生的格子文字的 SHA-256 記在結果裡。
  - 最壞區塊：一個區塊的立方體棋盤格（2,048 格，沒有剔除也沒有 greedy）。
- **記錄（描述用，不設門檻）**：兩張圖的 v1 與區塊化各自的 quads、colliders、文字大小、全部烘焙時間；T-junction 頂點數（落在同平面另一四邊形的邊內部的頂點數）；棋盤格區塊的重烘時間。
- **判定 T5（受け入れ）**：像建築的合成圖每個區塊重烘（`RebakeChunks` 一個區塊＋`ValidateVoxelChunk`＋`ExpandVoxelChunk`）5 次，取每個區塊的中位數；**全部區塊中位數的 p95 ≤ 16 ms**。
  件數：要求的區塊數＝量到的區塊數，失敗 0；不一致就不判定，交使用者。
- **T-junction 目視**：本批還沒有能畫區塊化地圖的預覽，**範圍的限界**：目視移到 window 批（第一次畫出區塊化地圖時），由使用者看。本批只記頂點數。

## 判定（現在固定）

| 代號 | 判定 |
|---|---|
| T1 | Engine 不收的區塊或文字不回傳、不寫檔（`ValidateVoxelChunk`、`ParseVoxelMap`、`ParseVoxelRegion` 各一個失敗案例＋CLI 的 `build_map_refused`） |
| T2 | 決定性：區塊化 golden 逐位元相同；同一格子建兩次相同；差分重烘＝全部重烘（200 組隨機編輯序列） |
| T3 | 無損：每區塊碰撞盒不重疊、聯集相等（200 組）；greedy 面積等價且不重疊（200 組）；跨區塊 cullface |
| T4 | 存檔：中途失敗（模擬在寫 `map.json` 前失敗）時，舊地圖仍以 `ParseVoxelMap`／`ParseVoxelRegion` 讀得回且內容不變；只刪除符合命名形式、新索引沒列的檔 |
| T5 | 上面量測的重烘時間 |
| T6 | 標記：`ParseMarkerKinds` 每條 Q 一個規則 case；`UnknownMarkerKinds` 回傳的路徑與順序；工具寫出後 `ParseMapMarkers` 讀回相等；`check-markers` 對清單外的 kind 報錯、不寫任何檔；Engine 的 `gyo_map_markers.neutral_names` 與 `gyo_expect_map_markers_links` 通過 |
| T7 | ミューテーションテスト：新突變（`tc-*`，加進 `voxel_map_editor_mutations.json`）全部被殺；相關的既有突變重跑（記憶 D184 的範圍） |
| T8 | 依賴守衛：自我測試通過；突變 `tc-guard-core-links-render` 等被殺 |
| T9 | 全量建置成功；本機 `cpu|shader` 全部通過；L1 三平台通過 |

突變存活時：先判斷是測試不足還是等價突變；等價突變不靠推理，要有反例搜尋或另一位審查者確認（engine-grid 的教訓），都交使用者決定。

## §14.4 的 1～5（草稿；實作後以實際資料補完，再請核准）

**DI 1 的守衛（依賴守衛）**
1. 守的性質：編輯器不連遊戲、不提產品；core 沒有 SDL、ImGui、render、platform。
2. 緩的面：core 可以連 `GYO::MapMarkers`、include `engine/map_markers/`（以前會失敗）。
3. 緊的面：無。
4. 緩的面之外的性質仍由同一守衛的其他清單守；`GYO::MapMarkers` 本身只依賴 Base、Math（Engine 的 `gyo_expect_map_markers_links`）。
5. 證據：守衛自我測試的輸出、突變結果、驗證者（另一個 Opus 子 Agent）的實測，存主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/`。

**DI 3、DI 4 的守衛**：只加測試，v1 的判定不變。緩的面預期為無；以「舊判定失敗的輸入（既有失敗例與舊突變）新判定也全部失敗」示之，並以新突變示檢出力。示不出來時當作有緩的面，照 §14.4 請核准。

## 分批、模型、ギア、估時

```text
v8-arch-big-map-tool-chunked（上段：資料形式、DI、Data Contract）
├── tc-declare   Opus／medium｜1.5～2 h ：本宣告
├── tc-bake      Opus／high  ｜10～14 h ：烘焙、greedy、差分、寫出與讀回、CLI、守衛
├── tc-markers   Opus／high  ｜4～6 h   ：Engine 的 ParseMarkerKinds／UnknownMarkerKinds、標記讀寫、CLI（等 PR #160）
└── tc-measure   Opus／medium｜2～3 h   ：量測與 T5
```

| 角色 | 模型／ギア | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high（本會話） | 上段，不用小模型 |
| 審查者 | Opus／high（另一個子 Agent，與實作者不同） | 上段 |
| §14.4 驗證者 | Opus／high（另一個子 Agent，與提案者不同） | 要確認 4，不用小模型 |

- 合計 18～25 h。超過 37 h，或單一問題累計超過 2 h，停下回報。
- 證據：主 checkout `build/evidence/tool-v8-voxel-map/tool-chunked/`。
- 完成時：dev_log `docs/dev_logs/2026_11_xx_tool_v8_voxel_map_tool_chunked.zh-Hant.md`、HANDOFF、README 一覽、`architecture.md` 的 DI 與 RC。

## 要使用者決定的事

1. 本宣告（範圍、介面草案、判定 T1～T9、量測方法與 T5 的 16 ms）。
2. DI 1～5 的變更、DI 2 的改寫、RC 3 的處置與 RC 5／6。
3. 存檔方式：region 檔名帶雜湊、先寫 region 後寫索引、再刪舊檔（建議）；或每次全部覆寫（簡單，但中途失敗時地圖可能讀不回來）。
4. 調色盤用格子的全部方塊（建議，差分重烘才不會因為用到新方塊而全部重烘）；或照 v1 只放用到的方塊。
5. T-junction 目視移到 window 批（建議）；或本批另做一個簡單的畫面輸出。
