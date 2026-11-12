> **狀態：假說，由 survey（`v8-arch-big-map-survey`）檢驗。**
> 來源：架構師會話 2026-11-06 與使用者的討論稿＋方案 v0.1（原檔在架構師會話的 plan 檔，此處為逐字複製，只加這一段）。
> 使用者已核准「以它為假說去檢驗」。本稿第 3 節的業界細節是憑記憶寫的；與來源不符之處，以 [survey](survey.zh-Hant.md) 為準。
> 標 ⚠ 的數字是估算，沒量到就不是結論。

# 大地圖（開放世界）設計討論：業界做法 × GYO 的設計方式

狀態：討論稿（2026-11-06，架構師會話）。目的是**先討論**，不是開工。
討論定案後才開卡；卡片的第一批是「參照調查」，用實際讀來的資料核對本稿裡憑記憶寫的業界細節。

**使用者要求先看一版方案 → 第 11 節「方案 v0.1」。** 第 1～10 節是推導過程與討論紀錄。

## 1. 問題（Feature → Contract → Architecture）

- **Feature**：地圖編輯器的目的是為大地圖做準備（使用者原話）。大地圖＝玩家在一局裡走不完、
  不可能整張一次處理的地圖；幾何量比現在的 arena 高 1～3 個數量級。
- **觀測到的壓力**（探索試跑，證據 `build/evidence/tool-v8-voxel-map/window/explore-p1/`）：
  合成 64×16×64、填 30% → 124,236 四邊形、20,691 碰撞盒、5.3 MB 文字；每次編輯約 2.7 s，
  其中 2.4 s 是兩次 JSON 文字往返。pvp_corners（實際地圖）p95 13.1 ms。
- **契約天花板**：Engine `gyo.voxel_mesh` 文字 ≤ 16 MiB（約 3 倍合成圖）；產品 `walls ≤ 1024`，
  Movement 每 tick、ShotQuery 每槍線性掃全部 walls；Match 整張載入、Client 一個 mesh 一次 draw。
- **已成立的 Replacement Criteria**：工具 RC 3（build 時間與產物大小）、Engine RC 3（載入需要分區塊或空間索引）。
  規則要求交使用者決定「修還是重建」。

結論先講：**這不是編輯器的效能問題，是「一張地圖＝一份文件」的契約問題。** 開放世界的成熟做法全部建立在
「地圖＝可用座標定址的區塊集合」上。

## 2. GYO 自己的設計方式（對照時不能違反的）

| # | 原則 | 出處 | 對大地圖的含意 |
|---|---|---|---|
| G1 | Engine 中立、No build：只驗證、展開、轉換；不知道 block／shape | `voxel-mesh.md` DI 1、2 | 區塊格式仍是「面＋顏色＋盒子」；分區塊是 Engine 的事，怎麼烘是工具的事 |
| G2 | 一個驗證者 | `voxel-mesh.md` DI 3、工具 DI 2 | 工具不經文字也要能讓 Engine 驗證 → Engine 要提供對 document 的驗證 |
| G3 | 決定性：同一文件在所有平台展開成同樣的 float bits；固定 tick | `voxel-mesh.md` DI 5、`architecture.md` §Fixed simulation | 區塊的疊代順序、載入集合、碰撞查詢順序都要有定義，不能依賴雜湊表順序或執行緒 |
| G4 | 狀態的權威：判定用的狀態只有一個權威；派生暫存可重建、不進 digest | AGENTS §3、entity-model E-8 | 「哪些區塊已載入」若影響判定，就是權威狀態；要嘛由世界決定性地導出，要嘛不影響判定 |
| G5 | interest 只在 Match，過濾不改權威 digest | entity-model §4.2、G8 | 地形的「相關性」要和實體的 interest 用同一套想法：Match 決定、Client 只收 |
| G6 | Engine 不承擔工具職能；工具經資料契約交接 | memory engine-not-tool-duties | 區塊增量重建、跨區塊剔面是工具的；Engine 只定義區塊格式與讀取 |
| G7 | 三層：Engine 最高抽象／framework 通用結構／產品是實例 | memory three-layer | 區塊格式與 broadphase 進 Engine；預覽元件已在 framework；載入策略（整張／活動範圍）是產品的 |
| G8 | 先檢測再評估；判定先宣告、看結果後不放寬 | AGENTS §14.1、memory detection-before-evaluation | Match 掃 20k 盒子的成本、只畫不重建的畫面時間都還沒量；window 批 P1 已宣告 |
| G9 | 先參考成熟方案：理解為何、相似點、能用多少、要改裝多少；不是照搬 | memory copy-mature-solutions-first | 本稿第 3 節 |
| G10 | 格式是 Data Contract：版本、驗證規則都是契約的一部分；手寫、Script、別的 Tool 都能產生 | AGENTS §8 | 區塊格式仍要是能手寫、能 diff 的；二進位只能是「另一種編碼」，不能是唯一來源 |

## 3. 業界參照（憑記憶寫，survey 批要以實際資料核對）

| 參照 | 機制 | 為什麼這麼做 | 對 GYO 的套用 | 判定 |
|---|---|---|---|---|
| Minecraft | 世界切成 16×16 的 chunk 柱，柱內再切 16³ 的 section；一個 region 檔放 32×32 個 chunk；block 用 palette 壓縮 | 讀寫、網路、重建 mesh 的單位都要夠小；region 檔避免幾萬個小檔 | 區塊＝16³（和工具的 `gyo.voxel_grid` 一致）；一張地圖＝索引＋區塊產物；palette 已有 | **採用**（區塊 16³、索引＋區塊） |
| Minecraft | 伺服器只對每位玩家附近 view distance 內的 chunk 送資料、只在 simulation distance 內跑邏輯 | 伺服器成本和地圖大小無關，只和人數有關 | Match 的「活動範圍」＝玩家附近的區塊；和 entity-model 的 100 m interest 同一套想法 | **改裝**（Match 載入集合要由世界決定性地導出，見第 5 節） |
| Luanti（Minetest） | MapBlock 16³；地圖放 SQLite，以區塊座標為鍵；伺服器只模擬 active block range 內的區塊；Client 自己把 block 變 mesh | 開源、文件完整、設計最接近「伺服器權威＋Client 建 mesh」 | 證明「展開在 Client」可行；但 GYO 的烘焙（剔面、合併盒）在工具，不在 Client（G1、G6） | **改裝**（active range 的概念採；Client 建 mesh 不採，烘焙留在工具） |
| Veloren | 伺服器依「region」訂閱同步實體；地形區塊串流給 Client；ECS | 開源的開放世界多人範例；實體同步與地形串流分開 | 地形是資產、實體是 wire：GYO 已是這個分法（arena 由資產載入、digest 覆蓋）；只需確認大地圖仍不上 wire | **採用**（地形不上 wire 的分法） |
| Godot GridMap | cell 以 octant（預設 8³）分組，每個 octant 烘一個 mesh＋碰撞；navmesh 也按 octant | 編輯一格只重烘一個 octant；draw call 按 octant | 工具的增量重建單位＝區塊；Client 每區塊一個 draw（或合併鄰接區塊） | **採用**（每區塊一個 mesh；使用者先前已指定參考） |
| UE5 World Partition | 執行期以格子 cell 串流；HLOD 給遠處；Data Layers；One File Per Actor 給多人編輯 | 把「作者怎麼放」和「執行期怎麼載」分開；遠處用低精度代理 | 串流格子＝我們的區塊索引；HLOD 現在不需要（遠處先不畫或畫整區塊的盒子） | **改裝**（串流格子採；HLOD、Data Layers、OFPA 不採） |
| 物理引擎 broadphase（均勻格／spatial hash；Jolt、Bullet） | 靜態盒子先依格子分桶，查詢只看相鄰桶 | O(walls) 變 O(附近) | Engine `Collision` 加「靜態 AABB 的均勻格 broadphase」；疊代順序以區塊鍵排序保決定性（G3） | **採用**（Engine 中立能力；兩個產品端：Match、Client 預測） |
| 光照、AO、LOD、程序生成 | — | — | Engine RC 2 明寫頂點屬性要離開軸就換；現在沒有 Feature 點名 | **不採用**（記為 RC 候選） |

## 4. 對照後的候選契約（討論用，不是定案）

```text
地圖（一個資產目錄）
├── <map>.voxel_map.json        索引：schema、version、cell_size、origin、區塊清單（座標→檔名、sha256）、palette
└── chunks/<cx>_<cy>_<cz>.voxel_mesh.json   每區塊：quads＋colliders（沿用現在的規則 4～9），各自 ≤ 上限
```

- **Engine（`GYO::VoxelMesh` v2）**：索引格式＋區塊格式；`ParseVoxelMap`（索引）、`ParseVoxelChunk`、
  `ValidateVoxelMesh(document)`（不經文字的驗證，給工具的互動路徑，G2）；`ExpandVoxelChunk`；
  區塊鍵的全序（決定性）。不知道 block／shape（G1）。
- **Engine（`GYO::Collision`）**：靜態 AABB 的均勻格 broadphase（建立、ray／sweep 查詢），疊代順序定義在契約裡。
- **工具（voxel 線 owner）**：記憶體文件 → 只重烘被改到的區塊（＋相鄰區塊的邊界面）→ Engine 驗證 → 展開 → 預覽；
  存檔才寫文字。DI 2 的文字改寫（驗證者仍是 Engine，只是不經文字）；RC 3 的處置。
- **產品（產品會話 owner）**：arena v3＝索引檔；Match 載入策略（整張 vs 活動範圍，第 5 節）；
  Movement／ShotQuery 走 broadphase；撤 1024；Client 每區塊一個 mesh、依距離載入與卸載。wire 不變（地形是資產）。
- **framework**：OffscreenPreview 已有；不改。

## 5. 和 GYO 原則的衝突點（要討論）

1. **Match 的載入集合是不是權威狀態（G4）。** 若 Match 只模擬玩家附近的區塊，「附近」要由世界的位置決定性地導出，
   並且規則上不可能有判定碰到未載入區塊（玩家走不到）。靜態地形成立；PvPvE 的遠處 AI 要嘛凍結（Luanti 的做法），
   要嘛全圖模擬——這是玩法決定，不是架構能定的。**最簡單的第一步是 Match 整張載入**（只有碰撞盒，無 mesh；
   20k 盒子的記憶體可忽略），只做 broadphase；活動範圍留到真的量到載入時間或記憶體壓力再做。
2. **決定性（G3）。** 區塊疊代、broadphase 桶內順序、載入順序都要有全序；禁止依賴 unordered 容器順序與執行緒時序。
3. **一個驗證者（G2）。** 工具不經文字就要驗證 → Engine 公開介面加 `ValidateVoxelMesh(document)`，
   這是 Architecture Delta（Engine 公開介面），但保住 DI 3 的精神：規則只在 Engine。
4. **16 MiB 與 1024 兩個數字**：是契約的一部分；撤除或改成每區塊上限都是 Delta，要寫 7 項。
5. **window 批的 P1 已宣告**（每張地圖 p95 ≤ 16.7 ms，(b) 是合成圖）；看到結果後不能默默放寬（G8）。
   建議由使用者**重宣告**成兩個指標：畫面 p95（只畫靜態幾何）與「編輯到預覽更新」的延遲；理由是把兩者綁在一幀裡
   量的是錯的東西，不是因為過不了。
6. **Engine 現在的 JSON 讀取約 4 MB/s**（nlohmann＋重複鍵回呼）。分區塊後單檔變小，但 Match／Client 啟動時
   的總量不變；若量到載入時間是問題，Engine 內部換解析方式（契約不變）是另一個低風險選項。先量再說。

## 6. 要和使用者討論的分歧點

1. **目標規模**（Feature 的數字，只有你能定）：地圖大小與密度的量級、同房人數、可接受的載入時間、編輯延遲。
   沒有這些數字，RC 的「需要」無法判定，分區塊的大小也定不下來。
2. **Match 第一步整張載入＋broadphase，活動範圍延後**——同意這個順序嗎？
3. **格式**：維持 JSON 文字（可手寫、可 diff，G10），分區塊後單檔小；或容許二進位編碼作為第二種編碼？
4. **區塊大小**：16³（與 `gyo.voxel_grid` 一致）；或更大（32³）減少索引與 draw 數？
5. **window 批怎麼接**：(a) 先做小修（Engine `ValidateVoxelMesh(document)`，約把 2.7 s 降到 0.2 s）讓 window 批繼續，
   分區塊契約並行規劃；(b) window 批等分區塊契約定案。我傾向 (a)：小修是分區塊方案的子集，不會白做。
6. **P1 的重宣告**：同意改成兩個指標？

## 6a. 規模參照：逃離塔科夫（使用者 2026-11-06 補充）

使用者：「參考可以看看逃離塔科夫的規模。目前最多就 4 人玩家。遊戲裡會有多個區域、零散遊蕩怪物和怪物溢出區域、
任務區、boss 戰區域、隱藏區域；估計還有陷阱和寶箱。」

**這改變了問題的性質**：塔科夫的 raid 地圖是一張**整張載入的大型手工關卡**（約 1～3 km²，進 raid 時全圖載入，伺服器全圖模擬 AI），
不是無邊界、分區域串流的開放世界。所以：

| 面向 | 塔科夫模型對 GYO 的含意 | 對第 6 節決定的影響 |
|---|---|---|
| Match 載入 | 全圖載入碰撞＋全圖模擬 AI（塔科夫的 scav 是伺服器全圖跑的）；4 人房讓每收件人 snapshot 預算（1200 B）不緊 | 偏向「整張載入＋broadphase」；活動範圍不是第一需求。survey 仍要核對塔科夫怎麼處理伺服器端 AI 成本 |
| Client 渲染 | 視距可達數百公尺（狙擊是玩法）；一張 km² 等級的 voxel 圖若 cell 1 m、只剔面不合併，地面就上百萬四邊形 | **greedy 合併從「候選」變成必要**（工具的烘焙，G6）；每區塊一個 mesh＋視錐／距離剔除是底線；LOD／遠處代理從「不採用」改成 **RC 候選要量**。區塊編碼與區塊大小由此決定 |
| 地圖內容不只幾何 | 區域（任務區、boss 區、隱藏區、怪物溢出區）、出生點、撤離點、寶箱、陷阱、遊蕩怪物的路徑 | 需要**第二層資料：標記層**（點、體積、路徑，帶不透明的名字與鍵值）。Engine／framework 可擁有中立的標記格式（名字不透明，比照 `gyo.anim_events` 的先例：Engine 解析、產品賦義），產品定義意義；編輯器只放置與命名，不知道「boss 區」是什麼（G1、AGENTS §8） |
| 實體 | 怪物、寶箱、陷阱都是實體（entity-model E-1、E-4：Boss 與小怪是實體不是專用訊息；容器在 30 m 可見性） | 和 `v8-arch-entity-store`（ECS 容器）同一條線：大地圖＋多實體＋interest 100 m 是容器的第一個真實壓力；AI 尋路要靠地圖導出的導航資料（Godot GridMap 按 octant 烘 navmesh 的先例）——不在本案，但區塊格式要留得住 |
| 編輯方式 | 塔科夫地圖是多人長期手工；我們是單人編輯器 | One File Per Actor 仍不採；但「區塊各自一檔」讓 diff 與 undo 有自然的粒度 |

**估算用的前提（survey 要核對、使用者要定）**：cell_size（0.5 m 給建築細節 vs 1 m）決定一切數量級。
以 1 km×1 km、高 32 格、cell 1 m 粗估：地面 1M 格；只剔面 → 地面約 1M 四邊形；greedy 合併後同色地面可降到數千；
碰撞盒經 slab 合併後數量取決於建築密度（估 10⁴～10⁵）→ broadphase 必要、Match 記憶體可忽略（10⁵ AABB ≈ 2.4 MB）。
Engine 現在 16 MiB 文字、4 MB/s 解析 → 全圖載入數十秒等級 → **編碼與解析速度在這個規模下不是「survey 後再看」而是必答**。

**survey 批因此增加**：(7) 塔科夫 raid 地圖的結構與數量級（公開資料：地圖尺寸、區域種類、AI 密度、載入模型、視距）；
(8) 標記層的業界做法（Source 的 entity／brush 體積、Godot 的 Area 節點、UE 的 Volume／Data Layer、Minecraft 的結構與 spawn 規則）
與 GYO 的 owner 歸屬；(9) greedy 合併與 LOD 在 voxel 遊戲的做法與代價（決定性要保住）。

## 7. 使用者的決定（2026-11-06）

第一輪（第 6 節的四個分歧點）：目標規模、Match 載入策略、區塊編碼都「survey 之後再決定」；
window 批**等分區塊契約定案**，不先做小修。

第二輪（塔科夫參照之後）：
- **cell_size＝0.5 m**。估算全部以 0.5 m 重算：1 km×1 km×16 m（32 層）＝2000×2000×32＝1.28 億格；地面 400 萬格；
  只剔面的地面約 400 萬四邊形 → greedy 合併是必要條件；碰撞盒經 slab 合併取決於建築密度，估 10⁵～10⁶ 級。
  16³ 區塊（8 m 邊）在 1 km² 上約 15,625 柱 × 2 層＝約 3 萬個區塊 → 索引本身要能快速讀，draw 要按區塊剔除與合併。
- **標記層納入**：中立標記格式（點、體積、路徑，名字與鍵值不透明）由 Engine／framework 擁有（owner 歸屬由 survey 給證據），
  產品賦義；編輯器只放置與命名。
- **模擬模型**（Match 全圖模擬 vs 遠處 AI 降頻／凍結）：survey 後再決定。
- **greedy 合併納入契約討論；LOD／遠處代理先量再說**（記為 RC 候選）。

所以下一步只有一批：參照調查，範圍如第 8 節（含第 6a 節加的三項）。

## 8. 下一批：`v8-arch-big-map-survey`（唯一要開的卡）

- **段／模型／檔位**：上段（Architecture 判斷的根據）。調查者 Opus／high；對抗檢查者 Fable／xhigh（比主會話強，開卡時要使用者同意；
  不同意就用 Opus 並在報告寫明同模型）。估時 8～12 h（範圍比第一稿多了塔科夫、標記層、greedy／LOD 三項）；超過 1.5 倍停下回報。
- **目的**：用**實際讀到的資料**（不是記憶）核對第 3 節，產出一張「採用／改裝／不採用」表，作為之後 Engine／產品契約的 DI 根據；
  並為第 6 節的四個決定各給出證據與建議，讓使用者能決定。
- **要讀的參照與每個要回答的問題**（主要來源優先：官方文件、原始碼；維基只當索引）：
  1. Minecraft（區塊格式、region 檔、view distance 與 simulation distance）：區塊大小與分層為何；伺服器怎麼決定模擬範圍；
     palette 編碰的理由。
  2. Luanti／Minetest（`MapBlock`、`active_block_range`、map database）：伺服器只模擬活動區塊時，權威與決定性怎麼處理；
     未載入區塊的碰撞怎麼辦；為何選 SQLite 以座標為鍵。
  3. Veloren（regions、terrain streaming、server authority）：地形串流與實體同步分開的理由；Client 怎麼知道該載哪些區塊。
  4. Godot GridMap（octant、MeshLibrary、per-octant baking／navmesh）：增量重烘的單位與 draw 的分組；為何 8³。
  5. UE5 World Partition（runtime grid、streaming、HLOD、One File Per Actor）：作者配置與執行期載入分離的做法；
     哪些是我們現在不需要的。
  6. 一個物理引擎的靜態 broadphase（Jolt 或 Bullet）：均勻格／樹的取捨；查詢順序是否決定性。
- **每個參照寫一列**：機制｜為什麼這麼做｜對 GYO 的套用（指出 owner：Engine／framework／工具／產品）｜判定｜根據（URL 或檔案與行）。
  判定要對照第 2 節 G1～G10；違反的寫出衝突與改裝方式。
- **待決定事項各一節**：(1) 規模分級（以塔科夫 raid 地圖為上限、cell 0.5 m 重算數量級）；(2) Match 的模擬模型
  （全圖模擬 vs 遠處 AI 降頻／凍結；權威與決定性怎麼寫）；(3) 區塊編碼（JSON 可讀 vs 載入速度；3 萬個區塊時索引與區塊檔怎麼讀）；
  (4) 區塊大小（16³ vs 32³，對 draw 數、重烘粒度、索引大小的影響）；(5) 標記層的 owner（Engine／framework）與格式輪廓
  （點、體積、路徑、鍵值；名字不透明）；(6) greedy 合併進烘焙契約的方式（產物規則 6 的 w×h 已支持；決定性與剔面、碰撞盒的關係）。
  每節：業界怎麼做、對 GYO 的建議、建議的理由、不採建議的代價。不替使用者決定。
- **補量測（先檢測再評估，G8）**：用現有工具與合成圖量 (a) 只畫不重建的畫面時間（framework OffscreenPreview、497k 頂點）、
  (b) Match 一 tick 線性掃 20,691 盒子的成本（Movement 的 supported＋ShotQuery 一槍）、(c) Engine 讀 5.3 MB 文字的時間拆解
  （解析 vs 規則檢查）。只量，不改程式；寫明件數突き合わせ。
- **「不採用」的項目**寫成之後 Engine／工具 Replacement Criteria 的候選（第 3 節最後一列的做法）。
- **產出**：`docs/architecture/plans/big-map/README.md`（計畫、批次一覽）、`HANDOFF.md`（記錄器）、`survey.zh-Hant.md`（表與四節）；
  dev_log `docs/dev_logs/2026_MM_DD_arch_v8_big_map_survey.zh-Hant.md`；證據 `build/evidence/big-map/survey/`（量測 CSV、讀過的來源清單）。
  第 3 節憑記憶寫的內容若與來源不符，以來源為準並標出修正。
- **對抗檢查**：檢查者讀 survey 與來源，試圖否定每個「採用」的理由與四個建議；找出調查者沒讀的來源；結果附在 survey 末。
- **結束**：停下，回報使用者四個決定。不開 contract 批、不改 Engine／工具／產品程式碼、不改 PR #152 的文件。

## 9. survey 之後（不在本次開卡範圍，寫出來只為看見全貌）

```text
v8-arch-big-map
├── -survey    上述
├── -contract  high（上段）｜Opus／Opus／Fable（確認）｜Engine：VoxelMesh v2（索引＋區塊）、ValidateVoxelMesh(document)、Collision broadphase；Delta 7 項、DI／RC
├── -tool      voxel 線 owner｜記憶體文件、區塊增量重建、DI 2 改寫、RC 3 處置；window 批在此之後
└── -product   產品會話 owner｜arena v3、broadphase、撤 1024、Client 區塊載入；活動範圍與 PvPvE 另立 Feature
```

## 10. 開卡後架構師要做的事

- 告訴 voxel 地圖編輯器會話（`local_68daa545-2e66-4a17-a116-a31a89d2530f`）：使用者決定 window 批等分區塊契約定案；
  架構師開了 survey 卡；之後的 tool 批仍由它做。
- 記錄到 memory（已記錄觀測與方向；補上四個「survey 後再定」）。

---

## 11. 方案 v0.1：區塊化地圖（塔科夫規模、cell 0.5 m、4 人房）

這是**假說**，survey 批用實際來源與量測來否定或修正它；數字都是估算，標 ⚠ 的要先量。
原則：Engine 只多「分區塊、索引、標記、broadphase」四件中立的事；烘焙仍在工具；載入策略與賦義在產品；framework 不改。

### 11.1 一張地圖是哪些檔

```text
assets/<product>/maps/<map_id>/
├── map.json                         gyo.voxel_map v1      索引（Engine 格式）：cell_size、origin、區塊與 region 的大小、範圍、
│                                                          全圖共用 palette、region 清單（座標 → 檔名、sha256）、source_sha256、build_version
├── regions/r_<rx>_<rz>.json         gyo.voxel_region v1   一個 region＝8×8 個區塊柱（64 m×64 m）× 全高；內含其非空區塊的 quads＋colliders（Engine 格式）
├── markers.json                     gyo.map_markers v1    標記層（Engine 格式，名字不透明）：點、盒、路徑，各帶 id、kind、tags
└── <map_id>.arena.json              arena v3（產品格式）：引用 map.json；把 kind 賦義（spawn、extract、boss_zone、loot…）；玩法參數

build/art-source/<map_id>.voxel_grid.json      工具格式 gyo.voxel_grid（原始檔，git 不追蹤；已是 16³ 區塊分段）
```

- **編輯單位＝區塊（16³ 格＝8 m 立方）**；**檔案單位＝region**（避免一張 km² 地圖 3 萬個檔；資產守衛與 catalog 也撐不住 3 萬筆）；
  **渲染單位＝region**（載入時把 region 內區塊的 mesh 合成一個 GPU mesh；1 km² 約 256 個 draw，再做視錐與距離剔除）；
  **碰撞單位＝整張**（所有 colliders 進一個 broadphase）。
- 兩張現有地圖（pvp_arena、pvp_corners）由工具 `convert` 重烘成 1 個 region 的 v1 索引地圖；`gyo.voxel_mesh` v1 單文件格式**退役**，不留兩套。
- 維持 JSON 文字（可手寫、可 diff、AGENTS §8）。載入速度的問題放在 Engine 的解析實作（11.7），不放在格式上；
  二進位快取記為 Replacement Criterion，量到不夠才做。

### 11.2 Engine：`GYO::VoxelMesh` v2（格式與讀取，仍 No build、中立、一個驗證者、決定性）

| 項目 | 內容 |
|---|---|
| `gyo.voxel_map` v1 | `schema`、`version`、`cell_size`（[2⁻¹⁶, 2¹⁶]）、`origin`（格）、`chunk_size`＝16、`region_size`＝8（v1 固定，寫進檔以便日後改）、`bounds`（區塊座標的 min／max）、`palette`（全圖共用，上限 ⚠ 256 夠不夠要看內容，候選 4096）、`regions[]`（`at`、`file`、`sha256`，依 `at` 遞增）、`source_sha256`、`build_version`。嚴格、鍵不重複、文字 ≤ 1 MiB |
| `gyo.voxel_region` v1 | `schema`、`version`、`at`、`chunks[]`（依區塊座標遞增，省略空區塊）；每個 chunk：`at`、`quads[]`、`colliders[]`，座標是**區塊內的 1/16 單位**（0～256），規則沿用現在的 4～9（七個整數、遞增、不重疊）；文字 ≤ 16 MiB |
| `gyo.map_markers` v1 | 11.3 |
| API | `ParseVoxelMap(text)`、`ParseVoxelRegion(text, const VoxelMapDocument&)`、**`ValidateVoxelChunk(const VoxelChunkDocument&, const VoxelMapDocument&)`**（不經文字；工具互動路徑用，規則與 Parse 同一份實作）、`ExpandVoxelChunk(chunk, map) → VoxelMeshGeometry`（世界座標；float 公式不變：整數和在 ±2³⁰ 內精確，乘一次、除 16 精確）、`VoxelChunkKey` 的全序與 `ChunkWorldOrigin` |
| DI 增補 | 「區塊是驗證與展開的單位，region 是檔案的單位，Engine 不知道渲染單位」；「所有疊代順序由區塊鍵的全序定義」 |
| RC 增補 | 區塊需要自己的 palette；region 文字量到需要二進位；標記需要幾何以外的語意 |

### 11.3 Engine：`GYO::MapMarkers`（新小模組，只依賴 Base、Math）

```json
{ "schema": "gyo.map_markers", "version": 1,
  "markers": [
    { "id": "spawn_a1", "kind": "spawn", "point": [x, y, z], "yaw": 0.0, "tags": { "team": "any" } },
    { "id": "boss_yard", "kind": "zone", "box": { "min": [...], "max": [...] }, "tags": { "role": "boss" } },
    { "id": "patrol_3", "kind": "path", "points": [[...], [...]], "tags": { "loop": "true" } } ] }
```

- Engine 只檢查：id 唯一且依 id 遞增、三種幾何之一且有限、`kind` 與 `tags` 是不透明字串（長度與字元集限制）。
  **不知道 spawn／boss 是什麼**（比照 `gyo.anim_events`：Engine 解析、名字不透明、產品賦義）。
- 座標用 1/16 格的整數（和 quads 一樣），避免浮點差異。
- 為什麼是 Engine 不是 framework：它是單一資產格式、不引用別的 Engine 資產 id、沒有產品意義 → Layers 表的 Engine 欄。
- 編輯器需要的「可選 kind 清單」是**產品給的資料**（`marker_kinds.json`，產品格式），編輯器當作調色盤讀，不解釋。

### 11.4 Engine：`GYO::Collision` 的靜態 broadphase

- `StaticAabbGrid`：由 `span<const Aabb>` 建立，格子邊長由呼叫端給（建議＝區塊 8 m）；查詢 `RaycastNearest(ray, range)`、
  `SweepCapsule(capsule, delta)`、`Overlapping(aabb) → 遞增的索引`。
- **契約用等價定義**：每個查詢的結果定義為「對全部盒子依索引順序線性掃描會得到的結果」（距離相同取索引小者）。
  守衛＝隨機盒子集合上與線性掃描逐一比對（200 組＋邊界案例），加上突變。這樣決定性自動成立，Movement／ShotQuery 的語意不變。
- 兩個消費端：Match（Movement、ShotQuery、spawn 驗證）、Client（預測）；都是產品，Engine 不知道牆是什麼。

### 11.5 產品（object_fps_pvp）

- **arena v3**：`map`（目錄名）取代 `geometry`；`walls` 刪除、`walls ≤ 1024` 刪除（上限改由 Engine 的 region／區塊規則與 Match 的記憶體預算決定 ⚠）；
  `spawns` 從 arena 搬到 markers（kind `spawn`），arena v3 的 `marker_kinds` 列出產品認得的 kind 與必要 tags，不認得的 kind → 載入錯誤（抓錯字）。
- **Match**：載入 map 的全部 region → 所有 colliders → 一個 `StaticAabbGrid`；全圖模擬（11.9 的模擬模型若選「遠處 AI 降頻」再加）；
  權威 digest 不變（arena id＋內容 sha）。載入在房間建立時，不在 tick 內。
- **Client**：全部 region 載入後才 ready（簡單、無「已載入集合」；背景串流記為 RC 候選）；每個 region 一個 GPU mesh（區塊 mesh 合併），
  依 region AABB 做視錐與距離剔除；palette 一張貼圖。
- **wire 不變**：地圖是資產；interest 100 m 不變；標記不上 wire（實體的出現走 entity-model 的事件）。
- **遷移**：兩張現有地圖重烘成 v3；arena v2 退役（和 v1 退役同一批，照 §7 removability 清掉）。

### 11.6 工具（voxel_map_editor，voxel 線 owner）

- 編輯迴圈：改格 → 標髒的區塊（＋被影響的鄰接區塊，因為剔面看鄰格）→ 只重烘髒區塊 → `ValidateVoxelChunk` → `ExpandVoxelChunk` → 更新預覽中該區塊的 mesh。
  文字只在存檔時寫（只寫髒的 region＋索引＋markers）。
- **greedy 合併進烘焙**：同一平面、同方向、同 palette 的相鄰面合成最大矩形；掃描順序固定（決定性，產物規則 6 的 w×h 已支持）。
  碰撞盒的 slab 合併改為**區塊內**（盒子不跨區塊；數量會多一些，換得局部重烘；⚠ 要量）。
- DI 2 改寫：「有效性由 Engine 的 `ValidateVoxelChunk` 決定；存檔寫出的文字再經 `ParseVoxelRegion` 讀回比對」——驗證者仍只有 Engine。
- 標記編輯：放點、拖盒、畫路徑、改 kind／tags；kind 清單從產品的 `marker_kinds.json` 讀。
- CLI：`build`／`verify`／`inspect` 以地圖目錄為單位；`convert-arena-v2`。
- 相機：編輯平面用的自由相機（framework 的 OffscreenPreview 已接受客戶端的 `PerspectiveCamera3D`）。

### 11.7 Engine 內部：解析速度（契約不變）

- 現況 ⚠ 約 4 MB/s（nlohmann＋重複鍵回呼）。km² 地圖的 region 總文字量粗估 ⚠ 100～500 MB → 現在要 25～125 s。
- 方案：Engine 內部改成 SAX／串流式嚴格解析（同一套規則、同一組測試與黃金值、突變重跑），目標 ⚠ ≥ 50 MB/s → 2～10 s；
  region 可在多個執行緒平行解析（結果依 `at` 排回，決定性不受影響；`GYO::Threads` 已有具名角色執行緒）。
- 不夠時才啟用 RC「二進位快取」。

### 11.8 數量級（1 km × 1 km、高 32 m＝64 層、cell 0.5 m；⚠ 全部待量）

| 量 | 估算 | 依據／風險 |
|---|---|---|
| 格數 | 2000×2000×64＝2.56 億（大多為空） | 非空區塊才佔空間 |
| 區塊 | 125×125 柱 × 4 層＝62,500 個（非空估 ⚠ 1/3） | 索引不列區塊，只列 region |
| region | 16×16＝256 個 | 每個 ≤ 16 MiB 要驗 |
| 地面四邊形 | 只剔面 400 萬 → greedy 後 ⚠ 數千～數萬（取決於色塊） | greedy 是必要條件 |
| 建築四邊形 | ⚠ 10⁵～10⁶ | Client 頂點記憶體 ≈ 四邊形 × 4 × 16 B → 10⁶ 四邊形 ≈ 64 MB，可接受 |
| 碰撞盒 | ⚠ 10⁵～10⁶（區塊內合併） | Match 記憶體 24 B × 10⁶ ≈ 24 MB；broadphase 必要 |
| draw | 256 region → 視錐後 ⚠ 數十 | 不需要 LOD 就能開始 |

### 11.9 survey 要否定或補齊的（對應第 7 節留給 survey 的決定）

1. 模擬模型：全圖模擬所有 AI，還是遠處降頻／凍結（塔科夫、Luanti 的伺服器做法）。v0.1 預設全圖。
2. 區塊 16³／region 8×8 的大小（Minecraft 16／32、Godot 8³ 的理由）。
3. JSON＋快解析是否足夠，還是必須二進位（量 11.7）。
4. 標記層的 owner 與格式輪廓（Source、Godot Area、UE Volume、Minecraft spawn 規則）。
5. greedy 與碰撞盒區塊內合併的代價（量）。
6. LOD／遠處代理是否在 1 km² 視距下已經需要（量 draw 與頂點數）。

### 11.10 Design Invariants 與 Replacement Criteria 的輪廓（contract 批正式寫）

- Engine VoxelMesh v2：中立、No build、一個驗證者（含 document 驗證）、嚴格、決定性（區塊鍵全序）＋「region 是檔案單位、Engine 不知道渲染單位」。
  RC：區塊需要自己的 palette；二進位；非盒碰撞；頂點屬性。
- Engine MapMarkers：名字不透明；只檢查幾何與唯一性；不引用其他資產。RC：標記需要語意驗證（例如「spawn 必須站得住」——那是產品的）。
- Engine Collision broadphase：結果與線性掃描等價。RC：需要動態物體或非 AABB。
- 產品：載入在 tick 外；Client 無已載入集合；賦義只在 arena v3。RC：Client 需要背景串流；Match 需要活動範圍。
- 工具：區塊是重烘單位；驗證者是 Engine；文字只在存檔。RC：格子出現第二個讀者；需要斜坡。

### 11.11 分批（v0.1 的路線；模型照 navigator：上段 Opus／Opus／Fable 需確認）

```text
v8-arch-big-map
├── -survey            high｜Opus，對抗 Fable（先問）｜以來源與量測檢驗 v0.1（11.9 六項）；停下交使用者定案
├── -engine-map        high（上段）｜Opus／Opus／Fable｜VoxelMesh v2（map、region、ValidateVoxelChunk、ExpandVoxelChunk）、MapMarkers、
│                      StaticAabbGrid、解析加速；Delta 7 項、DI／RC、規則測試＋突變、黃金值、跨平台決定性
├── -tool-chunked      voxel 線 owner｜髒區塊重烘、greedy、區塊內碰撞合併、ValidateVoxelChunk 路徑、region 存檔、markers 編輯、convert；
│                      DI 2 改寫、RC 3 處置；之後接 window 批
└── -product-arena-v3  產品會話 owner｜arena v3、markers 賦義、StaticAabbGrid、撤 1024、Client region mesh＋剔除、兩張地圖遷移、v1／v2 退役
```

### 11.12 已知風險

- 解析速度若達不到 11.7 的目標，Client 載入會是分鐘級 → 二進位快取提前。
- palette 256 在 km² 手工地圖可能不夠 → 改上限是契約變更，survey 要給依據。
- 碰撞盒區塊內合併讓盒子數上升；broadphase 下影響小，但 Match 記憶體與載入要量。
- AI 尋路、遠處降頻、背景串流、LOD 都不在 v0.1；格式要留得住（區塊鍵、region 索引、markers 的路徑）但不先做。
- 這些估算是憑記憶與粗算；survey 批以實際資料取代。
