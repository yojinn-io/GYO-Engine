# 大地圖 engine-format：區塊地圖格式與標記層（`v8-arch-big-map-engine-format`）

更新：2026-11-06。Owner：Engine（`GYO::VoxelMesh`、新 `GYO::MapMarkers`；`GYO::Collision` 只改契約文件的 RC）。
計畫：[README](README.md)；記錄：[HANDOFF](HANDOFF.md)；根據：[survey](survey.zh-Hant.md) 與 HANDOFF「決定」的六項。
**狀態：事前宣告，使用者 2026-11-06 核准（原話「照建議核准」：第 7 節 1～5 全部照建議）。核准後不放寬。** 由 [engine-map 宣告](engine-map.zh-Hant.md)拆出：格式、MapMarkers、v1 退役 (a) 與 Delta A／B，使用者 2026-11-06 已在 engine-map 會話核准（「照建議核准」）。
本稿新增、需要核准的是第 7 節列的事：v1 退役的時點、`collision.md` 的兩條 RC、`AnimationEvents` 另案、突變存活的處理規則。核准前不推實作的審查。

## 0. 來歷與現況

- 拆批（`v8-arch-big-map-split-engine`）後，第一批 [engine-grid](engine-grid.zh-Hant.md)（PR #161）已合併進 master，merge commit 是 `c70e8fdc`。本批是第二批（PR 2）。
- 本批的實作在拆批前就寫好了，當時是在使用者核准的 engine-map 宣告之下：
  - 原提交：`447f9194` 的 v2 部分、`c2287961`。
  - 現在的提交：分支 `claude/v8-arch-big-map-engine-format` 上的一個提交，已 rebase 到 `c70e8fdc`。
  - 嚴格解析用第一批合併的 `engine/base/include/engine/base/StrictJson.hpp`，`GYO::MapMarkers` 也用它（以 `nlohmann::ordered_json` instantiate）。
- 拆批前那一輪在 engine-map 分支上跑過規則測試與突變，結果只當參考，受理判定照本稿重跑（第 4 節）。
  那一輪的突變在格式與 MapMarkers 的範圍裡存活 1 個：`mm-two-shapes-accepted`，見 F2。

## 1. 範圍

### 做

- `gyo.voxel_map` v1（索引）與 `gyo.voxel_region` v1（region 檔）的契約、驗證與展開。`ValidateVoxelChunk`（不經文字）和 Parse 共用一份規則實作。
- 新 Subsystem `GYO::MapMarkers`：`engine/map_markers/` 和 `docs/architecture/map-markers.md`。
- `gyo.voxel_mesh` v1 保留，文件與標頭標為 deprecated。刪除的時點見第 7 節。
- `docs/architecture/voxel-mesh.md` 升版：DI 6、7，RC 5～7，RC 3 改寫成「已處置」。`docs/architecture.md` 的模組表與目錄樹加上 MapMarkers。
- `docs/architecture/collision.md` 加兩條 Replacement Criteria（第 7 節，PR #161 審查者的 N4、U1）。
- 測試、突變 catalog `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`、黃金值、依賴守衛、名詞守衛、dev_log、HANDOFF、README。

### 不做

- 不改產品與工具：arena v3、遷移兩張地圖、工具的 region 存檔都屬後面的批次。
- 不換第三方解析器，不做平行解析、二進位、LOD。
- 不改 `StaticAabbGrid` 的程式。審查者的 N3（極小半徑時邊際是否足夠）不在本批；它是 Collision 的事，交使用者另外決定。
- `GYO::Model` 的 `AnimationEvents.cpp` 也用回呼查重複鍵，有同樣的二次方，改用 `StrictJson.hpp` 是另案（第 7 節，PR #161 的 U4）。本批不改。

## 2. 設計概要

### 2.1 區塊地圖格式：一張地圖的兩種檔

```text
<地圖目錄>/
├── map.json            gyo.voxel_map v1     索引（≤ 1 MiB）
└── regions/…json       gyo.voxel_region v1  一個 region＝8×8 個區塊柱 × 全高（≤ 16 MiB）
```

Engine 只解析文字，不開檔、不知道目錄；檔名只是索引裡的字串，讀檔是呼叫端的事。

**`gyo.voxel_map` v1**（欄位全部必填，沒有可選欄位）：

```json
{
  "schema": "gyo.voxel_map", "version": 1,
  "source_sha256": "<64 hex>", "build_version": "voxel_map_editor 2",
  "origin": [0, 0, 0], "cell_size": 0.5,
  "chunk_size": 16, "region_size": 8,
  "bounds": { "min": [-2, 0, -2], "max": [5, 3, 5] },
  "palette": [ { "name": "a", "faces": [[r,g,b,a], "…六色"] } ],
  "regions": [ { "at": [-1, -1], "file": "regions/r_-1_-1.json", "sha256": "<64 hex>" } ]
}
```

| 規則 | 內容（現 v1 規則沿用處標「同 v1」） |
|---|---|
| M1 | 一個 JSON 物件，欄位恰為上列 12 個；任何物件的鍵不重複（同 v1 規則 1） |
| M2 | `schema`＝`"gyo.voxel_map"`、`version`＝整數 1 |
| M3 | `source_sha256`、`build_version` 同 v1 規則 3 |
| M4 | `origin` 三個整數（格）；`cell_size` 同 v1 規則 4 |
| M5 | `chunk_size` 是整數 16、`region_size` 是整數 8（v1 固定；寫進檔，日後改值是新版本） |
| M6 | `bounds` 恰有 `min`、`max`，各三個整數（區塊座標，兩端都包含），每軸 `min ≤ max`；整個範圍的每個角（`16 × origin＋256 × 區塊座標＋{0, 256}`）在 ±2²⁰ 單位內 |
| M7 | `palette` 同 v1 規則 5（1～256 項，名字唯一） |
| M8 | `regions` 1 項以上；每項恰有 `at`（兩個整數：region 的 x、z）、`file`（1～128 字元的 `a-z 0-9 _ . - /`，不以 `/` 開頭、不含空段與 `.`、`..` 段，在清單內唯一）、`sha256`（64 個小寫十六進位）；`at` 在 `bounds` 換算的 region 範圍內；依 `at`（x, z）嚴格遞增 |
| M9 | 每個整數在 ±2³⁰ 內（同 v1 規則 8 前半） |
| M10 | 文字 ≤ 1 MiB |

**`gyo.voxel_region` v1**：

```json
{
  "schema": "gyo.voxel_region", "version": 1,
  "at": [-1, -1],
  "chunks": [ { "at": [-2, 0, -2], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": [{ "min": [0,0,0], "max": [16,16,16] }] } ]
}
```

| 規則 | 內容 |
|---|---|
| R1 | 一個 JSON 物件，欄位恰為 `schema`、`version`、`at`、`chunks`；鍵不重複 |
| R2 | `schema`＝`"gyo.voxel_region"`、`version`＝整數 1 |
| R3 | `at` 是兩個整數，且是索引 `regions` 的一項；文字的 SHA-256 等於該項的 `sha256`（用 `GYO::Base` 的 `Sha256`） |
| R4 | `chunks` 1 項以上，每項恰有 `at`、`quads`、`colliders`；`at` 三個整數，屬於本 region（`floor(x / 8)`、`floor(z / 8)` 等於 region 的 `at`），在索引的 `bounds` 內；依區塊鍵全序嚴格遞增（空區塊省略，所以重複不可能） |
| C1 | 一個區塊至少有一個 quad 或一個 collider |
| C2 | quad 同 v1 規則 6（七個整數、`dir` 0～5、`w`、`h` ≥ 1、palette 是索引的 palette 的索引） |
| C3 | collider 同 v1 規則 7 |
| C4 | 座標是**區塊內的 1/16 格單位**：quad 與 collider 的每個角在每軸 [0, 256] 內（256＝區塊的對面，面可以落在區塊邊界上） |
| C5 | quads、colliders 各自嚴格遞增（同 v1 規則 9） |
| R5 | 每個整數在 ±2³⁰ 內；文字 ≤ 16 MiB |

- **世界座標**：`float(local + 256 × key + 16 × origin) × cell_size / 16`，整數和用 int64。M6 讓每個角在 ±2²⁰ 內，所以轉 float 精確，**公式與 v1 完全相同**（v1 的 `units + 16 × origin` 換成 `local + 256 × key + 16 × origin`）。
- **區塊鍵全序**：`VoxelChunkKey{x, y, z}` 依 (x, y, z) 字典序；region 鍵 (x, z) 字典序。所有疊代順序（region 清單、區塊清單、展開）都由它定義。
- **哪個 region 屬於哪些區塊**：region (rx, rz) 擁有 `floor(cx / 8) = rx`、`floor(cz / 8) = rz` 的全部 cy。

**API**（`engine/voxel_mesh/VoxelMesh.hpp`，同一模組）：

```cpp
struct VoxelChunkKey { std::int32_t x, y, z; friend auto operator<=>(const VoxelChunkKey&, const VoxelChunkKey&) = default; };
struct VoxelRegionKey { std::int32_t x, z; friend auto operator<=>(…) = default; };
struct VoxelMapDocument   { …索引的欄位… };
struct VoxelChunkDocument { VoxelChunkKey at; std::vector<VoxelQuad> quads; std::vector<VoxelBox> colliders; }; // 區塊內單位
struct VoxelRegionDocument{ VoxelRegionKey at; std::vector<VoxelChunkDocument> chunks; };

VoxelMeshResult<VoxelMapDocument>    ParseVoxelMap(std::string_view text);
VoxelMeshResult<VoxelRegionDocument> ParseVoxelRegion(std::string_view text, const VoxelMapDocument& map);
VoxelMeshResult<void>                ValidateVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map); // C1～C5、R4 的 bounds；不經文字
VoxelChunkGeometry  ExpandVoxelChunk(const VoxelChunkDocument& chunk, const VoxelMapDocument& map); // 世界座標的頂點、索引、碰撞盒
VoxelPaletteImage   ExpandVoxelPalette(const VoxelMapDocument& map);  // 全圖一張 palette 圖
VoxelRegionKey      RegionOf(VoxelChunkKey key, const VoxelMapDocument& map);
Math::Vec3          ChunkWorldOrigin(VoxelChunkKey key, const VoxelMapDocument& map);
Math::Vec3          MapUnitsToWorld(Math::Vec3i units, const VoxelMapDocument& map); // 地圖原點起的 1/16 格整數（MapMarkers 的座標用這個換）
```

- **一份規則實作**：Parse 先做 JSON 層的檢查（型別、整數、欄位、±2³⁰），把值放進 int64 的內部形式；`ValidateVoxelChunk` 把公開型別（int32、`VoxelFace`）轉成同一個內部形式；**兩者呼叫同一個規則函式**（C1～C5、bounds）。錯誤路徑：Validate 回 `quads[3][4]`，Parse 回 `chunks[2].quads[3][4]`。
- **展開分成兩個函式**（proposal 寫 `ExpandVoxelChunk → VoxelMeshGeometry`）：palette 圖是全圖一張，每個區塊都帶一份是浪費，所以 `ExpandVoxelChunk` 回不含 palette 的 `VoxelChunkGeometry`，palette 圖由 `ExpandVoxelPalette` 給一次。uv 公式不變（以全圖 palette 的寬為欄數）。
- **錯誤碼**：`VoxelMeshErrorCode` 加 `InvalidVoxelMap`、`InvalidVoxelRegion`、`InvalidVoxelChunk`。
- **v1**：`ParseVoxelMesh`／`ExpandVoxelMesh` 依第 7 節的決定處理（建議：本批保留並標 deprecated）。

### 2.2 `GYO::MapMarkers`

```json
{
  "schema": "gyo.map_markers", "version": 1,
  "markers": [
    { "id": "m1", "kind": "a", "tags": {},              "point": [0, 16, 0], "yaw": 1.5 },
    { "id": "m2", "kind": "b", "tags": { "k": "v" },    "box": { "min": [0, 0, 0], "max": [32, 32, 32] } },
    { "id": "m3", "kind": "c", "tags": { "x": "1", "y": "2" }, "path": [[0, 0, 0], [16, 0, 0]] }
  ]
}
```

| 規則 | 內容 |
|---|---|
| K1 | 一個物件，欄位恰為 `schema`、`version`、`markers`；鍵不重複 |
| K2 | `schema`＝`"gyo.map_markers"`、`version`＝整數 1 |
| K3 | `markers` 0～65,536 項，依 `id`（位元組序）嚴格遞增（所以唯一） |
| K4 | 每項恰有 `id`、`kind`、`tags`，再加**恰好一組**幾何：`point`＋`yaw`、`box`、`path` |
| K5 | `id` 與 `kind`：1～64 字元的 `a-z 0-9 _ . -`；Engine 不解釋 |
| K6 | `tags`：0～32 個鍵值；鍵 1～64 字元的 `a-z 0-9 _ .`，在文字中依位元組序嚴格遞增；值是 0～256 字元的可列印 ASCII 字串 |
| K7 | `point`：三個整數；`yaw`：數字，轉成 float 後有限（弧度，範圍與方向的意義屬於產品） |
| K8 | `box`：恰有 `min`、`max`，各三個整數，每軸 `min < max` |
| K9 | `path`：2～4,096 個點，每點三個整數 |
| K10 | 座標是地圖原點起的 1/16 格整數，每個在 ±2²⁰ 內；文字 ≤ 4 MiB |

- **Engine 只檢查形式**。`kind` 和 tags 的意義、哪些 kind 合法、標記是否站得住，都是產品的（產品的 `marker_kinds.json`）。
- **座標換成世界**：MapMarkers 不知道 cell_size 與 origin（那是地圖的事實，只放一處）；消費端用 `GYO::VoxelMesh` 的 `MapUnitsToWorld`。MapMarkers 回整數。
- **API**：`ParseMapMarkers(text) → MapMarkersResult<MapMarkersDocument>`；錯誤碼 `MapMarkersErrorCode::InvalidMapMarkers`，detail 是路徑（`markers[2].tags`）。
- **鍵在文字中的順序**：nlohmann 的 `json` 用 `std::map`，會把鍵排序而看不到文字裡的順序；K6 要求文字順序，所以 MapMarkers 用 `nlohmann::ordered_json` 解析（第 0 節的共用解析入口是模板，兩種 DOM 都能用）。
- **玩法名詞**：Engine 的文件、範例、測試資料、程式碼不出現 `spawn`、`boss`、`loot`、`extract`、`patrol`、`team`、`zone`（守衛見 J7）。

## 3. Architecture Delta（AGENTS §3 的 7 項）

### Delta A：`GYO::VoxelMesh` v2 的公開介面（`gyo.voxel_map`／`gyo.voxel_region`）與 v1 退役

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | 塔科夫規模的 raid 地圖（約 1 km²、cell 0.5 m）：編輯器能局部重烘、產品能整張載入與依 region 畫（`-tool-chunked`、`-product-arena-v3`） |
| 2 現在的問題 | v1 是單文件、單一陣列：工具每改一格要重烘整張並來回文字（explore-p1 每幀 2.7 s）；單文件 16 MiB 上限撐不住 km²；Engine 沒有「區塊」這個驗證單位，工具若自己驗證區塊就違反 DI 3（一個驗證者） |
| 3 變化的邊界 | Data Contract（工具 → Engine 格式 → 產品）多兩種格式；Engine 的驗證單位從「一份文件」變成「一個區塊」（文字是 region、索引是 map） |
| 4 影響 | Engine `GYO::VoxelMesh`；之後的 voxel_map_editor（寫 region、呼叫 `ValidateVoxelChunk`）、object_fps_pvp（讀 map＋region）。本批不改兩者 |
| 5 依賴方向 | 不變：VoxelMesh 仍只連 Base、Math、nlohmann（PRIVATE）；新增使用 `Base::Sha256`（同一個 Base，沒有新邊） |
| 6 Ownership | 格式與唯一驗證者仍是 Engine；「區塊是驗證與展開的單位、region 是檔案單位」是 Engine 的；渲染單位、載入策略、平行解析是產品的；烘焙、greedy、區塊內合併是工具的 |
| 7 更小的變更為何不夠 | 只提高 v1 的 16 MiB 上限：工具仍要整張重烘、解析仍是一個巨大陣列、沒有區塊驗證入口；讓工具自己切檔：格式變成工具的，Engine 失去唯一驗證者；只加 `ValidateVoxelChunk` 不加 region 格式：產品讀不到分檔的地圖 |

- **DI 增補**：6「區塊是驗證與展開的單位，region 是檔案單位；Engine 不知道渲染單位與載入策略」；7「所有疊代順序由區塊鍵與 region 鍵的全序定義」；DI 3 改寫成「`ParseVoxelMap`、`ParseVoxelRegion`、`ValidateVoxelChunk` 共用同一份規則實作」；DI 2 的入口清單加上新函式。
- **RC 增補**：5「區塊需要自己的 palette（全圖 palette 超過 256，或量到 palette 索引是 region 文字量的主要部分）」；6「換掉解析方式後，最大地圖的載入時間仍超過該批宣告的值 → 二進位快取」；7「平行解析不進本模組：需要時由呼叫端以 `GYO::Threads` 平行呼叫 `ParseVoxelRegion`，結果依 region 鍵排回；若本模組必須自己平行才夠快，停下交使用者」。
- 現 RC 3（「量到需要分區塊或空間索引」）：本批就是它的處置；改寫成「已處置（v2）」並留紀錄。

### Delta B：新 Subsystem `GYO::MapMarkers`

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | raid 地圖的放置資料（出生點、撤離點、區域、巡邏路徑之類，**意義是產品的**）要能在編輯器放、在產品讀；survey 決定 `v8-arch-big-map-markers-owner` |
| 2 現在的問題 | 放置資料寫在產品的 arena 檔（手寫座標表，`spawns`）；voxel 編輯器要編輯它就得讀產品格式，違反工具 DI 1（不點名產品） |
| 3 變化的邊界 | 新 Top-level 以下的 Engine 模組（`engine/map_markers/`），新 Data Contract `gyo.map_markers` v1 |
| 4 影響 | Engine（新模組、`engine/CMakeLists.txt` 一行 `add_subdirectory`、測試目錄）；之後的 voxel_map_editor、object_fps_pvp |
| 5 依賴方向 | 新邊：`GYO::MapMarkers → GYO::Base, GYO::Math`（＋nlohmann PRIVATE）。沒有模組依賴它（之後由產品與工具依賴）；不依賴 VoxelMesh、Collision |
| 6 Ownership | 格式與形式驗證：Engine；`kind`／tags 的意義、合法 kind 清單、語意驗證：產品；編輯：工具。比照 `gyo.anim_events`（Engine 解析、名字不透明、產品賦義） |
| 7 更小的變更為何不夠 | 放進 `GYO::VoxelMesh`：違反它的 DI 1（「no spawn points」）與 DI 2（入口清單），而且標記與幾何的生命週期不同（標記改了不必重烘幾何）；放進 framework：不是「多個 Engine 資產的組合」（Layers 表）；放進產品：工具要讀產品格式 |

- **DI**：1 中立（只知道 id、kind、tags、點、盒、路徑；名字不透明；文件與資料不出現玩法名詞）；2 只連 Base、Math、nlohmann（PRIVATE）；3 一個驗證者 `ParseMapMarkers`；4 嚴格；5 整數座標（不在本模組換成 float）。
- **RC**：1 標記需要語意驗證（例如「必須站得住」）→ 那是產品的，不擴本模組；2 需要點、盒、路徑以外的形狀（多邊形、球）→ 新版本；3 標記需要引用別的資產 id → 那是組合，重新判斷 Layers 表的歸屬。

## 4. 受理判定（看結果前宣告；核准後不放寬）

| 代號 | 判定 | 怎麼量 |
|---|---|---|
| F1 規則 | `gyo.voxel_map` 的 M1～M10、`gyo.voxel_region` 的 R1～R5／C1～C5、`gyo.map_markers` 的 K1～K10：每條至少一個 test case（名稱以規則代號開頭），正例與反例都有。`ValidateVoxelChunk` 對 C1～C5 有自己的反例；同一缺陷 Parse 與 Validate 都失敗 | doctest |
| F2 突變 | `engine_format_mutations.json`：每條規則至少一個突變，加上「Validate 不呼叫共用規則」、展開的區塊偏移、`ChunkWorldOrigin` 和 `MapUnitsToWorld` 漏掉原點、palette 圖的排列、依賴守衛、名詞守衛。**每個都要被殺**；既有的 `voxel_mesh_mutations.json` 與 `engine_grid_mutations.json` 也全部仍被殺 | 突變腳本 |
| F3 黃金值 | `ExpandVoxelChunk` 的頂點、索引、uv、碰撞盒，以及 `ChunkWorldOrigin`、`MapUnitsToWorld`：以 float bits 和字面值比對，含 ±2²⁰ 兩端與負區塊鍵。另有「同一份幾何以 v1 單文件與以區塊展開，位元相同」。Mac 本機與 CI 的 Linux、Windows 都要通過 | doctest＋L1 |
| F4 依賴與名詞守衛 | configure 時檢查：`gyo_voxel_mesh` 和 `gyo_map_markers` 都只連 Base、Math、nlohmann，各配一個突變。名詞：Python 守衛檢查 MapMarkers 的程式、測試、契約文件不含玩法名詞，也配一個突變 | CMake＋Python |
| F5 迴歸 | 本機全量 cpu\|shader 與 gpu 測試全部通過；CI L1 三平台通過 | ctest＋L1 |

**突變存活時的處理（PR #161 的教訓）：**
- 存活的突變不能由實作者自己認定成「等價突變」。PR #161 的 `grid-margin-zero` 就是這樣被推定為等價，後來審查者用反例推翻。
- 存活就是 F2 不成立：先報告使用者，再補測試。
- 若要主張等價，要附上推理所依據的程式碼位置，交審查者找反例，由使用者決定。
- 拆批前那一輪存活的 `mm-two-shapes-accepted`，在這條規則下先補測試：K4 的測試除了檢查路徑，也要檢查訊息「exactly one of point, box and path」。補完後它應該被殺。

**範圍的限界：** 本批沒有時間判定（解析時間在第一批）。合成圖是雜訊，不代表手工地圖。

## 4a. 結果（摘要；細節在 dev_log）

- 本機 F1～F5 成立。突變：engine-format 55/55、v1 20/20、engine-grid 16/16。cpu\|shader 132/132，gpu 4/4。
- L1 三平台待 PR #162。

## 5. 證據位置

- 主 checkout 的 `build/evidence/big-map/engine-format/`（git 不追蹤）：突變結果、全量建置與 ctest 的輸出。
- 拆批前那一輪的參考結果在 `build/evidence/big-map/engine-map/`。

## 6. 模型、檔位與估時

上段（Engine 公開介面、新 Subsystem、DI）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus 5.5／high（主會話） | 2026-11-06 使用者同意「領航員和檔位照建議」 |
| 審查者 | engine-grid 會話（Opus） | 架構師指定；與實作者不同會話 |
| 驗證者（§14.4 的 1～5，若有判定變更） | Fable 5.1／xhigh，比主會話強，啟動前先問使用者 | 同上 |

- 估時：4～6 h，實作已有，主要是補測試、跑突變、文件與 L1。超過 9 h、或單一問題累計超過 2 h，就停下回報。
- 停下條件：AGENTS §14 冒頭；F2 有突變存活；黃金值跨平台不一致。

## 7. 要使用者決定的事

1. **v1 退役的時點**（選項 (a) 已核准，這裡寫成可檢查的條件）：
   - 本批保留 `ParseVoxelMesh`、`ExpandVoxelMesh`、`VoxelMeshDocument`、`VoxelMeshGeometry`，標 deprecated，不再擴充。
   - 刪除放在 `v8-arch-big-map-product-arena-v3`：該批把 `pvp_arena`、`pvp_corners` 遷移到 `gyo.voxel_map` 後，**在同一批**刪掉 v1 的程式、測試、`voxel_mesh_mutations.json` 裡只守 v1 的突變，以及 voxel-mesh.md 的 v1 節。
   - 前提：工具在 `-tool-chunked` 已經改成寫 region。刪除時 repo 裡不能再有呼叫 v1 的程式；用 grep `ParseVoxelMesh` 是否為 0 件來確認，寫進該批的受理判定。
2. **`collision.md` 加兩條 Replacement Criteria**（PR #161 審查者的 N4、U1）：
   - RC 4：盒子對格子的登記數或建構時間，量到超過產品批宣告的值時，改用大盒子清單或分層格（以分層格優先）。
   - RC 5：一個離群盒子會讓格長自動加倍、整體變慢（結果不變）。產品批要記錄 `CellLength()`，量到它是給定格長的 4 倍以上時，回報給使用者。
3. **另案記錄（不開卡）：** `GYO::Model` 的 `AnimationEvents.cpp:40` 改用 `StrictJson.hpp`（PR #161 的 U4）。寫進 HANDOFF 的「之後」。
4. **突變存活的處理規則**（第 4 節）：存活就是不成立，不由實作者自己認定等價。
5. **模型：** 實作者 Opus、審查者 engine-grid 會話、驗證者 Fable（先問）。
6. **DI 1 改成中性說法**（`v8-arch-big-map-neutral-wording`）：`voxel-mesh.md` 的 DI 1 原文以否定句列出「spawn points」，改成「placement data」。
   守的性質不變（模組不知道放置資料），只換詞。名詞守衛擴到 `GYO::VoxelMesh` 的程式、測試與契約，並加突變 `vm-guard-gameplay-word`。
   使用者的決定經架構師會話轉達（2026-11-06）。本會話原本的建議是照原文保留。
