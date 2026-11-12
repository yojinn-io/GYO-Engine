# 大地圖 engine-map：事前宣告（`v8-arch-big-map-engine-map`）

更新：2026-11-06。Owner：Engine（`GYO::VoxelMesh`、新 `GYO::MapMarkers`、`GYO::Collision`、`GYO::Base` 的一個標頭）。
計畫：[README](README.md)；記錄：[HANDOFF](HANDOFF.md)；根據：[survey](survey.zh-Hant.md) 與 HANDOFF「決定」的六項（與 [proposal v0.1](proposal-v0.1.zh-Hant.md) 不一致處以決定為準）。
**狀態：2026-11-06 使用者核准（「照建議核准」：第 7 節四件事全部採建議；v1 退役＝(a)、J5 門檻 450 ms）。之後不放寬。**

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-big-map-engine-map` | 本批：Engine 的區塊地圖格式、標記層、靜態碰撞 broadphase、解析加速 |
| `em-format` | 區塊地圖格式：`gyo.voxel_map` v1（索引）＋`gyo.voxel_region` v1（region 檔）與它的 API |
| `em-markers` | 新 Subsystem `GYO::MapMarkers`：`gyo.map_markers` v1 |
| `em-grid` | `GYO::Collision` 的靜態 broadphase `StaticAabbGrid` |
| `em-parse` | 解析替換：不經回呼的嚴格解析（契約不變） |
| `em-v1-retire` | `gyo.voxel_mesh` v1 單文件格式的退役時點（第 7 節，使用者決定） |

## 1. 範圍

### 做

- `em-format`：`gyo.voxel_map` v1、`gyo.voxel_region` v1 的契約、驗證、展開；`ValidateVoxelChunk`（不經文字）與 Parse 共用一份規則實作。
- `em-markers`：`engine/map_markers/`、`docs/architecture/map-markers.md`。
- `em-grid`：`StaticAabbGrid` 與等價契約、與線性掃描逐一比對的守衛。
- `em-parse`：`GYO::VoxelMesh` 與 `GYO::MapMarkers` 共用的「SAX 查重複鍵＋不帶回呼的 `json::parse`」。
- 文件：`docs/architecture/voxel-mesh.md` 升版、`docs/architecture/map-markers.md` 新增、`docs/architecture.md` 的模組表與 Model／collision 段各加一句。
- 測試、突變 catalog、黃金值、依賴守衛、文件字串守衛、解析時間量測、dev_log、HANDOFF、README。

### 不做

- 不改產品（arena v3、Movement、ShotQuery、ArenaMesh）與工具程式碼；不碰 framework、Gateway、pv10；不改 PR #152 的文件。
- 不換第三方解析器、不做平行解析、不做二進位、不做 LOD。
- 不替使用者決定 `em-v1-retire` 與解析門檻（第 7 節）。

## 2. 設計概要（契約文件在實作時寫成英文正式版）

### 2.1 `em-format`：一張地圖的兩種檔

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

### 2.2 `em-markers`：`GYO::MapMarkers`

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
- **鍵在文字中的順序**：nlohmann 的 `json` 用 `std::map`，會把鍵排序而看不到文字裡的順序；K6 要求文字順序，所以 MapMarkers 用 `nlohmann::ordered_json` 解析（2.4 的共用解析是模板，兩種 DOM 都能用）。
- **玩法名詞**：Engine 的文件、範例、測試資料、程式碼不出現 `spawn`、`boss`、`loot`、`extract`、`patrol`、`team`、`zone`（守衛見 J7）。

### 2.3 `em-grid`：`StaticAabbGrid`

```cpp
enum class TieBreak : std::uint8_t { LowestIndex, HighestIndex };
struct RaycastHit { float distance; std::uint32_t index; };
struct ContactHit { Contact contact; std::uint32_t index; };

class StaticAabbGrid final {
public:
    StaticAabbGrid(std::span<const Math::Aabb> boxes, float cellLength); // 複製盒子；cellLength 只是效能提示
    std::size_t Size() const noexcept;
    std::vector<std::uint32_t> Overlapping(const Math::Aabb& bounds) const;
    std::optional<RaycastHit>  RaycastNearest(const Math::Ray& ray, float maximumDistance, TieBreak tie) const;
    std::optional<ContactHit>  SweepVerticalCapsuleNearest(const VerticalCapsule& capsule, Math::Vec3 displacement, TieBreak tie) const;
    std::vector<ContactHit>    SweepVerticalCapsuleAll(const VerticalCapsule& capsule, Math::Vec3 displacement) const;
    std::vector<ContactHit>    OverlapVerticalCapsuleAll(const VerticalCapsule& capsule) const;
};
```

**契約＝等價定義**：每個查詢的結果，定義為「對全部盒子依索引 0, 1, …, n−1 線性掃描」的結果，位元相同：

| 查詢 | 等價的線性掃描 |
|---|---|
| `Overlapping(b)` | 收集 `i`，使盒子 i 與 b 相交（閉區間，相切算相交）；遞增 |
| `RaycastNearest(ray, d, tie)` | `h = RaycastAabb(ray, d, box[i])`；有值且（還沒有結果、或 `h < best`（LowestIndex）／`h <= best`（HighestIndex））就換成 (h, i) |
| `SweepVerticalCapsuleNearest(c, δ, tie)` | 同上，比 `SweepVerticalCapsuleAgainstAabb(c, δ, box[i])->fraction` |
| `SweepVerticalCapsuleAll(c, δ)` | 收集所有有值的 `(SweepVerticalCapsuleAgainstAabb(c, δ, box[i]), i)`；遞增 |
| `OverlapVerticalCapsuleAll(c)` | 收集所有有值的 `(OverlapVerticalCapsuleAabb(c, box[i]), i)`；遞增 |

- **為什麼比卡片多兩個 `…All`**：對照消費端，產品有三個迴圈不是「取最近」而是「任一個符合」或「取最深」，而且條件是產品的：
  `IsCharacterSupported`（任一 `normal.y > 0.5`）、`CanPlaceCharacterBody`（任一穿透 > 1e-5）、`MoveCharacterBody` 的穿透解除（取最深、`>` 取先、每面牆再做 `PlanarPenetration`）與 `constrainToFloor` 的掃掠（先改法線、略過、再 `<`）。
  若只給 `Overlapping`，產品要自己算「掃掠或膠囊可能碰到的範圍」，而那個範圍要比浮點的捨入保守，等於把 broadphase 的邊界知識搬到產品。`…All` 讓 Engine 負責保守的候選範圍，產品只換掉迴圈的來源、條件不動。
- **消費端零語意變更的對應**（產品批照這張表換）：

  | 產品現行程式（只讀，本批不改） | 平手 | 換成 |
  |---|---|---|
  | `ShotQuery.cpp:99` 牆：`*distance <= closest.distance` | 取後（`<=`） | `RaycastNearest(…, HighestIndex)`；結果只用距離 |
  | `Movement.cpp:76`→`IsCharacterSupported`：任一 `normal.y > 0.5` | 無（任一） | `SweepVerticalCapsuleAll`＋同條件 |
  | `CanPlaceCharacterBody`：任一穿透 > 1e-5 | 無 | `OverlapVerticalCapsuleAll`＋同條件 |
  | `MoveCharacterBody` 穿透解除：最深、`>` | 取先 | `OverlapVerticalCapsuleAll`（遞增）＋同迴圈；`PlanarPenetration` 用 `walls[index]` |
  | `MoveCharacterBody` 掃掠、`constrainToFloor = false`：`<` | 取先 | `SweepVerticalCapsuleNearest(…, LowestIndex)` |
  | `MoveCharacterBody` 掃掠、`constrainToFloor = true` | 取先（過濾後） | `SweepVerticalCapsuleAll`＋同迴圈 |

  牆之後才看 actor 的順序不變：牆的結果等於線性掃描，actor 迴圈照舊接在後面。
- **實作**：均勻格（密集 CSR，覆蓋所有盒子的範圍）；格長由呼叫端給，**格子數超過 2²² 時把格長加倍到不超過為止**（結果依契約與格長無關，所以這不是錯誤路徑）。
  格子索引用 double 的 `floor((x − 原點) / 格長)`，盒子登記到它的閉區間覆蓋的全部格子；查詢用同一個函式，單調性保證「相交的兩個區間，格子範圍必有交集」。
  掃掠、膠囊、射線的候選範圍在 double 中以邊際 `1e-3 + 1e-5 × 最大座標絕對值` 擴大（比 Collision 的 1e-7 容許值與 double 捨入大很多個數量級；多的候選只花時間，不改結果）。
  候選排序去重後，**用 Collision 的精確查詢依索引順序跑同一個比較**，所以結果等於線性掃描。
  射線把 [0, d] 切成長度＝格長的段，逐段收集候選；已找到的最近距離 `h` 之後、起點超過 `h＋邊際` 的段不再看。
- **不做**：動態物體、非 AABB、多執行緒建構（RC）。查詢是 const，不改內部狀態。
- **前提**（違反是 Programmer Error，`GYO_ASSERT`）：每個盒子 `IsValid`、`cellLength` 有限且 > 0、盒子數 < 2³²；查詢的輸入與 Collision 現有查詢相同的前提。

### 2.4 `em-parse`：解析替換（契約不變）

- 現在：`json::parse` 帶回呼查重複鍵 → nlohmann 回呼解析器在 `end_object` 對父陣列的線性掃描，物件數的二次方（survey §5(c)）。
- 換成：(1) `sax_parse` 掃一次，每個開著的物件一個鍵集合，重複就失敗；(2) 不帶回呼的 `parse`。規則程式不動。
- **放哪裡**：`engine/base/include/engine/base/StrictJson.hpp`，一個**只依賴標準函式庫的模板**：`template <class Json> Json ParseStrictJson(std::string_view text, auto onError)`。
  它的 SAX 處理器只透過模板參數（`Json::number_integer_t`、`Json::sax_parse`、`Json::exception`）碰 nlohmann，標頭本身不 include nlohmann，所以 Base 仍是「header-only、只依賴標準函式庫」；只有 instantiate 它的模組（VoxelMesh、MapMarkers，都 PRIVATE 連 nlohmann）需要 nlohmann。
  - 為什麼不放 voxel_mesh 內部：MapMarkers 不能依賴 VoxelMesh（兩個並列的格式模組，一個依賴另一個沒有理由）。
  - 為什麼不另開模組：卡片指定不開；一個 60 行的模板不值得一個 target。
  - 為什麼不複製兩份：同一個 Adapter 重複（AGENTS §3 的 evidence）；而且 `GYO::Model` 的 `AnimationEvents.cpp:40` 已經是第三份同樣的回呼（**本批不改**，記為 Code Smell）。

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

### Delta C：`GYO::Collision` 的靜態 broadphase

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | raid 地圖的 Match（4 人＋AI）與 Client 預測；survey §5(b)：2 萬盒時一位玩家一步約 2.2 ms，4 人約用掉 tick 的 53 %，加 AI 超過 |
| 2 現在的問題 | 產品對全部牆線性掃描；`walls ≤ 1024` 是現在不出事的原因，大地圖要撤掉它 |
| 3 變化的邊界 | `GYO::Collision` 的公開介面多一個型別與五個查詢；它的契約新增「結果與線性掃描等價」 |
| 4 影響 | Engine `GYO::Collision`；之後的 object_fps_pvp（`Movement`、`ShotQuery`、`CharacterCollision`、spawn 驗證） |
| 5 依賴方向 | 不變（Collision → Math，Base PRIVATE）；不新增依賴 |
| 6 Ownership | 空間索引與等價契約：Engine；牆是什麼、移動與命中的政策（平手以外的條件）：產品 |
| 7 更小的變更為何不夠 | 產品自己做格子：每個用碰撞的產品各寫一份，而且保守範圍的浮點知識留在產品；只給 `Overlapping`：同上（產品要算保守範圍）；換成樹或 Jolt：動態結構與非決定性（survey §2.6），而 GYO 的盒子是靜態的 |

- **DI**（Collision 文件新增一節）：1 等價：每個 broadphase 查詢的結果與對全部盒子依索引線性掃描的結果位元相同，平手依 `TieBreak`；格長只影響效能；2 靜態：建好不變，查詢不改內部狀態。
- **RC**：1 動態物體（門、可破壞物）；2 非 AABB 的靜態形狀；3 量到格子的桶極不均勻或盒子大小差很多 → 分層格（Ericson §7.2，survey 未讀，記為候選），再來才是樹。

### Delta D：解析替換與 `GYO::Base` 的 `StrictJson.hpp`

| 項 | 內容 |
|---|---|
| 1 需要它的 Feature | 同 Delta A（km² 地圖的載入時間）；survey 決定 `v8-arch-big-map-encoding` |
| 2 現在的問題 | 回呼解析對同一陣列的物件數二次方（20,691 個盒子約 1.1 s），格式稍改就可能從 0.1 s 變成數秒，不可預測 |
| 3 變化的邊界 | VoxelMesh 的內部（契約不變）；Base 的公開標頭多一個模板 |
| 4 影響 | `GYO::Base`（一個標頭）、`GYO::VoxelMesh`、`GYO::MapMarkers`；`GYO::Model` 的 `AnimationEvents` 同樣的回呼本批不改 |
| 5 依賴方向 | 不變：Base 仍只依賴標準函式庫（模板不 include nlohmann）；VoxelMesh、MapMarkers 各自 PRIVATE 連 nlohmann |
| 6 Ownership | 「嚴格 JSON（拒絕重複鍵）」這個機制放 Base；各格式的規則仍在各自模組 |
| 7 更小的變更為何不夠 | 不換：km² 地圖載入不可預測；只在 VoxelMesh 內換：MapMarkers 要複製一份（同一 Adapter 重複）；換第三方解析器：Engine 新依賴，使用者決定的另案 |

## 4. 受理判定（看結果前宣告；核准後不放寬）

| 代號 | 判定 | 怎麼量 |
|---|---|---|
| J1 規則 | `gyo.voxel_map` 的 M1～M10、`gyo.voxel_region` 的 R1～R5／C1～C5、`gyo.map_markers` 的 K1～K10：每條至少一個 test case（名稱以規則代號開頭），正例與反例都有；`ValidateVoxelChunk` 對 C1～C5 有自己的反例，且同一缺陷 Parse 與 Validate 都失敗 | doctest，Red→Green |
| J2 突變 | 每條規則至少一個突變（打掉該規則的判斷），加上 broadphase 的平手、格子邊界（floor 換 trunc、上界不含）、射線提前結束、候選不排序、邊際歸零（若存活就記錄為「等價突變」並說明理由，不刪測試）、共用規則（Validate 不呼叫共用函式）；**每個都要被殺**；catalog `docs/architecture/plans/big-map/scripts/engine_map_mutations.json`，用 `run_mutations.py` 跑；既有 `voxel_mesh_mutations.json` 全部仍被殺（`find` 字串因解析替換而失效的，改成新實作中的同一個判斷，記在 dev_log） | 突變腳本 |
| J3 黃金值 | `ExpandVoxelChunk` 的頂點、索引、uv、碰撞盒與 `ChunkWorldOrigin`、`MapUnitsToWorld`：以展開的 float bits 與字面值比對，含 ±2²⁰ 兩端與負區塊鍵；同一份測試在 Mac 本機與 CI 的 Linux、Windows 都通過（＝三平台一致）；另有一個測試：把 v1 的 `test` 地圖切成區塊後，`ExpandVoxelChunk` 的頂點集合與 `ExpandVoxelMesh` 位元相同 | doctest＋L1 |
| J4 broadphase 等價 | 200 組隨機盒子集合（種子固定，盒數 1～400，含負座標、零厚度、巨大盒子、重複盒子），每組每種查詢至少 50 次隨機查詢；加邊界案例（共面、相切、零厚度、起點在盒內、距離相同的平手、盒子恰在格線上、查詢在格子範圍外、射線平行軸、`maximumDistance = 0`、極小格長觸發加倍）；**每個查詢與測試內的線性掃描逐一比對位元相同**（optional 有無、index、float bits、Contact 各欄位 bits）；不一致件數＝0 | doctest |
| J5 解析時間 | 主 checkout 的 `build/evidence/big-map/survey/synthetic.voxel_mesh.json`（5,334,270 B，SHA-256 `1557bb3b…162a8b`；卡片寫的 explore-p1 目錄裡沒有文字檔，這份是 survey 量測用的同一張合成圖）；本機 Intel Mac、RelWithDebInfo（`-O2`），`ParseVoxelMesh` 連續 5 次、每次都成功，**中位數 ≤ 450 ms**（門檻由第 7 節核准；survey 的分項相加推算 0.31～0.35 s，加上分項之間的開銷，留約 30 % 餘裕）；另外同一程式在替換前（master `d54b60ef`）與替換後各算一次展開結果的 SHA-256，**必須相同**（契約不變的證據） | 量測程式，結果寫 HANDOFF |
| J6 迴歸 | 本機全量 cpu|shader 與既有 gpu 測試結果不變；CI L1 三平台通過 | ctest＋L1 |
| J7 依賴與名詞守衛 | configure 時：`gyo_voxel_mesh` 只連 Base、Math、nlohmann；`gyo_map_markers` 只連 Base、Math、nlohmann；`gyo_collision` 只連 Math、Base（現狀）；加突變（多連一個庫要讓 configure 失敗）。名詞：一支 Python 檢查 `engine/map_markers/`、`tests/common/map_markers/`、`docs/architecture/map-markers.md` 不含 `spawn|boss|loot|extract|patrol|team|zone`（不分大小寫），加突變（插入一個詞要失敗） | CMake＋Python |

**件數の突き合わせ**（J4、J5 的報告第一行）：J4＝產生的查詢數／比對數／不一致數；J5＝要求次數／完成次數／成功次數，以及展開後的 quads、colliders 數（124,236／20,691）。

**範圍的限界**：解析時間只量 Intel Mac 一台（Windows、Linux 不量，只靠 L1 的正確性）；broadphase 不設效能判定（不是本批的受理條件），只在 dev_log 記一次探索用的量測（未宣告、不判過與不過）；合成圖是雜訊，不代表手工地圖。

## 5. 證據位置

- 主 checkout `build/evidence/big-map/engine-map/`（git 不追蹤）：解析量測程式、原始輸出（`parse_time.txt`、`parse_digest_before.txt`／`after.txt`）、突變結果、broadphase 探索量測。
- 正式紀錄：本檔、[HANDOFF](HANDOFF.md)、dev_log `docs/dev_logs/2026_11_06_arch_v8_big_map_engine_map.zh-Hant.md`（日期依完成日）。

## 6. 模型與檔位

上段（Engine 公開介面、新 Subsystem、DI）。2026-11-06 使用者同意「領航員和檔位照建議」。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus／high（主會話）；解析替換與 broadphase 的平手、邊界局部 xhigh | 上段不用小模型；平手與浮點邊界是容易無聲出錯的部分 |
| 審查者 | Opus／high（子 Agent） | 上段 |
| 驗證者（§14.4 的 1～5，若有判定變更；以及 J4 的獨立重算） | Fable／xhigh（子 Agent，使用者已同意） | 與實作者不同模型，盲點不重疊 |

- 估時 16～24 h；超過 36 h 或單一問題累計 > 2 h，停下回報。

## 7. 要使用者決定的事

1. **`em-v1-retire`：`gyo.voxel_mesh` v1 的退役時點。**
   - **建議 (a)**：本批保留 `ParseVoxelMesh`／`ExpandVoxelMesh`，文件與標頭標 deprecated；`-product-arena-v3` 遷移兩張地圖後，在同一批刪掉 v1（含它的測試與突變）。理由：v1 現在被產品（arena v2）與工具使用，本批若刪就得同批改產品與工具，超出 Engine 範圍。代價：一段時間內 Engine 有兩種地圖格式（v1 只讀、不再擴充）。
   - (b) 本批刪 v1，同批遷移產品與工具：產品與工具的改動進本批，違反卡片的「不改產品與工具」。
2. **J5 的門檻：中位數 ≤ 450 ms**（5 次）。核准後看結果不放寬；不成立就停下回報。
3. **broadphase 比卡片多兩個查詢**（`SweepVerticalCapsuleAll`、`OverlapVerticalCapsuleAll`，理由在 2.3）與「格子數上限時格長自動加倍」。
4. **Delta A～D 的同意**（新 Subsystem `GYO::MapMarkers`、Base 新增 `StrictJson.hpp` 模板、Collision 公開介面、VoxelMesh v2）。
