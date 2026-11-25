# voxel_map Module（`gyo.voxel_map`、`gyo.voxel_region`）

Owner：Engine（`engine/modules/voxel_map` → `GYO::VoxelMap`；`include/engine/voxel_map/VoxelMap.hpp`、`VoxelMapLoaders.hpp`）。
來歷：大地圖線（[宣告](big-map-declaration.zh-Hant.md)第 2.1 節、第 12 節，`v8-arch-gyo2-big-map-s1`）。格式取自 master 的 `docs/architecture/voxel-mesh.md`「The chunked map (version 1)」，**wire 一字不改**；master 的工具與已烘好的地圖可以直接讀。
**狀態：S1 實作中（2026-11-25），合併前核准前。**

## 1. 它是什麼

工具在格子上做地圖、烘成「面＋碰撞盒」，切成區塊寫出來；這個 Module 只**讀、驗證、展開**工具寫的東西。它認得面、顏色、盒子、區塊、region 和不透明的名字，不認得遊戲賦予它們的意思，也不開檔：region 的 `file` 是讀者自己解析的名字。

- 作者選用（`library_only`），Engine 核心不認得它。依賴：Base、Math、Asset（loader 介面）、Json（私有）。
- 烘焙、隱藏面剔除、greedy、顏色的決定都是工具的（Engine 不承擔工具職能）。工具用「產出通過本 Module 的驗證」對齊。
- **區塊**（16³ 格）是驗證與展開的單位；**region**（8×8 個區塊柱 × 全高）是檔案的單位。讀者怎麼畫、怎麼載入、要不要快取是讀者的事。
- 所有疊代順序由鍵的全序定義：區塊鍵 (x, y, z)、region 鍵 (x, z)，字典序。

```text
<地圖目錄>/
├── map.json            gyo.voxel_map v1     索引（≤ 1 MiB）
└── regions/…json       gyo.voxel_region v1  一個 region（≤ 16 MiB）
```

## 2. 格式

索引與 region 的 JSON 形狀、單位、世界座標公式照 master `voxel-mesh.md` 的「The chunked map (version 1)」與單檔版的規則 3～9（被引用的部分）。這裡只重列規則的編號與要點，條文以程式與測試為準。

- **單位**：區塊內的 quad 與盒子以 1/16 格為單位，從區塊的最小角算起，每軸 0～256（256＝區塊的對面）。
- **世界座標**：`float(local + 256 × key + 16 × origin) × cell_size / 16`。整數和是精確的；M4、M6 讓每個角在 ±2²⁰ 單位內，所以轉 float 精確，乘法只捨入一次，除以 16 精確。
- **quad**：`[x, y, z, dir, w, h, palette]`。`dir` 0～5＝+X、−X、+Y、−Y、+Z、−Z（也是 palette 圖的列）。X 面沿 Z（w）與 Y（h），Y 面沿 X 與 Z，Z 面沿 X 與 Y。
- **palette**：整張地圖一份，1～256 項，每項六個 sRGB RGBA8 顏色（每個方向一個）。

### 2.1 索引（`ParseVoxelMap`，錯誤 `InvalidVoxelMap`）

M1 欄位恰為 11 個、鍵不重複、是 JSON｜M2 schema 與 version 1｜M3 `source_sha256` 64 個小寫十六進位、`build_version` 1～64 個可印 ASCII｜M4 `origin` 三個整數且 16×origin 在 ±2²⁰ 內、`cell_size` 轉成 float 在 [2⁻¹⁶, 2¹⁶]｜M5 `chunk_size` 16、`region_size` 8｜M6 `bounds` 的 min ≤ max、整個範圍的每個角在 ±2²⁰ 內｜M7 palette｜M8 `regions`：至少一項，`at` 在 bounds 的 region 範圍內、依 `at` 嚴格遞增，`file` 合法（1～128 個 `a-z 0-9 _ . - /`、沒有空段（所以不以 `/` 開頭、不以 `/` 結尾）、沒有以 `.` 結尾的段、沒有 Windows 裝置名的段）且不重複，`sha256` 64 個小寫十六進位｜M9 每個整數在 ±2³⁰ 內｜M10 文字 ≤ 1 MiB。

### 2.2 region：兩步

asset 的 loader 只拿得到位元組，拿不到索引（`LoadContext.hpp`），所以 region 分兩步讀，兩步跑同一組區塊規則：

| 步 | 函式 | 規則 |
|---|---|---|
| 1（不需要索引） | `ParseVoxelRegion(text)` | R1 欄位、JSON｜R2 schema 與 version｜R4 至少一個區塊、欄位、區塊在本 region 內、鍵嚴格遞增｜R5 整數 ±2³⁰、文字 ≤ 16 MiB｜每個區塊：C1 非空、C2 方向 0～5 與尺寸 ≥ 1、C3、C4、C5。記下文字的 SHA-256 |
| 2（對照索引） | `CheckVoxelRegion(region, map, expected)` | R3 `at`＝`expected`、文字的 SHA-256＝該項的 `sha256`｜每個區塊：C1 在 bounds 內、C2 palette 索引在 palette 內 |
| 兩步一起 | `ParseVoxelRegion(text, map, expected)` | 上面兩步 |
| 整張地圖 | `CheckVoxelMap(map, regions)` | 每個索引項各跑第 2 步，依索引順序；錯誤路徑以 `regions[i]` 開頭。經 asset 讀的地圖在全部 region Loaded 之後呼叫一次，回答「這張地圖合不合法」（設計審查 S-2） |
| 不經文字 | `ValidateVoxelChunk(chunk, map)`（錯誤 `InvalidVoxelChunk`） | 一個區塊的 C1～C5（含 bounds 與 palette）；工具的工作副本用 |

錯誤的 message 以規則名開頭（例 `C2: a width below 1`），detail 是出錯值的路徑：region 給 `chunks[2].quads[3][4]`，`ValidateVoxelChunk` 給 `quads[3][4]`；文字不是 JSON、重複的鍵、整份文字的 SHA-256 不合給空路徑。`expected` 不在索引裡是呼叫端的錯（assert）。

### 2.3 展開

- `AppendVoxelChunk(geometry, chunk, map)`／`ExpandVoxelChunk`：每個 quad 四個頂點、六個索引，依 quad 順序；形狀對上 `Model::Mesh`（positions、normals、uvs、indices）。三角形從面朝向的那一側看，在螢幕上是**逆時針**（gyo2 的正面，`RenderCommands.hpp:140`）；空間是左手系（`math.md`），所以 (v1−v0)×(v2−v0) **背向**法線（master 的正面是順時針，四個角的順序與它相反）。法線由方向導出；uv 指到 palette 圖裡那一格的中心。碰撞盒是世界座標的 `Math::Aabb`。
- 只接受驗證過的區塊（與同一份索引）。展開本身依賴的前提（區塊在 bounds 內、每個 quad 的方向與 palette 索引、32 位元索引放得下）是 assert；其餘規則不重跑（設計審查 S-1：重跑整套規則是擋誤用的守衛），沒驗證過的區塊得到的幾何契約不描述。
- `ExpandVoxelPalette`：palette 圖，一欄一項、一列一個方向，第 0 列在最上面。
- `RegionOf`、`ChunkWorldOrigin`、`ChunkWorldBounds`、`MapUnitsToWorld`：同一個公式。

### 2.4 當 asset

- `VoxelMapLoader`：type `voxel_map`、format `json`，輸出 `VoxelMapAsset`（索引＋每個 region 的 `AssetRef`）。**索引依順序把每個 region 宣告成參照**（`ReferencePath`，相對於索引檔的目錄），所以讀索引就是讀整張地圖。Product 的 catalog 要列出每個 region 檔，一個檔只能有一個條目（否則參照是 AmbiguousReference）。
- `VoxelRegionLoader`：type `voxel_region`、format `json`，輸出第 1 步的 `VoxelRegionDocument`。第 2 步由同時拿著索引與 region 的讀者呼叫 `CheckVoxelMap`（或逐個 `CheckVoxelRegion`）。
- **位元組要原樣保存**：索引用 SHA-256 綁住每個 region 的位元組（R3），所以存放地圖的地方不能改動它們。在 git 裡要對 `map.json` 與 `regions/*.json` 設 `-text`，否則 Windows checkout 會把換行轉成 CRLF，sha 就對不上（PvP 的 `assets/object_fps_pvp/maps/.gitattributes`；2026-11-25 的 L1 windows-x64 曾因此失敗）。
- region 清單在索引與 catalog 各有一份；兩者不合時參照失敗（UnlistedReference）。catalog 的 region 條目由烘焙工具和地圖一起產生，不手寫。
- 兩者都不收 settings（`{}` 可以），上限是 1 MiB／16 MiB。平行解析由 asset 的 worker 負責，Module 本身單執行緒。

## 3. Design Invariants

改下面任何一條都是 Architecture Delta，要使用者同意。

- **VM-1 中立**：只認得面、顏色、盒子、區塊、region 和不透明的名字；連結恰為 Base、Math、Asset、Json，不連 render、physics_3d、Kernel、World。守衛：`tests/common/voxel_map/CMakeLists.txt` 的連結檢查（突變 `vlink-physics`）。
- **VM-2 不烘焙**：只讀、驗證、展開、換算；不剔除、不合併、不決定顏色。守衛：審查。
- **VM-3 一個驗證者**：一份檔或一個區塊合不合法，只由本 Module 決定；region 的兩步與 `ValidateVoxelChunk` 跑同一組區塊規則函式。守衛：規則測試（每條規則一個 case，以規則名開頭）與 `vchunk-*`、`vreg-*`，其中 `vchunk-validate-skips-rules`。
- **VM-4 嚴格**：不認得的 schema、版本、欄位，重複的鍵、非正規的順序、超出範圍的值都是錯，不忽略、不夾、不修。守衛：規則測試與 `vmap-*`。
- **VM-5 決定性**：一份文件在每個平台展開成相同的 float 位元。守衛：展開測試逐位元比對（±2²⁰ 兩端、公式、捲繞、uv）與 `vexp-*`。
- **VM-6 區塊與 region**：區塊是驗證與展開的單位、region 是檔案的單位；讀者怎麼畫、怎麼載入是讀者的事。讀索引就宣告整張的參照（整張載入）。守衛：`vload-no-references`、`vreg-chunk-in-region`；其餘審查。
- **VM-7 鍵的順序**：region 在索引裡、區塊在 region 裡、quad 與盒子在區塊裡，都由鍵的全序定義。守衛：M8、R4、C5 的測試與 `vmap-region-order`、`vreg-chunk-order`、`vchunk-*-order`。

## 4. Replacement Criteria

停止修補、交使用者決定修還是重建，當：

- **RC-VM1** 碰撞需要軸對齊盒以外的形狀（斜坡），或面要離開軸向。
- **RC-VM2** 頂點要有自己的屬性（AO、烘焙光照），不再只由方向導出。
- **RC-VM3** palette 需要超過 256 項，或量到 palette 索引佔 region 文字的大半。
- **RC-VM4** 量到解析時間超過大地圖線 S5 宣告的值（yyjson 之下）：二進位快取（第二種編碼）交使用者決定（大地圖 RC-BM1）。
- **RC-VM5** 要只讀地圖的一部分（串流）：索引宣告整張的參照（VM-6）擋住它，要改 loader 的形狀（大地圖 RC-BM3、asset RC-AS1）。

## 5. 和 master 的差別

| master `GYO::VoxelMesh` | gyo2 `GYO::VoxelMap` | 理由 |
|---|---|---|
| Engine 擁有、核心模組 | 作者選用的 Module | gyo2 的 Module／Plugin 分法；使用者 11-21「用 Module 來增加地圖」 |
| `ParseVoxelRegion(text, map, expected)` 一步 | 兩步＋一步的組合 | asset loader 拿不到索引 |
| 展開只給位置與 uv、順時針 | 多給法線、逆時針（gyo2 的正面），角的順序反過來 | 直接對上 `Model::Mesh`；gyo2 照 Bevy 的正面 |
| 單檔 `gyo.voxel_mesh` v1（deprecated） | 不帶 | PvP 自己的讀者照舊，大地圖 S4 退役 |
| `MapMarkers` | 不帶 | 大地圖 RC-BM4 |
| 解析用 nlohmann＋StrictJson | gyo2 的 json Module（yyjson） | 已有的 Module |
