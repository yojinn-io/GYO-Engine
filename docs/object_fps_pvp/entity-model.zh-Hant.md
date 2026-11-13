# PvP Match 的實體資料模型（ECS 資料佈局）

> **Legacy**（現況紀錄；`v8-arch-docs-legacy`，2026-11-12）。設計權威在 [`docs/architecture/target/`](../architecture/target/README.md)。本文件只更新兩類事實：仍存在的程式的現況、仍有測試守衛的 Design Invariants。現有程式受本文件的 Design Invariants 約束；新程式（`engine/kernel` 起）受 target 約束。目標設計：[`target/world.zh-Hant.md`](../architecture/target/world.zh-Hant.md) 與 [kernel 規格](../architecture/plans/kernel/spec.zh-Hant.md)；本文件由 Match host 的遷移批修訂（遷移批待 `v8-arch-migration-plan` 排定）。

更新：2026-11-01（`v8-product-entity-contract`）。Owner：`object_fps_pvp`。**狀態：定稿（`v8-product-entity-contract-approve`，2026-11-01）：§12 的 8 個決定全部照建議；`Design Invariants` 節與 Replacement Criteria 作為 Architecture Delta 已核准（AGENTS §3）。** 之後修改條文或門檻都是新的 Architecture Delta。「實作現況」（2026-11-03，`v8-product-match-entity-split` S5）：Match 內部已是五欄（`bodies_`、`combats_`、`animations_`、`actionLedgers_`、`connections_`）＋兩個 Tick 快取（`tickShapes_`、`actionStartCombats_`），E-5、E-7、E-8 的 Match 內部已落地，`RequireEntityOrder` 守 E-5／E-9 的快照順序；E-2、E-3 的配發器與 E-12 的待辦機制在第一個非玩家批才有程式（條文生效、程式尚無非玩家實體）。

推導：ultracode（偵察 2×Opus 5.5、提案 3×Opus 5.5、統合 Fable 5.1、敵對檢查 Opus 5.5 xhigh），紀錄在 [v8 HANDOFF](plans/v8/HANDOFF.md)，各 agent 的原始輸出存在主 checkout 的 `build/evidence/v8-product-entity-contract/`（git 管理外）。敵對檢查的 must 3、should 12、note 7 全部處置，見本文末「敵對檢查的處置」。程式行號以工作樹 `1c598a83` 為準，定稿前逐一重核。

需求來源：[實體需求問卷](plans/v8/entity-requirements.md)的「結論」節（全照典型值，`v8-product-entity-req-typical-values`）。本文件的 vN 指遊戲版本、pvN 指協議版本。

## 0. 範圍與起點

已確認的起點（`v8-product-entity-model-start`，本文只細化不推翻）：

1. 採「ECS 的資料佈局，不採通用 ECS 執行期」：元件＝按 EntityId 排序的型別化陣列（現在 `PlayerState`／`CombatState`／`PlayerAnimation` 拆法的推廣）；系統＝對這些陣列切片的自由函數。不做 archetype、sparse set、執行期查詢語言；不以 EnTT／flecs 當核心。
2. 實體儲存留在產品 `apps/object_fps_pvp`。Engine 不加 ECS；Engine 該補的是格子空間索引與 Job，那是「能力」，不在本文件範圍。
   （2026-11-12 註：「Engine 不加 ECS」已由使用者 2026-11-11 核准的 Architecture Delta（spec K.3 第 7 項）取代：Kernel——World、System Contract、Scheduler、Executor——進 Engine，目標設計見 [`docs/architecture/target/`](../architecture/target/README.md)；實體儲存的遷移由遷移批規劃排定。本項與下行 11-05 註是當時契約的紀錄，不改寫。）
   （2026-11-05 註：本項由 entity-store Delta（`v8-arch-entity-store-delta`，已核准）取代：欄的機制（升冪、lower_bound 插入、保序刪除、二分查找、讀鍵）移到 Engine `GYO::EntityStore`（[entity-store 契約](../architecture/entity-store.md)，`v8-arch-entity-store-impl-3a`）；實體有哪些欄、id 配發與分段、種類、唯一刪除入口仍是產品的。）
3. 不改現有 wire（pv9 已定稿並合併，PR #128）。本文件只定契約，pv10 的 wire 形狀給方向與建議，另批宣告。

第一個非玩家實體＝AI 小怪（問卷 7-1）。它要上 wire 才能被看見，所以第一個 AI 批依賴 pv10；`v8-product-match-entity-split` 不依賴。

## 1. 參考的成熟方案：EnTT 與 flecs

讀取範圖：EnTT master 與 flecs master 的文件與原始碼（2026-11-01 讀取，外部頁面未保存，只作出處）。回答三個問題：為何這麼做、能用多少、要改裝多少。結論：**起點「不以 EnTT／flecs 當核心」沒有被推翻，但原本的一條理由要修正措辭（§1.4）。**

### 1.1 它們為何這麼做

| | EnTT（sparse set） | flecs（archetype） |
|---|---|---|
| 元件怎麼存 | 每個元件型別一個 pool＝sparse set：sparse 陣列以 entity 的 index 為下標（分頁）、packed 陣列緊密排列。目的：O(1) 查找、線性迭代、不必事先宣告元件集合（[entity.md](https://github.com/skypjack/entt/blob/master/docs/md/entity.md)〈Storage〉〈Type-less and bitset-free〉） | 每個「元件組合」一張 table，每元件一欄、每 entity 一列；加減元件把 entity 搬到另一張 table。目的：同組合的實體連續存放，query 走 table 零間接（[table.c](https://github.com/SanderMertens/flecs/blob/master/src/storage/table.c) 檔頭） |
| 刪除 | 預設 swap-and-pop：最後一個搬進洞再 pop_back（`sparse_set.hpp`）。另有 in_place（tombstone）與 swap_only。代價：順序隨增刪歷史變，`begin()` 從尾端往回走，類別註解明寫不保證插入順序 | 刪一列也是「最後一列搬進來」（`flecs_table_delete`）。table 間順序未定義；唯一的決定性保證是 `Any` 萬用字元同一資料兩次迭代相同（[Queries.md](https://github.com/SanderMertens/flecs/blob/master/docs/Queries.md)） |
| id | index＋version：uint32 拆 20＋12 位元（`entity.hpp` 的 `entt_traits<std::uint32_t>`）；回收時 version＋1、跳過全 1、溢位回繞。目的：回收 index 讓 sparse 陣列有界，舊 handle 不誤指重用同 index 的新實體 | uint64：低 32 位元識別、0 無效；回收時 generation＋1。文件說上 32 位元是 version，原始碼 `ECS_GENERATION_MASK = 0xFFFFull << 32` 實際只有 16 位元，最高 8 位元是 id flags（[api_defines.h](https://github.com/SanderMertens/flecs/blob/master/include/flecs/private/api_defines.h)）。entity index 是 dense＋分頁 sparse，回收取 `dense[alive_count++]`（`entity_index.c`） |
| 多元件交集 | view 以最小 pool 領頭、順序跟它；owning group 重排 pool 讓迭代接近依序存取，建立後不能再一般排序、與 in-place delete 不相容（〈Views／Iteration order〉〈Groups〉〈Pointer stability〉） | query 外層走 table、內層走列；`order_by` 兩步 quicksort 並快取，文件自承資料常變時昂貴（〈Sorting〉）；`DontFragment` 是為了避免「很多只有少數 entity 的 table」（[ComponentTraits.md](https://github.com/SanderMertens/flecs/blob/master/docs/ComponentTraits.md)） |
| 和網路有關 | `snapshot_loader`（保留原 id、目的 registry 須為空）、`continuous_loader`（遠端 id 對照本地 id） | `make_alive` 強制某 id 存活，文件明說用途之一是讓網路 peer 保持同步；entity range（`FLECS_ENTITY_RANGES`）每個 range 自己配發、回收不跨 range、用盡時 assert（[EntitiesComponents.md](https://github.com/SanderMertens/flecs/blob/master/docs/EntitiesComponents.md)、[entity_ranges.h](https://raw.githubusercontent.com/SanderMertens/flecs/master/include/flecs/addons/entity_ranges.h)「Each range maintains its own list of recycled entity ids」） |

### 1.2 能直接用的（約四成）

1. **每個元件一個型別化 packed 陣列，系統走陣列切片**（EnTT 的 pool 與單型別快速路徑）。就是起點 1；對所有 Body 跑 `StepMovement` 直接線性走欄，不需要 view。
2. **id 空間分區＝flecs 的 entity range**：每個 range 自己配發、不跨 range。「Gateway 配玩家、Match 配其餘」不是我們特有的需要，成熟方案遇過同樣的問題（§2.2）。
3. **用盡就報錯不回繞**：flecs range 用盡時 assert；與 `player_ids_exhausted`、需求「不回繞」一致。Assert 或 Result 由我們的 error-handling 分類決定（決定 6）。
4. **Client 採用權威 id 而不自己配＝flecs `make_alive` 的用途**：Client 只「讓權威給的 id 存活」，從不 `new`（E-3）。
5. EnTT 的 `continuous_loader` 提醒：若 Client 與 Match 用不同 id，就得維護對照表；我們選同一個 id，快照才能逐實體比、id 才能直接進 digest。

### 1.3 要改裝的（約六成）

1. **刪除不用 swap-and-pop，改保序 erase**。兩者預設的「最後一個搬進洞」正是順序依增刪歷史的來源；我們的不變式是「欄依 EntityId 升冪」（E-5）。400 個元素的 memmove 很便宜（推論，未量；RC-1 設門檻）。
2. **插入用 lower_bound，不是 append**。Match 段單調配發所以 append 即排序；但玩家段由 Gateway 配，有了 Match 段後新玩家的 id 一定小於所有 AI 的 id，所以插入要找位置（§3.1）。
3. **多欄交集用排序合併，不用 view／group／archetype**。所有欄依同一鍵升冪，兩欄交集就是雙指標走一遍。等於把 EnTT `sort_as`／group 的目的（讓幾個 pool 對齊）變成恆真的不變式，而不是執行期操作。
4. **查找用排序陣列二分搜尋，不用 sparse 陣列**。sparse 陣列能有界是因為 id 回收；我們單調不回收，340 萬累計 id 會讓 sparse 陣列長在死掉的區段上。400 個元素二分約 9 次比較（推論，未量）。
5. **generation／version 位元：機制不採，目的保留**。目的是「舊 handle 不誤指重用了同 index 的新實體」；我們一個世代內 id 單調不回收，等於 version 恆 0，舊參照只會查不到、不會指錯，所以 32 位元全給 index。EnTT 的 uint32 拆法只剩 20 位元＝約 104 萬，不夠 340 萬累計。將來若要回收 id（RC-2）才引入。`life_generation` 是遊戲語意「同一人第幾條命」，不是 id 的 generation，不混用。
6. 不採 `continuous_loader` 式的 id 對照：Client 與 Match 同一個 id。

### 1.4 不採用的部分與起點理由的修正

- 不採：sparse set 作主索引；archetype／table（我們只有 6 種固定組合，archetype 的好處用不到，順序、搬移、碎片化的代價全付）；owning group（重排破壞升冪）；`order_by` 每 Tick 排序；執行期查詢、cached query、change detection、observer（通用執行期，也是「API 滲透每個系統」的來源）。
- 起點原本的理由「迭代順序取決於增刪歷史會讓 Client 與 Match desync」：預設行為下確實如此，但兩者都有官方補救（EnTT `sort`／`sort_as`，flecs `order_by`、`make_alive`、entity range）。精確的說法是：**要得到決定性，每次增刪後都得付排序成本，或繞開它們的預設路徑，而那正是它們的效能來源。** 另一條理由「API 滲透每個系統」讀到的事實支持。比原文更強的理由是**規模**：約 400 個同時實體、6 種固定組合，它們的主要價值用不到，而它們預設放棄的「順序」正是我們最需要的；單調不回收的 id 讓「Match 段 append 即排序」「id 永不指錯」免費成立，這是回收 id 的庫沒有的。

### 1.5 本 repo 的現成先例

（2026-11-12 圖書管理員補註：本節是 2026-11-01 撰寫時、`match-entity-split` 之前的狀態；現在 `PvpMatch.hpp` 的成員已改為 `bodies_`、`combats_`、`animations_`、`actionLedgers_`、`connections_` 等欄，行號也已移動。）

- `players_`、`shapes_`、`actionsCombat_` 都是 `std::map<PlayerId, …>`（`PvpMatch.hpp:165-171`）；`Snapshot()` 照 map 順序推成三個平行陣列（`PvpMatch.cpp:592-604`）。「按 id 升冪的平行陣列」在快照層已存在，只是由 `std::map` 隱含保證，沒有條文與守衛。
- 單機版 `EnemySystem`（已移除；見 git 歷史 `56b8dda3` 的 `apps/object_fps/src/Gameplay/Enemy/EnemySystem.cpp`）：`nextEnemyId_` 單調配發、用盡報錯不回繞（:599-601、:626）；`enemies_` 與 `snapshots_` 是同索引平行 vector，push_back 加保序 erase（:633、:645-657）。能借：單調 id 與明確用盡錯誤、保序刪除、「刪實體＝同索引刪所有平行陣列」。不能借：浮點秒、AoS 的 `RuntimeEnemy`。

## 2. 第 1 件：單一 EntityId 空間

### 2.1 型別與常數

產品層新檔 `apps/object_fps_pvp/include/RetroFPS/Pvp/EntityId.hpp`（Client 也會 include；配發用的段界常數**不放這裡**，見 G6）：

```cpp
using EntityId = std::uint64_t;          // C++ 內部維持 u64（pv8-seq-cpp-types (b)，protocol-v8 §2a）
using PlayerId = EntityId;               // 「有連線的實體」這個角色的別名；數值就是 Gateway 配的 player_id
inline constexpr EntityId NoEntity = 0;  // 現況：Join 拒 0（PvpMatch.cpp:43）、Miss 的 targetId 0（ShotQuery.hpp:34）
```

Match 端私有標頭（`apps/object_fps_pvp/src/Pvp/EntityAllocation.hpp`，Client 不可見）：

```cpp
inline constexpr EntityId PlayerEntityIdLast = 0x00FF'FFFF;  // 2^24−1（決定 1 (a)）
inline constexpr EntityId MatchEntityIdFirst = 0x0100'0000;  // 2^24
inline constexpr EntityId MatchEntityIdLast  = 0x0FFF'FFFF;  // 2^28−1：契約最大仍是 4 位元組 varint
```

- `PlayerId` 現在已是 `using PlayerId = std::uint64_t`（`Movement.hpp:14`），改成別名是純型別變更，所有簽名照常編譯。wire 維持 uint32，編碼邊界照舊用 `wire::Counter` 的 `GYO_ASSERT`（`Wire.hpp:21-24`）。
- 位寬：uint32 夠（問卷結論：程序累計約 340 萬；uint16 不夠）。段界照決定 1 (a)：玩家段 [1, 2^24−1]（需求 65,535 的 256 倍），Match 段 [2^24, 2^28−1]（約 2.5 億，需求 340 萬的約 74 倍）；Match 段 id 的 varint 在契約最大也是 4 位元組。
- **用盡的單位是「Match 世代（一條 IPC link）」，不是程序生命期**：`PvpMatch::Reset()` 歸位計數器，IpcHost 每條新連線與關閉都 `RequestReset`（`IpcHost.cpp:478-489`、`:545-553`）。世代 ≤ 程序，餘裕只會更大；條文明寫，免得之後有人把計數器改成跨 link 保留。

### 2.2 配發對照

| 項目 | 玩家實體（有 Connection） | 非玩家實體（AI、Boss、投射物、物品、容器、屍體、放置物、門） |
|---|---|---|
| 配發者 | Gateway，`apps/object_fps_pvp/gateway/server.go:359-363`（`nextPlayer` 單調 ++ 後轉 uint32） | `PvpMatch` 的私有成員 `nextEntityId_`，從 `MatchEntityIdFirst` 起單調 +1（第一個非玩家批建立） |
| 用盡 | 現在 `nextPlayer >= math.MaxUint32` 回 503 `player_ids_exhausted`（`server.go:359`）；pv10 改成 `>= PlayerEntityIdLast`，`player_id_ceiling_test.go:19-27` 跟著改。這改寫 protocol-v8 §2a 用盡表的那一行（:104），是 pv10 的契約變更 | 到 `MatchEntityIdLast` 報錯不回繞；機制見決定 6 |
| 重設 | 程序內不重設；一個 Gateway 程序只服務一條 runtime link（`server.go:1131-1151`） | `Reset()` 歸回 `MatchEntityIdFirst`，和 `players_`、`tick_` 一起（`PvpMatch.cpp:580-586`） |
| Match 的檢查 | pv10 起 `Join` 拒 `playerId > PlayerEntityIdLast`，回 `invalid_player`；現在只拒 0。行為變更，不在 split | Client 沒有配發路徑（G6） |
| 狀態分類 | Gateway 的 `nextPlayer`、`reservation.playerID` 仍是 classConnection（`state_whitelist_test.go:61`、`:73`）；network-architecture 已把「id 配發」列為允許類別。只改常數不加欄位 | 權威狀態，`nextEntityId_` 進 digest（§5.3） |
| 重連 | 換 UDP 端點沿用同一個 reservation，id 不變（`server.go:504-524`）；session 逾時或 HTTP leave 後重新加入拿到新 `player_id`（`server.go:362`，從不回收）＝新實體 | Leave 的世界效果（屍體、掉落）是 Match 配新 id 的事件，由背包批與撤離模式的功能批決定 |

常數跨語言一致：照 `tests/object_fps_pvp/test_max_players.py` 的手法（讀原始碼文字核對）加一支核對 `PlayerEntityIdLast` 與 Go 端上限的測試（G5）。

### 2.3 不採「EntityId 全由 Match 配、PlayerId 只是對照」（三案一致）

1. digest 直接編碼 `player`、`combat`、`animPlayer`、`decisionOwner`、`targetId`、`lastAttackerId` 的數值（`AuthorityDigest.cpp:150-151`、`:176`、`:188`、`:194-201`、`:212-227`）；重新編號讓 split 的 0 差分不成立。
2. Gateway 依 `player_id` 路由 session（`server.go:554`）；若 snapshot 帶 Match 配的 id，Gateway 就得持有 player_id↔entity_id 對照表，那是 Match 配發狀態的副本（network-architecture「不持有 Match 決策狀態的副本」）。
3. 成熟先例支持分區：flecs 的 entity range（§1.2）。

### 2.4 分段的兩個後果（寫進 Design Invariants）

- 段只表示「誰配的」，不表示種類（E-4）。玩法程式不得用高位元或大小判斷是不是玩家。讀段界常數的只有：配發器、`Join` 的值域檢查、Gateway 的上限與單則檢查（pv10 的 `recipient_id` 在玩家段）；玩法判定不得讀。
- 平手鍵＝EntityId 升冪（E-6）。分段後玩家與 AI 平手時一定由玩家先：`ResolveActions` 的 (A, PlayerId, ActionId) 排序（`PvpMatch.cpp:411-414`）、`QueryShot` 等距取最小 id（`ShotQuery.hpp:34-35`、`ShotQuery.cpp:119-122`）、`ResolveLifeBoundaries` 的重生順序（`PvpMatch.cpp:271-273`）。這是有意的規則；改段界＝改判定＝升 digest version。要使用者確認（決定 2）。

### 2.5 pv8 預留號碙的處置（pv10 建議；pv9 一個都沒挪用）

| 預留（protocol-v8 §2a :109-130） | 處置 |
|---|---|
| `ShotHitKind` 3 `HIT_NPC` | 撤回，`reserved 3` 永久保留不挪用；註解「withdrawn: hit_entity_id covers every entity」（問卷 2-3） |
| `ShotDecision` 6 `target_id`、10 `target_life_generation` | 同號改名 `hit_entity_id`、`hit_life_generation`，型別不變，二進位相容。`HIT_PLAYER=2` 改名 `HIT_ENTITY=2`（`mutations.json` 的 find 字串跟著改）。非重生實體的 life_generation 填 1。C++ `ShotDecision::targetId`、`CombatState::lastAttackerId` 的語意改成 EntityId；名字在 pv10 批改，split 不改 |
| `ShotDecision` 12～13 | 12 `cooldown_end_tick` 不動；13 `item_instance_id` 改成 `item_entity_id`（§8 第 3 項） |
| `WorldSnapshot` runtime 6／client 7 `BossState` | 改成 `repeated BossPhase boss_phase`（元件，§8 第 1 項） |
| `RuntimeEnvelope` 22、23 | 依決定 5-ii |
| `PlayerState` 22 `team`、23～24 技能 | 照原定；pv10 新加的 `kind`、`catalog_id` 用 25 號以後 |
| 未決 | 打到沒有部位的實體（門、容器）時 `hit_part` 填什麼：現在 `HIT_PLAYER` 不允許 UNSPECIFIED（`adapter.go:284-285`、`Combat.hpp` 的 `ValidDecisionHitPart`）。由第一個可被射擊的非生物實體的功能批決定。Client 收到不在自己 interest 內的 `hit_entity_id`：pv10 定成「不是協議錯誤，只是查不到種類」 |

## 3. 第 2 件：元件＝現在拆法的推廣

落點（分欄 vector 或捆綁 map）交使用者（決定 4）；下面照建議（分欄）寫。

### 3.1 原則

- 元件＝按 EntityId **嚴格升冪、無重複**的 `std::vector<T>`（下稱「欄」）。每個元素帶自己的 id（`PlayerState::playerId`、`CombatState::playerId`、`PlayerAnimation::playerId`；新結構加 `EntityId id`），用多載 `EntityOf(const T&)` 取鍵；split 不改欄位名，避免 Client 大量修改。
- 系統＝走欄的自由函數或 `PvpMatch` 的私有成員；多欄交集用雙指標合併；查找用 `std::lower_bound`。
- 產品層新檔 `EntityColumns.hpp`，只有三個自由函數：`FindEntity(span, id)`、`InsertEntity(vector&, T)`（lower_bound 位置插入，已存在就 Assert；**不是 append**）、`EraseEntity(vector&, id)`（保序 erase）。根據：≥5 處相同的 find／insert／erase（`players_`、`shapes_`、`actionsCombat_`、Connection、之後的 PoseHistory；AGENTS §2「同一 Conversion 的重複」）。不做模板類別、不做 registry。
  （2026-11-05 註：三個函數與 `StrictlyAscending` 已移到 Engine `GYO::EntityStore`（entity-store Delta，已核准；`v8-arch-entity-store-impl-3a`），本檔變成轉接：讀鍵改為 Engine `EntityKey<T>` 的特化（`playerId` 型別由本檔以 concept 限定的部分特化涵蓋，不再是多載），以 using 宣告保留原名，呼叫處不改。「不做模板類別、不做 registry」在 Engine 的容器版仍成立。）
- 為什麼 Insert 不是 append：玩家 id 由 Gateway 配，有了 Match 段之後新玩家的 id 一定小於所有 AI 的 id。既有 digest 情境沒有任何一個會插入較小的 id（都是 {1} 或 {1,2} 升冪 Join，combat-rejoin 是 `Rejoin(2)` 而 1 仍在，`AuthorityDigest.cpp:560-567`），所以 append 突變殺不掉，G1 的單元測試必要。

### 3.2 欄的清單

種類：P 玩家、A AI 小怪、B Boss、J 投射物、I 地上物品／容器／屍體／放置物、D 門。「split」欄＝`v8-product-match-entity-split` 實際建立的。

| 欄（C++ 名） | 元素型別 | 欄位來源 | 種類 | 性質 | split |
|---|---|---|---|---|---|
| `bodies_` | `PlayerState`（型別名過渡期不改） | `Participant::state`。欄位分類：身體 position、yaw、pitch、verticalVelocity、grounded、crouched、sprinting；生命 lifeGeneration、lifeState、lifeStateTick、respawnTick；**輸入游標** lastResolvedCommand、movementEpoch、contiguousPendingCommands（Tick 與 ResetMovementEpoch 也寫它，`PvpMatch.cpp:446`、`:516-517`；Snapshot 再算一次 `:596-597`）；**host 診斷** movementSlack*、connectionQualityFailures（`MatchRuntimeHost.cpp:683-687`、`:742-744`） | P A B | 元件（Body） | 做 |
| `combats_` | `CombatState` | `Participant::combat`。pv10 再分成公開的 CombatPose（lastShotActionId、lastShotTick、damageCount、lastDamageTick、reloadActionId、reloadStart／EndTick、lifeGeneration：判定姿勢與呈現讀的欄位，`PlayerPose.cpp:268-281`、`SoundCues.cpp:108-117`）與私人的 Vitals（hp、magazineAmmo、nextAllowedShotTick、lastAttackerId、lastHitPart） | P A B | 元件（Combat） | 做（整份一欄，不拆） |
| `animations_` | `AnimationTrack{PlayerAnimation value; PlayerState before;}` | `Participant::animation`、`animationState`（`AdvanceAnimation`，`PvpMatch.cpp:557-560`） | P A B | 元件（Animation）；`before` 只在 Match 內 | 做 |
| `actionLedgers_` | `ActionLedger{EntityId id; ActionId retiredThrough; std::map<ActionId, ActionEntry> actions;}` | `Participant` 的 retiredActionThrough、actions | split 只有 P；AI、Boss 開槍時共用 | 輸入來源的一部分；所有射手走同一條 `ResolveActions`（`PvpMatch.cpp:395-422`）；ACK 與退役只有 Connection 用。**沒有 Connection 的射手不量 reference age，用規則定的固定值，也不走 `ActionDue` 的 command_sequence 條件**（否則 AI 的裁決取決於 host 回呼的牆鐘，`MatchRuntimeHost.cpp:441-446`，違反 E-10）；判斷依據是「有沒有 Connection 元件」，不是種類分支 | 做 |
| `connections_` | `Connection{EntityId id; commands, lastActualCommand, missingInputTicks, started, backlogSamples, backlogSum, fallbackSamples, fallbackCount, lastMovementResetTick, movementResetScheduled, movementResetReason, quality}` | `Participant` 其餘欄位 | 只有 P | 輸入來源＋診斷，不是實體元件；`quality` 歸診斷：它決定逐出，但逐出由 host 做，Match 判定不讀它 | 做 |
| `brains_` | `Brain{EntityId id; std::uint64_t rngState; std::uint64_t nextDecisionTick; Intent intent;}` | 新 | A B | 輸入來源；每 6 Tick 決策（問卷 4-5）；進 digest | AI 批 |
| `bossPhases_` | `BossPhase{id, phase, shieldMilli, hpMilli, weakPoints}` | 新 | B | 元件；公開 | Boss 批 |
| `placements_` | `Placement{id, catalogId, position, yaw, containerId, open}` | 新 | I D | 元件；事件 | 物品批 |
| `inventories_` | `Inventory` | 新 | P | 元件；私人 | 背包批 |
| `poseHistories_` | `PoseHistory` | 新 | 有 Body＋Combat＋Animation 的實體 | 派生歷史；不複寫、不進 digest | rewind 批（§10） |
| （存在表）`entities_` | `EntityRecord{EntityId id; EntityKind kind; std::uint32_t catalogId;}` | 新 | 全部 | 種類是標籤：只決定有哪些欄、誰提供輸入 | **第一個非玩家批**，不在 split |

- 種類標籤 `EntityKind` 與「種類→元件組合」的 constexpr 表，都在第一個非玩家批才加。split 只有一種實體，加了就是沒有壓力的一般化（AGENTS §1 MUST NOT）。系統不得以種類分支判定，只看有沒有元件（E-4）。
- 投射物只有 id 與生成參數，不進任何每 Tick 的欄（問卷 1-5）。
- **PlayerState 的拆欄不在 split 做**：`StepMovement` 是 Client 預測與 Match 共用的純規則且自己寫 `lastResolvedCommand`（`Movement.cpp:104`）；預測整份複製 PlayerState（`LocalPlayerPrediction.cpp:79`）；digest 的 `player` 列穿插著游標欄（`AuthorityDigest.cpp:150-166`）。拆欄會改 Client 與 digest 欄序，超出 split「不改 wire 與行為」。拆欄的壓力在第一個 AI 批出現：那一批決定 brain 是否照 Connection 的形狀產生帶序號的 `MovementCommand`（讓 `lastResolvedCommand` 有意義、StepMovement 一行不改——推論，未驗證），或拆成 Body＋InputCursor。契約表標「過渡期：Body 欄存整份 PlayerState」。
- `MatchRuntimeHost` 的按 PlayerId 的 map（`MatchRuntimeHost.hpp:254-258`，`ingress_` 在 `:267`）、`substitutions_`、`ledger_`、`shotShadow_`、IpcHost 的 `actionLanes`：分類為「host 的輸入來源與診斷」，鍵就是連線實體的 id；非連線實體永遠不會出現在裡面。split 不動。

### 3.3 Participant 的拆法（split）

`PvpMatch.hpp` 的 `Participant` 與 `std::map<PlayerId, Participant> players_` 換成上表前五欄。用到 `Participant&` 的私有函式改簽名、算式不改：`Decide(EntityId, PlayerState&, CombatState&, ActionId, ActionEntry&)`、`ResetMovementEpoch(PlayerState&, Connection&, reason)`、`Kill(PlayerState&, CombatState&)`、`AdvanceAnimation(AnimationTrack&, const PlayerState&)`、`ContiguousPending(const PlayerState&, const Connection&)`、`ActionDue(const PlayerState&, const ActionEntry&)`。公開介面簽名不變。

`players_` 的 16 處使用：

- 迭代（`std::map` 升冪 → 欄的索引順序，逐位元同序）：`FindSpawn`、`ResolveLifeBoundaries`、Tick 移動迴圈、`Decide` 建 targets（改走 `bodies_` 與 `tickShapes_` 合併，**保留現在 `shapes.at(id)` 的不變式 Assert**）、`ResolveActions`、`AdvanceAnimation` 迴圈、`TickShapes`、`Snapshot`。
- 查找（`at`／`find`／`contains` → `FindEntity`）：Join、Leave、AdmitInput、SubmitInput、CanSubmitActions、SubmitActions、CanAcknowledgeActions、AcknowledgeActions、GetActionResults、GetMovementQuality、RequestMovementResync、GetActionTiming、Decide 的目標、ResolveActions 的射手、ContainsPlayer。
- **連線相關的公開 API 一律查 `connections_`**（有 Connection 元件才算玩家）：ContainsPlayer、AdmitInput、SubmitInput、Leave、Submit／AcknowledgeActions、GetActionResults、GetMovementQuality、RequestMovementResync、GetActionTiming；`Join` 的人數上限也算 `connections_`。`bodies_` 只給判定用。查到連線後取其他欄的列用帶 GYO_ASSERT 的 `Row()`，不直接解參照（noexcept 的 `AdmitInput` 查不到回 UnknownPlayer）。否則 AI 批之後 `Leave(aiId)` 會經玩家路徑刪掉 AI、`AdmitInput` 會接受 AI 的輸入（PlayerId 只是別名，型別擋不住）。AI 批加測試：對 AI 的 id 呼叫這些 API，全部回 Unknown 或 false。
- Join → `InsertEntity` 到五欄；Leave → `PvpMatch::EraseEntity(EntityId)`。

**唯一的刪除入口** `void PvpMatch::EraseEntity(EntityId)`：從所有欄（元件、輸入來源）、所有 Tick 內快取、將來的 PoseHistory 同時刪；沒有任何容器在它之後還持有該 id 的列。`Leave` 呼叫它並照舊回傳「原本有沒有」。

### 3.4 `shapes_` 與 `actionsCombat_` 的歸位

兩者都是「Tick 內派生暫存」：每 Tick 重建、不跨 Tick 讀、不進 snapshot、不進 digest、Reset 清掉；都能從 `bodies_`、`combats_`、`animations_` 重建（`PvpMatch.cpp:566-575`），所以是快取不是權威（E-8）。

- `actionsCombat_` → `std::vector<CombatState> actionStartCombats_`：`ResolveActions` 開頭 `actionStartCombats_ = combats_;`（整欄複本、同序），查找用 `FindEntity`，不依賴索引對齊。它是判定姿勢的輸入，也是 rewind 每 Tick 要存的 CombatPose。
- `shapes_`／`shapesTick_` → `std::vector<EntityShapes{EntityId id; std::vector<PlayerWorldHitShape> shapes;}> tickShapes_`／`tickShapesTick_`，升冪，只收活著且有形狀的實體；一個 Tick 只建一次照舊。

### 3.5 Snapshot 的形狀

- split 期間 C++ `WorldSnapshot{tick, players, combat, animation}` 一欄都不改。`Snapshot()` 依 `bodies_` 的順序走；三個元件欄恰等長且同序（Join／`EraseEntity` 同時進出），所以 `combats_`、`animations_` 以同索引讀並 GYO_ASSERT id 相同（實作選擇，xhigh 審查 should-4：迴圈前先 Assert 三欄等長）；`connections_`（算 contiguousPendingCommands）以 `FindEntity` 取。順序與內容和現在逐位元相同。
- 新增 `RequireEntityOrder(snapshot)`（GYO_ASSERT：三個 vector 的 id 序列嚴格升冪且相同），和 `RequireValidAnimation` 放在一起。
- 新增自由函數 `EraseEntity(WorldSnapshot&, EntityId)`，一次刪掉所有欄的那一列；host 的逐出改用它（`MatchRuntimeHost.cpp:759-760` 現在只刪兩個）——在缺陷批做（§9.0）。發布前加 `RequireValidSnapshot`。
- 契約規則：**快照永遠是「一個元件一個 repeated」，每個 repeated 依 EntityId 升冪，含所有有該元件的實體**；不出現每種類一個訊息。C++ `WorldSnapshot` 每多一個可複寫元件就多一個 vector（例如 `bossPhases`，id 集合是 `players` 的子集，仍升冪）。
- 消費端現況（已核對）：Client 要求 players ≤4、combat 數＝players 數、animation 同長且 id 一對一順序不限（`ClientConnection.cpp:601-602`、`:639-664`）；Gateway 同（`adapter.go:79`、`:132`）；配對一律用 id，不用索引（`SnapshotTimeline.hpp:194-203`）。所以**不改 wire 時非玩家實體不能進這三個 repeated**，第一個 AI 批依賴 pv10。

## 4. 第 3 件：複寫屬性掛在元件上

### 4.1 元件與通道

每個元件在契約表多一欄 `Replication = Public | Private(owner) | Event | None`。投影是每元件一支手寫函式（和 `IpcHost.cpp:116-148` 一樣），不做執行期屬性系統或反射（E-11）。

| 元件 | 通道 | 送給誰 | 過濾 | 備註 |
|---|---|---|---|---|
| Body（P A B） | 公開、高頻、latest-wins | 收件者視距 100 m 內 | Match 依 Body 距離過濾；自己永遠在內 | `movement_epoch` 留在公開：Client 用別人的 epoch 切插值區段（`SnapshotTimeline.hpp:70-77`），回溯夾到 epoch／life 區段起點（protocol-v9 §5） |
| Animation（P A B） | 公開、高頻 | 跟著 Body | 同一個 interest 集合；一個實體「可見」是整體判斷 | |
| Combat（P A B） | 現況全公開（pv9 `combat`，含 hp、彈藥）；問卷 3-3 說數值不公開 | — | — | pv10 要拆成公開 CombatPose（§3.2）與私人 Vitals；列進 pv10，由使用者排批。`hp==0 ⇔ DEAD` 的檢查（`adapter.go:116`、`ClientConnection.cpp:641`）要改成只看 lifeState |
| BossPhase（B） | 公開、高頻（狀態）＋事件（換階段、擊倒） | 全員 | 不過濾（Boss 1 隻，血量公開） | |
| Placement、DoorState（I D） | 事件：出現、消失各一次；加入或重連送一次基準 | 公開 | 物品 30 m | |
| Inventory、彈藥、冷卻、任務、開著的容器內容 | 私人：基準加差量 | 只給擁有者 | — | 容器內容只給開啟者 |
| 投射物（J） | 事件：生成參數＋共用純軌跡規則 | 公開 | 依發射點距離 | 不進每 Tick 的欄 |
| Connection、Brain | None（不複寫） | — | — | Connection 的游標跟著 players 出去是 pv9 現況；pv10 收進收件者自己的 `own` |

### 4.2 interest 的過濾點

- 只在 Match：純函數 `RecipientView ViewFor(EntityId recipient, const WorldSnapshot& world, const InterestRules&)`，在 `PvpMatch::Snapshot()` 之後、IPC 編碼之前，只讀 world。interest 集合放在 Match，可重建、不參與判定。priority accumulator 是跨 Tick 累積的**送出排程狀態**：不參與判定、不進 digest；重建後送出順序可能不同，但不影響權威（E-8 另列）。
- 權威 digest 編碼**過濾前**的 world（`AuthorityDigest.cpp:209-227` 吃 `Snapshot()`）；RecipientView 不進權威鏈。守衛 G8 要驅動真正接受 interest 參數的物件（§7）。
- Gateway 只在單一框架內依收件人的 `player_id` 分送，不做跨訊息判斷。「這算不算 Gateway 允許類別的擴充」：`recipient_id`→session 的對應已屬 classConnection（`state_whitelist_test.go:73`），只多逐欄複製與單則檢查——由使用者判定（決定 5-i）。

### 4.3 pv10 必須先面對的容量問題（推論，未以 `proto.Size` 實測）

- pv9 實測 4 人 snapshot 契約最大 40＋211N B（protocol-v9 §6，:134），datagram 上限 1200 B（`engine/net/include/engine/net/GyopDatagram.hpp:28`）。N≥6 就超過：**8 人房（問卷 0-2）在沒有 AI 時就放不下**，視野內 29 個 Body（8＋20＋1）更放不下。
- 所以 pv10 的每收件者份要由 Match 依契約最大值預算、用 priority accumulator 決定每 Tick 放哪些實體；切分不能交給 Gateway（它不能依內容判斷）。每份都必須能放進一個 datagram（否則 Gateway `runtimeFailed`，`server.go:728-730`），8 份約 ≤9.6 kB，遠小於 IPC 的 MaxFrame 65,536（`Wire.hpp:18`）。
- 容量證明的是「需要每收件者一份並由 Match 依預算選實體」（決定 5-i），**不是**「非玩家實體要放進同一個 repeated」（決定 5-ii）；兩軸分開。

### 4.4 pv10 要加的東西（建議；依決定 5 取捨）

共同：版本 9→10；`ShotDecision` 6／10 改名、`HIT_ENTITY`、`reserved 3` 保留、13＝`item_entity_id`；WorldSnapshot runtime 6／client 7＝`repeated BossPhase boss_phase`；Gateway 上限改 `PlayerEntityIdLast`、Match `Join` 加值域檢查；Client 與 Gateway 的 ≤4、同長檢查改成「每欄 id 集合＝種類表推出的集合、總數 ≤ MaxSnapshotEntities」（三方契約變更；`MaxPlayers=4`，`Movement.hpp:21`、`adapter.go:20`，與問卷 ≤8 的落差一併重寫）；`PlayerPresentation` 容量 MaxPlayers−1（`PvpApplication.cpp:1041`）跟著改；CombatState 拆公開／私人；事件的可靠送達照附錄 B-1（must 1～7 是前提）。

- 5-i（兩案都需要）：每收件者一份（`recipient_id`＝連線實體的 EntityId）＋Match 依預算選實體；搭在既有 envelope 16，沿用 Gateway 的 snapshot 佇列、coalescing 與 DropReason。
- 5-ii (a)（建議）：`PlayerState.player_id`（1）改名 `entity_id` 號碼不變，加 `kind`、`catalog_id`（25 號以後；每 Tick 都帶，因為實體每 Tick 可能進出 100 m，latest-wins 不保證「第一次出現」送到）；`players`／`combat`／`animation` 的語意擴成「所有有該元件的實體」；私人 latest-wins（自己的游標、Vitals）放收件者份的 `own`；可靠事件與私人可靠狀態走 B-1 串流；22／23 永久保留。
- 5-ii (b)：`players`／`combat`／`animation` 照舊只給連線實體；新增 runtime 8／client 9 `repeated EntityBody bodies`、runtime 9／client 10 `repeated PlayerAnimation body_animation` 給其他實體；22＝世界實體事件、23＝私人狀態。Client 既有路徑改動最小，但違反「每個元件一個 repeated」。

未查證：client 端以哪個 GYOP Type 承載下行事件；recipient 已離開時那一份計入哪個 DropReason（pv10 批定）。

## 5. 第 4 件：決定性契約

### 5.1 迭代順序

- 每個元件欄、輸入來源欄、派生暫存欄、Snapshot 的每個 repeated、digest 的每段，一律依 EntityId 嚴格升冪。
- 現存三處順序依賴語意不改、鍵改名為 EntityId（§2.4）。Tick 內各實體互相獨立的迴圈（移動、動畫）可以改走別的順序或並行，但要證明結果與升冪循序相同，且 `TraceMovement` 的輸出順序（診斷資料契約）與 Boss 的「不受推擠」規則都會破壞獨立性，所以**預設不並行**。
- 之後新增的規則平手時也用 EntityId 升冪：AI 決策、投射物生成、兩人同時撿同一件（附錄 B-3 的 ITEM_GONE）。
- 不採 swap-and-pop（§1.3）。

### 5.2 Tick 內的結構變更（E-12，敵對檢查 must 3）

`Decide`／`ResolveActions` 在整個裁決過程持有參照（`PvpMatch.cpp:374` 的 `players_.at`、`:383` 的 `Kill(target)`、`:422-425`；targets 持有指向 shapes 的 span，`:363-364`）。`std::map` 對其他元素的插入與刪除保證參照不失效，`std::vector` 不保證。AI 會死、屍體會另生實體、Boss 會發射投射物、撿物品就是刪實體，這些都自然發生在 `ResolveActions` 裡。所以：

- **Tick 內不改任何欄的成員。** 生成與刪除先記進本 Tick 的待辦（`pendingSpawns_`、`pendingDestroys_`），在固定階段（`ResolveActions` 之後、`AdvanceAnimation` 之前）依 EntityId 升冪套用；新 id 依裁決順序 (A, EntityId, ActionId) 配發。
- `Join`／`Leave` 在 Tick 外（host 呼叫），不受此限；split 本身不受影響。
  **2026-11-11 修訂（`v8-arch-kernel-membership-in-tick`、`v8-arch-loading-schedule`，使用者決定；target, not implemented——現況仍是上面兩行，遷移批改碼時搬進條文）**：機制下 Join／Leave **進 tick 內**——Ingress（(b)）只把加入／離開請求寫進 `MembershipRequests`（role=Input、以請求序號為鍵；`StageMembershipRequests` 在 FixedUpdate 與 Loading 的 Ingress 各有一個掛點），(c) 的 `ApplyMembership` System 在 Simulate stage 以 `SpawnWithId`／`Despawn` 生成與刪除；初始成員走 Loading schedule 的「玩家」stage，由同一個 System 生成，不走 Startup、不是 host 直寫。後果：加入與離開晚最多一個 tick，`combat-rejoin` 等 golden **可能**差一 tick（audit §4.1 另點名 `host-play`、`host-poor-connection` 為確定受影響，待實測），其餘以對局開始（ready 後第一個 tick（計數 1））為基準預期不變；一律由遷移批實測，不先假設（limits LIM-5）。E-12 的「待辦」由 World 的延後佇列承擔（spec §F.2），`inTick_` 守衛由 W-3 取代。
- 守衛：`InsertEntity`／`EraseEntity` 加 `GYO_ASSERT(!inTick_)`，並加一個在 `ResolveActions` 內直接呼叫它們的突變。

### 5.3 id 配發

- Match 段只由 `PvpMatch` 配發（私有成員，第一個非玩家批建立）；單調、不回收；Reset 歸位；用盡報錯不回繞（決定 6）。
- 單調不回收的兩個免費性質：Match 段的新增永遠是尾端 append；舊 id 永遠不會指到另一個實體（不需要 generation；舊參照只會「查不到」）。
- 玩家 id：Match 不保證也不禁止重用（測試情境 Rejoin 用同一 id）；正式環境由 Gateway 單調配發保證不重用。條文：「世代內 Match 段不重用；玩家 id 的唯一性由 Gateway 保證」。
- Client 不配 EntityId：只採用 snapshot 或事件帶來的 id。本地預測的效果（彈道、命中標記）用暫時鍵 (自己的 EntityId, ActionId)，不碰 EntityId 空間。
- AI 亂數（問卷 4-6）：`Brain.rngState` 初值＝`SplitMix64(matchSeed ^ id)`，每實體獨立流，增刪一個實體不會讓其他實體的序列移位；`matchSeed` 在有 AI 的情境第一筆記錄寫進 digest。程式裡目前沒有任何 Match 種子來源（grep 無結果），正式環境種子由誰給**未查證**，AI 批決定；否則 E-10 無法落地。

### 5.4 digest 的變更（`tests/object_fps_pvp/AuthorityDigest.cpp`）

- split：**完全不變**。`DigestVersion = 4`（:39）、`DigestFields`（:49-69）、`EncodeSnapshot` 順序（:209-227）、dump 標籤都不改。標籤不進雜湊（`Record::U64` 只把數值進 `bytes_`，:78-81），所以 digest 相同時 compare 的一般模式不會發現標籤改了；**標籤不變另外用兩樹全部情境 `--dump` 再 diff 證明**（§9.2 第 3 項）。
- 第一個非玩家批（AI，pv10 之後）：升 `DigestVersion` 5。照 v2「只在為 true 時寫」的先例（:40-42、:164-165），**只在非玩家的東西出現時才編碼**，所以只有玩家的 37 行 golden 不變。在 `animations` 段之後依序追加：(1) `spawned`（本 Tick 新生的 Match 段實體 `entity kind catalog`，升冪）與 `destroyed`；(2) `nextEntityId`，只在 ≠ `MatchEntityIdFirst` 時寫；(3) 非玩家的 Body、Combat、Animation 列（非連線實體寫 `entity kind catalog` 再寫 Body 欄，省略游標與 host 診斷欄位；Match 段 id 一定排在所有玩家之後）；(4) `brain`；(5) `bossPhase`；(6) 有 AI 的情境第一筆附 `matchSeed`。
- **連線實體的編碼**：若那一批把 PlayerState 拆成 Body＋InputCursor，digest 編碼器**從兩欄重組舊的位元組順序**，所以 `player` 列的欄序不變、37 行 golden 不變；不接受「拆欄＝golden 重生」（否則就要照 §14.4 走核准，兩者擇一寫進那一批的事前宣告）。
- AI 的 digest 情境用合成的 AI 規則與合成 catalog（同 v4 用合成動畫規則的理由，:45-47），golden 不隨產品內容編輯而變。讀出 `brains_`、生成與刪除的觀察 API 在 AI 批的事前宣告一併宣告。
- 新增欄位的情境用 `compare_authority_trees.py --added nextEntityId,…` 比對。
- PoseHistory 不進 digest：每格等於某 Tick 已編進 digest 的 snapshot 值；由 G9 證明。

## Design Invariants

> 2026-11-13 的標註由 `v8-arch-a6-revision` 加上（legacy 條文的重標，`docs/architecture/plans/kernel/a6-revision-report.zh-Hant.md`）。「已被取代」＝該部件的遷移批完成後，這一條對任何程式都不再成立；「相容（措辭更新）」＝它繼續約束同一份程式，只有措辭或機制換了。遷移批完成前，標註不改變現有程式受的約束；條文本身不在這裡改寫，目標措辭在 [`target/entity-model.zh-Hant.md`](target/entity-model.zh-Hant.md) §2。

本節是 Architecture Delta（AGENTS §3），新增與之後的每一處修改都要使用者同意。守衛測試的設計在 §7；「審查」表示只能靠 code review（AGENTS §13），照 network-architecture 的先例寫明。

| 編號 | 原則 | 維持的性質 | 未來修改時要問的問題 | 守衛 |
|---|---|---|---|---|
| E-1 | 一個 EntityId 空間；0＝無 | 命中、攻擊者、物品、事件、屍體都用同一個 id，不分種類，不需要對照表 | 是否為某種實體另開一套 id、對照表，或 `HIT_*` 種類加各自的 id？ | 審查；G5 |
| E-2 | 兩段配發：[1, PlayerEntityIdLast] 由 Gateway 配給連線實體，[MatchEntityIdFirst, MatchEntityIdLast] 由 PvpMatch 配；EntityId＝player_id 不轉換；段界常數在 Match 端私有標頭一處定義，跨語言核對 | 同一世代內不撞號；玩家的 id、wire、digest 一個位元組都不變；Gateway 不持 Match 狀態的副本 | 是否讓 Match 配玩家 id、讓 Client／Gateway／host 配 Match 段，或第三個配發者？ | G5、Gateway 上限測試、常數核對 |
| E-3 | Match 段單調、不回收、Reset 歸回 First、用盡報錯不回繞；Client 沒有配 id 的程式路徑；本地預測用 (自己的 id, ActionId) 當暫時鍵〔2026-11-13 `v8-arch-a6-revision`：**相容（措辭更新）**——配發策略仍是這一條；「Reset 歸回 First」改為世代切換＝重建 World，「不需要 generation」改為格式含 generation、PvP 恆 0〕 | 一個 id 永遠不會指到另一個實體，不需要 generation；快照能逐實體比對、id 能直接進 digest | 是否回收 id、讓計數器跨 Reset，或讓 Client 先建實體再等權威改號？（是→RC-2／RC-5） | G4、G6 |
| E-4 | 段只表示配發者，不表示種類；種類只來自標籤；系統以「有沒有元件」挑實體，不以種類分支判定；輸入來源（Connection、Brain）不是實體元件；讀段界常數的只有配發器、Join 的值域檢查、Gateway 的上限與單則檢查 | 加一個種類只要加資料與元件組合，不必改每個系統；AI 不帶連線狀態 | 是否用 id 高位元或大小判斷「是不是玩家」？是否寫了 `if (kind == …)` 改判定？是否把命令視窗或品質放進 Body？ | 審查（測不到） |
| E-5 | 每個元件欄依 EntityId 嚴格升冪、無重複；插入用 lower_bound、刪除保序；實體的欄成員關係恰等於種類表 | 迭代順序只取決於 id，不取決於增刪歷史 | 是否用了 swap-and-pop、unordered 容器、append 一個較小的 id，或某欄沒跟著刪？ | G1、`RequireEntityOrder`（2026-11-05 註：機制部分——升冪插入、保序刪除、二分查找——改由 Engine 的 common 測試 `gyo_entity_store_tests` 與 Engine 突變目錄的 `es-row2-insert-appends`、`es-row2-erase-swap-and-pop` 守（產品的 M1、M2 於 2026-11-06 退役）；G1、`RequireEntityOrder` 守「成員關係恰等於種類表」與快照順序；entity-store Delta） |
| E-6 | 跨實體平手鍵只用 EntityId 升冪；段界因此是判定的一部分 | 判定結果不依容器而定；同樣輸入得到同樣 digest | 是否改了段界、配發順序或平手鍵？（是→升 digest version） | G7、authority_digest |
| E-7 | 刪實體只走 `PvpMatch::EraseEntity`，從所有欄、輸入來源、快取、歷史同時刪；snapshot 列的刪除只走 `EraseEntity(WorldSnapshot&, id)`；發布前 `RequireValidSnapshot`〔2026-11-13 `v8-arch-a6-revision`：**相容（措辭更新）**——「沒有孤兒元件」仍約束產品；欄的同時刪除改由 World 的 `Despawn` 保證，World 以外的輸入來源、快取、歷史仍由產品清〕 | 沒有孤兒元件（反例：`MatchRuntimeHost.cpp:759-760` 逐出時只刪 players 與 combat） | 是否在某處手刪某一欄？host 是否在 Snapshot 之後自己刪？ | G2、G3 |
| E-8 | 元件是權威；Tick 內派生暫存（tickShapes_、actionStartCombats_、interest 集合、PoseHistory）可從元件重建，不進 snapshot、不進 digest；送出排程狀態（priority accumulator）不參與判定、不進 digest；interest 參數不改變權威 digest | 權威只有一份；可見性不改變判定 | 這個欄位是否要知道「元件已經有什麼」？是的話它是元件。是否讓過濾結果回流到判定或交給 Gateway？ | G8、G9 |
| E-9 | id 的數值與順序進權威 digest，每欄依 EntityId 升冪編碼；新種類與新欄只在出現時才編碼；連線實體的 `player` 列欄序不變 | 加新種類不讓既有情境的 golden 變 | 是否改動既有列的欄位順序或穿插？ | authority_digest（37 golden） |
| E-10 | AI 的亂數只來自 `Brain.rngState`，由 (matchSeed, EntityId) 導出並進 digest；判定路徵不讀時鐘、指標位址、unordered 容器順序；沒有 Connection 的射手不量 reference age | Match 自己一致、可在 digest 重現（問卷 4-6） | 是否用 `std::random_device`、時鐘或共用一條亂數流？AI 是否走到 host 的 reference age 回呼？ | G10 |
| E-11 | 每個元件在契約表有 Replication 欄；投影是每元件一支手寫函式；過濾只在 Match，Gateway 只在單則訊息內分送與檢查 | 不出現執行期屬性系統；Gateway 仍是無狀態邊緣 | 是否加了反射、屬性表或讓 Gateway 依內容過濾？ | 審查；`state_whitelist_test.go` |
| E-12 | Tick 內不改任何欄的成員；生成與刪除記入待辦，在固定階段依 EntityId 升冪套用；新 id 依裁決順序配發〔2026-11-13 `v8-arch-a6-revision`：**相容（措辭更新）**——性質不變；「Tick 內不改成員、固定階段依 id 套用」改由 Kernel 保證（`docs/architecture/target/kernel.zh-Hant.md` DI-KS 9），「新 id 依裁決順序配發」仍是產品的配發器〕 | 裁決途中持有的參照不失效；結果不取決於生成／刪除發生的順序 | 是否在 `ResolveActions`、`Decide`、移動迴圈裡直接 Insert／Erase？ | `GYO_ASSERT(!inTick_)`＋突變 |

**過渡期（不是 RC，是預定的演進）**：Body 欄在 split 期間存整份 `PlayerState`（含游標與 host 診斷欄位）；拆成 Body＋InputCursor 的時機是第一個 AI 批，digest 編碼器重組舊欄序（E-9）。

### Replacement Criteria

任一條出現就停止修補，向使用者提出「重建或修改」（AGENTS §3）。門檻數字已核准（決定 7 (a)）。

- RC-1：同時存在的實體實測超過 4,000（需求的 10 倍），或在 Linux Match 上實測欄維護（insert、erase、lower_bound、合併）每 Tick 的 p95 超過 1 ms（16.7 ms 預算的 6%）→ 換結構（例如分頁或索引），但不得換成會改變順序的結構。
- RC-2：需要回收 id（一個世代內實際用盡，或需要跨世代保留 id）→ E-3 不成立，引入 generation 位元（照 EnTT `combine`／`next` 或 flecs 的遮罩）。〔2026-11-13 `v8-arch-a6-revision`：**相容（措辭更新）**——條件仍是產品的置換條件；處置從「引入 generation 位元」改為「改配發策略」，因為鍵格式已含 generation〕
- RC-3：元件組合從固定表變成執行期動態加減且成為多數，或種類超過 16 個 → 種類表不再是標籤。
- RC-4（判定系統）：加一種實體要改超過 3 個**判定用的自由函數**（移動、射擊、生死、動畫、AI 決策），或得改守衛本體而不是只改種類表與它自己的測試，或重做 E-5／E-7 的驗證 → 模型失效。
- RC-4b（複寫）：每加一個可複寫元件要改的位置（C++ WorldSnapshot、IpcHost 投影、adapter.go 逐欄複製與檢查、ClientConnection 解碼、RequireValidSnapshot、EraseEntity(WorldSnapshot&)）超過 6 處，或位置清單無法列舉 → 投影層失效，考慮以資料描述投影。
- RC-5：Client 必須在本地生出有權威意義的實體（例如預測投射物要參與命中）→ E-3 的 Client 條款重設計。

## 7. 守衛測試設計

放 `tests/object_fps_pvp/`（新檔 `EntityModelTests.cpp` 與 Python 掃描腳本），CTest 標籤 `cpu`，L1 會跑。這些是依 Agent 理解寫的測試，照 AGENTS §14.2 以突變證明檢出力；突變只跑相關的（D184），只在預期失敗時算 killed。判定之後要改時一律照 §14.4 取使用者核准。

| 守衛 | 批 | 做法與斷言 | 突變（應被殺） |
|---|---|---|---|
| G1 `EntityColumnsStayOrdered` | split | 3 個 id 依降冪 Join（3, 2, 1），每 Tick 檢查 Snapshot 三個 vector 的 id 序列嚴格升冪＝{1,2,3}；Leave(1)→{2,3}；再 Join(1)→{1,2,3}；跑 120 Tick。另在 `RequireEntityOrder` 加 GYO_ASSERT 讓 37 個 golden 情境每 Tick 經過它。既有 golden 殺不掉 append 突變，所以必須是單元測試，不新增 digest 情境（情境數維持 42） | M1 `InsertEntity` 改 push_back；M2 `EraseEntity` 改 swap-and-pop；M4 Snapshot 反向走欄（2026-11-06 註：M1、M2 已從產品突變目錄退役，機制由 Engine 突變目錄 `docs/architecture/plans/entity-store/scripts/mutations.json` 的 `es-row2-insert-appends`、`es-row2-erase-swap-and-pop` 以 common 測試守；使用者決定，`v8-arch-entity-store-impl-3a`） |
| G2 `EraseEntityRemovesEveryColumn` | split | Join 兩人、開槍讓 `tickShapes_`、`actionStartCombats_` 有值，Leave(1)。**觀測手段**：friend 測試存取器 `PvpMatchTestAccess::ColumnIds` 回傳各欄（含快取）的 id 序列，斷言都不含 1；另加 Join(1)＋跑 Tick，依靠 `InsertEntity` 的重複 Assert（GYO_CHECK_ASSERTS）；公開 API：`GetActionResults(1)` 等 nullopt、`ContainsPlayer(1)` false、Snapshot 三 vector 同長 | M3 `EraseEntity` 漏刪 `animations_`；漏刪 `connections_` |
| G3 `PublicationComponentsAgree` | 缺陷批（先 Red） | 重現 host-poor-connection 的逐出（`MatchRuntimeHost::JudgeConnectionQuality`），斷言 publication 的 players、combat、animation id 序列相同；發布前 `RequireValidSnapshot`。現況 Red | host 的 `EraseEntity(WorldSnapshot&)` 漏刪 animation |
| G4 `MatchEntityIdsMonotonic` | 第一個非玩家批 | 連續配發嚴格遞增、第一個是 `MatchEntityIdFirst`；Destroy 後再配不重用；Reset 後回 First；以私有測試 seam 把 `nextEntityId_` 設到接近 `MatchEntityIdLast`：最後一個成功、再配報錯（依決定 6）不回繞 | 重用最近刪除的 id；從 1 起配；用盡時回繞；`>` 改 `>=` |
| G5 `JoinRejectsMatchRangeId`＋Go 上限測試＋常數核對 | pv10 | `Join(MatchEntityIdFirst)`、`Join(PlayerEntityIdLast+1)` 回 `invalid_player`；`Join(PlayerEntityIdLast)` 接受。`player_id_ceiling_test.go` 的期待值移到 `PlayerEntityIdLast`。Python 讀原始碼核對 C++ 與 Go 的常數 | 拿掉值域檢查；兩邊常數不一致 |
| G6 `NoEntityIdAllocationOutsideMatch` | 第一個非玩家批 | 段界常數與配發器放 Match 端私有標頭（`src/Pvp/`），Client 想用就編譯失敗（`PvpMatch.hpp` 已被 Client 端 include，公開標頭擋不住）。另加原始碼掃描：Client 端（ClientConnection、ClientSimulation、LocalPlayerPrediction、SnapshotTimeline、PvpApplication）不出現 `MatchEntityIdFirst`／`PlayerEntityIdLast`／`AllocateEntityId`，也不以整數字面值建構 EntityId。依內容的濫用（E-4）測不到，靠審查 | 在 `ClientSimulation.cpp` 加一次呼叫；Client 以字面值建 EntityId |
| G7 `TieBreakByEntityId` | split | `QueryShot` 放兩個等距目標 id 5 與 `MatchEntityIdFirst+1`，輸入順序正反各一次都選 5；`ResolveActions` 兩個同 A 的動作依 id 先後裁決；同時重生時 id 小的先放（部分已由 ShotQuery 測試涵蓋，未逐一查證） | 平手鍵改成插入順序 |
| G8 `InterestDoesNotChangeAuthorityDigest` | interest 批 | 驅動**真正接受 interest 參數的物件**（host 的發布＋`ViewFor`），半徑用小於 arena 距離的值（0 m、1 m）與無限各跑一次，權威 digest 全部相同；0 m 時 RecipientView 只剩自己（證明它確實過濾）。arena 都 ≤20 m，100 m 與無限一定相同，不能當對照 | Snapshot 或 host 在發布前依半徑刪 Body |
| G9 `PoseHistoryMatchesSnapshots` | rewind 批 | 每 Tick 後 `poseHistories_` 最新一格的每個欄位等於該 Tick Snapshot 的對應 body、combat 動作 tick 與 animation | 歷史在 `AdvanceAnimation` 之前寫入 |
| G10 `AgentRngReproducible` | AI 批 | 同一 seed 跑兩次 digest 相同；換 seed 後 brain 紀錄不同；AI 開槍的情境不經 host 的 reference age 回呼 | `rngState` 用 `std::random_device` 或時鐘；AI 走 reference age |
| G11 既有 `authority_digest` | 持續 | E-9 的守衛（`tests/object_fps_pvp/CMakeLists.txt:395-444`）；加新種類的批不得改 golden 檔 | — |
| G13 `ActionStartCombatIsTickStart` | split（M5 存活後加，`v8-product-match-entity-split-m5-g13`） | 記錄型假命中模型斷言 `Shapes()` 收到的 combat 是 Tick 開頭的值（第一槍的紀錄，不是預設也不是第二槍的結果）。digest runner 的合成命中模型不讀 combat，所以 action-start 的正確性只能在 cpu 測試守 | M5b `actionStartCombats_` 只複製一次；`TickShapes` 改讀即時 `combats_`（既有 `s2-combat-after-actions`） |
| G12 `NoColumnMutationInsideTick` | 第一個非玩家批 | `InsertEntity`／`EraseEntity` 的 `GYO_ASSERT(!inTick_)`；待辦在固定階段套用 | 在 `ResolveActions` 內直接 Insert／Erase |

## 8. pv10 附錄 C 的改寫建議

原 protocol-v8 附錄 C（:328-340）；pv9 §0 說附錄 C 照舊只記形狀。每行標依據的條文。

1. **「Boss、小怪」列**改成：「Boss 與小怪都是實體，不是專用訊息（E-1、E-4）。Boss＝Body＋Combat＋Animation＋Brain＋BossPhase；身體、姿勢、動畫和玩家一樣走元件的 repeated，受 100 m interest 過濾。血量公開（問卷 3-3），所以 `BossPhase{entity_id, phase, shield_milli, hp_milli, weak_points}` 用 WorldSnapshot runtime 6／client 7 的 `repeated BossPhase boss_phase`（原預留 BossState 的號碼，用途收窄為元件），全員、不過濾；每筆的 entity_id 必須出現在 Body 的 repeated 裡。換階段、擊倒走事件。小怪＝Body＋Combat＋Animation＋Brain，沒有 Connection；不再有『每人 snapshot』的另一套形狀。」刪掉「前提是產品有 PvE」（撤離模式已是 PvPvE）與「BossState」這個名字。
2. **`ShotHitKind` 3 `HIT_NPC`**：從預留表移出，標「撤回：命中只用 `hit_entity_id`（ShotDecision 6，原 target_id 同號改名），種類由 Client 從已知實體查（問卷 2-3）；號碼 3 永久 reserved 不挪用」。`HIT_PLAYER`→`HIT_ENTITY`（值 2 不變）；10 `target_life_generation`→`hit_life_generation`；`CombatState.last_attacker_id` 值域變成 EntityId（可能是 AI）。未決：門、容器的 `hit_part`。
3. **`ShotDecision` 13 `item_instance_id`**→`item_entity_id`：被動作作用的世界物品的 EntityId，沒有時 0。物品進背包後離開世界（問卷 2-4），背包內的格子身分屬於 Inventory 元件的私人命名空間（`InventorySlotId`），不是 EntityId；丟回地上時 Match 配新的 EntityId（E-3 不回收）。兩人同時撿同一件依交接順序，後到回 ITEM_GONE（附錄 B-3），平手鍵 (A, EntityId, ActionId)（E-6）。此列待背包功能批定案。
4. **`RuntimeEnvelope` 22、23**：依決定 5-ii。(a)：「不挪用，永久 reserved。世界實體與私人 latest-wins 搭在既有 WorldSnapshot（16）的每收件者份；可靠事件（出現、消失、門、Boss 換階段、擊殺）與私人可靠狀態（背包基準加差量）走附錄 B-1 的 MatchEvent 串流，可見性＝全員／本人；理由：沿用 16 的有界佇列、coalescing 與 DropReason，Gateway 不另立狀態」。(b)：22＝`WorldEntityEvents{EntityAppeared{entity_id, kind, catalog_id, position, yaw, container_entity_id}, EntityRemoved{entity_id, reason}, baseline}`、23＝`PrivateState{owner_entity_id, baseline | delta}`；兩者的 id 都在同一空間；可靠送達照 B 層的 `events_acknowledged_through`。
5. **「防透視、遠處降頻、20 隻 AI、8 人房」列**補成：「Match 每 Tick 一則 WorldSnapshot，每位收件人一份（recipient_id＝連線實體的 EntityId）；Gateway 依 recipient_id 逐欄複製到對應 session，單則訊息內檢查（recipient_id 不重複且在玩家段、各 repeated 的 entity_id 非零不重複、combat 與 animation 的 id 都在同一份的 body 裡且 life 一致）。interest＝對 Body 元件依距離過濾（100 m），物品 30 m 由事件串流的可見性決定；interest 集合是 Match 的派生暫存、priority accumulator 是送出排程狀態，都不進 digest（E-8）。容量：4 人契約最大 40＋211N B、datagram 1200 B，N≥6 超過，所以每份要由 Match 依預算選實體，切分不交給 Gateway。守衛：interest 參數不改變權威 digest（G8）。『是否算 Gateway 允許類別的擴充』：recipient_id→session 已屬 classConnection，只多逐欄複製與單則檢查——由使用者判定（決定 5-i）。」
6. **「地上物品、容器、屍體」列**補充：它們是帶 Placement 元件的實體（Match 段配 id）；出現與消失是事件；進房與重連的基準＝事件串流開頭的 EntityAppeared；屍體是另一個實體（問卷 2-2），死亡時由 Match 配新 id，不沿用玩家的 id。
7. **新增「實體 id」列**：單一 EntityId 空間，uint32；[1, PlayerEntityIdLast] 由 Gateway 配發（`player_ids_exhausted` 的上限從 UINT32_MAX 降到 PlayerEntityIdLast，`player_id_ceiling_test` 跟著改），[MatchEntityIdFirst, MatchEntityIdLast] 由 Match 配發；Client 不配；投射物、物品、屍體、門都佔用 id；投射物只有生成事件與 id。這也改寫 pv8 §2a 用盡表 `gateway/server.go` 那一行（:104）。
8. **「不採用」列**加上：Client 配 id；由 Gateway 依內容做 interest 過濾；（決定 5-ii (a) 時）22／23 的新訊息。

## 9. `v8-product-match-entity-split` 的步驗序與驗收

檔位照 README：high，digest 與迭代順序局部 xhigh；上段；實作者主會話 Fable 5.1、審查者 Opus 5.5 xhigh、§14.4 檢證者 Opus 5.5。

### 9.0 前置：孤兒 animation 缺陷（決定 3）

- 現象（已逐行核對）：`MatchRuntimeHost::Advance` 內 `state = match_.Snapshot()`（`MatchRuntimeHost.cpp:447`）→ `TrackSlack`、`JudgeConnectionQuality`（:450-451）→ 逐出時 `match_.Leave` 後只 `erase_if` players 與 combat（:756-760）→ `publication = std::move(state)`（:472）→ IpcHost 照三個 vector 編碼 → Gateway `len(in.Animation) != len(in.Players)` 回錯（`adapter.go:132-134`）→ `runtimeFailed`，所有人斷線（`server.go:704-707`、`:1131-1151`）。`RequireValidAnimation` 只在 `PvpMatch::Snapshot()` 內，蓋不到 host 之後的刪除。
- 證據狀態：偵察以另一棵樹的 runner dump 到 host-poor-connection/corridor 的 record #2399 是 players=1、combats=1、animations=2，表示 golden 已鎖進這個狀態；該 dump 在 scratchpad，**不算證據**，要在本工作樹重新 build runner、重跑 `--dump`，存到主 checkout `build/evidence/v8-product-evict-animation-orphan/`。真正 Gateway 上的 runtimeFailed 未重現。
- 建議：另開缺陷批 `v8-product-evict-animation-orphan`（high，3～5 h）。照 §14.3：(1) 重跑 dump 重現；(2) 寫 G3 確認 Red；(3) 加 `EraseEntity(WorldSnapshot&, EntityId)`，host 改用它，發布前加 `RequireValidSnapshot`，確認 Green；(4) 事前宣告「只有 host-poor-connection/corridor 這 1 行 golden 改變；compare 的第一個差異是 record #2399 的 `animations` 2→1；其他 41 個情境 0 差分；DigestVersion 不升」。
- **golden 的重生有「緩める面」**（敵對檢查 must 2）：舊 golden 判新輸出失敗、新 golden 判它通過，照 §14.4 的定義就是緩める面，不能當「只締緊」由 Agent 先做。要寫 1～5（守的性質＝發布的 snapshot 各元件 id 一致；緩める面＝只有 record #2399 的 animations 2→1；守著同一性質的其他判定＝G3 與 `RequireValidSnapshot`），核准前做突變測試並請 Opus 檢證者以實際資料核對，拿到使用者核准後才重生 golden。
- **與 split 驗收的 base**（must 1）：決定 3 選 (a) 時，split 的 base＝缺陷批的 commit（不是 master 的 merge-base），§9.2 第 2、4 項的比較對象也是缺陷批 commit，並寫明 master 到缺陷批之間只有 host-poor-connection 這一行 golden 改變。選 (b) 則 base＝merge-base。base 的 commit 寫進 HANDOFF 的事前宣告。

### 9.1 步驗序（每步 build、跑 `authority_digest` CTest、再跑兩樹 compare；任一步不是 0 差分就停下照 §14 回報）

- **S0 事前宣告與基準**：寫進 HANDOFF 並取得同意（§9.2）。在 base commit 另建一棵樹的 runner（-O0 與最佳化兩種），記錄 `--list` 是 42 個情境、37 個 golden（`AuthorityDigest.cpp:740-843`；`authority_golden.txt` 非註解 37 行）。證據放主 checkout `build/evidence/v8-product-match-entity-split/`。
- **S1 型別**：加 `EntityId.hpp`、`EntityColumns.hpp`（FindEntity、InsertEntity、EraseEntity）與它們的單元測試。行為沒用到；不跑 compare，但記錄「未跑」。段界常數與配發器不在 split（第一個非玩家批）。
- **S2 輸入來源**：`Participant` 的 retiredActionThrough、actions 搬進 `ActionLedger`，其餘搬進 `Connection`，兩欄都是升冪 vector；Join 時 Insert、Leave 時 Erase；相關私有函式改簽名。預期 0 差分。
- **S3 元件欄**（局部 xhigh）：`players_` 拆成 `bodies_`、`combats_`、`animations_`；16 處使用逐一改成走欄或 FindEntity（§3.3，連線相關 API 查 `connections_`）；Leave 改呼叫 `PvpMatch::EraseEntity`。G1、G2 先寫先跑。預期 0 差分。
- **S4 歸位暫存**：`actionsCombat_`→`actionStartCombats_`、`shapes_`→`tickShapes_`；Reset 清掉五欄與快取。預期 0 差分，特別看有開槍的 combat 與 hitbox 情境。
- **S5 Snapshot**：依 `bodies_` 順序以 FindEntity 組三個 vector，加 `RequireEntityOrder`。compare `--scale 1` 與 `--scale 10` 都 0 差分；golden 不變；DigestVersion 仍 4。更新本文件的「實作現況」。
- **S6 突變**（只跑相關）：M1 InsertEntity→push_back；M2 EraseEntity→swap-and-pop；M3 漏刪 animations_；M4 Snapshot 反向走欄；M5 `actionStartCombats_` 改在動作之後才複製。期待：M1、M2 被 G1；M3 被 G2；M4 被 G1 與 golden；M5 被 hitbox 的 golden 情境與 compare。紀錄放 `build/evidence/`。（2026-11-06 註：M1、M2 已退役：欄的機制移到 Engine `GYO::EntityStore`，由 Engine 突變目錄的 `es-row2-insert-appends`、`es-row2-erase-swap-and-pop` 以 common 測試 `gyo_entity_store_tests` 守；使用者決定，`v8-arch-entity-store-impl-3a`。本項是當時的紀錄，不改寫。）
- **S7 文件**：dev_log、計畫 README 一覽、HANDOFF；同一變更提交。

### 9.2 驗收（結果出來前宣告，取得使用者核准）

報告開頭一行：`件數の突き合わせ：一致（base 與 branch 情境數 42＝42；每個情境的 record 數相等，由兩樹 --run --ticks 輸出的 json 各情境 records 欄位逐一比對，AuthorityDigest.cpp:938）／不一致（…）／確認できない（…）`。一般模式的 compare 只比 digest、只在不同時才算 record 數，所以件數要另外從 json 讀。

1. `compare_authority_trees.py --base <S0 runner> --branch <S6 runner>`，`--scale 1` 與 `--scale 10` 各一次，exit 0，42 個情境全部 0 差分；不得用 `--scaled` 或 `--added`。
2. `authority_digest` CTest 通過；`git diff <base> -- tests/object_fps_pvp/fixtures/authority_golden.txt` 為空。
3. `DigestVersion = 4`、`DigestFields` 不變；dump 標籤不變：兩樹對全部情境 `--dump` 再 diff 為空（一般模式的 compare 不比標籤）。
4. `git diff --stat <base> -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空；Client 端原始碼為空；MatchRuntimeHost、IpcHost 只允許因簽名改動而跟著改的行。
5. G1、G2、G7 Green；M1～M5 全部被殺。
6. L1 的 cpu|shader 全部通過。
7. 範圍的限界：效能不量；network 標籤只在本機；孤兒 animation 若決定 3 選 (b)，寫明 host-poor-connection/corridor 的 golden 仍含該狀態。
8. AGENTS §3 七點（§11）。

估時：split 14～20 h（README 12～18 h；加 G1、G2、G7、RequireEntityOrder、5 個突變、`--scale 10`）；若決定 4 選 (b) 捆綁 map 則 8～12 h。缺陷批 3～5 h。pv10 wire 批粗估 40～60 h、第一個 AI 批（PlayerState 拆欄、配發器、digest v5、Brain、E-12 待辦）粗估 15～25 h，都未精估、不在本批。

## 10. 與 hitbox-rewind 姿勢歷史的接法（rewind-line）

- 新欄 `std::vector<PoseHistory> poseHistories_`，升冪；`PoseHistory{EntityId id; std::array<PoseSample, 16> ring; std::uint8_t newest;}`，容量 R＋1、R≤15（protocol-v9 §5，:112）。
- `PoseSample{tick, position, yaw, pitch, crouched, grounded, movementEpoch, lifeGeneration, lifeState, lifeStateTick, CombatPose 的 7 個欄位, PlayerAnimation}`：就是 protocol-v9 §5 規定的歷史欄位（:116）。
- 寫入：Tick 結尾 `AdvanceAnimation` 迴圈之後，用 `bodies_`、`actionStartCombats_`、`animations_` 合併各寫一筆。刪除：`EraseEntity`（E-7 自然涵蓋）。清空：Reset。
- 讀取只經 `ShotRewind` 單一入口（protocol-v9 §7）；守衛分兩段核准。命中形狀由 PoseSample 經 `hitModel_->Shapes` 重建；`TickShapes` 變成「τ＝J 的特例」。
- 不進 snapshot、不進 digest（E-8）；G9 證明等於 snapshot。回溯會改變裁決、digest 跟著變，那是 rewind 批自己的事前宣告，與 split 無關。
- `animationState`（一 Tick 歷史）在 split 保留在 `AnimationTrack::before`；rewind 批若改成讀 history[t−1]，要另外證明 0 差分。
- AI 進來時有 Body＋Combat＋Animation 就自動有 PoseHistory，回溯不必改（問卷 4-4）。
- 現況：程式裡沒有 Rewind／PoseHistory；hitbox-plan 的 R6 寫「沒有」；Match 唯一的歷史是 `animationState`。README 把 split 排在 rewind 之前，正是為了讓姿勢歷史第一個以元件寫。

## 11. Architecture Delta（AGENTS §3 七點）

三個 Delta：(A) 新增本文件的 `Design Invariants` 節；(B) Match 內部的 Ownership 重整（`v8-product-match-entity-split`）；(C) Data Contract 的未來變更（pv10 建議，本批不實作）。

1. **哪個 Feature 需要**：迷你塔科夫的第一個非玩家實體（AI 小怪，之後 Boss、投射物、地上物品、容器、屍體、門），以及 hitbox-rewind 的姿勢歷史（README「split 排在 rewind 之前，讓姿勢歷史第一個以元件寫」）。明示的目的：架構例行掃描 2026-11-01 開的卡 `v8-product-entity-model`。推導照 AGENTS §2：Feature → Contract（EntityId 空間、元件清單、複寫通道、決定性；pv10 建議）→ Architecture（Match 內部的欄佈局與唯一刪除入口）。(C) 只支撐已點名的 Feature，⑤ 的對局規則不在內。
2. **現在的結構有什麼問題**：`PvpMatch::Participant` 把實體元件、輸入來源與診斷捆在同一個結構，鍵是 PlayerId；非玩家實體沒有連線卻會被迫帶著這些欄位，或另開一套容器（Responsibility 重複、Ownership 曖昧）。同一個「按 PlayerId 的 find／insert／erase」在 `players_`、`shapes_`、`actionsCombat_` 與 host 的 map 重複出現（同一 Conversion 的重複）。元件陣列「各刪各的」已造成實證缺陷：host 逐出時只刪 players 與 combat、沒刪 animation（`MatchRuntimeHost.cpp:759-760`），Gateway 的同長檢查會讓整個 Gateway 失效（Lifecycle 的衝突）。`PlayerState` 混著身體、生命、輸入游標與 host 診斷欄位，host 在 Snapshot 之後改寫其中欄位（State Authority 的張力；本批只分類不拆）。「按 id 升冪的平行陣列」在快照層已存在，但只由 `std::map` 隱含保證，沒有條文、沒有守衛。需求要求單一 EntityId 空間，而現在 `player_id` 由 Gateway 配、Match 不知道誰配的；非玩家 id 沒有配發者。
3. **哪個 Architecture Boundary 變化**：(A) 新增 owner 層級的 Design Invariants 節。(B) `PvpMatch` 內部：Participant 拆成「實體元件欄」「輸入來源欄」「Tick 內派生暫存」三類；刪除收成單一入口。公開介面、`WorldSnapshot`、wire、digest 都不變。(C)（未來，pv10）：Match↔Gateway↔Client 的 Data Contract——`player_id` 的語意擴成 `entity_id`、快照成為每元件一個 repeated 且含所有有該元件的實體、每收件者一份框架、Gateway 的玩家 id 上限常數、`HIT_PLAYER`→`HIT_ENTITY`、CombatState 的公開／私人拆分。Gateway 仍是無狀態邊緣。
4. **影響的 Module／Product／Tool**：Product `object_fps_pvp`（新檔 EntityId.hpp、EntityColumns.hpp、Match 端私有 EntityAllocation.hpp；PvpMatch.hpp／.cpp；缺陷批另改 MatchRuntimeHost.cpp）；測試 `tests/object_fps_pvp/`（新檔 EntityModelTests.cpp 與掃描腳本；AuthorityDigest.cpp 在 split 不改）；文件（本文件、protocol-v8 附錄 C 的改寫以 pv10 文件承載、plans/v8 README、HANDOFF、dev_log）。(C) 之後：proto、gateway/、Client 端。不碰：`engine/`、`services/gyo_gateway/`、其他產品。（2026-11-05 註：「不碰 `engine/`」由 entity-store Delta（已核准）取代：欄的機制移到 `GYO::EntityStore`，`match_domain` 公開連結它；本項是當時 Delta 的紀錄，不改寫。）
5. **Dependency Direction**：不變。Engine 不加 ECS；所有新型別與函數都在 `apps/object_fps_pvp`。（2026-11-05 註：「Engine 不加 ECS」由 entity-store Delta（已核准）取代：新增 `object_fps_pvp`（`match_domain`）→ `GYO::EntityStore` → `GYO::Base` 的邊，[entity-store 契約](../architecture/entity-store.md)；本項是當時 Delta 的紀錄，不改寫。 2026-11-12 註：Kernel 進 Engine 的 Delta（spec K.3 第 7 項）再次改變此邊：目標見 `docs/architecture/target/`。）Client 端原始碼已經 include `PvpMatch.hpp`；split 不新增任何 include 方向，段界常數與配發器放 Match 端私有標頭讓 Client 無法引用（G6）。
6. **Ownership**：`PvpMatch` 仍是唯一的玩法權威；新增一項權威：Match 段 EntityId 的配發（第一個非玩家批）。Gateway 繼續擁有玩家段的配發，只是上限常數收窄；不持任何對照表。`Participant` 的欄位依 owner 歸位：元件欄＝實體的權威；ActionLedger、Connection＝輸入來源（Connection 只屬於連線實體）；quality＝診斷；tickShapes_、actionStartCombats_＝可重建的派生暫存。`PlayerState` 內的游標與 host 診斷欄位：owner 分別標為 Connection 與 MatchRuntimeHost，暫存在 Body 型別裡（過渡期）。刪實體的責任收到 `PvpMatch::EraseEntity` 一處；host 對 snapshot 列的刪除收到 `EraseEntity(WorldSnapshot&, id)` 一處。
7. **為什麼更小的變更解決不了**：只在 `Participant` 加 optional 欄位或 `EntityKind` 分支來容納 AI，每個系統都得 `if (kind == …)`，違反「種類是標籤」，非玩家實體會帶著連線狀態。只加註解不拆欄：E-5 與 E-7 沒有可檢查的落點；孤兒 animation 這類缺陷會再出現。只在 host 補一行刪 animation：修了症狀，但「各刪各的」的結構仍在。不寫 Design Invariants：決定性契約只存在對話裡，跨多個 Agent 與多個版本守不住。另一方面，本批刻意**不做**更大的變更：不拆 PlayerState、不在 split 加種類表、不做通用 EntityColumn 類別或 registry、不改 wire、不在 Engine 加任何東西；pv10 只給方向。

## 12. 決定記錄（2026-11-01 `v8-product-entity-contract-approve`：8 項全部照建議 (a)）

原本提給使用者的選項、機制、取捨與證據照原樣保留，作為決定的根據。採用的選項在各項的「建議」。

- **決定 1：EntityId 段界與 Match 段上限。** (a) 玩家段上限 2^24−1，Match 段 [2^24, 2^28−1]：每個非玩家 id 在 wire 上**契約最大也是 4 位元組**（varint 到 2^28−1），Match 段約 2.5 億、需求 340 萬、約 74 倍餘裕；玩家 256 倍。(b) 玩家段上限 2^24−1，Match 段到 UINT32_MAX：典型值 4 位元組，但契約最大 5 位元組，capacity_test 以 MaxUint32 算預算時省不到。(c) 玩家段上限 2^31−1，Match 段從 2^31 起：契約最大 5 位元組。建議 (a)。機制：段界是 Match 端私有標頭的一組常數，Gateway 的 `player_ids_exhausted` 上限與 `player_id_ceiling_test.go` 跟著改，Match 的 Join 在 pv10 加值域檢查，兩邊常數用原始碼核對測試綴住。取捨：(a) 讓容量預算（契約最大）真的省下每個非玩家實體每 Tick 約 3 B（Body、Combat、Animation 各 1 B），20 隻 AI 約 60 B／份；餘裕從 1,250 倍降到 74 倍，仍遠超需求。證據：varint 規則；問卷結論；`capacity_test.go:42-52` 用 MaxUint32−i 當 id；`server.go:359`、`player_id_ceiling_test.go:19-27`。
- **決定 2：平手時玩家一律先於 AI／Boss，是否接受為有意的判定規則。** (a) 接受：寫進 E-6，改段界＝改判定＝升 digest version。(b) 不接受：要求平手鍵不依賴配發段——但 E-4 禁止以種類分支判定，任何總序都得回到某個 id，所以 (b) 實際上只剩「讓 Match 段與玩家段交錯配發」，會讓 Match 段失去「append 即排序」且 Gateway 上限要和 Match 協調。建議 (a)。取捨：(a) 是免費、可解釋的規則；玩家受益（PvP 優先）符合「AI 只打玩家」的玩法。證據：`PvpMatch.cpp:271-273`、`:411-414`；`ShotQuery.cpp:119-122`；問卷 4-6。
- **決定 3：孤兒 animation 缺陷批 `v8-product-evict-animation-orphan` 的排序。** (a) 排在 split 之前，同一條 entity-line PR、不同 commit；split 的 base＝缺陷批 commit；golden 重生照 §14.4 走核准（有緩める面）。(b) 排在 split 之後，split 的 base＝merge-base；E-7 在 host 層暫無守衛。(c) 當成 split 的已知例外、暫不修（不建議：留著一個會讓整個 Gateway 失效的路徑）。建議 (a)。證據：`MatchRuntimeHost.cpp:447-451`、`:756-760`、`:472`；`adapter.go:132-134`；`server.go:704-707`。證據狀態：偵察的 dump 未保存，要在本工作樹重跑；真正 Gateway 的 runtimeFailed 未重現。
- **決定 4：split 批的 Match 內部落點。** (a) 每元件一欄、依 EntityId 升冪的 `std::vector`＋`EntityColumns.hpp` 三個自由函數；估 14～20 h。(b) 保留 `std::map`，Participant 只拆成 `std::map<EntityId, EntityComponents>`（捆綁）與 `std::map<EntityId, Connection>`；估 8～12 h；第一個非玩家批再拆成每元件一欄。建議 (a)。取捨：(a) diff 大、S3 要局部 xhigh、必須有 G1 的降冪 Join 單元測試；(b) 0 差分幾乎無風險，但 split 交付的不是起點要求的資料佈局，等於搬兩次家，且要多一條 RC 防捆綁結構長成大結構。證據：`PvpMatch.hpp:116-137`、`:165-171`；README 估時。
- **決定 5-i：pv10 的每收件者一份＋Match 依預算選實體。** 兩案都需要（§4.3 的容量）。請一併判定「Gateway 依 recipient_id 逐欄複製到對應 session、單則訊息內檢查」算不算 Gateway 允許類別的擴充（protocol-v8 附錄 C 已點名由使用者判定）。建議：不算（recipient_id→session 已屬 classConnection，`state_whitelist_test.go:73`，不加狀態、不跨訊息）。
- **決定 5-ii：非玩家實體進同一個 repeated，還是另開欄位。** (a) 「每個元件一個 repeated」推廣到所有有該元件的實體，22／23 永久保留，事件走 B-1（§4.4）。(b) `players`／`combat`／`animation` 照舊只給連線實體，新增 `bodies`、`body_animation`，22／23 給世界實體事件與私人狀態。建議 (a)。取捨：(a) 符合起點 2，但 Client 的 SnapshotTimeline、PlayerPresentation、Gateway 的 ≤4／同長檢查都要重寫，粗估 40～60 h；(b) Client 既有路徑改動最小，但違反「每個元件一個 repeated」，22／23 要另立 Gateway 佇列與 DropReason。證據：`ClientConnection.cpp:601-602`、`:639-664`；`adapter.go:79`、`:132`；`PvpApplication.cpp:1041`。
- **決定 6：Match 段 id 用盡的機制。** (a) Runtime Error：配發器回 Result，那一次生成被拒並計入診斷（`entity_ids_exhausted`），局照常進行，不回繞。(b) 沿同檔先例拋 `std::overflow_error`，和 life、epoch、tick 用盡一致（`PvpMatch.cpp:276-279`）。建議 (a)。機制：依 error-handling 的 Status 行，產品新程式照 Assert/Result 規則；「程式正確但環境讓它失敗」是 Runtime Error。取捨：(a) 要定義「被拒的生成」對玩法的意義（AI 不出生、投射物只有射擊沒有軌跡 id），G4 要測 Result；(b) 實作最小，但新 site 沿用舊機制。兩者在 74～1,250 倍餘裕下幾乎不會發生，是分類原則的選擇，影響小、可逆。
- **決定 7：Replacement Criteria 的門檻數值。** (a) 照提案值：RC-1 同時實體 >4,000 或 Linux Match 實測欄維護每 Tick p95 >1 ms；RC-3 種類 >16；RC-4 加一種實體要改 >3 個判定函數；RC-4b 加一個可複寫元件要改 >6 處。(b) 另定數值。建議 (a)。取捨：太鬆讓模型帶病修補、太緊讓正常演進被誤判為置換；提案值取需求的 10 倍與 Tick 預算的 6%，都是未量測的提案。
- **決定 8：本文件的 Design Invariants 節（E-1～E-12）與 Replacement Criteria 核准為 Architecture Delta。** 已核准（使用者原話「照建議核准」）；split 批可以開工。E-12 與 E-4 的「連線 API 查 connections_」是敵對檢查補進來的。

## 13. 風險與未查證

- 段界讓玩家在所有平手上永遠先於 AI／Boss；這是玩法結果，需使用者明示接受（決定 2）。
- 孤兒 animation 的 dump 屬未保存證據，必須在本工作樹重跑；真正 Gateway 的 runtimeFailed 未重現。
- Gateway 上限改常數是 pv8 §2a 表的契約變更，連同 `player_id_ceiling_test` 都要經使用者同意（pv10）。
- Match 計數器用盡的單位是 link 世代，需求寫程序；結論不變，條文已明寫；需求原意未查證。
- PlayerState 混欄在 split 不拆；AI 以 brain 產生帶序號命令讓 `lastResolvedCommand` 有意義是推論，第一個 AI 批驗證。
- Match 種子來源不存在，AI 批要定，否則 E-10 無法落地。
- pv9 CombatState 全公開與問卷 3-3 衝突；pv10 若只加實體不拆 CombatState，私人通道被現況架空。
- 容量 40＋211N B 是推論，未以 `proto.Size` 實測。
- 排序 vector 的 insert／erase 成本是推論，RC-1 門檻是提案值。
- split 的 0 差分只有 2 人情境，偵測不到 3 個以上實體的順序錯誤，所以 G1 必要。
- 有了 interest，Client 可能收到不在自己 snapshot 裡的 `hit_entity_id`；`rules_.shotRange` 與 100 m 的關係未查證。
- E-4、E-11 的「依內容的判斷」測不到，只能靠審查。
- 行號：敵對檢查抽查約 30 處，大部分屬實；有偏移的已照它的更正改寫（protocol-v9 :134、:112、:116；network-architecture 的 DI 表位置；protocol-v8 :47；MatchRuntimeHost.hpp:267）。定稿前以 HEAD 逐一重核。

## 敵對檢查的處置

| 嚴重度 | 發現 | 處置 |
|---|---|---|
| must | 決定 3(a) 與 split 驗收的 base／golden／diff master 互相矛盾 | 採修正：base＝缺陷批 commit，§9.0、§9.2 改寫 |
| must | 缺陷批重生 golden 依 §14.4 有緩める面，不能當「只締緊」 | 採修正：§9.0 明寫要 1～5、突變、檢證者、使用者核准 |
| must | 條文沒規定 Tick 內何時可改欄的成員，vector 讓參照失效 | 新增 E-12 與 G12（§5.2） |
| should | §2.1 的 combat-rejoin 出處不支持「玩家 id 可能小於現存」 | 採修正：理由改成「有 Match 段後新玩家 id 一定小於 AI」＋G1 |
| should | G2 用公開 API 殺不掉漏刪 animations_ 的突變 | 採修正：friend 存取器＋Join(1) 重複 Assert |
| should | G8 在構造上是空的（runner 不經過 ViewFor；arena ≤20 m） | 採修正：驅動 host 發布＋ViewFor，半徑 0／1 m |
| should | AI 走同一條射擊路徑會引入 host 回呼的牆鐘 | 採修正：沒有 Connection 的射手不量 reference age（E-10、§3.2） |
| should | §4.3 既說 golden 不變又說拆欄會改 digest 欄序 | 採修正：編碼器重組舊欄序（E-9） |
| should | compare 一般模式不比對標籤與 record 數 | 採修正：`--dump` diff；件數從 json 的 records 欄位讀 |
| should | 缺 EnTT／flecs 的對照匯總（引用指向不存在的 §10） | 採修正：§1 完整寫出 |
| should | 公開 API 查哪一欄沒規定，AI 批後 Leave(aiId) 會刪 AI | 採修正：連線 API 一律查 connections_（§3.3、E-4） |
| should | RC-4 定義會在第一個 Boss 批誤判 | 採修正：RC-4 限判定函數、RC-4b 另立複寫門檻 |
| should | 決定 5 把容量與 repeated 形狀兩軸混在一起 | 採修正：拆成 5-i、5-ii |
| should | 決定 1 的「省 60 B」在契約最大不成立 | 採修正：加選項 (a) Match 段上限 2^28−1 |
| should | G6 依名字掃描擋不住換名字的違反 | 採修正：段界常數與配發器放 Match 端私有標頭 |
| note | 行號偏移約 10 處 | 已改寫；定稿前重核 |
| note | 「8 份接近 MaxFrame」不成立 | 已刪，改成 ≤9.6 kB |
| note | priority accumulator 不是 Tick 內暫存 | 已在 E-8 另列為送出排程狀態 |
| note | E-4 讀常數的位置不只 Join 與配發器 | 已改寫 |
| note | 重連、離開再加入的 id 行為沒寫 | 已補進 §2.2 |
| note | AI 的 digest 讀產品資料會讓 golden 隨內容變 | 已寫：合成 AI 規則與 catalog |
| note | G1 的 3,1,2 不是降冪；shapes.at 的 Assert；contiguousPendingCommands 的分類；kind／catalog_id 用 25 號以後；HIT_PLAYER 改名影響 mutations.json；移動迴圈並行會改 TraceMovement 順序 | 全部已改寫 |
