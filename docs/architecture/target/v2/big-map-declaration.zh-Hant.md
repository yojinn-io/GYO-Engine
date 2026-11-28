# 事前宣告：大地圖線（`v8-arch-gyo2-big-map`）

更新：2026-11-25（修訂 0）。Owner：大地圖線的任務卡（GYO2 主會話開，`v8-arch-gyo2-big-map-and-json-pretty-start`）。
**狀態：事前宣告，等使用者選第 4 節「地圖從哪來」。** 照新 AGENTS，不依賴地圖來源的段（第 8 節的 S0 量現狀、S1 格式 Module、S2 可見性）可以先動工；依賴的段（S3 起）等選定。

**核心一句**（每一項都拿來對照）：**大地圖是「同一張地圖變大」，不是新玩法。Engine 只補「只有 Engine 做得到」的機制（看不到的不畫、遠的換簡化、同一網格畫很多份）；格式是工具與遊戲之間的資料契約；地圖怎麼切、載哪些、場景放什麼，是作者的。**

**用詞**：標「**卡片判**」的是這張卡照命名規範、別家原始碼與已定原則自己判的，不是使用者的話。每一項都寫前提：**〔Engine〕**＝只有 Engine 做得到；**〔作者〕**＝作者（遊戲或工具）能處理。

## 0. 根據與讀的版本

- 工作樹 `gyo2-big-map-declaration-65f33f`，基底 `origin/gyo2` 895c1a08。
- master 的 big-map（`git show master:docs/architecture/plans/big-map/…`）：README、proposal-v0.1、survey、engine-grid、engine-format、engine-map、todo、HANDOFF「決定」與「product-arena-v3（核准版）」。共用筆記 `big-map-status-for-gyo2.zh-Hant.md`（PvP 卡 11-24 整理）。
- master 的量測證據（主 checkout `build/evidence/tool-v8-voxel-map/editor-next/scale/`，11-07）：合成圖 km2-building＝46,875 區塊、4,402 萬格、256 region、487 萬 quads、103 萬碰撞盒、區塊化地圖 242 MB、工具烘焙中位數 46.6 s、峰值 RSS 3.1 GB。**只當比較基準**（11-01 之後的數據，`gyo2-no-master-algorithms-data`）；這條線不碰權威與回滾的算法。
- gyo2 已定的決定（HANDOFF）：`-asset-large-map-like-bevy`（照 Bevy 不內建串流；asset 只給上限與 region 級 id；「後續如果需要，我們可以使用 Module 來增加地圖」）、`-build-world-phase-is-mechanism`、world-gen（`WorldGeneration`、`Hold()`）、`-replace-order-author-choice`（換大圖時可先丟舊的）、`-render-after-big-map`（視錐剔除、instancing、LOD 隨大地圖線）、`-game-flow-and-gateway`（「加載資源、讀檔 → 進入遊戲」）、`-pvp-playtest-2026-11-24` 3。
- gyo2 程式（本卡讀過）：
  - `games/object_fps_pvp/src/world/PvpWorld.cpp:732-813` `LoadGameContent`：程序啟動時同步讀 **全部** arena（`arenas.json` 三張），每張讀 v1 單檔、`BoxSet::Build` 一次；`:674`、`:915` 再各建一次。`PvpWorld.hpp:45` 的 `InstalledArena` 連 quads 一起留在 Match。
  - `client/VoxelModelLoader.hpp:45`：v1 文字上限 16 MiB。biome（63 m 見方）1.38 MiB → **v1 契約本身在約 210 m 見方就到頂**（面積 ×11.6）。共用筆記的「約 200 m 見方以內可行」與這個上限重合，效能那一面沒量過。
  - `engine/modules/mesh_render/src/MeshRenderModule.cpp:45-95`：`ExtractMeshes3d` 把每個 `Mesh3d` 都抽出、`Queue3dPasses` 全部排進 pass，**沒有剔除**。`engine/core/math/.../Frustum.hpp` 有 `MakeFrustum`／`Intersects`，沒有消費端。render 沒有 instancing。`Vertex3D` 32 B（`RenderCommands.hpp:121-126`）。
  - `engine/modules/physics_3d`：`BoxSet` 是 BVH（`Shapes.cpp:24` 葉 4 個、`:48` 中位數切分），`FixInternalEdges` 的鄰接只在同一個 `BoxSet` 之內（`BoxSetData.hpp:30-37`）；`SpatialQuery.hpp` 依 EntityId 升冪逐一看每個 collider 實體（實體層沒有 broadphase）。
  - `engine/modules/json`：adapter 是 yyjson（master 的 450 ms／5.3 MB 是 nlohmann 的數字，不適用）。
  - `engine/orchestration/.../WorldGeneration.hpp`（`Hold()`、`WhileGenerating`／`AfterGenerating`）；`engine/modules/world_asset/.../AssetView.hpp:66-131`（`Ready()` 回 `loaded`、`failed`、`ids`，可算進度）。
- 別家原始碼：Bevy `6a911653f`（本機）、Godot `94fc09c`（本機）、Goxel（本機，GPL-3.0，只讀設計）；bevy_voxel_world `main`（0.19.3，網頁讀源碼，行號是估計）、minecraft.wiki 與 Epic 文件（網頁）。第 3 節逐項列。

## 1. master 的 big-map 取什麼、不取什麼

| master 的內容 | gyo2 | 理由 |
|---|---|---|
| 目標約 1 km²、cell 0.5 m、4 人房、高建築密度（`-density`） | **取**（規劃尺度） | 使用者的目標，不是算法 |
| `gyo.voxel_map` v1＋`gyo.voxel_region` v1（區塊 16³、region 8×8 柱×全高、索引綁每個 region 的 sha256、JSON 唯一編碼） | **取格式，wire 一字不改**（卡片判） | 格式是資料契約，不是權威算法；一字不改讓 master 的工具（第 4 節 (a)、(d1)）與已烘好的資料直接可用。規則逐條在 S1 重審，要改就升版本 |
| Engine 擁有格式（`GYO::VoxelMesh`） | **改**：可選的 Module（第 2.1 節） | gyo2 的 Module／Plugin 分法與 `-asset-large-map-like-bevy` |
| `GYO::MapMarkers`（Engine 的標記層） | **現在不取** | 標記屬場景；gyo2 沒有第二個讀者（場景編輯器未開），PvP 的 arena 檔已帶出生點。RC-BM4 |
| `StaticAabbGrid`（Match 的 broadphase） | **不取** | gyo2 的 `BoxSet` 自帶 BVH，同一角色 |
| Match 整張載入、沒有「已載入集合」（`-sim-model`） | **取**（第 2.3 節） | 4 人房、權威簡單；量到才加分區（RC-BM2） |
| Client 每 region 一個 mesh＋視錐／距離剔除；LOD 量到才做（`-lod`） | **取方向，機制照 Bevy 重劃**（第 2.4 節） | 剔除與距離是 Engine 機制；LOD 代理是工具烘的資產 |
| 地圖＝幾何、場景＝放在幾何上的東西（`-map-scene-split`） | **取** | 第 2.5 節 |
| 視錐數學進 Engine（`-frustum-in-math`） | gyo2 已有 `Frustum.hpp` | — |
| 量測等真的大地圖做出來（`-measure-after-real-map`） | **改**：先量現狀（S0），km² 等地圖來源（S5） | 使用者這次要先知道現狀撐到多大 |

## 2. 照 gyo2 的分層重劃

### 2.1 分塊格式：資料契約 〔作者〕＋一份共用的讀者

- **前提**：讀一張地圖檔，遊戲自己做得到（PvP 現在就自己讀 v1，PV8）。所以不是「只有 Engine 做得到」。
- 但這份格式有**兩端以上的讀者**：工具（寫完自查）、PvP、之後的遊戲。master 定 `marker_kinds` 時的理由同樣適用：兩個讀者共用一個驗證者，規則才不會分岔。
- **放法**（卡片判，依使用者 11-21「後續如果需要，我們可以使用 Module 來增加地圖」）：新 Module **`voxel_map`**（`engine/modules/voxel_map/`），作者選用，Engine 核心不認得它。名字是概念（voxel 地圖），不是產品。
  - 內容：`ParseVoxelMap`、`ParseVoxelRegion`、`ValidateVoxelChunk`、`ExpandVoxelChunk`（世界座標的頂點、碰撞盒）、`ExpandVoxelPalette`、座標換算；兩個 asset loader（索引、region），照 image Module 的 `PngLoader` 那樣由 Product 註冊。
  - 只讀不寫：烘焙、greedy、編輯留在工具（`engine-not-tool-duties`）。工具持有寫的知識，用「產出要通過 `voxel_map` 的驗證」當對齊測試。
  - 依賴：Base、Math、json、asset。不依賴 render、physics_3d、Kernel（展開出的是純資料，遊戲自己放進 `Mesh3d`／`Collider`）。
- region 是一個 asset id（asset 宣告 F8、決定 8）。region 的平行解析交給 asset 的 worker pool，不在 Module 裡另開執行緒。
- 單檔 `gyo.voxel_mesh` v1：PvP 的讀者（`data/VoxelMeshFile.cpp`）在三張地圖遷移完時退役（S4），不搬進 Module。

### 2.2 讀取器放哪一層

| 部件 | 層 | 前提 |
|---|---|---|
| 文字 → 文件、驗證、展開 | Module `voxel_map` | 〔作者〕能做，但多讀者共用一份（2.1） |
| 讀檔、排程、worker | asset／world_asset（已有） | 〔Engine〕已有機制，不另做 |
| 讀哪張地圖、讀哪些 region、什麼時候算好 | 遊戲 | 〔作者〕 |
| region → `Mesh3d` 實體、→ `Collider` | 遊戲（先）；第二個遊戲出現再看要不要進 Module | 〔作者〕；bevy_voxel_world 把這層做成生態 crate，但它是程序生成＋相機串流，和我們的整張讀不同 |

### 2.3 Match 的載入與碰撞

- **前提**：碰撞查詢已是 Engine 機制（physics_3d）；整張載入還是分區，是〔作者〕的選擇。
- **`BoxSet` 撐不撐得住 km²**（推算，未量）：103 萬盒 × 盒 24 B＋節點（約 N/2 × 36 B）＋順序 4 B＋面鄰接（faceStart 6N × 4 B＋鄰接）≈ 100 MB；查詢深度約 log₂(10⁶/4) ≈ 18 層。上限 2³²−1。**記憶體與查詢看起來撐得住，建構時間沒量**（中位數切分每層排序，約 O(N log² N)）。S0 量。
- **要不要分區：建議不分，整張一個 `BoxSet`**（卡片判）。理由：
  - `FixInternalEdges` 的鄰接只在同一個 `BoxSet` 內。每個 region 一個 `BoxSet` 的話，每 64 m 的 region 邊界上會出現「內部邊」，角色貼地滑過接縫會被卡（Godot GridMap 每個 octant 一個 static body，接縫正是這個問題）。
  - `SpatialQuery` 在實體層沒有 broadphase；256 個 region 實體每次查詢都要逐一看，一個實體反而簡單。
- **改的是載入路徑**（S4）：只讀這一局的地圖（不再啟動時讀全部）；在生成世界階段讀，region 全部 Loaded 才建 `BoxSet`（在 worker 建，建好前 `Hold()`）；Match 不留 quads（只要碰撞盒）；同一份 `BoxSet` 不重建三次。
- **RC-BM2**（分區或睡眠）：S5 量到 Match 的載入時間、記憶體或 tick 超過 S5 宣告的值 → 先查是哪一項，再選分區（要先替 physics_3d 寫跨集合的鄰接）或遠處睡眠（master `-sim-model` 的形狀：睡眠是權威元件）。

### 2.4 Client：分區網格、視錐剔除、instancing、LOD

| 項目 | 層 | 前提 | 照誰 |
|---|---|---|---|
| 每個 region 一個 `Mesh3d` 實體 | 遊戲（用 `voxel_map` 的展開） | 〔作者〕 | master `-lod`、Godot octant、bevy_voxel_world 每 chunk 一個實體 |
| **視錐剔除** | Engine：bounds 元件＋每個相機的剔除清單，排隊時跳過（第 13 節） | 〔Engine〕作者伸不進 `ExtractMeshes3d` | Bevy `bevy_camera/src/visibility/mod.rs`：`Aabb`（`calculate_bounds` :557）、`ViewVisibility` :226、`VisibleEntities` :344、`NoFrustumCulling` :318、`check_visibility_cpu_culling` :748 |
| **距離剔除**＝最簡單的 LOD | Engine：`VisibilityRange` | 〔Engine〕同上 | Bevy `visibility/range.rs:80`、`check_visibility_ranges` :230；Godot `visibility_range_begin`（`visual_instance_3d.h:128`） |
| **LOD 代理網格**（遠處 region 的簡化網格） | 工具烘（另一個 asset，不改 region 格式）＋Engine 的 `VisibilityRange` 切換 | 產生網格〔作者〕（工具）；切換〔Engine〕 | Unreal HLOD（編輯器／commandlet 離線建，未載入的格子用 HLOD 代替）；master `-lod` 的 RC |
| **instancing** | Engine：render＋`sdl_gpu` 的 instanced draw | 〔Engine〕 | Bevy 的自動 batching；Godot GridMap 每種 item 一個 MultiMesh（`grid_map.cpp:946`） |

- **instancing 對 voxel 幾何沒有幫助**：每個 region 的網格都不一樣（greedy 後更是）。它幫的是場景裡重複的東西（箱子、樹、路燈）。所以排在剔除之後，測試場景用重複的模型，不用地圖。
- **Client 的記憶體是 km² 的第一個風險**（推算，未量）：km2-building 487 萬 quads × 4 頂點 × 32 B ≈ 623 MB 頂點＋117 MB 索引；`ModelData` 在 CPU 端還有一份同大小。合計可能 1.5 GB 以上。對策候選（S5 量到才選）：上傳後放掉 CPU 端的頂點、voxel 專用的壓縮頂點（要遊戲自己的 shader，屬渲染管線線）、LOD 代理、背景串流（RC-BM3）。
- 剔除的放法：判斷照 Bevy 在**來源 World** 做（`CheckVisibility`），結果是每個相機一份清單，`Queue3d` 排隊時跳過清單裡的。抽取仍照抽（gyo2 的抽取每幀重建、多相機共用一份）。取捨與膠水見第 13.3 節。

### 2.5 地圖和場景分開

- **前提**：〔作者〕。地圖＝幾何（`voxel_map` 目錄）；場景＝「用哪張地圖、放了什麼、規則」，是遊戲的檔。
- PvP：arena 檔升版本（v3），`geometry` 換成地圖目錄；出生點等仍在 arena 檔；`width`／`depth` 從地圖的 bounds 導出（事實只在一處）。名字照 PvP 自己的詞，不在 Engine 立「scene」概念。
- 地圖的身分：索引文字的 sha256（索引已綁每個 region 的 sha）；Match 與 Client 對照沿用現有的 `MatchInfo{arenaId, wallsDigest}`（`wallsDigest` 由 `BoxSet` 的內容導出），不改 wire。
- Unreal 的 Data Layers 是同一個想法（同一個世界上疊不同組的 actor）；我們用「場景檔指向地圖」做到，不做圖層機制。

## 3. 別家對照

| 項目 | Bevy（主參考）＋生態 | Minecraft | Godot | Unreal World Partition | gyo2 的選擇 |
|---|---|---|---|---|---|
| 分塊 | 核心沒有；bevy_voxel_world `CHUNK_SIZE` 32³、`ChunkKey{IVec3, lod}`（chunk.rs，估 :20-60） | chunk 16×16 柱、section 16³；region 檔 32×32 chunk（`r.x.z.mca`） | GridMap octant 預設 8³（`grid_map.h:176`） | 2D runtime grid，格長例 256 m | 區塊 16³、region 8×8 柱（master 的格式，和 MC 的 section／region 同型） |
| 載入 | 生態：相機距離＋視錐決定生成／回收（`ChunkSpawnStrategy::CloseAndInView`） | server 依 ticket 按需載入；client 從 server 收 chunk | 整個節點 | 依 streaming source（玩家）距離載入格子 | **整張載入**（4 人、權威簡單）；串流是 RC-BM3 |
| 檔案 | 生態沒有地圖格式（存「編輯差分＋種子」） | region 檔：4 KiB sector、位置表、Zlib | 場景資源 | One File Per Actor（編輯器端） | JSON 索引＋region 檔（契約）；二進位快取是 RC-BM1 |
| 碰撞 | 生態不提供（範例用 avian） | 每 chunk | 每 octant 一個 static body | 跟著 actor | 整張一個 `BoxSet`（2.3） |
| 剔除 | 核心 `check_visibility`（Aabb、Frustum） | 不畫空 section（wiki）；其他未從讀到的頁確認 | 渲染伺服器的實例剔除 | — | 照 Bevy |
| LOD | 核心 `VisibilityRange`；生態 chunk LOD（降取樣再放大） | — | `visibility_range` | HLOD（離線建、代替未載入格子） | `VisibilityRange`＋工具烘的代理 |
| instancing | 自動 batching | — | MultiMesh | HLOD 的 Instancing 層 | 照 Bevy 的自動合批；偏離處見第 16.3 節的表（S6） |

**偏離 Bevy 的地方與膠水**：
1. **整張載入、不照 bevy_voxel_world 的相機串流**：衝突點是權威——Match 要整張碰撞，Client 的「看不到的先不載」和 Match 的狀態無關，但 region 何時進碰撞要決定性（asset RC-AS1）。不串流就沒有這個膠水。這不是偏離 Bevy 核心（核心沒有串流），是不採用一個生態 crate 的策略。
2. **格式自己定（master 的）**：Bevy 生態沒有檔案格式可照；接合點是 Bevy 式的 asset loader（我們的 asset Module 已照 Bevy），膠水只有兩個 loader。
3. **Match 碰撞整張一個 `BoxSet`**：avian 有實體層 broadphase、我們沒有（physics_3d 的取捨，3D 物理卡已定）。一張地圖一個實體正好繞開這個差異，不寫膠水。
4. 剔除、`VisibilityRange` 照 Bevy，沒有偏離。instancing 照 Bevy 的自動合批，偏離處（實例資料的放法、bin 的順序、沒有 `NoAutomaticBatching` 等）見第 16.3 節的表。

## 4. 地圖從哪來（交使用者選）

**先用遊戲的例子**：想做一張 1 km² 的工廠區，四個人在裡面打。地圖要「有人畫出來」，畫好再「烘」成遊戲讀的區塊檔。問題是：畫在哪裡、誰負責烘？

| 選項 | 畫在哪 | 烘 | 能多大 | 代價 | 對大地圖線的影響 |
|---|---|---|---|---|---|
| **(a)** 等 master 的 `voxel_map_editor` 整個移植 | GYO 自己的編輯器（照 Goxel 的操作） | 編輯器內建 | km²（master 的 tool-chunked 已支援區塊存檔） | 排在工具線後面（ImGui 接法→編輯器，master 原始碼約 286 KB，含 1,135 行的視窗） | S3～S5 全部等工具線；這段期間只能用合成圖 |
| **(b)** 轉換器：現有資料 → 新格式 | 不畫，轉現有的 | 轉換器 | 現有三張（最大 63 m 見方） | 小（估 4～6 h）。biome 有原始格子（證據夾 `biome.voxel_grid.json`）；pvp_arena、pvp_corners 只有 v1 成品，要從 v1 切成區塊 | 只能遷移舊圖、驗證格式管線，**給不出大地圖** |
| **(c)** Goxel 畫、匯出再轉 | Goxel | 轉換器＋烘焙 | Goxel 本身無界；`.vox` 匯出每軸 < 255 格（`vox.c:589-591` 的 assert）＝127 m；`.txt` 匯出無界（一格一行 `X Y Z RRGGBB`，`txt.c:94-101`） | 只有顏色，沒有 slab／stair 等形狀與方塊名；km² 的 `.txt` 是數千萬行；Goxel 編輯 4,000 萬格順不順沒驗過 | 使用者現在就能開始畫；仍要一個烘焙器（見 (d1)） |
| **(d1)** 先只移植 master 編輯器的**無視窗烘焙命令**（`build-map`、`import-boxes`），不帶視窗 | 任何能產出 `gyo.voxel_grid` 的來源 | 移植的 CLI | km²（master 的烘焙器已處理過 km2-building） | 估 8～12 h（master `Build.cpp`＋`MapBuild.cpp`＋`GridDocument.cpp`＋`BoxImport.cpp` 約 1.15k 行，改接 gyo2 的 `voxel_map` Module 與 json Module） | 大地圖線不再等工具線；之後的編輯器移植直接重用它 |
| **(d2)** 程式生成（合成城市） | 不畫 | 生成器＋(d1) | 任意 | master 有 `Synthetic.cpp`（約 100 行）可參考 | 給量測用的 km² 圖；**不是能玩的地圖** |

**建議：(d1)＋(c)＋(d2)，(b) 併進 (d1)。**
- (d1) 是所有路都要的「烘焙」那一段，先做就不被工具線卡住；pvp_arena、pvp_corners 用 (d1) 的 `import-boxes` 或一個 v1→格子的小轉換器遷移，biome 用它的原始格子重烘（＝(b)）。
- (c) 讓你現在就能在 Goxel 畫真的地圖：加一個 Goxel `.txt`（無界）／`.vox`（≤127 m）→ `gyo.voxel_grid` 的匯入命令（master 用過一次性腳本 `vox_to_grid.py`）。形狀（slab、stair）Goxel 表達不了，用顏色對照表指定（例：某個顏色＝stair），對照表是匯入命令的參數。
- (d2) 給 S5 的 km² 量測，在真的大地圖畫好之前就能量。
- (a) 照原排程在後面，接上 (d1) 的烘焙核心。
- 選 (a) 的代價：大地圖線在 S2 之後停下等工具線。

### 4.1 已定（`v8-arch-gyo2-big-map-source`，2026-11-25）

使用者原話：「照建議，(d1)＋(c)＋(d2)」。(b) 併進 (d1)：biome 用原始格子重烘，另外兩張用 `import-boxes` 遷移。(a) 照原排程，放在後面的 tools 移植。

**使用者的三張 Goxel 圖**（`~/Code/GitHome/Map/`，使用者：「你可以當成大地圖的測試」）。下表由卡片自寫的分析腳本照 `.gox` 的格式說明讀出（只讀 GitHome 的路徑，沒有複製進 repo）：

| 檔（大小） | 格子（x×y×高，Goxel 的 z 朝上） | 0.5 m 換算 | 可見格數 | 顏色 | 圖層 |
|---|---|---|---|---|---|
| `Kiriyu_Town_Graybox.gox`（0.2 MB） | 200×200×40 | 100×100 m，高 20 m | 653,642 | 88 | 11 |
| `Kiriyu_Incident.gox`（1.0 MB） | 500×500×64 | 250×250 m，高 32 m | 3,247,851 | 32 | 27 |
| `Kiriyu_Extraction_250m.gox`（1.0 MB） | 500×500×64 | 250×250 m，高 32 m | 3,241,046 | 29 | 21 |

- **127 m 的限制有影響**：兩張 250 m 的圖每軸 500 格，Goxel 的 `.vox` 匯出會擋下（每軸要小於 255 格）。`.txt` 匯出沒有上限，但一行一格（約 324 萬行），而且圖層會被合併。
- **圖層裡有場景的東西**：除了幾何以外，還有「Spawns and exits」「M1～M6」（怪物佔位，「Boss hands only - exclude from count」）、「REF | Player 2x2x4」「Story 01～06」等。照「地圖＝幾何、場景＝放在幾何上的東西」（2.5），這些不該烘進碰撞。
- **卡片判：直接讀 `.gox`**，不經 `.vox`／`.txt`。理由有三：250 m 的圖過不了 `.vox`；圖層名要留著，才能把幾何和場景標記分開；檔案只有 1 MB。
  - 格式照 `.gox` 的格式說明自己寫，不抄 Goxel 的碼（GPL-3.0）。
  - 匯入命令的參數：
    - 哪些圖層當幾何（排除圖層名的樣式）。
    - 顏色 → 方塊的對照（Goxel 表達不了 slab／stair）。
    - 座標換軸（Goxel 的 z 朝上、右手系 → gyo 的 y 朝上、左手系：y、z 互換，形狀不鏡像）。
  - 場景圖層的取出（轉成場景的標記）屬場景線，現在只排除，不轉。
- **fixture**（`v8-arch-gyo2-big-map-gox-fixture`，使用者「照建議 (ii)」）：只把 `Kiriyu_Town_Graybox.gox`（0.2 MB）放進 repo，當 CI 的 fixture。兩張 250 m 的圖留在 GitHome，只在本機與 Linux 量測時用。
- **烘好的 arena**（`.gox` 原始檔都不進 repo，只放 `import-gox`＋`build-map` 的輸出）：
  - 2026-11-26 夜，使用者「只放烘好的 Extraction」：`kiriyu_extraction_250m`。
  - 同日改成三張都放，使用者「Town_Graybox 和 Incident 也做成 arena」：加 `kiriyu_town_graybox`（100 m）與 `kiriyu_incident_250m`。
  - Incident 的 16 個 region 裡有 15 個和 Extraction 逐位元組相同，git 共用同一個 blob，repo 實際只多一個 region。
- **和量測的關係**：250 m 見方、324 萬格是 S0「現狀能撐到多大」與 S5 之間的真實資料點（km2-building 是 4,402 萬格）。

## 5. 量測

### 5.1 S0：先量現狀能撐到多大（**輕量，不需要同意**）

- **地圖**：把 biome（真資料，63 m）的 v1 平鋪 1×1、2×2、3×3（3×3＝189 m、約 12.5 MB，在 v1 的 16 MiB 之內；4×4 超過契約上限，現狀讀不了）。
- **Match（無視窗、只用 CPU）**：載入時間（讀檔、解析、`BoxSet::Build`）、峰值 RSS、一個 tick 的移動與射擊查詢時間（中位數、p95）。另外把碰撞盒放大到 10⁴～10⁶（平鋪、與 master km2-building 同量級的 103 萬盒）只量 `BoxSet::Build` 與查詢，回答「`BoxSet` 撐不撐得住 km²」。預估峰值 RSS < 2 GB。
- **性質**：探索與描述，不設門檻、不判過與不過；方法在看結果前寫進 S0 的紀錄，看了不改。件數照實記。
- 證據放主 checkout `build/evidence/gyo2-big-map/s0/`。

### 5.1.1 S0 的結果（2026-11-25，Linux 主機）

- 機器：AMD Ryzen 7 6800H（16 執行緒）、30 GiB、Ubuntu 24.04.5、g++ 13.3，preset `core`（RelWithDebInfo），量測程式 `-O2`。原始碼 `220378f9`。證據在主 checkout 的 `build/evidence/gyo2-big-map/s0/`：腳本 `s0_linux.sh`、`probe.cpp`、`tile_v1.py`，輸出在 `linux/`。
- 方法照 5.1。biome 的平鋪間距是 1,824 單位＝57 m（照碰撞盒與面的最大座標取整）。查詢在範圍內的亂數點各做 2,000 次：
  - 向下的射線（找地面）、水平一槍的射線。
  - 膠囊（半徑 0.3、半高 0.6）的小位移 cast 與接觸查詢。
- 件數：每項 asked 2000、done 2000。

| 地圖 | 文字 | quads | 碰撞盒 | 解析 | `BoxSet::Build` | 展開網格（CPU） | 峰值 RSS |
|---|---|---|---|---|---|---|---|
| biome 1×1（57 m） | 1.0 MB | 38,214 | 1,260 | 12.7 ms | 1.6 ms | 4.6 ms | 17 MB |
| 2×2（114 m） | 4.1 MB | 152,856 | 5,040 | 50.8 ms | 6.6 ms | 21.9 ms | 57 MB |
| 3×3（171 m） | 9.3 MB | 343,926 | 11,340 | 111 ms | 17.8 ms | 50.8 ms | 101 MB |

| 只放碰撞盒（biome 的盒平鋪） | 碰撞盒 | 範圍 | `BoxSet::Build` | 峰值 RSS | 射線（槍）p95 | 膠囊 cast p95 | 接觸 p95 |
|---|---|---|---|---|---|---|---|
| 4×4 | 20,160 | 204 m | 31 ms | 16 MB | 2.6 µs | 1.1 µs | 0.8 µs |
| 8×8 | 80,640 | 408 m | 141 ms | 27 MB | 3.5 µs | 1.4 µs | 0.8 µs |
| 16×16 | 322,560 | 816 m | 637 ms | 74 MB | 4.7 µs | 1.7 µs | 0.8 µs |
| 29×29 | 1,059,660 | 1,479 m | 2,385 ms | 244 MB | 7.6 µs | 2.1 µs | 0.9 µs |

**讀法**（描述，不是判定）：

- **Match 的碰撞撐得住 km²**。103 萬盒（master km2-building 的量級）時，每次查詢的 p95 都在 8 µs 以內。一個 tick 4 個玩家做幾十次查詢，算出來不到 1 ms。2.3 節「整張一個 `BoxSet`」不必改。
- **要處理的是建構時間**。103 萬盒要 2.4 s，而 PvP 現在每張地圖建三次（`PvpWorld.cpp:674、:778、:915`），載入時就是 7 s 以上。S4 要改成只建一次，並放在生成世界階段的 worker 上。
- **解析隨文字線性成長**，約 12 ms/MB（單執行緒，yyjson 之下）。照這個推，km2-building 的 242 MB 約 3 s；分成 region 由 asset 的 worker 平行解析，可以再縮短。RC-BM1（二進位快取）目前沒有壓力。
- **現狀的上限是契約，不是效能**。v1 單檔的 16 MiB 大約對應 biome 密度下 200 m 見方。在那以內，解析、建構、記憶體都很小。
- **沒量的**：
  - Client 的 GPU 與幀率，要同意才跑（5.2）。
  - 高建築密度的面數。biome 是地形密度，km2-building 有 487 萬 quads；Client 的記憶體推算見 2.4。
  - Mac。

### 5.2 要同意才跑的（**本機重 GPU 或大記憶體**）

| 量測 | 段 | 影響（先講） |
|---|---|---|
| Client 幀率：biome 平鋪 1×1～3×3，開視窗、最壞角度 | S0 的 Client 部分 | GPU 滿載數分鐘；3×3 約 35 萬 quads，預估不會卡機，但 11-07 曾在 km² 預覽卡死，所以先問 |
| km² 合成圖（(d2)）的 Match 載入／記憶體、Client 幀率 p95 | S5 | 預估 Client 1.5 GB 以上 RAM＋數百 MB VRAM；工具烘焙約 47 s、峰值 3 GB（master 的數字）。會先從 1/4 km² 往上，每一級看 RSS 再決定下一級 |

S0 的 Client 部分與 S5 開跑前，我會先送「量什麼、多大、預估影響、怎麼中止」給主會話轉使用者，同意後才跑。

### 5.2.1 250 m 圖的 Client 畫面時間（2026-11-26，Linux 主機，使用者同意後）

- **同意**：使用者原話「登入了，可以量」，經主會話轉達。範圍照 5.2 送出的計畫：4 個出生點加中央，每處 8 個方向，每方向 300 幀。
- **開跑前檢查**：karasu 讀得到 `/dev/dri/renderD128`，`vulkaninfo` 看得到 `AMD Radeon Graphics (RADV REMBRANDT)`。
- **機器**：AMD Ryzen 7 6800H、Radeon 680M（RADV），Ubuntu 24.04.5 的 Wayland 桌面，g++ 13.3，RelWithDebInfo。
- **證據**：主 checkout 的 `build/evidence/gyo2-big-map/gpu/`。
  - `s1_linux_gpu.sh`（`build`／`measure` 兩段，只寫 `~/Code/gyo2-big-map-s1`）、`probe/`（量測程式，不進 repo）。
  - 輸出在 `linux/frames.csv`、`probe.txt`。
- **量測程式**：照 PvP Client 的畫法，每個 region 一個 `Mesh3d`（持有的模型＋`Aabb`），經 `visibility` 剔除，由 `mesh_render` 與 `sdl_gpu`（Vulkan）畫。
  - 視窗 1280×720，相機用 PvP 的投影（垂直視角 1.2 rad、near 0.05、far 150 m），眼睛在地面上 1.6 m。
  - VSync 關閉，實際的 present mode 是 Mailbox。
  - 量的是每幀 `Pump`＋`RunFrame` 的牆鐘時間。SDL GPU 有 frames-in-flight 上限，所以這個時間是出幀間隔，由 CPU 與 GPU 中較慢的一方決定；兩者各自的時間（GPU timestamp）沒有量。
- **地圖**：`kiriyu_extraction_250m`，16 region、130,273 quads、33,022 盒。
- **件數**：40 個方向 × 300 幀＝12,000 幀量測（加上暖機，共 13,260 幀）。13,260 幀都交給了視窗（skipped 0、failed 0），被拒的貼圖、網格請求都是 0。

| 量 | 值 |
|---|---|
| 40 個方向的中位數，再取中位數 | 0.18 ms |
| 最差方向的 p95（p2 朝 225°、畫 13 個 region、剔除 3 個） | 0.47 ms |
| 所有方向的最大單幀 | 4.6 ms（出現在 p0 開頭的三個方向；原因未確認） |
| 每個方向畫的 region | 1～13 個（16 個裡）；看向地圖中央時畫得最多 |
| 程序峰值 RSS | 194 MB |

**讀法**（描述，不是判定）：

- 250 m 這個量級，Client 的畫面時間遠在 16.7 ms（60 fps）之內。目前的著色器沒有光照，是最簡單的那一種；光照、陰影等加進來時（渲染管線線），要重量。
- 剔除有作用：一個方向大多只畫 2～9 個 region。
- 第一輪（`linux-run1-unverified/`）沒有記錄「實際交給視窗的幀數」，所以不採用那一輪。補上 `FramesShown`／`FramesSkipped` 重跑後，兩輪各方向的差：中位數最多 0.011 ms、p95 最多 0.108 ms、最大單幀最多 0.807 ms。
- **沒量**：km²（S5，要用 (d2) 的合成圖）、GPU timestamp、Windows 與 Mac 的 GPU、有角色和 HUD 的實際遊戲畫面、Client 端的地圖載入時間、1280×720 以外的解析度。

### 5.2.2 S5：km² 合成城市（2026-11-26，Linux 主機）

- **開始**：使用者「linux啟動了，s5可以開始了」（主會話轉達）。量之前**沒有事先寫下的數字門檻**（第 15 節以前只寫了「超過 S5 宣告值」，值沒有寫）。使用者看過數據後判定：「S7 往後排,記憶體要處理」；之後要做載入時間的改善（平行解析，不做 RC-BM1 的二進位快取）。
- **門檻**（2026-11-26，使用者「對s5門檻，2個都要」，經主會話轉達）。下面的判定門檻是主會話**看過數據之後**才提的，使用者同意採用：
  - 判定門檻：Client 幀 p95 ≤ 8 ms、Client 記憶體 ≤ 2 GB、載入時間 ≤ 10 s（Match、Client 各自）、Match 記憶體 ≤ 1 GB。
  - 回歸基準（之後動到大地圖的批次用同一套工具重量，不能比這差）：1 km 的 Client 1.79 GB、幀 p95 1.27 ms、Match 讀圖＋建牆 7.9 s／471 MB、Client 讀圖 4.8 s（probe 單執行緒的讀、解析與展開）。
    - 2026-11-27 更新 Match 的基準（17.8，預設的執行緒數）：讀圖＋建牆 3,686 ms、峰值 RSS 602 MB（`/usr/bin/time`，精確；兩次取較差的一次）。
  - 照這組門檻，S5 的 1 km 全部在門檻內。
- **機器與原始碼**：同 5.2.1（Ryzen 7 6800H、Radeon 680M／RADV、30 GiB），gyo2 `41504a7f`（含 S4-C、S6）。證據：主 checkout `build/evidence/gyo2-big-map/s5/`（`s5_linux.sh`、`probe/`、`content.py`、`watch.py`；輸出在 `linux/`）。
- **地圖**：(d2) 的 km2-building（master 的合成城市：同一棟 128 m 見方的建築平鋪），三級 1008、1504、2000 格（504 m、752 m、1 km；格數要是 16 的倍數）。
  - 合成格子是 1 m 一格。km² 的格子檔約 230 MB，超過格子檔 16 MiB 的上限，不能改格子再 `build-map`；所以烘好後把索引的 `cell_size` 改成 0.5。256 格時兩種做法的 region 檔逐位元組相同，索引只差 `source_sha256`（`s5/cell_equivalence.txt`）。
  - 1 km：256 region、4,872,510 quads、1,032,504 碰撞盒、242 MB 文字。
- **Match**：真的 Match 程式（`--arena`，生成期間讀圖），另用 probe 照 `LoadArena` 的步驟分段計時。查詢各 2,000 次（done 2000／asked 2000）。

| 大小 | Match 讀圖＋建牆 | 解析（單執行緒） | `BoxSet::Build` | Match 峰值 RSS | 查詢 p95（射線、膠囊） |
|---|---|---|---|---|---|
| 504 m | 1.9 s | 1.0 s | 0.63 s | 161 MB | ≤ 4.1 µs |
| 752 m | 4.3 s | 2.3 s | 1.47 s | 314 MB | ≤ 4.9 µs |
| 1 km | 7.9 s | 4.0 s | 2.71 s | 471 MB | ≤ 5.1 µs |

- **Client**：5.2.1 的 GPU probe（每 region 一個 `Mesh3d`、visibility 剔除、mesh_render＋sdl_gpu），5 處 × 8 方向 × 300 幀（12,000／12,000 幀），VSync 關。
  - 主機沒有接螢幕（所有輸出埠 disconnected），改用 SDL 的 offscreen（`VK_EXT_headless_surface`），只用 radeon 的 ICD。同一個設定重量 5.2.1 的 250 m 圖：中位數 0.20 ms（5.2.1 開視窗 0.19）、最壞 p95 0.44 ms（0.47），一致；最大單幀 0.90 ms（開視窗 4.60 ms，有合成器）。

| 大小 | 中位數 | 最壞 p95 | 最大 | 畫的 region（共有） | 讀圖＋展開（單執行緒） | probe 峰值 RSS |
|---|---|---|---|---|---|---|
| 504 m | 0.77 ms | 1.28 ms | 1.60 ms | 8～21（64） | 1.2 s | 503 MB |
| 752 m | 0.83 ms | 1.28 ms | 1.31 ms | 13～21（144） | 2.9 s | 1.04 GB |
| 1 km | 0.89 ms | 1.27 ms | 1.34 ms | 14～22（256） | 4.8 s | 1.79 GB |

**讀法**（描述）：

- 畫面時間不隨地圖變大：一個方向只畫 14～22 個 region（視錐＋遠裁面 150 m）。
- Client 的記憶體隨面數成長。1 km 的 1.79 GB（峰值）**推算**的去處：作者的 `ModelData` 約 740 MB（每頂點 32 B＋索引）、sdl_gpu 的 transfer buffer 約 740 MB（一幀上傳全部，之後不縮回）、probe 自己留著的 region 文件與碰撞盒約 0.18 GB。前兩項是「上傳 GPU 後還留在 CPU 端的頂點」。這是從面數算的，不是量的；17.7 量到的穩定值和它對不上（見那裡）。
- 載入時間：Match 7.9 s 裡解析佔 4.0 s（Match 讀檔只用一條執行緒）。
- **沒量**：GTT（內顯的顯存只有 512 MB，其餘在系統記憶體；`mem_info_vram_used` 峰值 486 MB）、GPU timestamp、真正的 Client 程式（沒有角色與 HUD）、Windows 與 Mac。

**處置**（使用者的判定）：

- S7（LOD 代理網格）**延後**，不是取消：畫面時間沒有壓力。
- 記憶體：先做宣告列的對策裡成本最低的「上傳 GPU 後放掉 CPU 端的頂點」（第 17 節）。
- 載入時間：多條執行緒平行解析與展開（第 17.8 節）；RC-BM1 不做。

## 6. 和 loading 的關係（流程框架卡 `local_8a21ecb9`）

- 單人「開始遊戲 → 加載資源、讀檔 → 進入遊戲」、多人「開始遊戲 → 加載…（邊連 Match 邊載）」的「加載」，大地圖接在**生成世界階段**（`WorldGeneration`）：
  - 遊戲在生成期間發出索引與全部 region 的 asset 請求；地圖的系統在「region 全部 Loaded 且碰撞／網格建好」前呼叫 `Hold()`。
  - **進度**＝`AssetView::Ready(region handles)` 的 `loaded`／`ids.size()`（`AssetView.hpp:66-131`），再加「驗證」「建碰撞」「建網格」三步。大地圖給的是數字，畫面與進度條是流程框架卡的。
  - **合法**：asset 的 Loaded 只代表 region 過了第 1 步。整張地圖合不合法，由 `CheckVoxelMap(map, regions)` 一次回答（R3 的 sha、bounds、palette，設計審查 S-2）。它的錯誤和 asset 的 failures 一樣交給流程。
  - **就緒條件**：Match＝碰撞建好；Client＝網格實體已生成（碰撞在 Client 預測也要，同一份）。失敗（缺 region、sha 不合）→ `failures` 交給流程，怎麼顯示由流程決定（異常只告訴發生了什麼）。
  - 換地圖時兩張大圖不能同時在記憶體：用 `-replace-order-author-choice` 的「先丟舊的」。
- 要和流程框架卡對齊的只有介面形狀（進度怎麼取、就緒怎麼判、失敗怎麼交）。本宣告交出後我直接傳訊息給那張卡。

## 7. 不做

- 串流（Client 背景串流、Match 活動範圍）、二進位快取、Match 遠處睡眠、LOD 代理網格：各有 RC，量到才做。
- 地圖編輯器的視窗（工具線）、場景編輯器、`MapMarkers`、水方塊、光照／AO（渲染管線線）。
- 權威與回滾的任何改動；net wire 的改動。
- 改 `gyo.voxel_map`／`gyo.voxel_region` 的 wire（要改就升版本，另寫宣告）。

## 8. 分段計畫

```text
v8-arch-gyo2-big-map
├── S0 量現狀           Opus／medium｜探索｜不依賴來源｜估 4～6 h      biome 平鋪＋BoxSet 放大；Client 部分要同意
├── S1 格式 Module      Opus／high（上段：資料契約、新 Module）｜不依賴來源｜估 8～12 h   voxel_map：解析、驗證、展開、兩個 loader
├── S2 可見性機制       Opus／high（上段：Engine 公開介面）｜不依賴來源｜估 8～12 h   bounds、視錐剔除、VisibilityRange、NoFrustumCulling
├── S3 地圖工具         Opus／high｜依賴第 4 節｜估 8～14 h（選 (d1)＋(c)＋(d2) 時）  烘焙 CLI、Goxel 匯入、合成圖
├── S4 PvP 接分塊地圖   Opus／high（上段：資料契約、Match 判定）｜依賴 S1、S3（S2 給剔除）｜估 14～20 h
│                       arena v3、只讀這局的地圖、生成階段載入＋進度、整張一個 BoxSet、每 region 一個 Mesh3d、三張遷移、v1 讀者退役
├── S5 量 km²           Opus／medium｜依賴 S3 (d2)、S4｜估 4～6 h｜要同意      結果決定 RC-BM1～BM3、頂點壓縮
├── S6 instancing       Opus／high（上段：Engine 公開介面）｜估 8～12 h        測試場景用重複模型
└── S7 LOD 代理網格     延後（S5 的判定：畫面時間沒有壓力，5.2.2）
```

| 段 | 實作者 | 審查者（每批的設計審查） | 驗證者 | 段別根據 |
|---|---|---|---|---|
| S0、S5 | Opus／medium | — | 同實作者 | 下段：只量、不改程式 |
| S1 | Opus／high | Opus／high | Opus | 上段：資料契約、新 Module |
| S2、S6 | Opus／high | Opus／high | Opus | 上段：Engine 公開介面 |
| S3 | Opus／high | Opus／high | Opus | 下段：工具（不改契約） |
| S4 | Opus／high | Opus／high | Opus | 上段：資料契約、Match 判定 |

- 先後：S0、S1、S2 可以並行（S2 碰 render／mesh_render／camera；視窗模式卡碰 window／`sdl_window`／`sdl_gpu`，碰到同檔再協調）。S6 可在 S2 之後任何時候。
- 每段開工前寫該段的小宣告（判定、測試、突變），照批次表〈每批的設計審查〉在合併前核准前審查；純淨 Code Review 照主會話排。
- PR：S0～S2 各自一個 PR 或併成一條線，照 `fewer-prs` 由主會話定；本宣告先以 draft PR 交出。

## 9. 測試與突變清單（候選；各段小宣告時定稿）

**S1 `voxel_map`**
- 測試：M1～M10、R1～R5、C1～C5 每條一個反例＋一個正例；世界座標公式（±2²⁰ 邊界精確）；`ValidateVoxelChunk` 與 Parse 走同一條規則；master `tests/common/voxel_mesh/VoxelMapTests.cpp` 裡的文字向量（格式的資料，不是算法）原樣通過；兩個 loader 在 asset 上讀到同一份結果；依賴守衛（不依賴 render、physics_3d、Kernel）。
- 突變：`vmap-schema-name`、`vmap-version`、`vmap-region-order`（`at` 嚴格遞增）、`vmap-region-file-charset`、`vregion-sha-skip`、`vregion-at-not-in-index`、`vchunk-outside-region`、`vchunk-local-range`（[0, 256]）、`vchunk-empty-accepted`（C1）、`vmap-int-range`（±2³⁰）、`vmap-text-cap`（1 MiB／16 MiB）、`vexpand-world-formula`、`vexpand-palette-uv`。

**S2 可見性**
- 測試：完全在視錐外的不抽、跨平面的保留（保守）、`NoFrustumCulling` 永遠保留、沒有 bounds 的保留、兩個相機各算各的、`VisibilityRange` 邊界（含／不含）、RenderLayers 不受影響、剔除不改任何來源 World 的狀態雜湊。
- 突變：`vis-cull-off`（全部保留時測試要失敗）、`vis-plane-sign`、`vis-no-cull-ignored`、`vis-range-inclusive`、`vis-camera-mixup`。

**S4 PvP**
- 測試：只讀這局的地圖；region 未全到時 `Hold()`、全到後一幀內結束生成；進度單調不減；缺 region／sha 不合→載入失敗且帶原因；Match 與 Client 的 `wallsDigest` 相同；遷移後三張地圖的碰撞盒集合與 v1 等價（遷移前後的移動 golden 不變）；Match 不留 quads。
- 突變：`bm-hold-skip`、`bm-load-all-arenas`、`bm-digest-from-index-only`、`bm-region-missing-accepted`。

**S0、S5**：只量，沒有突變。

不加擋誤用的守衛（`no-guards-against-misuse`）；守衛只守寫明的契約（依賴方向、格式規則）。

## 10. Replacement Criteria（候選）

- **RC-BM1** 二進位快取：S5 量到 Client 或 Match 的解析時間超過 S5 宣告值（yyjson 之下）→ 快取（RC，不是契約；契約仍只有 JSON）。
- **RC-BM2** Match 分區或睡眠：見 2.3。
- **RC-BM3** Client 背景串流：S5 量到 Client 的載入時間或記憶體超過宣告值，且頂點壓縮、放掉 CPU 端頂點、LOD 代理都不夠 → 串流（region 進碰撞的決定性要先寫，asset RC-AS1）。
- **RC-BM4** `MapMarkers` 類的通用標記層：出現第二個讀者（場景編輯器或第二個遊戲）。
- **RC-BM5** region → 實體的那一層進 `voxel_map` Module：第二個遊戲也用分塊地圖。

## 11. 交使用者的事

1. **第 4 節地圖從哪來**：建議 (d1)＋(c)＋(d2)，(b) 併進 (d1)，(a) 照原排程。
2. 確認（卡片判、你可推翻）：格式放可選 Module `voxel_map`（2.1，依你 11-21「用 Module 來增加地圖」）；Match 整張一個 `BoxSet`（2.3）；`MapMarkers` 現在不做（RC-BM4）。
3. S0 的 Client 部分、S5：到時候先問（5.2）。

## 12. S1 小宣告：`voxel_map` Module（`v8-arch-gyo2-big-map-s1`）

2026-11-25 開工（主會話轉達：S1、S2 不依賴地圖來源，現在動工）。Opus／high。

### 12.1 範圍

- 新 Module `engine/modules/voxel_map`（`GYO::VoxelMap`，`library_only`，作者選用）。依賴：`GYO::Base`、`GYO::Math`、`GYO::Asset`（公開，loader 介面）、`GYO::Json`（私有）。
- 格式：`gyo.voxel_map` v1、`gyo.voxel_region` v1，規則 M1～M10、R1～R5、C1～C5 照 master `docs/architecture/voxel-mesh.md`（wire 一字不改）。契約頁在 gyo2 寫 `docs/architecture/target/v2/voxel-map.zh-Hant.md`。
- 不做：v1 單檔（PvP 自己的讀者照舊，S4 退役）、`MapMarkers`、烘焙、平行解析（asset 的 worker 負責）。

### 12.2 和 master 不同的兩處（卡片判）

1. **region 的驗證拆成兩步**：asset 的 loader 只拿得到位元組與 settings，拿不到索引（`LoadContext.hpp`）。
   - `ParseVoxelRegion(text)`：不需要索引的規則（R1、R2、R4 的結構、順序與「區塊屬於 `at`」、R5、C1 的非空、C2 的形狀、C3、C4、C5），並記下文字的 SHA-256。
   - `CheckVoxelRegion(region, map, expected)`：要對照索引的規則（R3 的 `at`＝`expected` 與 sha、R4／C1 的 bounds、C2 的 palette 範圍）。
   - `ValidateVoxelChunk(chunk, map)`＝兩步的區塊規則一起。三者呼叫同一組規則函式（master DI 3「一個驗證者」照守）。
2. **索引的 loader 宣告每個 region 為參照**（`ReferencePath`，依索引順序）：讀索引就會讀整張（整張載入，2.3）；就緒用 asset 的「連同依賴 Complete」。Product 的 catalog 要列出每個 region 檔（asset 決定 8 的「region 級 id」）。
3. 展開多給法線（由面的方向導出，不是新資料），輸出形狀直接對上 `Model::Mesh`（positions、normals、uvs、indices），並提供「附加到既有幾何」的版本，讓遊戲把一個 region 的區塊併成一個網格不必再複製。三角形照 gyo2 的正面（螢幕上逆時針，`RenderCommands.hpp:140`）；gyo2 是左手系，所以 (v1−v0)×(v2−v0) 背向面的法線（和 master 的順序相反）。

### 12.3 判定（合併前）

- J1：每條規則（M1～M10、R1～R5、C1～C5）各有一個「違反就被拒、路徑指到那個值」的測試，與一個通過的例子。
- J2：同一個違規，`ParseVoxelRegion`＋`CheckVoxelRegion` 與 `ValidateVoxelChunk` 給同一個規則結論（錯誤路徑前綴不同）。
- J3：世界座標在 ±2²⁰ 兩端逐位元等於公式 `float(local + 256 × key + 16 × origin) × cell_size / 16`；三角形朝外；uv 指到 palette 圖的格中心。
- J4：兩個 loader 經 asset 讀到與直接 Parse 相同的結果；索引的參照依索引順序、路徑相對於索引檔。
- J5：連結守衛：`gyo_voxel_map` 的連結恰為 Base、Math、Asset、Json（不連 render、physics_3d、Kernel）。
- J6：第 9 節 S1 的突變全部 killed（件數照實記）；建置與 `cpu` 標籤測試通過；L1 三平台。

### 12.4 測試與突變

- 測試：`tests/common/voxel_map/VoxelMapTests.cpp`（doctest，標籤 `cpu`）。
- 突變：`tests/common/voxel_map/mutations.json`，id 前綴 `vmap-`（索引）、`vreg-`（region）、`vchunk-`（區塊）、`vexp-`（展開）、`vload-`（loader）、`vlink-`（連結）。第 9 節的候選為準，加減照 `tests/common/mutation/README.md`，數量與理由寫 dev_log。

## 13. S2 小宣告：可見性（`v8-arch-gyo2-big-map-s2`）

2026-11-25 開工。Opus／high（上段：Engine 公開介面、新 Module）。

### 13.1 放哪裡（卡片判）

- **不放 camera**：camera 的 CM-1 寫明「不做可見性」。**不放 `world_render` 的 System**：render 頁第 97 行寫明它「不跑 System」。改這兩條都是 Architecture Delta。
- **照 Bevy 的分法**：Bevy 的元件（`Aabb`、`NoFrustumCulling`、`VisibilityRange`、`VisibleEntities`）是 bevy_camera 的 visibility，System `check_visibility_cpu_culling`（`bevy_camera/src/visibility/mod.rs:748`）在 PostUpdate、傳播與相機更新之後。gyo2：
  - **元件**是可畫物的設定，型別在 `render` 的 `Drawables.hpp`，由 `world_render` 登記（它的職責「登記可畫元件與相機的繪製設定」）。和其他可畫元件一樣不進雜湊（不是決定的狀態）。
  - **System** `CheckVisibility` 在新 Module **`visibility`**（來源側，依賴 render、transform、camera；`default=no`），掛在 frame／PostUpdate，排在 `TransformPropagate` 與 `CameraUpdate` 之後，公開 set `CheckVisibility`。
  - **抽取與排隊**：`camera_render` 把相機的 `CulledEntities` 抽進 `ExtractedCamera::culled`；`Queue3d` 跳過清單裡的實體。修訂 1（設計審查 S-3）把清單由正向（看得到的）改成反向（受管而看不到的）：「哪些實體受剔除管」只在 `visibility` 一處決定，`mesh_render` 不再標旗標。
- **沒帶 `visibility` 的 World**：相機沒有清單 → 全部照畫（和現在一樣）。PvP、Object_Connect 不必改。

### 13.2 規則

- **`Aabb`**（實體本地座標的盒）：有它的實體由 `CheckVisibility` 逐相機判斷。盒子經 `GlobalTransform` 的八個角換成世界 AABB（保守），用 `Math::Intersects(Frustum, Aabb)`；視錐是 `MakeFrustum(clipFromView × viewFromWorld)`。
- **`NoFrustumCulling`**：不做視錐判斷（仍受 `VisibilityRange` 管）。
- **`VisibilityRange`**（照 Bevy 的形狀：`startMargin`、`endMargin` 兩段、`useAabb`）：相機位置到實體位置（或盒心）的距離在 `[startMargin.start, endMargin.end)` 內才看得到（Bevy 的 `is_visible_at_all`）。漸變（crossfade）要 shader，屬渲染管線線；現在兩段 margin 重疊時兩個 LOD 都會畫，作者把 margin 設成零寬即是硬切換。
- **`CulledEntities`**（相機上的推導值，`CheckVisibility` 寫）：受剔除管（有 `Aabb` 或 `VisibilityRange`）而這個相機看不到的實體，EntityId 升冪。沒有 `Aabb` 也沒有 `VisibilityRange` 的實體永遠不在清單裡（Bevy：沒有 Aabb 的不被視錐剔除）。只在內容改變時寫（同 CM-3 的做法）；第一份清單用延遲指令插入，所以相機第一幀沒有清單、照畫全部。
- `RenderLayers` 照舊在 `Queue3d` 判斷，與可見性無關。
- **不做**：自動算 `Aabb`（Bevy 的 `calculate_bounds`；大地圖的 region 盒由 `ChunkWorldBounds` 精確給出，RC-VS1）、2D 的剔除、遮擋剔除、GPU 剔除、`ViewVisibility`（給玩法讀的「這一幀有沒有被看到」，RC-VS2）。

### 13.3 和 Bevy 的差別與膠水

| Bevy | gyo2 | 膠水 |
|---|---|---|
| `Aabb` 由 `calculate_bounds` 自動算 | 作者給 | 無；RC-VS1 |
| 正向的 `VisibleEntities`，按 `VisibilityClass` 分類、queue 每個 view 讀它 | 反向的 `CulledEntities`，只列受管而看不到的；renderer 只問「在不在清單裡」 | 無旗標；camera 與 renderer 都不必認得可畫物的種類。代價：Bevy 剛生成、還沒檢查的實體先藏一幀，這裡先畫一幀 |
| `ViewVisibility`、`InheritedVisibility`、`Visibility` 階層 | 不做 | RC-VS2 |
| 抽取前就剔除（只抽看得到的） | 每幀照抽、排隊時剔除 | gyo2 的抽取每幀重建、多相機共用一份；被剔除的蒙皮網格仍在 CPU 蒙皮（RC-VS3） |

### 13.4 判定（合併前）

- K1：視錐外的不排、跨平面的保留、`NoFrustumCulling` 不做視錐判斷、沒有 `Aabb` 的永遠排、兩個相機各算各的、`VisibilityRange` 的兩端（含下界、不含上界）、`useAabb`。
- K2：沒帶 `visibility` 的 World 照舊全部排（既有的 render 測試不變）。
- K3：`CulledEntities` 只在內容改變時寫（`Changed` 不每幀亮）。
- K4：連結：`gyo_visibility` 的連結恰為 Base、Kernel、Orchestration、Math 與 render、transform、camera。
- K5：突變全部 killed；既有 render、camera、transform 的測試通過；L1 三平台。

### 13.5 RC

- **RC-VS1**：要畫很多個不知道盒子的網格（角色、道具）且要剔除 → 加自動 bounds（照 Bevy 的 `calculate_bounds`）。
- **RC-VS2**：玩法要讀「這一幀有沒有被看到」（動畫降頻、AI 降頻）→ 加 `ViewVisibility`。
- **RC-VS3**：量到被剔除的項目在抽取的成本（CPU 蒙皮）超過預算 → 把剔除移到抽取前。

## 14. S1／S2 的設計審查（2026-11-25）

審查者 Opus／high，只拿成果物與原始要求。報告在主 checkout 的 `build/evidence/gyo2-big-map/s1-s2/design-review.md`：B 0、S 6、N 7。

| 項 | 內容 | 處置 |
|---|---|---|
| (a) | 捲繞方向與 render 的正面、`math.md` 的座標系 | 一致（推導，另有 `sdl_gpu` 的 Pass3D cull 測試支撐；GPU 測試這次沒跑） |
| (b) | region 兩步是否仍是一個驗證者 | 是；缺的是整張的出口 → S-2 |
| S-1 | `AppendVoxelChunk` 的 assert 每次重跑整套規則（擋誤用、還有成本） | 採用：只留展開依賴的前提（bounds、方向、palette 索引、32 位元索引上限） |
| S-2 | asset 的 Loaded 不等於合法，沒有「整張合法」的出口 | 採用：`CheckVoxelMap(map, regions)`，第 6 節的就緒改用它 |
| S-3 | 「受剔除管」的判斷在 visibility 與 mesh_render 各寫一次 | 採用 (i)：反向清單 `CulledEntities`，拿掉 `visibilityChecked` |
| S-4 | clip-from-world 在 visibility、Pipeline2d、Pipeline3d 各算一次（Bevy 在 `update_frusta` 一處） | 不在本批：其中兩處是既有程式；收斂要改 camera 的推導值（CM 條文）。記為後續 |
| S-5 | 第 2.4 節與第 13 節矛盾 | 採用：2.4 改寫 |
| S-6 | render 頁沒更新、visibility 沒有 DI；`Mesh3d` 只吃 `Handle<ModelData>`，S4 的「每 region 一個 `Mesh3d`」作者做不到 | 採用：render 頁補第 8.6 節（VS-1～VS-3）。`Mesh3d` 的缺口記進 S4：候選是照 `ImageRef` 的先例讓 `Mesh3d` 也接受作者持有的 `shared_ptr<const ModelData>`（Engine 改動，S4 宣告時定） |
| N-1 | palette 的負值在第 1 步拒、越界在第 2 步拒 | 不改：負值是「表示不了」，越界是「要對照索引」，分法本身就是 12.2 的兩步 |
| N-2 | 每個相機重查所有候選 | 採用：候選在相機迴圈外建一次 |
| N-3 | `Visibility.hpp` 為了測試公開了純規則 | 不改：純規則公開讓作者也能用（例如工具的預覽） |
| N-4 | `Render::Aabb` 與 `Math::Aabb` 同名 | 不改：照 Bevy 的名字，namespace 分開 |
| N-5 | 兩個 loader 共用 settings 型別名 | 採用：分成 `VoxelMapLoaderSettings`、`VoxelRegionLoaderSettings` |
| N-6 | 第 9 節的雜湊測試候選沒進判定 | 不改：元件照可畫元件不進雜湊，沒有可測的行為 |
| N-7 | region 清單在索引與 catalog 各一份 | 採用：契約頁寫明 catalog 由烘焙工具一起產生（(d1)） |

## 15. S4 小宣告：PvP 接分塊地圖（`v8-arch-gyo2-big-map-s4`）

2026-11-25。使用者「S4 同步做」（`v8-arch-gyo2-big-map-s4-parallel`）。Opus／high（上段：資料契約、Match 判定）。Engine 的缺口（`Mesh3d` 接作者持有的模型）另成 PR #348，S4 疊在 #342＋#348 上。

### 15.1 現況（程式）

- `LoadGameContent`（`PvpWorld.cpp:732`）在建 World 之前，同步讀**全部** arena 的 v1 單檔，`GameContent` 由所有 World 共用。Client 在 `ApplySnapshot` 尾段跟著 Match 的 `MatchInfo{arenaId, wallsDigest}` 換競技場（#338）。
- 同一份碰撞盒的 `BoxSet` 建三次（`:674`、`:778`、`:915`）。S0：103 萬盒建一次要 2.4 s。
- Client：`ShowArena` 為整張地圖做一個 `Mesh3d`（`ArenaModelId`、`VoxelModelLoader`），palette 圖由 `PaletteOf` 給。

### 15.2 改成（卡片判）

1. **arena 檔 v3**：`geometry`（v1 單檔）換成 `map`，也就是地圖索引在 catalog 裡的 id。出生點、角色參數留在 arena 檔（場景）。`arenas.json` 照舊，只列 arena 檔。
   - **`width`／`depth` 留著**（修訂：第一版寫成刪掉，是錯的）。它們不只是範圍，也定義「y＝0、[0, width]×[0, depth] 的地板」這條遊戲規則（`Arena.hpp`）；地圖的 bounds 以 16 格的區塊為單位，對不上 20×20 這種尺寸。要不要拿掉隱藏地板是 PvP 的移動規則（master `-gravity` 那一類），不在這批。
2. **地圖不在建 World 之前讀**：`GameContent` 只留 arena 檔、規則、角色。地圖由**這個 World 要玩的那一張**在**生成世界階段**讀：
   - 送出索引的請求；索引宣告的 region 參照由 asset 一起讀（VM-6）。
   - region 沒全到之前 `Hold()`。
   - 全部到齊後 `CheckVoxelMap`。
   - 用 `ExpandVoxelChunk` 的碰撞盒建**一次** `BoxSet`。建構在 worker 上做，建好前 `Hold()`。
   - Match 不留 quads。
3. **Client 的網格**：每個 region 用 `AppendVoxelChunk` 合成一個 `ModelData`，經 #348 的 `ModelRef` 持有，交給一個 `Mesh3d`；`Aabb` 是 region 的世界盒；palette 圖是持有的 `ImageData`（`ImageRef`）。Client World 帶 `visibility` Module。
4. **身分**：沿用 `MatchInfo{arenaId, wallsDigest}`，`wallsDigest` 由那一次建好的 `BoxSet` 導出。wire 不變。
5. **遷移與退役**：
   - 三張地圖用 (d1) 的 `build-map`／`import-boxes` 轉成分塊格式，catalog 的 region 條目由工具一起產生（契約頁 2.4）。
   - 遷移完畢後刪掉 `data/VoxelMeshFile.*`、`client/VoxelModelLoader.*`。
   - `Town_Graybox`（fixture）當第四張測試地圖。

### 15.2.1 分兩階段（卡片判，2026-11-25）

- **B（本批）**：照 15.2 的 1、3、4、5 做，讀的時點不變，仍在建 World 之前同步讀全部 arena。
  - 碰撞盒在 `LoadGameContent` 建**一次**；`BoxSet` 的本體是共用、不可變的，所有 World 共用同一份。三次建構的問題（S0）就此消失。
  - Client 每個 region 一個 `Mesh3d`（持有的模型＋`Aabb`），region 實體由 Frame World 的配發器生成，換圖時移除。
  - 小地圖（三張遷移的圖）在這個時點讀沒有問題。
- **C（等 15.3 對齊）**：改成「這個 World 要玩的那一張、在生成世界階段讀」（15.2 的 2）。250 m 以上的圖要靠這一步。

### 15.3 要和流程框架卡對齊的一點

「Client 跟著 Match 換競技場」（#338）在大地圖下行不通：換一張要重新讀幾百 MB，不能在 `ApplySnapshot` 裡同步做。卡片的方向：
- 要玩哪張圖，在**建 World 時**就決定。來源是流程框架的房間或選圖，World 的 options 帶 arena id。
- Match 說的 arena 和自己的不同時，照現有路徑記 `ContentMismatch`，不在同一個 World 裡換。
- 要換圖就換 World（`-replace-order-author-choice` 的「先丟舊的」）。

這會改 #338 寫的那一段，已告知 PvP 卡；選圖的接點等流程框架卡回覆。

### 15.4 判定（合併前）

- L1：三張遷移的地圖，**被碰撞盒佔住的格子**遷移前後相同。盒子的切法會變，因為工具會在區塊內合併。現有的 PvP 測試不改、照舊要通過；如果有 golden 因為盒子的切法而變，停下來回報，不改 golden。`Town_Graybox` 是新加的地圖，沒有「遷移前」。
- L2：生成期間 region 沒到齊不結束生成；到齊後一幀內結束；進度單調不減。
- L3：缺 region、sha 不合 → 載入失敗並帶原因，不 assert。
- L4：`BoxSet` 每個 World 只建一次。Match 與 Client 的 `wallsDigest` 相同。
- L5：Client 每個 region 一個 `Mesh3d`，帶 `Aabb`；相機背後的 region 不排進 pass（端到端）。
- L6：突變全部 killed，L1 三平台。

### 15.5 不做

串流、LOD 代理、GPU 量測（量之前先問）、場景標記的轉換（`.gox` 的場景圖層照舊排除）、Seats 的上限。

### 15.6 S4-C：建 World 時才讀地圖（`v8-arch-gyo2-big-map-s4c`）

2026-11-26。使用者：「大地圖卡接著做 S4-C 和 S6」（主會話轉達）。Opus／high（上段：資料契約、Match 判定）。15.3 的選圖接點由流程框架卡做好（#346～#353）；名字與欄位動工前已通知流程框架卡。

**前提**

| 前提 | 誰定的 | 掛在它上面的決定 |
|---|---|---|
| 一個 World 只玩一張圖，換圖就換 World | 15.3（卡片判，`-replace-order-author-choice`） | Client 不在 World 裡跟著 Match 換圖 |
| 載入放在生成世界階段，沒好就 `Hold()` | world-gen（O-14）；object_connect 的 `LoadGameData` 已用同一形狀 | 遊戲的步在生成結束前不跑 |
| 讀圖走 asset（索引帶 region 參照，VM-6） | 第 6 節、S1 | 同一個 Product 的 World 共用一次讀取 |

**改成（卡片判）**

1. `GameContent` 只留 arena 檔、規則、角色。`InstalledArena`、`ContentRef`、`PresentationContent::arenaPictures` 拿掉。
2. arena 檔 v3 的 `map` 是 catalog 裡索引的 id（15.2 第 1 點原本就這樣寫；B 階段暫用目錄名）。
3. 每個玩這張圖的 World（Match、Client 的 Sim／Solo）在生成期間由 `LoadArena` 讀：
   - 向 asset 要索引，region 全到後 `CheckVoxelMap`。
   - 只把碰撞盒複製給 worker 建 `BoxSet`，同時放掉 handle（Match 不留 quads）。
   - 建好後檢查出生點，放進 `kArenaEntity` 的 `Collider`，寫 `GameRules::wallsDigest`；權威端再寫 `MatchInfo{arenaId, wallsDigest}`。
   - 完成的那一幀不 `Hold()`，生成在那一幀結束；其他幀都 `Hold()`。
   - 讀不了（缺 region、sha 不合、出生點在牆裡）：維持生成，`MapLoad{done, total, stage, failure}` 寫原因。Match 的 main 記錄後結束；Client 帶原因回上一頁。
4. Client 的 Frame World 用同一個 asset（store 只讀一次），在生成期間把 faces 交給 worker 做每個 region 的模型，再生成每個 region 一個 `Mesh3d`。loading 畫面等兩邊的 `MapLoad` 都是 Done；region 數計入進度。
5. 固定步在生成結束前不跑（`WorldTimeOptions::fixedStepConditions = {AfterGenerating()}`），Match 與 Client 條件相同。生成期間來的連線，`SeatPlayers` 先不讀（未讀的紀錄不會被裁掉），牆好了才入座。
6. Match 玩的圖和自己的不同：記 `ContentMismatch{reason, matchArena}`，不在 World 裡換。arena id 不同時一收到 `MatchInfo` 就判（不必先讀完自己的圖）；同一張圖則等自己的牆建好再比 digest。Client 有那張圖就以那張重建遊戲，沒有就離開。只要 Match 的圖和這局的不同就會重建；Gateway 的房間照理不會遇到（房間的圖就是 Match 的圖），程式沒有另外限制。
7. worker 由產品給（`WorldOptions::jobs`、`ClientOptions::jobs`）；沒給時在 System 內同步建。產品給的是**計算用**的執行緒，和 asset 讀檔的執行緒分開（Bevy 的 `IoTaskPool`／`AsyncComputeTaskPool`：`bevy_tasks/src/usages.rs:67`、`:75`）。介面借用 `AssetExecutor`（只是「交一個 job」），不經 asset store。
8. 生成期間累積的連線紀錄，牆好後才讀：讀到時已經斷掉的連線不入座（設計審查 B1：否則會留下沒人控制的玩家）。

**照誰**

- 背景建、每幀取結果：Bevy `examples/async_tasks/async_compute.rs:66`（`AsyncComputeTaskPool` 上 spawn）、`:128`（`check_ready` 每幀取）。Godot `WorkerThreadPool::add_task`／`is_task_completed`（`core/object/worker_thread_pool.h:246`、`:249`）同型。丟掉 task 時取消還沒開始的 job（Bevy 的 `Task` drop 即取消：`bevy_tasks/src/task_pool.rs:558`）；已在跑的跑完後丟掉結果（Bevy 會在下一個 await 點停，我們的 job 沒有 await 點）。
- 生成期間的連線改用「牆好後讀積壓的訊息」，而不是 Bevy 常見的「照狀態入座」（每個還在、沒有玩家的 Peer 給一個）：`SeatPlayers` 原本就以訊息處理換手（同一使用者的新連線接手玩家），改成照狀態要重寫整個 System，不在本批。積壓造成的問題（已斷的連線）在讀時排除。
- 生成期間讀＋`Hold()`：gyo2 world-gen，沒有別家對應（Bevy 用 States 自己寫 loading，同樣是作者的系統等資產）。

**判定（合併前）**

- 15.4 的 L2：region 沒到齊、牆沒建好都維持生成；建好的那一幀結束；進度不倒退；生成期間步數為 0。
- 15.4 的 L3：缺 region、sha 不合、出生點在牆裡 → `MapLoad` 為 Failed 並帶原因，World 維持生成，不 assert。
- 15.4 的 L4：Match 與 Client 各自讀圖，`wallsDigest` 相同；Match 牆建好後不再持有地圖。
- 生成期間連進來的 Client：牆好之前沒有玩家，之後入座。
- Client：Match 玩另一張已安裝的圖時，以那張重建遊戲並畫出那張圖。
- 新加與改到的突變全部 killed；L1 三平台。

**不做**

串流、km² 量測（S5，要先問）、Gateway 與房間的流程（不變）。

## 16. S6 小宣告：instancing（`v8-arch-gyo2-big-map-s6`）

2026-11-26。主會話轉達使用者：「S6：instancing — 照宣告寫的做：做成 Engine 的公開介面，用測試場景裡重複出現的模型驗證。照 Bevy 的做法，偏離 Bevy 時寫出理由。」Opus／high（上段：Engine 公開介面）。第 2.4 節：instancing 對 voxel 幾何沒有幫助，幫的是重複的東西（箱子、樹、路燈），所以驗證用重複的模型，不用地圖。

讀的版本：gyo2 `425d192c`；Bevy `6a911653f`；Godot `94fc09c`；SDL3 是建置抓下來的 `SDL_gpu.h`。

### 16.1 現況（程式，S6 前）

- **排隊**：`Queue3d`（`engine/modules/render/src/Pipeline3d.cpp:117-175`）每個抽出的項目一個 `Draw3D`；`Draw3D` 自己帶 `world`、`color`（`RenderCommands.hpp:145-162`）。
- **後端**：`sdl_gpu` 的 `RecordPass3D`（`plugins/sdl_gpu/src/adapters/sdl/GPUDevice.cpp:669-770`）每個 draw 推一次 vertex uniform（`clipFromWorld × world`，`:756-757`）和一次 fragment uniform（顏色，`:758-759`），再畫 1 個實例（`:766`）。
- **抽取已經共用網格**：`ExtractMeshes3d`（`engine/modules/mesh_render/src/MeshRenderModule.cpp:46-99`）經 `RenderModelMeshes::Use`（`:88-91`，RD-9）讓同一個模型網格的實體拿到同一個 `MeshId`。缺的只是「把它們併成一個 draw」。
- render 頁第 8.5 節已預告「網格實例不留 GPU 狀態（每個 draw 推 uniform；大量相同網格時 RC-RD 2 的實例化）」。

### 16.2 前提：誰要解決

| 項 | 前提 | 理由 |
|---|---|---|
| 判斷哪些 draw 可以併、把每個實例的資料交給 GPU | **〔Engine〕** | 作者伸不進 `Queue3d` 和後端；只有 renderer 知道哪些 draw 共用網格、貼圖與 pipeline |
| 讓東西重複 | **〔作者〕** | 作者給同一個 `ModelRef`＋`ImageRef` 就是重複；不需要新的元件 |
| 一個實體帶很多實例（Godot `MultiMesh`） | **〔作者〕（現在）** | 每個實例一個實體作者就做得到；Bevy 核心也沒有這種元件。RC-IN3 |

### 16.3 設計（卡片判）

**公開介面**是 `render` 的資料契約與 shader ABI（後端與測試讀它）；作者那一側**不加任何東西**，合批是自動的（照 Bevy）。

1. **資料**（`RenderCommands.hpp`）：
   - 新 `Instance3D{world, color}`（68 bytes）＝Bevy 的 `MeshUniform`（`bevy_pbr/src/render/mesh.rs:532-535` 的 `world_from_local`，加上 bindless 材質槽那一份）。
   - `Pass3D::instances`；`Draw3D::instanceOffset`、`instanceCount`。
   - `Draw3D::world`、`color` **拿掉**：只留一條路，畫一個東西＝畫一個實例（Bevy 每個 phase item 也都有 `batch_range`，單一項目是長度 1）。
2. **shader ABI**（`ShaderAbi.hpp`、`Mesh3D.hlsli`）：
   - 頂點 slot 0 照舊（每頂點）；新的 slot 1 每實例：location 2～5 是 world（`Matrix4` 照存放順序的四欄，HLSL 的 `float4x4` 把它們當列）、6 是顏色。
   - vertex uniform 改成 pass 的 `clipFromWorld`（每個 pass 推一次）；fragment uniform 拿掉（顏色跟著實例走）。
   - 位置＝`mul(mul(p, world), clipFromWorld)`，同 Bevy 的 `get_world_from_local(instance_index)`（`mesh_functions.wesl:15`）再乘 view 的 clip_from_world。
3. **自動合批**（`Queue3d`，〔Engine〕）：
   - **鍵**：保留網格（`MeshId`）、貼圖（`TextureId`，含 filter）、混合、深度測試、深度寫入、cull。照 Bevy：網格是 `BatchCompareData = AssetId<Mesh>`（`mesh.rs:2910`），貼圖是材質 bind group（`:2892-2899`），其餘是 pipeline id（`bevy_render/src/batching/mod.rs:41-52`）。
   - **不透明**（RD-8 的第 0 組）：同鍵的進一個 bin，照 Bevy 的 binned phase（`no_gpu_preprocessing.rs:122-154`，一個 bin 一段 `instance_range`）。
   - 這也會重排 `depthWrite = false` 的不透明項目（它們的先後影響畫面），和 Bevy 一樣。
   - **半透明、不測深度**：排序後**相鄰**且同鍵的才併，照 Bevy 的 sorted phase（`batching/mod.rs:236-243`）。遠到近不變；GPU 照實例順序畫，混合的先後也不變。
   - **自己帶頂點的項目**（CPU 蒙皮）不併。Godot 也一樣：有 `mesh_instance` 的不 repeat（`render_forward_clustered.cpp:896`）。
   - 被剔除的（`CulledEntities`）、層不符的，照舊在合批前去掉（第 13 節不變）。
4. **後端**（`sdl_gpu`，〔Engine〕）：
   - 每幀的實例放進一個 instance buffer，和 pass 的頂點一樣每幀重建（Bevy 的 CPU 路徑也每幀清：`no_gpu_preprocessing.rs:64-72`）。
   - slot 1 每個 pass 只綁一次（在這個 pass 的實例起點）；每個 draw 用 `first_instance`＝`instanceOffset` 指出自己的範圍，同 Bevy 的 `draw_indexed(.., batch_range)`（`mesh.rs:4734-4738`）。
   - instance-rate 的頂點輸入在三個後端都從 `first_instance` 開始：Vulkan 的 `firstInstance`、D3D12 的 `StartInstanceLocation`、SDL Metal 的 `baseInstance:firstInstance`（`SDL_gpu_metal.m:2862`）。SDL 的註（`SDL_gpu.h:3502-3509`）只說 shader 讀內建的實例編號（`SV_InstanceID`）時不一致，這裡的 shader 不讀它。
   - 第一版把 slot 1 綁在每個 draw 的第一個實例、`first_instance` 恆 0，理由寫成「`first_instance` 不一致」。這個理由錯了，而且每個 draw 重綁在 SDL Vulkan 上會重綁整組頂點緩衝（`SDL_gpu_vulkan.c:8014-8026`）。設計審查指出後改掉。
   - `SDL_DrawGPUIndexedPrimitives` 帶 `instanceCount`。

**偏離 Bevy 的地方、理由與膠水**：

| Bevy | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| 實例資料放 storage buffer，shader 用 `instance_index` 取（`mesh.wesl:43`；`draw_indexed(..., batch_range)`，`mesh.rs:4734-4738`） | 每實例的頂點輸入（instance-rate），draw 用 `first_instance` 指出範圍 | SDL_GPU 的頂點輸入在三個 API 一樣、照 `first_instance` 開始；shader 不讀內建的實例編號（只有它在各 API 不一致）；shader 工具不必多查 storage buffer | 無（ABI 多一組輸入） |
| 不透明的 bin 照鍵排（`sort_binned_render_phase`，`batching/mod.rs:199-209`） | bin 照「最近的成員」排，bin 內近到遠 | 留住 RD-8 的近到遠；沒有任何東西可併時，輸出和 S6 前一樣（既有的 `[p3-*]` 不改） | 無 |
| `NoAutomaticBatching`（`batching/mod.rs:24-26`） | 不做 | Bevy 用它是因為自訂 draw function 需要每實體的資料；gyo2 沒有自訂 shader，合批在畫面上看不出來，沒有使用者的開關測不了 | 無；RC-IN1 |
| GPU preprocessing、multi-draw indirect（`gpu_preprocessing.rs`） | 不做；只有 CPU 建實例（Bevy 自己的 `no_gpu_preprocessing` 路徑） | 剔除在 CPU（第 13 節）；沒量到 draw 數或實例上傳是瓶頸 | 無；RC-IN2 |
| 顏色在材質 | 顏色在實例 | gyo2 的材質就是 `Mesh3d` 的 `color`＋`image`（render 頁 8.3）；顏色放實例，只差顏色的也能併（＝Bevy bindless 的材質槽放在 `MeshUniform`） | 無 |

**別家對照**：Godot 兩條都有。自動的是：不透明照鍵排序（`render_forward_clustered.cpp:1999-2001`），**相鄰**同鍵的合成一個 instanced draw（`:896-910`、`:608`、`:623`）。手動的是 `MultiMesh`（`scene/resources/multimesh.h:36`、`:107`、`:112`，GridMap 每種 item 一個，`grid_map.cpp:946`）。gyo2 照 Bevy 只做自動的，手動的留給 RC-IN3。

**clip-from-world（第 14 節 S-4）：不在本批做。**
- S6 沒有碰相機的推導值。`Pass3D::clipFromWorld` 原本就是每個相機每幀在 `Queue3d` 算一次（`Pipeline3d.cpp:126`）；S6 只是把每個 draw 的 `clipFromWorld × world`（S6 前 `GPUDevice.cpp:756`）移進 shader。
- 收斂 S-4 要讓 `ComputedCamera`（現在只有 `clipFromView`，`Camera.hpp:122`）多帶 clip-from-world 或 frustum，也就是改 camera 的 CM 條文。這不是 instancing 自然帶出來的，照第 14 節，仍記為後續。

### 16.4 判定（合併前）

- **I1**：N 個同模型同圖的實體＝一個 draw，`instanceCount` N；每個實例帶自己實體的 world 與顏色。驗證有兩處：`Queue3d` 單元測試；測試場景（20 個箱子＋3 個換圖的＋1 盞燈＋5 個在相機背後的）。實體再加 5 個，draw 數不變。
- **I2**：網格、貼圖、混合、深度測試、深度寫入、cull 任一不同，就不併。
- **I3**：被剔除的、層不符的不是實例；自己帶頂點的（蒙皮）各自一個 draw。
- **I4**：
  - 半透明、不測深度的只併相鄰同鍵的，遠到近不變。
  - 不透明的 bin 在最近成員的位置，bin 內近到遠。
  - 沒有東西可併時，draw 的順序和 S6 前一樣（既有 `[p3-*]` 不改）。
- **I5**（GPU，`sdl_gpu.pass3d`）：
  - 一個 draw 三個實例畫在三個位置、各自的顏色；第二個 draw 從它自己的第一個實例開始。
  - 混合的實例照順序疊。
  - 既有的像素測試照過。
- **I6**：第 16.5 節的突變全部 killed；`cpu|shader` 通過；L1 三平台（Linux 的 `sdl_gpu.pass3d` 在 Lavapipe 上跑）。

**既有測試的改動**（只改量法，守的性質不變）：
- `[vs-queue]`／`[vs-queue-no-list]`（`VisibilityTests.cpp`）原本數 draw、代表「畫了幾個項目」，現在同網格的會併成一個 draw，改成數實例。
- `sdl_gpu` 的 3D 像素測試：`Draw3D` 的 `world`／`color` 改由實例給（資料契約變了）。
- `[g3-abi-fragment-uniforms]`：fragment uniform 由 1 改成 0。

### 16.5 測試與突變

- 測試：
  - `tests/common/render/Pipeline3dTests.cpp`（`render.pipeline3d`，標籤 `cpu`）三案。
  - `PipelineWorldTests.cpp`（`render.extract`）的測試場景一案。
  - `plugins/sdl_gpu/tests/Pass3DTests.cpp`（`sdl_gpu.pass3d`，標籤 `gpu`）一案。
- 突變：`tests/common/render/instancing_mutations.json`，15 個。
  - 合批：`in-batch-off`。
  - 鍵：`in-key-mesh`、`-texture`、`-cull`、`-depth-write`、`-blend`、`-depth-test`。
  - 順序：`in-blend-binned`、`in-bin-far-first`。
  - 蒙皮：`in-own-joined`。
  - 實例資料：`in-instance-world-lost`、`in-instance-colour-lost`。
  - GPU：`in-g3-one-instance`、`in-g3-instance-offset-ignored`、`in-g3-instance-base-not-advanced`。
- `render_mutations.json` 裡 find 已失效的 7 個（`g3-abi-frag-no-texture`、`g3-abi-frag-no-colour`、`g3-transform-world-ignored`、`g3-transform-order`、`g3-colour-ignored`、`g3-mesh-index-offset-ignored`、`g3-vertex-offset-ignored`）改寫到新程式，守同一個性質。

### 16.6 RC

- **RC-IN1**：出現自訂 shader 或 draw function，需要每個實體自己的 uniform → 加 `NoAutomaticBatching`（照 Bevy）。
- **RC-IN2**：量到 draw 數或每幀的實例上傳成為幀時間的主要部分 → 實例改成保留式（以來源實體為鍵，Bevy 的 `RenderMeshInstances`），或 GPU 剔除＋indirect。
- **RC-IN3**：量到「每個實例一個實體」本身的成本（抽取、剔除）超過預算（例：上萬株草）→ 加一個實體帶多個實例的元件（Godot `MultiMesh` 的形狀）。

### 16.7 不做

LOD、串流、GPU 效能量測（本機的重 GPU 量測要先問）、2D 的 instancing（2D 已有「相鄰同貼圖同混合」的合批，RD-7）、遮擋剔除。

## 17. S5 後的記憶體：上傳 GPU 後放掉 CPU 端的頂點（`v8-arch-gyo2-big-map-memory`）

2026-11-26。使用者對 S5 的判定「記憶體要處理」（5.2.2），照宣告列的對策先做成本最低的一項。Opus／high（上段：render 的公開介面）。

### 17.1 現況（程式）

上傳 GPU 之後，CPU 端還留兩份頂點，各約 740 MB（1 km）：

- 作者的 `ModelData`：`Mesh3d::model` 的持有形式（`Drawables.hpp:107` 的 `shared_ptr<const ModelData>`）一直留著；`RenderModelMeshes` 的 holder 也留一份參照（`Pipeline3d.hpp`，為了讓位址當鍵）。
- sdl_gpu 的 transfer buffer：一個、每幀循環，長到最大一幀的上傳量後不再縮（`GPUDevice.cpp` 的 `EnsureTransfer`）。PvP 一幀上傳 256 個 region，它就留在 740 MB。
- 另有建立那一幀的 `MeshCreate`（`Vertex3D` 一份），幀結束就清掉，只影響峰值。

### 17.2 前提：誰要解決

| 項 | 前提 | 判 |
|---|---|---|
| 作者交出的模型由 render 端拿走 | 〔Engine〕作者伸不進 `ExtractMeshes3d`；作者自己丟掉模型就沒得畫 | Engine：render 加一種持有形式 |
| 每幀上傳量的上限 | 〔Engine〕機制在 render（照 Bevy）；用多少由作者選 | Engine 給選項，預設不限；PvP 設值 |

### 17.3 設計（卡片判）

1. **交給 render 的模型**（Bevy 的 `RenderAssetUsages::RENDER_WORLD`＋`take_gpu_data`：`bevy_asset/src/render_asset.rs:32`、`bevy_render/src/render_asset.rs:348-358`、`bevy_mesh/src/mesh.rs:2722`）：
   - `ModelRef` 多一種：`shared_ptr<Render::HandedModel>`。作者把 `ModelData` 交給它，之後只握著這個小物件。
   - render 為某個 mesh 做好 GPU mesh 的那一幀，拿走那個 mesh 的頂點與索引（清空）。nodes 留著（Bevy 留 AABB：拿走後算不出）。
   - 帶 skin 的 mesh 不拿（每幀在 CPU 上 skin，要頂點）。
   - GPU mesh 的壽命跟著所有權：除了 render 的快取，還有人握著這個 `HandedModel`（`Mesh3d` 或作者），就留著；沒人握了才放（Bevy 是最後一個 Handle 丟掉才放 GPU 資產：`bevy_render/src/render_asset.rs:331-339`）。所以一幀沒畫（暫時拿掉 `Mesh3d`、貼圖還在讀）不會失去它，沒有 `Mesh3d` 畫的那幾個網格也留著。持有模型（`shared_ptr<const ModelData>`）照舊是「一幀沒畫就放」（RD-9）。
   - 和 Bevy 的差別：Bevy 在抽取時（上傳前）就取走、交給 render world 的佇列，以網格為單位；這裡在 GPU 網格做好後取，以模型為單位（模型的所有沒有蒙皮的網格都做好才取）。render 端改的是來源 World 讀進來的物件（kernel 宣告的存取裡看不到這次寫入）；Bevy 是可變地借 MainWorld 做同一件事。一個來源有兩個 render World 時，第二個畫不到（前提是一個消費者）。
2. **每幀上傳量的上限**（Bevy `RenderAssetBytesPerFrame`：`bevy_render/src/render_asset.rs:509-560`）：
   - `MeshRenderOptions::meshBytesPerFrame`，預設不限（Bevy 的預設）。
   - 軟上限，照 Bevy：一幀內建到超過為止，超過的那一個仍建；之後的留到下一幀，這一幀不畫。
   - transfer buffer 因此只長到上限附近。
3. **PvP**：region 的模型改用 `HandedModel`；Render World 設 64 MiB 的上限。
   - 被上限留到下一幀的網格不會出 draw，所以不算「被跳過的 draw」。mesh_render 因此在 Product 的 buffer 放一個 `MeshRenderStatus`（照 sdl_gpu 的 `SDLGPUStatus`），寫上一次抽取「為了上限沒做的網格數」；PvP 的暖機除了等沒有被跳過的 draw，也等它是 0（設計審查 S1）。

**偏離 Bevy**：Bevy 的用途旗標放在資產上（`Mesh::asset_usage`）。gyo2 的持有模型不是資產（沒有執行期的 `Assets::add`，見 `Drawables.hpp:104-106`），所以改成持有形式的一種；資產形式（`Handle<ModelData>`）這一批不加（沒有壓力）。

### 17.4 怎麼量、預期

- 同一套 S5 工具，Linux 上 1 km 那一級。probe 改成和 Client 一樣：建好網格就放掉 region 文件與碰撞盒，模型只交給 `Mesh3d`。前後各量一次，用同一個 probe：
  - 舊的形式＋不限上傳；
  - `HandedModel`＋64 MiB。
- RSS 取「畫完第一個位置之後」的穩定值與整段的峰值。
- 預期（推算）：穩定值從約 1.6 GB 降到約 0.2 GB；峰值降到約 0.9 GB（worker 做好的 `ModelData` 在交給 render 前仍是一份）。

### 17.5 判定（合併前）

- L1：交出的模型在 GPU mesh 做好的那一幀之後，該 mesh 的頂點與索引是空的，nodes 還在，畫面照舊。
- L2：帶 skin 的 mesh 不被拿走。
- L3：上限下，一幀內建立的 mesh 位元組不超過「上限＋最後一個」；沒建到的那一幀不畫，之後的幀補上。不限時行為和現在相同。
- L4：PvP 的 region 用交出的模型；loading 等到沒有網格在等（`MeshRenderStatus`）。
- L5：Linux 1 km 的 Client 穩定 RSS 比改前少，數字寫進 dev_log。
- 新加的突變全部 killed；L1 三平台。

### 17.6 不做

資產形式的「只給 render」旗標、transfer buffer 的縮回、頂點壓縮（要遊戲自己的 shader）、RC-BM1 的二進位快取、`BoxSet::Build` 本身的平行化、LOD（S7，延後）。

### 17.7 結果（2026-11-26，Linux 主機，1 km）

原始碼 `b63ba3f2`。同一個 probe（17.4：建好網格就放掉 region 文件與碰撞盒，模型只交給 `Mesh3d`），同樣 5 處 × 8 方向 × 300 幀（12,000／12,000 幀，跳過 0）。證據：主 checkout `build/evidence/gyo2-big-map/s5/linux/client_2000*`。

| 形式 | 載入後的 RSS | 結束時的 RSS | 峰值 RSS | 中位數 | 最壞 p95 |
|---|---|---|---|---|---|
| 改前（持有的模型、不限上傳） | 1,045 MB | 1,045 MB | 1,767 MB | 0.89 ms | 1.27 ms |
| 改後（`HandedModel`、64 MiB） | 345 MB | 346 MB | 1,127 MB | 0.89 ms | 1.27 ms |

- 穩定值少 700 MB（−67%），峰值少 640 MB；畫面時間不變。
- 兩項分開（設計審查 S2，同一個 probe 多量一格「`HandedModel`、不限上傳」）：

| 形式 | 載入後的 RSS | 峰值 RSS |
|---|---|---|
| 持有的模型、不限上傳 | 1,045 MB | 1,767 MB |
| `HandedModel`、不限上傳 | 572 MB | 1,283 MB |
| `HandedModel`、64 MiB | 345 MB | 1,127 MB |

  交出模型省約 470 MB，上限再省約 230 MB（transfer buffer 不再長到整張圖）。
- 預期的穩定值約 0.2 GB，量到 0.35 GB；改前的穩定值也比推算的 1.6 GB 少（1.05 GB）：推算的 740＋740 MB 沒有全部算進 RSS（GPU 可見的記憶體有多少算進 RSS，我沒有量）。
- 峰值的大部分**推算**是 worker 做好、交給 render 之前的 `ModelData`（17.4 預期的那一份），沒有分開量。
- 最終的原始碼 `f9285a22`（設計審查的修正後）重量：載入後 345 MB、峰值 1,127 MB、最壞 p95 1.27 ms，和上表相同。

### 17.8 載入時間：多條執行緒讀與做（`v8-arch-gyo2-big-map-memory`，同一條 PR）

2026-11-26。使用者要改善載入時間（主會話轉達）：多條執行緒平行解析，RC-BM1 的二進位快取不做。

- **現況**：region 的解析本來就是 asset store 每個 region 一個 job，但 Match 讀檔只有 1 條執行緒、Client 2 條；Client 的網格展開是一個 job 做全部 region。
- **改成**（卡片判）：
  - Match 與 Client 的 asset 執行緒照機器的硬體執行緒數，2～8 條（Match：`match/LoadThreads.hpp` 的 `MachineLoadThreads`；Client：`client_main.cpp` 的 `LoadingThreads`）。Match 可以用 `--load-threads N` 覆寫（下面的處置）。
  - Client 的 region 網格每個 region 一個 job，計算用執行緒 1～4 條；完成順序不定，**組回時照 region 的索引順序**，所以生成的實體和單執行緒相同。
  - Match 的牆仍是一次 `BoxSet::Build`（建構本身的平行化不在這批）。
- **決定性**：同一張地圖用 1 條與多條執行緒讀，`BoxSet` 的 digest 相同（`[pvp-map-parallel]`）；每個 region 單獨做的網格和整張一起做的逐位元組相同（`[pvp-region-alone]`）；4 條執行緒下 Client 畫出全部 region、照 region 順序（`[pvp-regions-parallel]`）。
- **量法**：真正的程式（不是 probe）。腳本的 `load` 模式：Match 在生成期間讀圖並記「the map is ready」；Client 用 offscreen 連到同一台的 Match，`--verbose` 記從進入遊戲場景到兩個 World 的地圖都好的時間。
- **結果**（Linux，16 執行緒，1 km，各兩次）：

| | 改前 | 改後 |
|---|---|---|
| Match 讀圖＋建牆 | 7,920 ms | 3,674～3,686 ms |
| Client 讀圖（真的程式） | 6,234 ms | 3,950～3,963 ms |
| Match 峰值 RSS（`/usr/bin/time`，精確） | 563 MB | 597～602 MB |

  - S5 的基準 4.8 s 是 probe 單執行緒的數字；真正的 Client 改前是 6.2 s。兩個都比改後的 4.0 s 長。
  - **Match 的記憶體比基準多 37 MB（+6.6%）**：同時解析的 region 變多。4 條執行緒時 579 MB、4.2 s。S5 寫的 471 MB 是看門狗每 0.5 s 的取樣，低估了峰值；同一個看門狗量改後是 505 MB。照判定門檻（≤ 1 GB）在內；「不能比基準差」這一條要不要容許這 37 MB，交使用者決定。

**處置**（2026-11-27，使用者，經主會話轉達；出處 HANDOFF `v8-arch-gyo2-364-load-threads`）：

- 原話：「2個都要，如果不加參數，系統執行期自動決定，如果加參數則按照參數來，超過最大（最小）設定值（例如：-1，100 這種），就轉回自動判斷。」
- **沒有參數**：照機器決定，硬體執行緒數夾在 2～8（就是上面量的 3.7 s／600 MB）。37 MB 照這個預設接受，回歸基準更新成精確數字（5.2.2）。
- **`--load-threads N`**（Match 的命令列，照既有參數的寫法）：N 在 1～8 就用 N。
  - 下限是 1，不是自動的 2（卡片判）：1 條就是改前的 Match（7.9 s／563 MB），要能用參數重現；上限 8 和自動相同（量過的最大值）。
  - N 超出 1～8（-1、0、100、超過 64 位元的數字）：**不夾值**，改回機器的決定，並記一行 info log 說明 N 被忽略、實際用幾條（只告訴外面發生了什麼）。
  - N 不是整數（`abc`、空字串）：和其他參數一樣是用法錯誤，結束碼 2。
- **測試**（`object_fps_pvp.logic`，cpu，CI 會跑）：`load threads:` 4 案，沒有參數（硬體 0、1、4、8、16）、範圍內（1、3、8，不論硬體）、範圍外（-1、0、9、100、極大數，取機器的數而不是最近的端點）、不是整數。程式層級另有 `object_fps_pvp.match_load_threads`（network，只在本機）：`--load-threads 100` 時 log 有那一行。
- **突變**：5 個（`pvp-load-threads-*`：自動不夾 2～8、範圍內被忽略、範圍外被夾到端點、範圍外沒說、空字串被當 0），5/5 killed。
- Client 不加覆寫（使用者「只給 Match」，2026-11-27，經主會話轉達）。
- **沒重量**：預設的程式路徑和量的那一版相同（同樣夾在 2～8），基準用 17.8 已量的數字。
- **回歸基準更新**（2026-11-27，模式配置 #367 的 U4）：Linux 重開後同日量，Match 讀圖＋建牆 3,805～3,885 ms、峰值 RSS 598～601 MB、Client 讀圖 3,920～3,996 ms。和上面的數字的差距大部分是機器狀態；剩下約 1% 是程式碼擺放位置的效應（[模式配置宣告](mode-config-declaration.zh-Hant.md) 11.8：改前塞 120 KB 不執行的填充就慢 25 ms）。同日交錯比，差距在 ±70 ms 內算雜訊。
