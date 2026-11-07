# 場景線（scene）：需求紀錄，未開工

更新：2026-11-07（架構師會話，討論後記錄）。**狀態：需求紀錄。使用者 2026-11-07：「這些需求先紀錄，目前要做的是把地圖處理好。」** 開工時先做 survey（範圍在下面），不直接定契約。
相關決定：[big-map HANDOFF](../big-map/HANDOFF.md) 的 `v8-arch-map-scene-split`；協議側的需求在 [pv10-requirements](../../../object_fps_pvp/plans/v8/pv10-requirements.md)。

## 原則：地圖與場景分開（使用者，2026-11-07）

- **地圖＝幾何**（`gyo.voxel_map`＋`gyo.voxel_region`，工具烘焙、Engine 解析），**場景＝放在幾何上的東西**（放置、連線、規則）。
- 理由（使用者）：Minecraft 能把兩者混在一起，是因為它的遊戲本身就是地圖編輯器，玩家一直在改世界。多數遊戲的玩家不改世界，改的是作者；
  有地圖不一定要編輯場景（同一張地圖配不同場景；程序生成房間、多房間跳轉是說明概念的**例子，不是現在要做的**；目前的地圖是開放式單張）。
- 推論：場景是另一份資料契約（`scene v1`，原 arena v3：用哪張地圖、放了什麼、怎麼連、規則）；markers 檔屬於場景，不放在地圖目錄；
  地圖不知道場景的存在（候選 Design Invariant）；**場景編輯器是另一個工具**，voxel 地圖編輯器只編幾何。

## 使用者要的東西（2026-11-07）

| 項 | 內容 | 使用者的定義 |
|---|---|---|
| 互動物件 | 按鈕、拉桿；觸發**門（開關改碰撞）、陷阱（傷害、機關）、生成（怪物、物品）、純表現（光／音／外觀）**，以及邏輯組合（例如按下三個按鈕打開某扇門） | 類似 MC 的按鈕、拉桿、箱子 |
| 箱子 | **真的物品與背包（塔科夫式）**：箱子有內容、玩家有背包、能撿能丟；內容只送開啟者 | — |
| 表示方式 | 使用者傾向**標記＋實體**（不是特殊方塊）；survey 後定案 | 見上「原則」 |
| 與 AI 批的順序 | 一起評估後再定 | — |

## 架構師的看法（腦力激盪，2026-11-07；survey 要用來源驗證或否定）

1. **物件是實體，不是方塊**：格子是工具的、烘焙後靜態、Engine 不認方塊（voxel_grid DI 6、voxel-mesh DI 1）；entity-model 已預見 I（物品／容器／放置物）與 D（門），E-4「以元件挑實體」直接適用。
2. **邏輯用 Source 式連線表**：（來源、事件、目標、輸入、參數、延遲）是資料不是語言；邏輯元件只要計數器、繼電器、延遲三種，再多就停下想（AGENTS §9）。Match 在固定階段依 id 升冪處理（E-12）。
3. **門的碰撞不碰 `StaticAabbGrid`**：門 ≤ 50 扇，各一個 AABB＋開關位；Movement 查完靜態格再線性掃動態盒子；Collision 的 RC「需要動態物體」是它的出口；門的狀態進 snapshot 只要 50 bit。
4. **多通道只要兩條**：不可靠 snapshot＋每收件者的可靠有序事件串流（multi-channel-evaluation 的選項 C，重送狀態在 Match，Gateway 無狀態）；私人資料塞進收件者那一份，不另開傳輸通道。
5. **物品先槽位，不做格子拼圖**：N 槽、可堆疊、有重量；二維格子是之後的替換。
6. **第一條垂直切片＝一根拉桿開一扇門**：一次碰到每一層（標記→實體→連線→Match 裁決→可靠事件→Client 表現→動態碰撞→場景編輯器放置），內容最簡單。
7. **AI 排在物件後面**：物件沒有腦、位置固定、數量少，拿它開實體基礎比拿 AI 開便宜。
8. 工作量（量級）：門／拉桿／陷阱／生成含 pv10 與實體基礎約 big-map 至今的 1.5～2 倍；物品與背包再加一份。

## 既有約束（2026-11-07 查，origin/master）

- entity-model：種類 P／A／B／J／I／D；沒有按鈕、拉桿、陷阱；`placements_`＝Placement{id, catalogId, position, yaw, containerId, open}（物品批建立）；容器內容只給開啟者；物品 30 m。
  **沒有條文**：實體改變碰撞或幾何；動態方塊；實體之間的連結。map-markers RC 3「marker 指向另一個資產」＝停下重判 owner。
- wire：pv9 沒有實體 kind／state；protocol-v8 預留 WorldSnapshot 7／6、ShotRequest 10～12（物品、技能、互動）、ShotDecision 13、RuntimeEnvelope 22／23；附錄 C 物品／容器走可靠事件。
- EntityStore（PR #156）：容器版；3b DeferredChanges 待第一個非玩家批；第一個非玩家批原定 AI 小怪（依賴 pv10）；物品、門屬「物品批」未排。
- voxel_grid：每格只有 [block, orientation, count]，沒有狀態；DI 6 格子只有工具一個讀者。
- 產品程式沒有 inventory／pickup／interact／use；ActionKind 只有 Shot、Reload。
- tool-chunked（PR #163）把 markers 讀寫核心與 `--marker-kinds` 放進 voxel_map_editor：照本線的原則是放錯位置，記為待搬（big-map todo #8）。

## 開工時的 survey 範圔（`v8-arch-scene-survey`，上段；調查者 Opus／high、對抗檢查者 Fable／xhigh（比主會話強，開卡時確認）；估 12～18 h）

要讀的參照（主要來源優先）：Minecraft（block entity、redstone 訊號模型、容器與 loot table、互動判定、方塊狀態同步）；Source／Hammer（entity I/O、logic_relay、math_counter、func_door 碰撞、trigger 體積、FGD）；
Luanti（node metadata、node timer、formspec）；Godot（Area／Signal、scene instancing）與 UE（Level Blueprint 的 actor 連結，只看作者怎麼表達）；逃離塔科夫（loot 容器種類與密度、鑰匙與門、容器可見性、背包格子）；GYO 既有約束（上節）。
每個參照一列：機制｜為什麼｜對 GYO 的當てはめ（owner）｜判定｜根據，對照 big-map proposal §2 的 G1～G10。

待決定事項各一節（業界怎麼做、建議、理由、不採的代價；不替使用者決定）：
0. 場景契約：scene v1 的欄位（map id、markers、logic、rules）；一張地圖多場景；場景編輯器的範圍與它和 voxel 編輯器、framework 預覽元件的關係。
1. 表示：標記＋模型 vs 特殊方塊 vs 混合（使用者傾向前者）。
2. 連結與邏輯：「三個按鈕開一扇門」怎麼寫成資料；誰評估（Match、固定階段、E-12）；決定性；資料放哪；MapMarkers RC 3 的重判。
3. 動態碰撞：門開關 vs 靜態 broadphase；小量動態盒子的第二集合；Client 預測怎麼看門。
4. 實體化：物件是實體還是地圖狀態；與 EntityStore 3b、digest v5、實體配發；與 AI 批共用的部分 → 誰先。
5. wire／pv10：見 pv10-requirements。
6. 物品與背包：物品目錄、堆疊／尺寸／重量、撿丟裁決（protocol-v8 附錄 B-3）、容器內容只給開啟者；範圍估算與拆批（可能自成一線）。
7. 互動輸入：use 鍵、距離、視線、Client 預測與 Match 裁決的延遲表現。

產出：本夾的 `HANDOFF.md`、`survey.zh-Hant.md`；dev_log；證據 `build/evidence/scene/survey/`。對抗檢查附在 survey 末。結束時停下，回報決定。
