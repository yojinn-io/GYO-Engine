# 大地圖 survey：以來源與量測檢驗方案 v0.1（`v8-arch-big-map-survey`）

更新：2026-11-06。Owner：架構。計畫：[README](README.md)；假說：[proposal-v0.1](proposal-v0.1.zh-Hant.md)；記錄：[HANDOFF](HANDOFF.md)。
**狀態：調查與量測完成；對抗檢查（Fable／xhigh）的指正已核對並修正，結果附在第 7 節。六個未決事項等使用者決定；本文只給建議，不替使用者決定。**

## 0. 摘要

- **量測**：慢的不是畫面，也不是 JSON 本身。
  - 只畫不重建的畫面 p95 4.6 ms；explore-p1 每幀 2.7 s 幾乎全是重建與文字往返。
  - Engine 解析 5.3 MB 要 1.3～1.4 s，其中約 1.1 s 來自 nlohmann「回呼式解析」的一個二次方行為：每結束一個物件，就線性掃一次它所在的陣列
    （合成圖的 20,691 個碰撞盒物件放在同一個陣列）。成本跟「同一陣列裡的物件數」的平方走，**不是每位元組或每個值的常數**，所以「4 MB/s」不能外推到別的地圖。
    換掉回呼（契約不變）可降到約 0.3 s。
  - Match 的線性碰撞掃描和盒子數成線性：合成圖（2 萬盒）上一位玩家站在地形上走一步約 2.2 ms，4 人約用掉 tick 的一半，加上 AI 會超過。
- **參照**：v0.1 的骨架（16³ 區塊、region 檔、索引、作者配置與執行期分離、broadphase、Engine 擁有中立的標記格式、產品擁有意義、greedy 在工具）
  都有成熟方案的對應。和 proposal 的記憶不符、會影響契約寫法的有兩件（第 3 節標「大」）：
  1. **broadphase 的決定性**：Jolt 明寫它的 broadphase 查詢不是決定性的，原因是多執行緒修改與動態樹的邊界放寬，這對 GYO「單執行緒、靜態、不變」的格子不成立。
     要帶走的是：**查詢結果的順序與平手要寫進契約，並以暴力線性掃描比對**。這個寫法沒有現成來源；Jolt 的「以精確邊界再測、一致排序」與它的線性掃描「參考實作」是最近的來源。
  2. **塔科夫「伺服器全圖模擬 AI」沒有根據**：官方沒有說明；社群擷取的設定裡有「啟動距離／睡眠距離」欄位（約 300 m），但它的語意只是從欄位名推的。
     遠處實體的處理在業界三家三樣（Minecraft 不處理、Luanti 存成靜態資料、Veloren 刪除後交給粗模擬），沒有一家宣稱決定性。
- **規模**（社群資料，非官方）：塔科夫每圖玩家 9～16、AI 上限 19～48、地圖影像邊界 0.3～1.9 km²（含不可玩區）。GYO 的 4 人房、約 1 km² 在這個範圍內。
- **之後的批次會帶的 Architecture Delta**（本批不做，contract 批要寫 AGENTS §3 的 7 項）：`GYO::VoxelMesh` v2 的公開介面與 v1 退役、新 Subsystem `GYO::MapMarkers`、
  `GYO::Collision` 的 broadphase、工具 DI 2／DI 3／DI 4 的改寫與守衛變更（§14.4 要使用者核准）、產品 arena v3。

## 1. 方法與來源

- **調查者**：主會話 Opus 5.5／high（開工時是 Fable 5.1，之後由使用者切成計畫指定的 Opus 5.5）。
  九項參照由三個子 Agent（general-purpose、Opus 5.5、high）讀主要來源，只回事實與來源；**判定（下表「當てはめ」與「判定」欄）由調查者做**。
- **來源的性質**（每列的「根據」都標出）：原始碼（釘選 commit）＞官方文件＞開發者部落格＞社群 wiki＞媒體。
  - Luanti `b4c73357a6159e48810dea24428f3ebaa24dd345`、Veloren `7daa95fd53ae62aa185d31ad316d0cabfe2d70ac`、Godot `8cc3a574e26e3640227861aef8266f2fd4fbb650`、
    Jolt `0f69a6b8f0bd562f23a29b27a665d2dab1524917`、Bullet3 `d3afe76132fc76b0f9b2359cbaef47a3766e7fbb`（GitHub／GitLab master，2026-11-06 讀）。下表的行號只對這些 commit 有效。
  - Minecraft 多數事實只有 minecraft.wiki（社群 wiki）；Mojang 一手來源只找到 21w38a 的 snapshot 文章。
  - UE5 只有 dev.epicgames.com 文件（5.8）；原始碼本帳號讀不到。
  - 塔科夫沒有官方技術資料；用的是 Fandom（自稱官方，有爭議）、SPT（社群離線伺服器，資料擷取自正式後端）、tarkov.dev（社群工具）與媒體轉述的 patch notes。
  - Valve 開發者 wiki 有 bot 檢查，沒有繞過；Source 的部分改讀 Valve 在 GitHub 的原始碼與 FGD。
- **證據**：三份子 Agent 報告（含每項的 URL 與行號）存在主 checkout `build/evidence/big-map/survey/sources/R1～R3-*.md`（git 不追蹤，本機 MacBook Pro）。
  子 Agent 的網頁暫存在會話暫存區，未保存，不作證據。

## 2. 九項參照

判定欄：**採る**（照用）、**改装**（用其想法，改寫成符合 G1～G10 的形狀）、**採らない**（現在不用；寫成第 6 節的 Replacement Criteria 候選）。
G1～G10 見 [proposal §2](proposal-v0.1.zh-Hant.md)。

### 2.1 Minecraft（Java Edition）

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| 16×16 的 chunk 柱、全高；柱內切 16³ 的 section；整柱一起載入 | **一手理由未找到**。wiki 只說 section 用於顯示與部分機制 | 驗證與重烘的單位＝16³ 區塊，和工具的 `gyo.voxel_grid` `ChunkSize = 16` 一致（工具 owner）。GYO 不需要「柱」：v0.1 是 3D 區塊，空區塊省略 | 採る（16³）；改装（不用柱） | (wiki) https://minecraft.wiki/w/Chunk |
| region 檔：32×32 chunk 一檔、4 KiB sector、位置表＋timestamp；取代 Alpha 的一 chunk 一檔 | 一檔一 chunk 的檔案數太多（取代 Alpha 格式的歷史）；32 的理由未找到 | 「檔案單位≠編輯單位」：region 是檔案單位（Engine 格式）。GYO 用 JSON 文字（G10），不用 sector 二進位 | 改装 | (wiki) https://minecraft.wiki/w/Region_file_format |
| 每 section 一個 palette＋packed 索引（最少 4 bit）；網路協議超過 8 bit 改用全域 id | 壓縮：全石頭的 section 不必每格 15 bit | v0.1 是全圖共用 palette（≤256），四邊形帶 palette 索引；文字格式不靠 bit packing 省空間 | 採らない（RC 候選：每區塊 palette） | (wiki) https://minecraft.wiki/w/Chunk_format 、https://minecraft.wiki/w/Java_Edition_protocol/Chunk_format |
| view-distance 與 simulation-distance 分開（預設 10、21w38a 加入）；ticket／load level；模擬距離外的實體「不處理但仍可存取」，等區塊等級回升後再處理（wiki 對恢復條件的描述不一致） | Mojang：讓視距可以大、CPU 負擔小 | 「Client 收什麼」與「Match 模擬什麼」分開，GYO 已是這樣（地形是資產、實體 interest 100 m 在 Match）。模擬範圍是玩法決定（第 4.1 節） | 改装（若採用，載入集合要由權威位置決定性導出，G4） | https://www.minecraft.net/en-us/article/minecraft-snapshot-21w38a ；(wiki) https://minecraft.wiki/w/Server.properties |
| 1.17 起實體存在另一組 region 檔 | 未說明 | 靜態幾何與實體分開存。GYO 的標記層（markers）與幾何（region）分檔是同一方向 | 採る（分檔） | (wiki) Chunk_format 的 History |

### 2.2 Luanti（原 Minetest）

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| MapBlock 16³（`MAP_BLOCKSIZE 16`）；每個 block 自帶 name→id 對照 | 16 的理由未找到 | 同 2.1 第一列 | 採る（16³） | `src/constants.h:64`、`doc/world_format.md:262-266` |
| 地圖存在資料庫，以 block 座標為鍵（SQLite；5.12 起新庫主鍵是 `(x,z,y)`，舊的整數鍵仍可讀） | 原本一 block 一檔，「檔案系統撐不住」；鍵的順序決定哪種範圍查詢快 | 「以座標為鍵、有全序」採用：region 清單依 `at` 遞增、區塊依座標遞增（G3）。資料庫不採：資產要能手寫與 diff（G10）、資產目錄與守衛以檔為單位 | 改装 | `doc/world_format.md:269-293`、`src/database/database-sqlite3.cpp:225-264` |
| 伺服器只讓玩家周圍 `active_block_range`（預設 4 block）的 block 活動，每 2 s 重算；離開的物件存成靜態資料，回來時以經過時間補算 | 物件與 ABM 只在活動區塊跑 | 「遠處凍結＋回來補算」是模擬模型的一個選項（4.1）。**Luanti 沒有決定性**：ABM 順序每輪隨機打亂。照搬不行 | 採らない（現在）；改装候選 | `builtin/settingtypes.txt:1210-1211, 2309-2341`、`src/serverenvironment.cpp:85-150, 879-963, 1009, 1640-1780` |
| 碰撞遇到未載入的 block：整列當實心；周圍全都沒載入時速度歸零 | 沒有載入就不知道節點的碰撞形狀 | v0.1 Match 整張載入，不存在「未載入」。若之後做活動範圍，規則要先寫明「判定碰到未載入區塊」怎麼辦（或證明不可能） | 採らない（現在）；寫進 RC 候選 | `src/collision.cpp:250-262, 294-298, 483-491` |
| Client 自己把 MapBlock 變成 mesh（多執行緒），`client_mesh_chunk` 預設把 2³ 個 block 合成一個 mesh 以減少 draw | 減少 draw call | GYO 的烘焙（剔面、合併）在工具，Engine 只展開（G1、G6）；Client 不建 mesh。「多個區塊合成一個 draw」採用：v0.1 以 region 為渲染單位 | 改装 | `src/client/mesh_generator_thread.cpp:289-348`、`builtin/settingtypes.txt:2063-2069` |

### 2.3 Veloren

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| 地形與實體同步走兩條 stream；地形視距與實體視距分開 | 未說明 | GYO 已是「地形是資產、實體是 wire」；大地圖仍不上 wire（產品） | 採る（維持現狀） | `client/src/lib.rs:321-322, 540-541` |
| Client 依視距一圈圈拉 chunk（最多 12 個待處理、每 tick 2 個）；伺服器每位玩家固定載入半徑 6 的螺旋 | 防「改視距重載」作弊；保持玩家附近的實體模擬 | Client 的背景串流不在 v0.1（Client 全部 region 載入後才 ready）。記為 RC 候選 | 採らない（現在） | `client/src/lib.rs:2429-2530`、`server/src/sys/msg/terrain.rs:83-143` |
| 實體訂閱以 2D region（512 block）為單位，換 chunk 時才更新，有 tether 防抖 | 決定哪些實體要同步給哪個 Client | GYO 的 interest 是 Match 的純函數、依距離（entity-model §4.2），不需要 region 訂閱 | 採らない | `common/src/region.rs:48-74`、`server/src/sys/subscription.rs:21-160` |
| 玩家物理**預設 Client 權威**，伺服器檢查速度與位置後接受；可選或強制伺服器權威 | 容許延遲 | GYO 是伺服器權威（E-8），不採 | 採らない | `server/src/sys/msg/in_game.rs:141-159, 456-543` |
| 所有玩家視距外的 chunk 卸載，裡面的 NPC **刪除**，交給 rtsim 粗模擬 | 「移除所有玩家視距外的 NPC」 | 兩層模擬（近處精細、遠處粗略）是模擬模型的另一個選項（4.1） | 採らない（現在）；RC 候選 | `server/src/lib.rs:979-1040` |

### 2.4 Godot GridMap

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| 格子以 octant 分組（`cell_octant_size` 預設 8，可 1～1024）；編輯一格只把它的 octant 標髒，下一幀只重建髒 octant | 文件：為了有效率的渲染與物理；**8 的理由未找到** | 工具的增量重烘單位＝區塊（16³）。髒區塊＋受影響的鄰接區塊（剔面看鄰格）—Godot 的 octant 各自獨立，沒有跨 octant 剔面，所以 GYO 多一步 | 採る（工具 owner） | `modules/gridmap/grid_map.h:175-176`、`grid_map.cpp:512-600, 773-1007` |
| 每 octant 一個 static body，所有格的碰撞形狀加在上面；每種 item 一個 MultiMesh | 未說明 | 碰撞盒不跨區塊（v0.1 §11.6）和「每 octant 一個 body」同方向。渲染不同：GYO 不用 instancing，烘成面 | 改装 | `grid_map.h:104-132`、`grid_map.cpp:877-888, 941-985` |
| **導航是每格一個 NavigationServer 區域**（proposal 記成「每 octant 烘 navmesh」是錯的）；另可由 NavigationRegion3D 從 GridMap 幾何烘焙 | — | 導航不在本案；格式要留得住區塊鍵，之後由產品或工具從碰撞盒導出 | 採らない（現在） | `grid_map.cpp:892-930, 1924-2080` |
| `make_baked_meshes`：每 octant×材質合成一個 mesh 給光照烘焙；之後任何編輯就丟掉 | 給 LightmapGI | GYO 沒有光照烘焙（Engine RC 2） | 採らない | `grid_map.cpp:1795-1872` |
| 格子座標每軸 int16 | 未說明 | GYO 的上限由 Engine 規則 8（±2²⁰ 單位）決定，0.5 m 時遠大於 km² | 參考 | `grid_map.h:56-86` |

### 2.5 UE5 World Partition

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| 一個持久 level 切成串流格子；actor 自由擺放，依設定被指派到 cell；串流來源預設是 PlayerController。預設 cell 大小與距離沒寫（例子 256 m／768 m） | 取代手動的 sublevel，自動依距離串流 | 「作者怎麼放」與「執行期怎麼載」分開：GYO 的作者資料是工具的格子（`gyo.voxel_grid`），執行期是烘好的 region。已是這個分法 | 採る（維持） | https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine |
| 伺服器串流由 `wp.Runtime.EnableServerStreaming` 控制；UE 5.1 的 KB 文章說開了可以「只串流玩家來源，而不是全部載入」 | 未說明（只有 KB 文章開頭可讀；預設值文件沒寫） | **推論**：不開時伺服器整張載入。只能當 v0.1「Match 整張載入」的弱旁證 | 參考（推論） | https://forums.unrealengine.com/t/knowledge-base-world-partition-server-streaming/688971 |
| HLOD：未載入 cell 的代理（Instancing／Merged Mesh／Simplified Mesh），每層自己的格子 | 遠處內容、減少 draw | 現在不需要（4.6） | 採らない；RC 候選 | https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine |
| One File Per Actor：每個 actor 一檔，**只在編輯器**，cook 後嵌回 level；不限於 WP | 多人同時編輯一個 level | GYO 是單人編輯器。region 各自一檔已給 diff 與存檔的粒度 | 採らない | https://dev.epicgames.com/documentation/en-us/unreal-engine/one-file-per-actor-in-unreal-engine |
| Data Layers：編輯器層與執行期層（Loaded／Activated），多人遊戲只有伺服器能啟用；用途是任務、進度、季節變化 | 同一張地圖的內容變化 | 現在沒有 Feature 點名「同圖不同內容」 | 採らない；RC 候選 | https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---data-layers-in-unreal-engine |

### 2.6 物理引擎的靜態 broadphase（Jolt、Bullet）

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| Jolt：四叉 AABB 樹（每節點 4 子），每 broadphase 層一棵；建議靜態一層、動態一層；大量加入要批次或之後最佳化 | 4 子＝一個 SIMD 指令測 4 盒；靜態物體很少更新、更新又貴 | GYO 只有靜態盒（地形），不需要動態樹。v0.1 選均勻格 | 改装 | Jolt `Docs/Architecture.md:579-595, 35-42` |
| **Jolt 明寫 broadphase 查詢不是決定性的**；原因是多執行緒修改、移動中的物體讓樹的邊界放寬到下次維護為止。建議在收集結果時以精確邊界再測、並一致排序；有一個線性掃描的「參考實作」可換用 | 多執行緒的動態 broadphase | 這個原因對 GYO「單執行緒、靜態、建好後不變」的格子不成立；要帶走的是「結果的順序與平手要定義」。GYO 的契約「每個查詢的結果＝對全部盒子依索引順序線性掃描的結果」**沒有現成來源**，是把 Jolt 的建議與參考實作寫成契約，守衛是與線性掃描逐一比對（Engine `GYO::Collision`）。**平手規則要依查詢分別寫**：現在的 Movement 取先到的（`<`），ShotQuery 對牆取後到的（`<=`）；後者對結果沒有影響（牆的命中只帶距離），但契約要寫明 | 改装 | Jolt `Docs/Architecture.md:787-812`、`BroadPhaseBruteForce.h:12` |
| Jolt 在求解前把 contact 與 constraint 排序（雜湊鍵，平手用 body id） | 「為了決定性的模擬」 | 同一想法：平手以索引（或 EntityId，entity-model §5.1）決定 | 採る（平手規則） | Jolt `ContactConstraintManager.cpp:1487-1505` |
| Bullet：動態樹（固定／動態兩棵）、三軸 sweep-and-prune（要事先給世界範圍）、O(n²) 的 simple broadphase 只當 unit test 的參考；可選依 id 排序 pair 以求決定性；**CPU 沒有均勻格** | 樹：插入刪除快；SAP：大量物體 | 均勻格 vs 樹：**一手的取捨來源沒找到**（Ericson《Real-Time Collision Detection》第 7 章沒讀）。GYO 的盒子靜態、軸對齊、大小有上限（區塊內合併）、密度大致均勻，均勻格的前提成立；「密度大致均勻」是**假設**：手工地圖是建築密集處加空地。對抗檢查者指出 Ericson 第 7 章（調查者未讀）的做法：格長取最大物體、大小差異大時用分層格（hierarchical grid），比直接換樹便宜 | 改装（均勻格）；RC 候選（分層格，再來才是樹） | Bullet `btDbvtBroadphase.h:59-72, 123`、`btAxisSweep3.h:30-45`、`btSimpleBroadphase.h:39-40`、`btDispatcher.h:50, 65` |

### 2.7 逃離塔科夫（規模參照）

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| 每圖玩家 9～16；AI 上限（BotMax）19～48；每區域 2～8；Boss 可超出上限 | 未說明 | GYO 4 人房；AI 上限的量級 30 左右可作為 Match 每 tick 的設計點（產品） | 採る（規模參照） | Fandom 各圖頁（準官方社群 wiki）；SPT `project/assets/database/locations/*/base.json`（社群擷取） |
| 設定裡有 `DistToActivate`（260～330 m）與 `DistToSleep`（300～350 m）兩個欄位，位於 bot 的位置修正設定中 | 未說明 | **推論**（只從欄位名推）：bot 依與玩家的距離睡眠與喚醒，是「遠處凍結」的實例；決定性未知 | 參考（推論，4.1） | SPT 同上（社群擷取，非官方） |
| 地圖影像邊界 0.3～1.9 km²（含不可玩區）；raid 35～50 分鐘 | — | 「約 1 km²、0.5 m」在範圍內 | 採る（規模參照） | tarkov.dev `src/data/maps.json`（社群工具） |
| 區域、spawn 點、Boss 區、撤離點以**字串名**對應到 Unity 場景物件；spawn 點帶陣營、類別、區域名、半徑 | 未說明 | 標記層的形狀（點＋名字＋鍵值；區域以名字被引用）與 v0.1 §11.3 一致 | 採る（標記層的形狀） | SPT 同上 |
| PvE 單人 raid 在本機跑，組隊與 Streets 用伺服器（2024）；Streets 的伺服器系統調整（2023）；新的 culling（2023） | 排隊時間、rubberbanding、效能 | 塔科夫在大地圖上也遇到伺服器負載與畫面剔除問題；GYO 的 broadphase 與 region 剔除對應這兩點 | 參考 | 媒體轉述的 patch notes（見 R3 報告的 URL） |
| 未確認：AI 是否在線上 raid 的伺服器上跑、進 raid 時是否整圖載入、LOD 設定 | — | — | 未確認 | — |

### 2.8 標記層的業界做法

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| Source：地圖的 entity lump 逐一解析，`classname` 與 keyvalue 都是字串；遊戲 DLL 以字串工廠建立類別、以 datadesc 對應欄位；**未知 classname 只警告並跳過** | 意義在遊戲的類別裡，不在地圖格式裡 | Engine 擁有中立格式（點、盒、路徑、`kind`、`tags`，名字不透明），產品賦義（v0.1 §11.3）。**改装**：Source 對未知名字寬容；GYO 由產品把未知 kind 當載入錯誤（抓錯字，G4 嚴格的精神） | 改装 | Valve `source-sdk-2013` `src/game/server/mapentities.cpp`、`src/game/shared/baseentity_shared.cpp` |
| FGD：遊戲提供給編輯器的定義檔（點類、體積類、鍵的型別與預設）；編輯 FGD 不會產生實體 | 編輯器需要知道遊戲期待什麼 | v0.1 的產品 `marker_kinds.json` 就是 FGD 的角色：編輯器當調色盤讀、不解釋 | 採る | Valve `halflife` `game/dmc/dmc.fgd`；Valve wiki 只看到搜尋摘要 |
| Godot：Marker3D（位置提示）、Area3D（進出偵測）、Path3D（曲線）；groups 是名字，引擎不賦義 | — | 點、體積、路徑三種幾何的分法相同 | 採る（三種幾何） | https://docs.godotengine.org/en/stable/classes/class_marker3d.html 、class_area3d.html、class_path3d.html、tutorials/scripting/groups.html |
| UE：Target Point（生成點、AI 路徑點）；Gameplay Tags 由專案定義在字典，編輯器只接受字典裡的 | 專案決定意義 | 同 FGD | 採る | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-tags-in-unreal-engine |
| Minecraft：structure block 的 data 模式存一個字串，由各結構的程式解釋；生怪規則（距玩家 24～128 格、上限、光照、生態域權重）在程式與資料包 | 意義在程式 | 生怪規則是產品的玩法（不在格式裡）；標記只給位置與名字 | 採る（分工） | (wiki) https://minecraft.wiki/w/Structure_Block 、https://minecraft.wiki/w/Mob_spawning |

共同模式：**引擎擁有格式（字串名、鍵值、幾何），遊戲擁有意義，編輯器的可選種類由遊戲提供。** owner 的判斷見 4.4。

### 2.9 greedy 合併與 LOD

| 機制 | 為什麼 | 對 GYO 的當てはめ（owner） | 判定 | 根據 |
|---|---|---|---|---|
| greedy：每軸切片，以固定字典序反覆取最大矩形；文章寫明以總序取唯一的最小元素，所以結果在固定順序下唯一；不一定最少（≤8 倍最佳） | 線性時間 | 進工具的烘焙（G6）；掃描順序寫進工具 DI 3 的決定性（同一格子→同一位元組）。Engine 規則 6 的 `w`、`h` 已支持 | 採る（工具 owner） | https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/ |
| 縮減依形狀：實心 8³ 384→6、地形 6634→1983、雜訊 2198→1670、球 4770→2100；成本和只剔面相當（65³：92～198 ms 對 82～297 ms） | — | 平坦的地面與牆面縮減大，雜訊幾乎無效。合成圖是雜訊，不能代表手工地圖 | 採る；GYO 數字未量（4.5） | 同上、https://0fps.net/2012/07/07/meshing-minecraft-part-2/ |
| 合併的代價在貼圖（貼圖集要每 tile 重複 UV）、光照、AO：只能合併光照與 AO 相同的面 | — | GYO 每面一色、uv 取 palette 貼圖的 texel 中心（`voxel-mesh.md`）、無光照、無 AO → **這些代價都不存在**。Godot Voxel Tools 的頂點色 mesher 預設開 greedy，是同一情況 | 採る | https://0fps.net/2013/07/09/texture-atlases-wrapping-and-mip-mapping/ 、https://voxel-tools.readthedocs.io/en/latest/api/VoxelMesherCubes/ |
| T-junction：合併後的大面與鄰接小面的頂點不對齊，可能有像素裂縫；0fps 第二篇作者提到但不確定是否真有問題，論壇有爭議 | — | 要在工具批用實際地圖目視確認；不行就只在同一平面內合併到 T 不出現（或加縫的處理） | 未確認（4.5） | 0fps part 2 留言、Urho3D 論壇（R3 報告） |
| LOD：Minecraft 原版沒有（以 chunk 視距控制）；Distant Horizons 為 256＋chunk（4 km 以上）視距另開一個 pass；Luanti 沒有，`viewing_range` 預設 200 node；Voxel Tools 的平滑地形用 Transvoxel | 極遠距離的畫面 | 1 km² 的視距在 Minecraft 的 chunk 視距範圍內（約 64 chunk），業界在這個距離多數不用 LOD | 採らない（現在）；RC 候選（4.6） | https://modrinth.com/mod/distanthorizons 、Luanti `builtin/settingtypes.txt` |

## 3. proposal 第 3 節（憑記憶）的修正

| proposal 的說法 | 來源的事實 | 影響 |
|---|---|---|
| Minecraft「伺服器只在 simulation distance 內跑邏輯」 | 21w38a 起初只管實體；之後方塊實體與排程 tick 也受限。模擬距離外的實體不處理、仍可存取 | 小。凍結模型的實例 |
| Minecraft「palette 壓縮」 | 磁碟永遠用 per-section palette（最少 4 bit）；「超過門檻改全域 id」是**網路協議**的事 | 小 |
| Luanti「SQLite 以區塊座標為鍵」 | 5.12 起新庫主鍵是 `(x,z,y)` 三欄；舊的整數鍵（`getBlockAsInteger`）仍可讀；LevelDB／Redis 仍用整數鍵 | 小 |
| Luanti「伺服器只模擬 active block range」 | 確認；預設 4 block；物件另有送出範圍（預設 8）與視線錐；**ABM 順序每輪隨機打亂，沒有決定性** | 中：凍結模型不能照搬 |
| Luanti「未載入區塊的碰撞」（proposal 只列為問題） | 整列當實心；周圍全無則速度歸零 | 中：若做活動範圍，規則要寫 |
| Veloren「伺服器權威」 | 玩家物理**預設 Client 權威**，伺服器檢查後接受；卸載區的 NPC **刪除**並交給 rtsim | 中：Veloren 不是伺服器權威的例子 |
| Godot「navmesh 也按 octant」 | **每格一個導航區域**；不是每 octant 烘焙 | 小（導航不在本案） |
| Godot「每個 octant 烘一個 mesh＋碰撞」 | 每 octant 一個 static body；渲染是每種 item 一個 MultiMesh；合成 mesh 是另一條給光照烘焙的路，編輯就丟 | 小 |
| UE「One File Per Actor 給多人編輯」 | 確認；但只在編輯器，cook 後嵌回；不限於 WP | 小 |
| 物理 broadphase「疊代順序以區塊鍵排序保決定性」 | Jolt 明寫它的 broadphase 查詢**不是決定性的**（多執行緒、動態樹）；沒有來源寫「結果＝線性掃描」的契約；Bullet 的 CPU 端沒有均勻格 | **大**：v0.1 §11.4 的等價契約是 GYO 自己要立的，要有守衛 |
| 塔科夫「伺服器全圖模擬 AI」 | **未確認**。社群擷取的資料顯示 bot 依距離睡眠（約 300 m）；PvE 單人 raid 在本機跑 | **大**：proposal §6a 以「全圖模擬」為前提推「整張載入」；整張載入（碰撞）仍可成立，但「全圖模擬 AI」沒有塔科夫的根據（睡眠也只是從欄位名推的） |
| 塔科夫「約 1～3 km²」 | 影像邊界 0.3～1.9 km²（含不可玩區）；媒體說約 0.8 km² | 中：上限偏小，1 km² 的設計點足夠 |

## 4. 六個未決事項（proposal §11.9）

每節：業界怎麼做、對 GYO 的建議、理由、不採建議的代價。**決定由使用者做。**

### 4.1 模擬模型：全圖模擬，還是遠處 AI 降頻／凍結

- **業界**：
  - Minecraft：模擬距離外的實體不處理、仍可存取（ticket／load level）。
  - Luanti：離開活動範圍的物件存成靜態資料，回來時以經過時間補算；ABM 順序隨機，**沒有決定性**。
  - Veloren：所有玩家視距外的 NPC 刪除，交給粗略的 rtsim 繼續；玩家物理是 Client 權威。
  - 塔科夫（社群擷取的資料，非官方）：每圖 AI 上限 19～48；有「啟動距離／睡眠距離」（約 300 m）兩個欄位，**依距離睡眠是從欄位名推的**。
  - UE：伺服器串流要開 CVar 才「只串流玩家附近」，**推論**不開時整張載入（KB 文章，預設值沒寫）。
  - **沒有一家宣稱決定性。**
- **建議**：分兩件事決定。
  1. **地形的載入**：Match 整張載入碰撞盒（v0.1 §11.5），沒有「已載入集合」。碰撞盒的記憶體小（10⁶ 盒 × 24 B ≈ 24 MB，推算）；UE 的伺服器很可能預設也是整張載入（推論）。
  2. **AI 的模擬**：第一步全部模擬（4 人＋AI 上限約 30），不做降頻。等 broadphase 落地後，在產品批**量一個 tick 的成本**（玩家＋AI 的移動、射擊），
     超過預算才加「遠處睡眠」。屆時的寫法：睡眠狀態是**元件**（權威、進 digest），每個 tick 開始時依權威位置與固定規則（距最近玩家 > D_sleep 入睡、< D_activate 醒來；
     兩個距離不同，所以需要前一 tick 的狀態，是權威狀態而不是可重建的暫存），依 EntityId 升冪處理（entity-model §5.1、E-8）。
- **理由**：量測顯示線性掃描下一位實體在地形上一步約 2.2 ms（2 萬盒），有 broadphase 後的單步成本**未量**；在量到之前加睡眠，是對沒有觀測到的壓力加權威狀態（AGENTS §2）。
  若塔科夫的欄位確實是睡眠距離（推論），約 300 m 也大於 GYO 的 interest 100 m，在 1 km² 上睡眠能省下的比例有限（未量）。
- **不採的代價**：
  - 現在就做睡眠：多一個權威元件與它的守衛（digest、決定性、補算規則），玩法上「AI 在遠處停住」也要先定義；如果之後量到不需要，就是白做。
  - 永遠全圖模擬：AI 數量往上加時，tick 會超過預算（每隻 AI 的成本和玩家同級）；這時再改是產品批的事，格式不受影響（可逆）。

### 4.2 區塊 16³、region 8×8 的大小

- **業界**（換成公尺比較）：

  | 系統 | 編輯／驗證單位 | 檔案單位 | 渲染單位 |
  |---|---|---|---|
  | Minecraft | 16×16 柱（16 m），section 16³ | region 32×32 chunk（512 m） | section |
  | Luanti | MapBlock 16³（16 m） | 資料庫的一列（一個 block） | 預設 2³ block 合成一個 mesh（32 m） |
  | Veloren | 32×32 柱（32 m），子區塊 32×32×16 | — | chunk |
  | Godot GridMap | octant 8 格（預設格 2 m → 16 m） | 場景檔 | octant 內每種 item 一個 MultiMesh |
  | v0.1（cell 0.5 m） | 區塊 16³（8 m） | region 8×8 柱 × 全高（64 m） | region |

  16 與 32 的**一手理由都沒有找到**（Minecraft、Luanti、Godot 皆然）；Veloren 的 32 有程式上的理由（前端把水平位置壓成每軸 5 bit）。
- **建議**：區塊維持 16³；region 維持 8×8（1 km² 約 256 個檔），`region_size` 寫進索引（v0.1 已如此）。
- **理由**：
  - 16³ 和工具的 `gyo.voxel_grid` 一致（`ChunkSize = 16`），改成 32³ 就是工具格式的變更，而且單次重烘的格數多 8 倍。
  - region 的大小決定「存檔寫多少」與「一個 draw 多大」。以合成圖的比例推算（每四邊形約 34 B、每盒約 55 B），整張 10⁶＋10⁶ 時約 89 MB，
    256 個 region 平均約 0.35 MB；密集處 10 倍也在 Engine 的 16 MiB 上限內。16×16（128 m、64 個檔）時密集處可能接近上限。
  - 渲染單位＝region：1 km² 約 256 個 draw，視錐剔除後更少。Luanti 也是「多個區塊合成一個 mesh」以減少 draw。
- **不採的代價**：區塊 32³：greedy 合併在區塊邊界中斷的次數減少（效果未量），但工具格式要改、重烘變粗。region 更大：檔數少，但存檔與重烘的寫入變大、密集區可能碰到 16 MiB。
  region 更小：檔數變多（資產目錄與守衛的負擔），draw 數變多。
- **範圍的限界**：「密集處 10 倍」是假設；手工地圖的每 region 文字量未量。

### 4.3 JSON＋快解析是否足夠，或必須二進位

- **業界**：Minecraft（NBT＋zlib 的 region 檔）、Luanti（資料庫裡的二進位 blob）、Veloren（二進位）都是二進位；UE 的 cook 產物也是。沒有一家用文字當執行期格式。
- **量測**（第 5 節 (c)，含對抗檢查者的追加量測）：
  - 現在的 1.3～1.4 s，約 1.1 s 是 nlohmann 回呼式解析的**二次方行為**：每結束一個物件，就線性掃一次所在的陣列。合成圖的 20,691 個碰撞盒是同一陣列裡的物件。
    同一份資料把碰撞盒改寫成 6 個整數的陣列，空回呼的解析從 1,328 ms 降到 143 ms。
  - 所以「4 MB/s」**不是解析器的速率**，是這一張圖「同一陣列裡有多少物件」的結果。v0.1 的格式把碰撞盒分到各區塊（每組數百個），現在的解析器在那種檔上就會快很多；
    反過來，一個陣列 4 萬個物件要 4.4 s。
  - 不經回呼：SAX 掃描 71～74 MB/s、建 DOM 36 MB/s、SAX＋重複鍵檢查 69 MB/s；規則檢查約 83～124 ms。
- **建議**：契約只有 JSON 文字一種編碼（G10）。在 engine-map 批把 Engine 內部的解析換成不經回呼的嚴格解析，規則與規則測試、黃金值不變，突變重跑：
  - 最小的改法：先用 SAX 掃一次查重複鍵（77 ms），再用不帶回呼的 `json::parse` 建 DOM（149 ms），規則程式不動。合成圖推算約 0.31～0.35 s（約 15～17 MB/s，**分項相加，端到端未量**）。
  - 並在該批事前宣告載入時間的判定。二進位快取記為 Replacement Criterion，條件寫成量得到的形式（「換掉解析方式之後，最大地圖的載入時間仍超過宣告值」）。
  - 換第三方解析器（simdjson、RapidJSON，對抗檢查者指出其嚴格驗證可達數百 MB/s 以上，調查者未讀其來源）是 Engine 的新依賴，是另一個 Architecture Delta，要使用者決定。
  - region 的平行解析**不能放進 `GYO::VoxelMesh`**（Engine DI 1：只連 Base、Math、JSON）；由呼叫端（產品）以 `GYO::Threads` 平行呼叫，結果依 `at` 排回。
- **理由**：
  - 推算的文字量（10⁶ 四邊形＋10⁶ 盒約 89 MB）÷ 15～35 MB/s ≈ 2.5～6 s（單執行緒、推算）；平行後更少。
  - 二進位要成為第二種編碼，就有兩份讀取程式，「一個驗證者」（G2、DI 3）要靠兩種編碼共用規則實作才能保住；文字的手寫、diff 也會失去。
- **不採的代價**：
  - 先做二進位：多一種編碼與它的守衛（兩種編碼往返一致），工具的存檔路徑也要寫兩種。
  - 保留現在的回呼解析：載入時間取決於每個陣列有多少物件（二次方），格式稍有改變就可能從 0.1 s 變成數秒，不可預測。
- **範圍的限界**：解析速度只量了 macOS／Intel 一台；Windows（Client 主戰場）與 Linux（Match）未量。推算的文字量來自雜訊合成圖的比例。

### 4.4 標記層的 owner 與格式輪廓

- **業界**：Source（字串 classname＋keyvalue，遊戲 DLL 賦義，FGD 由遊戲提供給編輯器，未知名字只警告）、Godot（Marker3D、Area3D、Path3D，groups 是名字）、
  UE（Target Point、Volume，Gameplay Tags 由專案定義字典）、Minecraft（structure block 的字串，由程式解釋）、塔科夫（spawn 點、Boss 區、撤離點都以字串名引用）。
  共同模式：**引擎擁有格式，遊戲擁有意義，編輯器的種類清單由遊戲提供。**
- **建議**：owner＝Engine（新的小模組 `GYO::MapMarkers`，只依賴 Base、Math），格式照 v0.1 §11.3：
  - 三種幾何：點（＋yaw）、軸對齊盒、路徑（點列）；座標用 1/16 格的整數，和 quads 相同。
  - 每個標記：`id`（唯一、依 id 遞增）、`kind`、`tags`（字串鍵值），名字不透明。
  - Engine 只檢查形式（唯一、遞增、有限、長度與字元集）；不知道 spawn、boss 是什麼。
  - 產品提供 `marker_kinds.json`（產品格式，FGD 的角色）：編輯器當調色盤讀；產品載入時把不認得的 kind 當錯誤（比 Source 嚴格，抓錯字）。
- **Architecture Delta**：`GYO::MapMarkers` 是新的 Subsystem（AGENTS §3），engine-map 批要寫 7 項並取得同意。Engine 的文件與範例**不能出現 `spawn`、`boss` 這類玩法名詞**
  （Layers 檢查 1）；proposal §11.3 的範例要換成中立的名字（例如 `kind: "a"`），玩法名詞只出現在產品的 `marker_kinds.json`。
- **理由**：`docs/architecture.md` 的 Layers 表——它描述**一個資產**（一張地圖的標記），不引用別的資產 id，沒有產品意義 → Engine 欄；
  framework 擁有的格式是「多個 Engine 資產怎麼組合」，標記不是。先例是 `gyo.anim_events`（Engine 解析、名字不透明、產品賦義）與 `gyo.voxel_mesh`（工具寫、遊戲讀，格式在 Engine）。
- **不採的代價**：
  - 產品擁有格式：voxel 編輯器（另一個產品）要讀寫 object_fps_pvp 的格式，編輯器就知道了產品（工具 DI 1「不連結遊戲、不點名產品」）。
  - framework 擁有：違反 Layers 表的 framework 欄（不是組合）。
  - 不做標記層、全部放在 arena 檔：編輯器不能放置，產品的 arena 檔變成手寫座標表。
- **範圍的限界**：路徑之外的形狀（多邊形區域、球體——塔科夫的 spawn 點帶半徑）沒有討論；需要時是格式的新版本。

### 4.5 greedy 合併與碰撞盒「區塊內合併」的代價

- **業界**：greedy 的縮減依形狀而定（實心方塊 384→6、地形 6634→1983、雜訊 2198→1670），時間和只剔面相當；代價在貼圖集、光照、AO，GYO 都沒有。
  Godot 的頂點色 mesher 預設開 greedy。T-junction 的裂縫有爭議，沒有一手的結論。Godot GridMap 每 octant 一個 body，碰撞也是以分組為界。
- **量測**：**未量**。合成圖是雜訊（隨機填、方向亂），greedy 幾乎無效，不能拿來估；碰撞盒「區塊內合併」比「整張合併」多多少盒子也未量。
- **建議**：
  - greedy 進工具的烘焙，掃描順序固定，寫進工具 DI 3（同一格子→同一位元組），Engine 不變（規則 6 已支持 `w`×`h`）。
    **但工具的 Design Invariants 守衛會變**：greedy 改變烘出的位元組與四邊形清單，DI 3 的黃金值與 DI 4（無損：面與盒的並集不變）的守衛都要改寫。
    這是 Design Invariants 守衛的判定變更，照 AGENTS §14.4 要附 1～5 並取得使用者核准（tool-chunked 批）。
  - 碰撞盒改為區塊內合併（盒子不跨區塊），換得局部重烘。
  - 在 tool-chunked 批的事前宣告中量：以 `pvp_corners` 與一張「像建築」的合成圖（平面牆與地面為主），記錄剔面後與 greedy 後的四邊形數、
    整張合併與區塊內合併的盒子數、每個區塊的重烘時間；並在預覽目視 T-junction。
- **理由**：0.5 m、1 km² 的地面只剔面就有約 400 萬四邊形（proposal §7 的推算），greedy 是畫面與檔案大小的前提；它的成本在工具，不在執行期。
- **不採的代價**：不做 greedy：檔案與頂點數大一到兩個數量級（推算）。碰撞盒跨區塊合併：一個區塊的編輯要重算跨區塊的盒子，增量重烘失去局部性。

### 4.6 LOD／遠處代理在 1 km² 視距下是否已經需要

- **業界**：Minecraft 原版沒有 LOD（以 chunk 視距控制，最大 32 chunk）；Distant Horizons 是為 256 chunk 以上（約 4 km 以上）另開一個 pass；Luanti 沒有，預設視距 200 node；
  UE 的 HLOD 給大型開放世界；塔科夫用 Unity 的 LOD（未確認細節），2023 年還在改 culling。
- **量測**：雜訊合成圖 497k 頂點，只畫不重建 p95 4.6 ms（預覽路徑、含讀回）。km² 地圖的頂點數與 Client 的畫面時間**未量**。
- **建議**：v0.1 不做 LOD。Client 以 region 為單位做視錐與距離剔除。在 product-arena-v3 批量 Client 的畫面時間（km² 大小、像建築的合成圖、最大視距、全部 region 可見的最壞角度），
  RC 寫成「最壞角度的畫面 p95 超過宣告值」→ 由工具烘每個 region 的低精度代理（UE 的 HLOD 想法），以另一個資產加入，不改 region 格式。
- **理由**：Minecraft 原版的視距上限 32 chunk 是**半徑**（約 512 m，看得到的範圍直徑約 1 km）；1 km² 的地圖從中央到角約 0.7 km，從一角到對角約 1.4 km。
  所以大部分位置的視距在 Minecraft 原版「不用 LOD」的範圍內，只有角對角超出；遠低於 Distant Horizons 針對的 256 chunk 以上。greedy 之後的頂點數未知，現在做 LOD 是對預測的壓力（AGENTS §2）。
  region 鍵已能讓代理以另一個資產掛上，延後是可逆的。
- **不採的代價**：現在就做：工具多一種烘焙與它的決定性守衛、Client 多一套切換。延後而之後需要：多一個批次，但格式不必改。

## 5. 量測（`v8-arch-big-map-survey-measure`）

```text
件數の突き合わせ：一致（(a) 要求 600 幀／完成 600／失敗 0；(b) 9 情境 × 要求 2,000 次／完成 2,000；(c) 第一輪 5 項 × 10 次、第二輪 7 項 × 10 次，全部完成、ParseVoxelMesh 20／20 成功；對抗檢查者另行核對 .txt／.csv 一致）
```

- **性質**：探索與描述，不是受け入れ判定；沒有門檻。方法在量之前寫進 [HANDOFF](HANDOFF.md)；(c) 看到第一輪後追加了兩項，第一輪保留。
- **機器**：本機 MacBook Pro，Intel Core i9-9980HK（8 核 16 執行緒）、64 GB、macOS 26.7.1；GPU 有 Intel UHD 630 與 AMD Radeon Pro 5500M 兩顆，
  SDL GPU（Metal）自選，**沒有記錄用的是哪一顆**。只有這一台；Windows、Linux 未量。
- **程式**：PR #158 分支（`claude/v8-tool-voxel-map-window`，`690604f7`）工作樹既有建置樹（RelWithDebInfo、`-O2`、`-ffp-contract=off`）的靜態庫；
  量測程式另行編譯連結，不改 repo 的程式。
- **證據**：主 checkout `build/evidence/big-map/survey/`（git 不追蹤）：`build.sh`、`draw_only.cpp`、`match_tick.cpp`、`gen_parse.cpp`（第一輪 `gen_parse_round1.cpp`）、
  各項的 `*.txt`（摘要）與 `*.csv`（每次的值）、合成圖產物 `synthetic.voxel_mesh.json`（sha256 `1557bb3b…162a8b`）。
- **合成圖**：explore-p1 的同一張（64×16×64 格、填 30%、四種方塊 24 個方向、固定種子）：124,236 四邊形、20,691 碰撞盒、5,334,270 位元組。
  它是**最壞情況的雜訊圖**（隨機填、方向亂），greedy 合併幾乎無效，不能代表手工地圖的面數。

### (a) framework OffscreenPreview 只畫不重建

合成圖 build 一次、上傳一次；之後每幀只 `Render`（1280×720，含 GPU 讀回）＋上傳成 SDL 貼圖＋Present。600 幀。

| 項目 | 值 |
|---|---|
| 頂點／索引 | 496,944／745,416 |
| 一次性：build＋Engine 讀回（`PreviewGeometry`） | 2,772 ms |
| 一次性：上傳 mesh＋調色盤（`Show`） | 26 ms |
| 每幀 p50／p95／p99／max | 3.7／4.6／6.6／13.8 ms |
| 其中 Render＋讀回 p50／p95 | 3.4／4.4 ms |
| 其中上傳貼圖 p50／p95 | 0.26／0.35 ms |

**解讀**：explore-p1 每幀約 2.7 s，幾乎全部是「每次編輯整張 build＋兩次文字往返」，畫面本身是 ms 級。
所以 window 批 P1 的問題不在 GPU，在編輯迴圈的重建與文字往返（proposal §5 第 5 點「綁在一幀裡量的是錯的東西」有了數字）。
**範圍的限界**：預覽路徑含 GPU 讀回，和遊戲 Client 直接畫到 swapchain 不同；Client 的畫面時間未量。km² 地圖的頂點數（⚠ 10⁶ 四邊形＝4×10⁶ 頂點）下的畫面時間未量，不能從本表線性外推。

### (b) Match 一 tick 的線性掃描

碰撞盒取自合成圖的 Engine 展開，放進產品 `Arena.walls`（不經 `Validate`）。各情境 2,000 次，單位 µs。

| 盒子數 | 一步（站地面）p50／p95 | 一步（空中）p50／p95 | 一槍（水平，4 人）p50／p95 |
|---|---|---|---|
| 20,691 | 1,204／1,567 | 2,282／2,782 | 290／436 |
| 103,455（×5） | 6,235／7,150 | 11,603／13,073 | 1,524／1,867 |
| 517,275（×25） | 31,810／35,167 | 58,585／63,581 | 7,977／9,015 |

每盒成本幾乎固定（一步地面約 60 ns、空中約 112 ns、一槍約 15 ns），和盒子數成線性。

**情境的偏差（對抗檢查者指出並追加量測，調查者重讀證據後採用）**：
- 「站地面」的玩家在 `y = 0`、而且在地圖外（z ≈ −8 m），沒有任何接觸：`IsCharacterSupported` 提前返回，只剩 `MoveCharacterBody` 各掃一次。這是**最便宜的路徑**，不是典型。
- 追加量測（`adversary/match_tick_realistic.txt`，同一機器，各 2,000 次）：站在圖內方塊頂（y = 16 m）走一步 p50 2,226 µs、站著不動 1,816 µs、在 y = 0 推向地圖邊緣 1,783 µs。
  有牆接觸時 `MoveCharacterBody` 最多 8＋5 輪掃描，會更貴。
- ×5、×25 的複本都在玩家遠處，每個盒子都是 miss，所以「線性」是必然的下界。

**解讀**（tick 預算 1/60 s＝16.7 ms，`Movement.hpp` 的 `AuthorityTickRate = 60`）：

| 情境 | 每 tick 的移動成本（推算＝人數 × 一步 p50） | 佔 tick 預算 |
|---|---|---|
| 20,691 盒、4 位玩家都在地形上（一步約 2.2 ms） | 約 8.9 ms | 約 53 % |
| 同上，最便宜的特例（y = 0、無接觸，一步 1.2 ms） | 4.8 ms | 29 % |
| 20,691 盒、4 位玩家＋20 隻 AI（若 AI 走同一套移動） | 約 29～53 ms | 超過 |
| 103,455 盒、4 位玩家 | 25～46 ms | 超過 |

所以 **broadphase 不是大地圖才需要**：合成圖的規模＋PvPvE 的 AI 就已經超過 tick。proposal §11.4 的「必要」由量測支持。
現在的上限 `walls ≤ 1024` 下一步約 60～115 µs，這是現狀沒有問題的原因。

### (c) Engine `ParseVoxelMesh` 讀 5.3 MB 的拆解

各 10 次的中位數（第一輪／第二輪）。

| 步驟 | 中位數 ms | MB/s |
|---|---|---|
| nlohmann SAX 掃描（不建 DOM） | 72／75 | 74／71 |
| SAX 掃描＋每物件鍵集合（重複鍵檢查，不建 DOM）〔第二輪追加〕 | —／77 | —／69 |
| `json::parse` 建 DOM（無回呼） | 149／149 | 36／36 |
| `json::parse`＋空回呼（只回 true）〔第二輪追加〕 | —／1,268 | —／4.2 |
| `json::parse`＋與 Engine 相同的重複鍵回呼 | 1,241／1,278 | 4.3／4.2 |
| `ParseVoxelMesh` 全部 | 1,324／1,403 | 4.0／3.8 |
| `ExpandVoxelMesh` | 5／5 | — |

- **規則檢查**≈`ParseVoxelMesh` −（DOM＋重複鍵回呼）＝ 83～124 ms。
- **歸因（對抗檢查後更正）**：調查者原本寫「每個值都建暫時物件、經 `std::function` 呼叫，百萬個數值各付一次」。這和本表的數據分不出「每值」還是「每物件」。
  對抗檢查者的追加量測（`adversary/nl_scaling.txt`，調查者重跑可重現）：
  - 7 個整數的陣列 2 萬～16 萬個：空回呼與無回呼的比值 1.0～1.1（**每值的成本可以忽略**）。
  - 物件 5 千／1 萬／2 萬／4 萬個放在同一陣列：空回呼 76／287／1,112／4,380 ms（數量翻倍、時間四倍，**二次方**）。
  - 同樣 4 萬個物件，每 1,000 個分一個陣列：175 ms。
  - 合成圖的碰撞盒改寫成 6 個整數的陣列（數字不變）：空回呼 1,328 → 143 ms，無回呼 134 ms。
  - 原因在 nlohmann 3.12.0 `include/nlohmann/detail/input/json_sax.hpp` 的 `json_sax_dom_callback_parser::end_object`（`:592` 起）：
    每結束一個物件，就從頭掃一次父容器找被丟棄的元素（`:628` 的「remove discarded value」迴圈，沒有「有沒有丟棄」的前置判斷）。
- 所以 proposal §5 第 6 點、§11.7 的「約 4 MB/s ⚠」**量到的值是 3.8～4.3 MB/s，但它不是速率**，不能用來外推別的地圖的載入時間。重複鍵檢查本身（SAX＋鍵集合）只多 2 ms。
- **契約不變、只換 Engine 內部解析方式**的空間（推算＝量到的分項相加，不是端到端量測）：
  SAX 重複鍵檢查 77 ms＋建 DOM 149 ms＋規則 83～124 ms ≈ 0.31～0.35 s（約 15～17 MB/s，約 4 倍）；
  若規則直接在 SAX 事件上檢查、不建 DOM，下限約是 SAX 掃描的 75 ms＋規則（約 25～35 MB/s）。端到端未量。
- **文字量的推算**（由合成圖的實際比例）：檔內每個四邊形約 33.8 B、每個碰撞盒約 54.8 B（縮排一層）。
  10⁶ 四邊形＋10⁶ 盒 ≈ 89 MB；10⁵＋10⁵ ≈ 9 MB。proposal §11.7 的「100～500 MB ⚠」需要每種 10⁶ 以上才會到；仍是未量的估算。

## 6. Replacement Criteria 的候選（「採らない」的項目）

contract 批（engine-map、tool-chunked、product-arena-v3）寫正式的 Design Invariants 與 Replacement Criteria 時，從這裡挑。條件都寫成量得到或觀察得到的形式。

| 候選 | 屬於 | 條件 | 來源 |
|---|---|---|---|
| 每區塊的 palette | Engine VoxelMesh v2 | 全圖 palette 超過上限，或量到 palette 索引佔 region 文字量的主要部分 | Minecraft（2.1） |
| 二進位快取（第二種編碼） | Engine VoxelMesh v2 | 換掉解析方式之後，最大地圖的載入時間仍超過宣告值 | 4.3 |
| 活動範圍（Match 只載入或只模擬玩家附近） | 產品 | 量到 Match 的載入時間或記憶體超過宣告值；或 4.1 的睡眠也不夠 | Minecraft、Luanti、Veloren（2.1～2.3） |
| 未載入區塊的碰撞規則 | Engine Collision＋產品 | 採用活動範圍時（先寫規則或證明不可能碰到） | Luanti（2.2） |
| 遠處 AI 的睡眠或粗模擬 | 產品 | product 批量到的 tick 成本（玩家＋AI）超過預算 | 塔科夫、Luanti、Veloren（4.1） |
| Client 背景串流 | 產品 | Client 的載入時間超過宣告值，或記憶體不夠放全部 region | Veloren（2.3） |
| 均勻格換成分層格，再來才是樹 | Engine Collision | 量到格子的桶極不均勻（少數桶裝大多數盒子），或盒子大小差很多 | Jolt、Bullet、Ericson 第 7 章（2.6，最後一項調查者未讀） |
| 動態物體的 broadphase | Engine Collision | 出現會移動的碰撞體（門、可破壞物） | Jolt（2.6） |
| region 代理（HLOD） | 工具＋產品 | 最壞角度的 Client 畫面 p95 超過宣告值 | UE、Distant Horizons（4.6） |
| Data Layers（同圖不同內容） | 未定 | 有 Feature 點名「同一張地圖依任務或狀態改變內容」 | UE（2.5） |
| 導航資料 | 未定 | AI 尋路的 Feature 開始 | Godot（2.4） |
| 標記的新形狀（多邊形、球） | Engine MapMarkers | 產品需要盒子與路徑表達不了的區域 | 塔科夫（4.4） |

## 7. 對抗檢查（`v8-arch-big-map-survey-adversary`）

- **檢查者**：Fable 5.1／xhigh（general-purpose 子 Agent，唯讀；使用者同意用比主會話強的模型）。和調查者（Opus 5.5）不同模型。
- **讀了**：本文、proposal、三份來源報告、量測程式與結果、AGENTS、voxel-mesh 契約、Layers 表、entity-model、工具架構、產品碰撞程式；
  自己重讀 Jolt、Luanti、Godot、Veloren 的釘選原始碼與 SPT 的四張圖設定；重跑並追加量測。
- **證據**：主 checkout `build/evidence/big-map/survey/adversary/`（`README.txt` 列出每個檔；`nl_scaling.*`、`match_tick_realistic.*`、重讀的來源副本）。

### 7.1 判定與處置

| 項目 | 檢查者判定 | 處置 |
|---|---|---|
| §2.1、§2.2、§2.3、§2.4、§2.8、§4.2、§4.6 | 成立 | 不改；§4.6 的 Minecraft 比較（半徑對直徑）依指正改寫，結論不變 |
| §2.1 Minecraft「暫停、回來恢復」 | 弱い：wiki 原文是「不處理但可存取」，恢復條件自相矛盾 | 改成原文的意思 |
| §2.5 UE 伺服器「預設整張載入」 | 弱い：預設值文件沒寫，§4.1 卻寫成事實 | §2.5、§4.1 改標「推論」 |
| §2.6 Jolt 非決定性 → 自立等價契約 | 弱い：Jolt 的原因（多執行緒、動態樹）對靜態格不成立；平手規則 Movement 取先、ShotQuery 取後 | §0、§2.6 改寫：帶走的是「順序與平手寫進契約、與暴力掃描比對」；平手依查詢分別寫 |
| §2.6 均勻格 | 弱い：「密度均勻」是假設；分層格沒列 | 標為假設；RC 候選加分層格（Ericson 第 7 章，調查者未讀） |
| §2.7 塔科夫睡眠 | 弱い：數字屬實，但「依距離睡眠」只從欄位名推 | §0、§2.7、§3、§4.1 改標「推論」 |
| §2.9 greedy 的措辭 | 成立，措辭要改（唯一性是文章寫的；T-junction 作者有談） | 改措辭 |
| §4.1 先全圖模擬 | 成立（理由裡的兩個推論要標） | 已標 |
| §4.3 JSON＋換解析 | **數字崩れる，方向成立**：4 MB/s 不是速率，22 s 外推無效 | §0、§4.3、§5(c) 重寫；刪除 22 s；加「SAX 查重複鍵＋普通解析」的最小改法 |
| §4.4 標記 owner＝Engine | 成立，有兩個缺口：新 Subsystem 的 Delta 沒標；Engine 範例不能出現玩法名詞 | §4.4 補上兩點；§0 列出之後批次的 Delta |
| §4.5 greedy 進工具 | 成立，但隱藏 Delta：工具 DI 3 黃金值與 DI 4 守衛要改（§14.4） | §4.5 補上 |
| §5(a) | 成立 | 不改 |
| §5(b) 區間 | 「站地面」是最便宜的路徑；地形上 4 人約 53 % | §5(b) 改寫，加追加量測 |
| §5(c) 歸因 | **最嚴重**：1.1 s 是同一陣列內物件數的二次方，不是每值成本 | §5(c) 重寫；調查者核對了 nlohmann 原始碼並重跑追加量測，可重現 |
| 算術、件數の突き合わせ | 全部重算無誤；與 .txt／.csv 一致 | — |

### 7.2 沒讀的來源（檢查者指出）

- nlohmann 回呼解析器的 `end_object`：已讀並寫進 §5(c)。
- Ericson《Real-Time Collision Detection》§7.1（格長）、§7.2（分層格）：調查者**未讀**；只在 §2.6 與 §6 記為候選。
- simdjson、RapidJSON 的嚴格解析速度：調查者**未讀**；§4.3 記為「換依賴是另一個 Delta」。
- Vintage Story（伺服器 chunk 32 的立方）、Teardown 作者的新引擎（8³ 區塊）：§4.2 的兩個資料點，仍沒有一手的尺寸理由；調查者**未讀**。
- 塔科夫的 AI 在伺服器還是 Client：仍沒有官方說法。

### 7.3 內部一致性

- 檢查者確認**沒有替使用者決定**的地方，六項都是建議。
- 摘要與本文的不一致（「三處不符」對上第 3 節的大／中分級、推論被寫成事實、隱藏的 Delta）已依上表修正。


## 8. 範圍的限界與未確認

- 量測只有一台（macOS、Intel、雙 GPU 未記錄用哪一顆）；Windows（Client）與 Linux（Match）未量。
- 合成圖是雜訊，不代表手工地圖；greedy、碰撞盒區塊內合併、Client 的畫面時間、broadphase 後的單步成本都**未量**。
- 推算（不是量測）：文字量 89 MB（10⁶＋10⁶）、換掉解析後 15～35 MB/s、碰撞盒記憶體 24 MB、每 tick 的移動成本（人數 × 一步）。
- 推論（不是來源明寫）：UE 伺服器預設整張載入、塔科夫依距離睡眠。
- 未確認或未讀的來源：Minecraft 16／32 的一手理由、Luanti 與 Godot 的尺寸理由、塔科夫的 AI 在伺服器還是客戶端與是否整圖載入、UE 的 cell 指派時機與伺服器串流全文、
  Ericson 第 7 章、simdjson／RapidJSON、Valve wiki 全文、Teardown 與 Vintage Story。
- proposal 標 ⚠ 而本批沒量的數字（palette 上限、建築四邊形數、碰撞盒數、draw 數、km² 的文字總量）仍是估算。
