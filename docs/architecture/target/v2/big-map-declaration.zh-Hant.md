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
| **視錐剔除** | Engine：bounds 元件＋每個相機的可見性，抽取只抽看得到的 | 〔Engine〕作者伸不進 `ExtractMeshes3d` | Bevy `bevy_camera/src/visibility/mod.rs`：`Aabb`（`calculate_bounds` :557）、`ViewVisibility` :226、`VisibleEntities` :344、`NoFrustumCulling` :318、`check_visibility_cpu_culling` :748 |
| **距離剔除**＝最簡單的 LOD | Engine：`VisibilityRange` | 〔Engine〕同上 | Bevy `visibility/range.rs:80`、`check_visibility_ranges` :230；Godot `visibility_range_begin`（`visual_instance_3d.h:128`） |
| **LOD 代理網格**（遠處 region 的簡化網格） | 工具烘（另一個 asset，不改 region 格式）＋Engine 的 `VisibilityRange` 切換 | 產生網格〔作者〕（工具）；切換〔Engine〕 | Unreal HLOD（編輯器／commandlet 離線建，未載入的格子用 HLOD 代替）；master `-lod` 的 RC |
| **instancing** | Engine：render＋`sdl_gpu` 的 instanced draw | 〔Engine〕 | Bevy 的自動 batching；Godot GridMap 每種 item 一個 MultiMesh（`grid_map.cpp:946`） |

- **instancing 對 voxel 幾何沒有幫助**：每個 region 的網格都不一樣（greedy 後更是）。它幫的是場景裡重複的東西（箱子、樹、路燈）。所以排在剔除之後，測試場景用重複的模型，不用地圖。
- **Client 的記憶體是 km² 的第一個風險**（推算，未量）：km2-building 487 萬 quads × 4 頂點 × 32 B ≈ 623 MB 頂點＋117 MB 索引；`ModelData` 在 CPU 端還有一份同大小。合計可能 1.5 GB 以上。對策候選（S5 量到才選）：上傳後放掉 CPU 端的頂點、voxel 專用的壓縮頂點（要遊戲自己的 shader，屬渲染管線線）、LOD 代理、背景串流（RC-BM3）。
- 剔除的放法照 Bevy：可見性在**來源 World** 算（camera 那一側），抽取跳過看不到的。偏離點：gyo2 的抽取每幀重建（Bevy 保留 `RenderMeshInstances`），所以「跳過」就是少抽一項，不需要 Bevy 的 change detection 那一套；膠水只有「抽取時讀可見性」一處。

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
| instancing | 自動 batching | — | MultiMesh | HLOD 的 Instancing 層 | 照 Bevy，排在後面 |

**偏離 Bevy 的地方與膠水**：
1. **整張載入、不照 bevy_voxel_world 的相機串流**：衝突點是權威——Match 要整張碰撞，Client 的「看不到的先不載」和 Match 的狀態無關，但 region 何時進碰撞要決定性（asset RC-AS1）。不串流就沒有這個膠水。這不是偏離 Bevy 核心（核心沒有串流），是不採用一個生態 crate 的策略。
2. **格式自己定（master 的）**：Bevy 生態沒有檔案格式可照；接合點是 Bevy 式的 asset loader（我們的 asset Module 已照 Bevy），膠水只有兩個 loader。
3. **Match 碰撞整張一個 `BoxSet`**：avian 有實體層 broadphase、我們沒有（physics_3d 的取捨，3D 物理卡已定）。一張地圖一個實體正好繞開這個差異，不寫膠水。
4. 剔除、`VisibilityRange`、instancing 照 Bevy，沒有偏離。

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

## 5. 量測

### 5.1 S0：先量現狀能撐到多大（**輕量，不需要同意**）

- **地圖**：把 biome（真資料，63 m）的 v1 平鋪 1×1、2×2、3×3（3×3＝189 m、約 12.5 MB，在 v1 的 16 MiB 之內；4×4 超過契約上限，現狀讀不了）。
- **Match（無視窗、只用 CPU）**：載入時間（讀檔、解析、`BoxSet::Build`）、峰值 RSS、一個 tick 的移動與射擊查詢時間（中位數、p95）。另外把碰撞盒放大到 10⁴～10⁶（平鋪、與 master km2-building 同量級的 103 萬盒）只量 `BoxSet::Build` 與查詢，回答「`BoxSet` 撐不撐得住 km²」。預估峰值 RSS < 2 GB。
- **性質**：探索與描述，不設門檻、不判過與不過；方法在看結果前寫進 S0 的紀錄，看了不改。件數照實記。
- 證據放主 checkout `build/evidence/gyo2-big-map/s0/`。

### 5.2 要同意才跑的（**本機重 GPU 或大記憶體**）

| 量測 | 段 | 影響（先講） |
|---|---|---|
| Client 幀率：biome 平鋪 1×1～3×3，開視窗、最壞角度 | S0 的 Client 部分 | GPU 滿載數分鐘；3×3 約 35 萬 quads，預估不會卡機，但 11-07 曾在 km² 預覽卡死，所以先問 |
| km² 合成圖（(d2)）的 Match 載入／記憶體、Client 幀率 p95 | S5 | 預估 Client 1.5 GB 以上 RAM＋數百 MB VRAM；工具烘焙約 47 s、峰值 3 GB（master 的數字）。會先從 1/4 km² 往上，每一級看 RSS 再決定下一級 |

S0 的 Client 部分與 S5 開跑前，我會先送「量什麼、多大、預估影響、怎麼中止」給主會話轉使用者，同意後才跑。

## 6. 和 loading 的關係（流程框架卡 `local_8a21ecb9`）

- 單人「開始遊戲 → 加載資源、讀檔 → 進入遊戲」、多人「開始遊戲 → 加載…（邊連 Match 邊載）」的「加載」，大地圖接在**生成世界階段**（`WorldGeneration`）：
  - 遊戲在生成期間發出索引與全部 region 的 asset 請求；地圖的系統在「region 全部 Loaded 且碰撞／網格建好」前呼叫 `Hold()`。
  - **進度**＝`AssetView::Ready(region handles)` 的 `loaded`／`ids.size()`（`AssetView.hpp:66-131`），再加「建碰撞」「建網格」兩步。大地圖給的是數字，畫面與進度條是流程框架卡的。
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
└── S7 LOD 代理網格     只在 S5 超過宣告值時開（RC）
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
