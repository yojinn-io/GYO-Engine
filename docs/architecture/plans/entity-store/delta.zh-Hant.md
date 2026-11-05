# 實體容器（entity store）：Architecture Delta

更新：2026-11-05（`v8-arch-entity-store-delta`）。Owner：Engine（架構）。
**狀態：提案，未核准。** 本文件是 AGENTS §3 的 Architecture Delta 紀錄；契約本體與 `Design Invariants`、`Replacement Criteria` 在 [entity-store.md](../../entity-store.md)（英文，下稱契約）。核准時由使用者決定 §7 的各項。
根據：[survey.zh-Hant.md](survey.zh-Hant.md)（batch 1，已核准；下稱 survey）、[entity-model.zh-Hant.md](../../../object_fps_pvp/entity-model.zh-Hant.md)（E-1～E-12、RC-1～RC-5，已核准）。行號以工作樹 `8bfc1335` 為準。契約不引用產品的檔案與行號；產品側的對應、守衛與行號集中在本文件（§1 第 4 項、§4、§6）。

## 0. 摘要

- 建議「容器版」：Engine 只收 `EntityId` 鍵型別、讀鍵的 trait `EntityKey<T>`、欄的四個函數（Find／Insert／Erase／StrictlyAscending）與延後結構變更的待辦 `DeferredChanges`。不收 generation、change tick、配發器、種類、world、scheduler 等。
- 建議「新建 Engine 模組，pvp 改裝」，不直接搬檔：現行兩個標頭含產品知識（`HasPlayerId`、`PlayerId`、`NoEntity` 的語意）。
- 搬進 Engine 的根據只有使用者的決定；技術上留在產品對唯一的消費端同樣有用（§1 第 2、7 項）。

## 1. AGENTS §3 的七點

1. **哪個 Feature 需要**：
   - 明示的目的：使用者 2026-11-05 的決定「ECS 的容器放進 Engine 共通模組（案 (b)）」。
   - 支撐的 Feature（AGENTS §2 的推導順序）：`object_fps_pvp` 的第一個非玩家實體（AI 小怪，依賴 pv10）需要 E-12 的待辦；之後的 Boss、投射物、物品、屍體同樣走這條路（entity-model §0、§3.2）。現有五欄與兩個 Tick 快取是已在運作的消費端。
   - 本 Delta 本身不交付新的玩家可見行為：它是 Ownership 的搬移加上契約。
2. **現在的結構有什麼問題**：

   | 壓力 | 類別 | 根據 |
   |---|---|---|
   | 使用者決定把容器放進 Engine | 決定（明示的目的，不是 AGENTS §2 的 evidence） | README「來源」、HANDOFF「決策」 |
   | `docs/architecture.md:129` 禁止 ECS，與決定衝突；survey 六次引用同一句當不收 world／plugin 的根據 | 決定的結果 | `architecture.md:129`；survey §5「範圍外」段 |
   | 容器的內容已與產品無關，Ownership 卻在產品：`EntityColumns.hpp` 的演算法只依鍵運作，產品知識只有 `HasPlayerId`（過渡期欄位名）與命名空間 | 觀測（唯一獨立於決定的一項）；但它不是 AGENTS §2 列舉的 valid evidence 種類（沒有重複、循環、Lifecycle 衝突或洩漏；最接近 AGENTS §2 的「Ownership 曖昧化」，但 Ownership 並不曖昧，只是位置與內容不符） | `EntityColumns.hpp:21-36`、`:47-90`；entity-model §3.1 |
   | 第一個非玩家實體需要 E-12 的待辦與套用 | 預測（依賴 pv10，尚無程式） | entity-model 檔頭「實作現況」、§5.2 |
   | pv10 每收件者分送可能要 change tick | 預測（survey §3c 判定第一版不採） | survey §3c |
   | 第二個產品使用容器 | 預測（沒有具名產品） | — |

   重複不是壓力：find／insert／erase 已由 PR #140 收攏在 `EntityColumns.hpp` 的單一實作；`PvpMatch.cpp` 的 30 行是呼叫處，不是重複。
   消費端只有一個（pvp）。AGENTS §1 禁止從單一 use case 做新的通用抽象；所以 Engine 模組**不比 pvp 現在用到的更一般**：欄的函數是現有形狀的搬移，唯一的新程式 `DeferredChanges` 的時機見 §7 決定 2。
3. **哪個 Architecture Boundary 變化**：
   - 新增 Engine 模組 `GYO::EntityStore`（header-only，`GYO::Base` 之上的葉，形狀同 `GYO::Math`，`engine/math/CMakeLists.txt`）。
   - 「欄的排序機制」（升冪、lower_bound 插入、保序刪除、二分查找、讀鍵）的 Ownership 從產品移到 Engine；E-5 的機制部分改由 Engine 的守衛測試保護（§6）。
   - 新增 Engine 的 `Design Invariants` 節（契約）。
   - `docs/architecture.md:129` 的 ECS 禁止收窄（§5）。
   - Data Contract（wire、WorldSnapshot、digest）不變。
4. **影響的 Module／Product／Tool**：
   - Engine：`engine/entity_store/`（新）、`engine/CMakeLists.txt`（加一行 `add_subdirectory`，放在 math 之後，`:4-5`）、`tests/common/entity_store/`（新）、`tests/common/CMakeLists.txt`（加一行，放在 time 之後，因為連結檢查沿用 `tests/common/time/CMakeLists.txt:22` 的 `gyo_expect_links`）、本資料夾 `scripts/mutations.json`（新）。
   - 產品 `object_fps_pvp`：
     - `EntityId.hpp`：改成 Engine 型別的別名，保留 `PlayerId`、`NoEntity`。
     - `EntityColumns.hpp`：變成轉接。`HasPlayerId` 型別的讀鍵改成 Engine `EntityKey` 的 concept 限定部分特化（不靠 ADL）；`HasEntityId` 版的 `EntityOf` 多載**刪除**（Engine 的預設已讀 `id`）；以 using 宣告引入 Engine 的 `FindEntity`／`InsertEntity`／`EraseEntity`／`StrictlyAscending`／`EntityOf`。
     - `apps/object_fps_pvp/CMakeLists.txt:27-29`：`match_domain` 公開連結 `GYO::EntityStore`（`PvpMatch.hpp:3` include 它，Client 端也 include `PvpMatch.hpp`）。
     - 保留函數名時，`PvpMatch.cpp` 中欄函數的 30 行呼叫不改（不含 `:83` 成員呼叫、`:86` 成員定義、`:674` 產品 snapshot 版的定義）。
   - 產品測試：`tests/object_fps_pvp/EntityColumnsTests.cpp`（77 行）的 `Fresh` 測試搬到 common（換合成型別），`Legacy`（`playerId` 轉接；在匿名命名空間，`:11-17`）留在產品，經 `EntityKey` 特化讀鍵；`EntityModelTests.cpp:24` 與 `EntityColumnsTests.cpp:26` 的 `EntityOf` 呼叫照用（using 宣告）。
   - 產品突變目錄 `tests/object_fps_pvp/mutations.json`：M1 `entity-split-insert-appends`、M2 `entity-split-erase-swap-and-pop`（`:8834-8853`）的 `file` 改到 Engine 路徑、`expect` 改成 common 測試名，搬到 Engine 的目錄；M3～M6（`entity-split-erase-skips-animations`、`-snapshot-reversed`、`-action-start-stale`（M5b）、`-respawn-reversed`）與 G1、G2、G7、G13 留在產品。
   - 產品文件（產品 owner 的 Delta，不在本線）：見 §6。
   - 不影響：Gateway、wire、digest、Client 行為、其他產品、工具、framework。
5. **Dependency Direction**：新增兩條邊，都往下：`GYO::EntityStore → GYO::Base`；`object_fps_pvp`（`match_domain`）`→ GYO::EntityStore`。不產生循環；Engine 不知道產品（AGENTS §6）。
6. **Ownership**：
   - Engine：鍵型別、讀鍵的規則、欄的排序機制、待辦的套用順序與其 Assert。
   - 產品：欄本身（`std::vector` 是 `PvpMatch` 的成員）、`DeferredChanges` 的實例（也是 `PvpMatch` 的成員；Engine 只提供型別）、`EntityKey` 對自己型別的特化、配發器與分段、種類、0 的意義、`PlayerId`、唯一刪除入口（E-7）、凍結區間與同步點的位置、digest、snapshot。
   - 狀態的權威（AGENTS §3）：Engine 模組不持有任何實例狀態；待辦清單是消費端成員裡的資料。沒有新增「另一個 Component 權威狀態的副本」。
7. **為什麼更小的變更解決不了**：
   - 技術上，留在產品對唯一的消費端同樣有用；搬進 Engine 的根據只有使用者的決定 (b)，其餘支撐它的壓力都是預測（第 2 項）。在這個前提下，「更小」的選項是：
   - 只改 `architecture.md:129`、程式留在產品：不符合決定 (b)。
   - 直接把兩個標頭搬進 `engine/`：`HasPlayerId`（產品過渡期的欄位名）、`PlayerId`、`NoEntity` 的語意與註解中的 E-1／E-2 會進入 Engine（AGENTS §6、§9）；測試也會留在產品（AGENTS §7）。
   - 只搬程式、不寫 Design Invariants：API 很小，正是要在長大之前寫好約束與置換條件的時候（AGENTS §3；survey §5「範圍外」段）。
   - 另一方面刻意**不做**更大的：不做持有多欄的型別、registry、world、scheduler、change tick、配發器（§3）。

## 2. 搬移 vs 新建＋改裝

| | (A) 搬移 `EntityColumns.hpp`／`EntityId.hpp` 進 Engine | (B) 新建 Engine 模組，pvp 改裝（建議） |
|---|---|---|
| 產品知識 | `HasPlayerId`、`PlayerId`、`NoEntity` 隨檔進 Engine，之後再拔 | Engine 只有 `EntityId`（`std::uint64_t`）與 `EntityKey` 的預設（`id`）；`playerId` 由產品特化 `EntityKey` |
| 讀鍵 | 現行兩個同名多載＋互斥 concept；搬進 Engine 後 Engine 內部的 `EntityOf(value)` 經 ADL 找不到產品的多載（例：匿名命名空間＋using-directive 的 `Legacy`） | trait 特化：特化永遠勝過主模板，同時有 `id` 與 `playerId` 的型別也不 ambiguous；不依命名空間 |
| 測試 | 產品測試測 Engine 程式；刪掉產品就失去守衛（AGENTS §7） | common 測試以合成型別守 Engine；產品測試只守自己的轉接與 G1、G2 |
| 演算法 | 不變 | 不變（同一組 lower_bound／insert／erase）；以 digest 前後 0 差分確認 |
| 呼叫端 | 改 include 與命名空間 | 產品以 using 宣告保留現名，30 行呼叫不改 |
| git 歷史 | 保留 rename 紀錄 | 新檔；舊檔變成轉接，歷史仍在產品路徑 |

建議 (B)：內容幾乎相同，但 (A) 會讓 Engine 先帶著產品知識再清除，而且現行讀鍵的寫法搬進 Engine 後無法運作；(B) 直接以 Engine 的名字與合成測試開始。差異只在歸屬，不在行為。

## 3. 容器版與「更多」：各加什麼

容器版＝契約 Components 的四項。下表是「更多」的候選；任一項都可以之後經它的 Replacement Criterion 加入，現在加都是預測的壓力。

| 候選 | 加什麼 | survey 判定 | 壓力 | 觸發它的 RC（契約） |
|---|---|---|---|---|
| B1 持有欄的型別 `Column<T>` | 讓「只能經 Insert／Erase 改成員」變成結構保證 | survey 未判定（本批提出） | 預測（消費端一個，沒有觀測到繞過的缺陷） | 14 |
| B2 持有多欄的型別 | E-7 的跨欄刪除進 Engine | 多欄型別 survey 未判定；§2.2 registry 列是「改裝後採」（容器、不接管迴圈照採；`ctx()`、signal、organizer 不收） | 預測；且要知道實體有哪些欄 | 13 |
| B3 change tick | 每元件每實體一條 changed tick | 不採（第一版，§3c 理由 a～c） | 預測（pv10） | 6 |
| B4 配發器與分段 | id 配發、range、用盡 | 不採（§4.1 第 2 列；根據只有 E-2） | 預測（無第二個產品） | 2 |
| B5 generation | 鍵帶 generation | 不採（§4.1 第 1 列；與 E-3 矛盾） | 無 | 1 |
| B6 合併／查詢輔助 | 雙指標交集、view | 不採（§4.1 第 4 列） | 預測（現在沒有手寫合併的重複） | 5 |
| B7 scheduler、world、resources、events、app、plugin | 通用執行期 | 不採（§2.1 各列、§5） | 無 | 7、8、9 |

建議：容器版。理由：B1～B7 都沒有觀測到的壓力；B2、B4 還會把產品知識（欄的組合、分段）帶進 Engine。

## 4. E-1～E-12、RC-1～RC-5 的歸屬與守衛

「DI n／RC n」指契約的編號。產品守衛的位置：`tests/object_fps_pvp/EntityModelTests.cpp`（G1 `:100`、G2 `:124-157`、G7 `:247`、G13 `:202`）、`RequireEntityOrder`（`PvpMatch.cpp:659-667`）。

| 條文 | 歸屬 | Engine 側 | 產品側（含守衛） |
|---|---|---|---|
| E-1 一個 id 空間、0＝無 | 拆 | 鍵型別 `EntityId`（DI 1） | 「一個空間」、0 的意義、`PlayerId` 別名；守衛：審查、G5 |
| E-2 兩段配發、段界常數 | 留產品 | DI 1「從不配發、不知道分段」是鏡像 | 全部；G5、Gateway 上限測試 |
| E-3 Match 段單調不回收 | 留產品 | DI 1 的前提依賴它與 Gateway 的單調配發；失效時 RC 1 | 全部；G4、G6 |
| E-4 段不表示種類、以元件挑實體 | 留產品 | DI 1、9 不知道種類 | 全部；審查 |
| E-5 欄升冪、lower_bound、保序刪除、成員＝種類表 | 拆 | 排序機制（DI 2、3）；common 測試＋從產品搬來的 M1、M2 | 「成員關係恰等於種類表」；G1、`RequireEntityOrder`（§6 第 1 列） |
| E-6 平手鍵＝EntityId 升冪 | 留產品 | 只保證迭代與套用依 id（DI 2、4、8） | 平手規則；G7、M6 |
| E-7 唯一刪除入口 | 留產品 | 容器版不知道實體有哪些欄（B2 才會改） | `PvpMatch::EraseEntity`（`PvpMatch.cpp:86-96`）；G2、G3、M3 |
| E-8 元件是權威、派生暫存可重建 | 留產品 | DI 6 是同一原則在 Engine 的版本 | 全部；G8、G9、G13、M5b |
| E-9 digest 編碼 | 留產品 | — | `authority_digest`、M4 |
| E-10 亂數、不讀時鐘／指標／unordered | 拆 | 模組內不用這些（DI 8） | `Brain.rngState`、matchSeed、reference age；G10 |
| E-11 Replication、手寫投影 | 留產品 | RC 10 不加反射 | 全部 |
| E-12 不改成員、待辦、升冪套用 | 拆 | `DeferredChanges` 的紀錄與「生成先、刪除後、各自升冪」的套用（DI 4） | 凍結區間、同步點位置（`ResolveActions` 之後、`AdvanceAnimation` 之前）、id 依裁決順序配發、G12 的 Assert（§6 第 2 列、§7 決定 7） |
| RC-1 >4,000 實體或 p95 >1 ms | 拆 | RC 3 沿用門檻 | 在 Linux Match 上量測 |
| RC-2 需要回收 id | 拆 | RC 1（加 generation） | E-3 的改寫 |
| RC-3 組合動態化或種類 >16 | 拆 | RC 4（容器模型失效；數值由消費端量） | 種類表 |
| RC-4／RC-4b 判定函數 >3、可複寫元件 >6 處 | 留產品 | RC 5 由 RC-4 衍生：RC-4 觸發且原因是交集時才算 | 全部 |
| RC-5 Client 生出權威實體 | 拆 | RC 11（不同 id 空間） | E-3 的 Client 條款 |

RC 14 的「繞過」不含每 Tick 整個重建或整欄複製的快取：`tickShapes_.emplace_back`（`PvpMatch.cpp:634`）、`actionStartCombats_ = combats_`（`:441`）是正當的直接變更。

## 5. 給圖書管理員的 `docs/architecture.md` 修改稿（只是文字提案；本線不改該檔）

何時套用見 §7 決定 8。下面每一段先給「完整版」（3b 之後），再給「3a 版」（`DeferredChanges` 還不存在時）。

**第 129 行**：把句子

> No umbrella framework, scene hierarchy, ECS, plugin ABI or service locator is introduced by this organization.

完整版改為

> No umbrella framework, scene hierarchy, plugin ABI or service locator is introduced by this organization. Of an ECS, the Engine has only the entity store ([contract](architecture/entity-store.md)): typed columns and a deferred spawn/destroy to-do, with no system, scheduler, world, resources, events, app or plugin.

3a 版：把「typed columns and a deferred spawn/destroy to-do」換成「typed columns」。同一行其餘文字不變。

**模組表（`:102-125`，資料列 `:104-125`）**：在 `GYO::Math` 列（`:105`）之後加一列。完整版：

> | `GYO::EntityStore` | entity columns: one typed `std::vector` per component kept in strictly ascending order of a caller-given `EntityId` (binary-search lookup, insert at the ordered position, order-preserving erase) and a deferred spawn/destroy to-do applied in ascending id at a sync point the caller names; no id allotment, kinds, schedule or world ([contract](architecture/entity-store.md)) | Base |

3a 版：刪掉「and a deferred spawn/destroy to-do applied in ascending id at a sync point the caller names」。

**§「Architecture changes and verification」（`:317` 起）**：在 audio 兩段之後、「Historical results」段（`:339`）之前加一段。完整版：

> The entity store (2026-11, v8-arch-entity-store of the [entity-store plan](architecture/plans/entity-store/README.md)) moves the container part of a networked game's ECS data layout into the Engine (a requested refactoring with one consumer). The game already kept one typed column per component in ascending entity-id order, with ordered insert, order-preserving erase and binary-search lookup, so its iteration order depends on ids alone; nothing in that code depended on the game except one transition field name. `GYO::EntityStore` is a new header-only leaf above `GYO::Base` (it links nothing else, checked at configure time): the key type, how a key is read, the column functions, and a to-do of spawns and destroys that the caller applies at a sync point it names, spawns before destroys, each in ascending id. It allots no id, knows no range or kind, runs no system and keeps no change tick; allotment, kinds, the frozen phases, the single erase entry and digests stay with the game. A reference survey of Bevy ECS and EnTT is the basis of its Design Invariants and Replacement Criteria, which are in the [entity-store contract](architecture/entity-store.md). The store adds nothing its one consumer does not use; moving the game's headers as they were would have carried its field names and id semantics into the Engine.

3a 版：刪掉「, and a to-do of spawns and destroys that the caller applies at a sync point it names, spawns before destroys, each in ascending id」，並把「the column functions」前的逗號改成「and」；3b 合併時再補回。（「2026-11」在合併時改成實際日期。）

## 6. 產品契約與計畫文件需要跟著改的地方（不在本批）

前兩列改的是產品已核准的 `Design Invariants`，依 AGENTS §3 要使用者同意（§7 決定 7）。

| 位置 | 現在的文字 | 與本 Delta 的衝突 | 誰改 |
|---|---|---|---|
| entity-model E-5 的守衛欄（`:265`） | 「G1、`RequireEntityOrder`」 | 機制部分（升冪插入、保序刪除）改由 Engine 的 common 測試與 M1、M2 守；產品的 G1、`RequireEntityOrder` 只剩「成員關係＝種類表」與快照順序。條文本身不變 | 產品 owner，batch 3a 同一條線（需同意） |
| entity-model E-12（`:272`）與 §5.2（`:234-236`）、G12（`:305`） | 「Tick 內不改任何欄的成員」；`InsertEntity`／`EraseEntity` 加 `GYO_ASSERT(!inTick_)` | 同步點本身在 Tick 內（`ResolveActions` 之後），照字面寫的 Assert 會在 `Apply` 內觸發；而且這兩個函數進 Engine 後不知道 Tick。改寫建議：凍結區間＝Tick 扣掉 `Apply`；Assert 放在產品自己的成員變更入口（Join、`PvpMatch::EraseEntity`、`Apply` 的 callback 以外的路徑） | 產品 owner，第一個非玩家批（需同意） |
| entity-model §0 第 2 項（`:14`） | 「實體儲存留在產品…Engine 不加 ECS」 | 被 2026-11-05 的決定取代 | 產品 owner，batch 3a |
| entity-model §3.1（`:128`） | 「產品層新檔 `EntityColumns.hpp`…不做模板類別、不做 registry」 | 檔案變成轉接；「不做模板類別、registry」在容器版仍成立 | 同上 |
| entity-model §11 第 4、5 項（`:376-377`） | 「不碰 `engine/`」「Engine 不加 ECS」 | 是當時 Delta 的紀錄；加註「2026-11-05 由 entity-store Delta 取代」，不改寫歷史 | 產品 owner |
| 本資料夾 README「這是什麼」、survey §1 的假說 | 「Entity＝index＋generation…change tick」 | 本 Delta 不收 generation 與 change tick | 協調者（survey 的假說是原文照錄，不改） |

## 7. 使用者要決定的事

1. **容器版或更多**：建議容器版（§3）；若要加，指定 B1～B7 的哪幾項。
2. **batch 3 的時機**（三案都不直接支撐新的 Feature；差在驗證與風險怎麼切）：
   - (a) 建議：3a 現在做欄的部分（模組、`EntityKey`、四個函數、守衛、pvp 換上），3b `DeferredChanges` 與第一個非玩家實體批同時做（它的第一個消費端；G12 也在那一批）。理由：把「搬移的 0 差分驗證」與 AI 批的行為變更分開，各自的驗收能單獨判定。代價：契約核准後 `DeferredChanges` 有一段時間只有文字。
   - (b) 全部現在做：一次完成；`DeferredChanges` 只有 common 測試、沒有消費端，它的 API 在真正的呼叫端出現前就固定。
   - (c) 全部延到第一個非玩家批：Engine 契約先核准但沒有程式；那一批（粗估 15～25 h，entity-model §9.2）同時承擔搬移與行為變更，0 差分無法單獨驗證。
3. **模組名稱與位置**：建議 `engine/entity_store/` → `GYO::EntityStore`（命名空間 `Engine::EntityStore`）：名字本身說「是容器」。替代案 `engine/entity/` → `GYO::Entity`：短，與其他模組一字命名一致，但容易被理解成實體框架。
4. **待辦的套用順序**：
   - (a) 建議：生成先、刪除後，各自依 id 升冪。同一同步點生成又刪除的實體不會留下；代價是「同一同步點刪除 id X 再以 X 重生」不可能（`onSpawn` 的 `InsertEntity` assert）。在本消費端，Match 段 id 不重用、玩家的重新加入走 Tick 外的 Join，所以目前不需要後者。
   - (b) 刪除先、生成後：允許「刪 X 再生 X」；代價是同一同步點生成又刪除的實體會留下。
   - (c) 交給呼叫端：Engine 不保證順序，決定性改由每個消費端自己守。
5. **同一同步點重複記錄刪除**：(a) 建議合併成一次（兩條規則可能指名同一實體）。(b) Programmer Error：更嚴，但每條規則都要先確認沒有別人刪過。
6. **函數名**：(a) 建議沿用 pvp 現名（`FindEntity`、`InsertEntity`、`EraseEntity`、`StrictlyAscending`），呼叫端與留在產品的突變 `find` 字串不改。(b) Engine 用新名，產品 30 行與相關突變跟著改。
7. **E-12 的守法（結構守衛）與產品 DI 的兩處變更**（survey §3b、§4.2 留給本批選；§6 前兩列）：
   - (a) 建議：消費端 Assert。產品在自己的成員變更入口 `GYO_ASSERT`（凍結區間＝Tick 扣掉 `Apply`）；Engine 不提供額外型別。取捨：改動小，違反在執行期才發現（需要突變證明）。
   - (b) 型別分離：裁決中的規則只拿到「只能記錄」的 handle（Engine 可加一個只有 `RecordSpawn`／`RecordDestroy` 的檢視型別）與 span，拿不到 vector。取捨：違反在編譯期就擋下；但 `ResolveActions`、`Decide` 現在是直接存取成員的私有成員函數，要改成自由函數，改動在 AI 批內變大；Engine 多一個型別。
   - (c) 兩者並用。
   - 同時核准產品 E-5 守衛欄與 E-12 措辭的改寫（§6 前兩列）。
8. **`docs/architecture.md` 何時寫入**：(a) 建議：3a 合併時用 §5 的「3a 版」，3b 合併時補成完整版（兩次修改，但每次都與程式一致）。(b) 等 3b 才一次寫完整版（一次修改，但 3a 合併後到 3b 之間第 129 行仍禁止 ECS，與程式不一致）。
9. **batch 3 的驗收判定**：依 AGENTS §14.1，判定在看結果前宣告並由使用者承認。本 Delta 只先寫內容，於 batch 3 開始時宣告並取得承認：
   - 件數的對照：兩樹情境數 42＝42、每個情境的 record 數相等（照 entity-model §9.2 的方法）。
   - `compare_authority_trees.py` `--scale 1` 與 `--scale 10` 各一次，42 個情境 0 差分；golden 不變；`DigestVersion` 不變。
   - Engine 的突變只靠 common 測試全部殺掉；留在產品的 M3～M6 照舊被殺。
   - L1 的 cpu|shader 全部通過；configure 時的連結檢查通過。
10. **核准契約的 `Design Invariants` 1～10 與 `Replacement Criteria` 1～15** 為 Architecture Delta。

## 8. batch 3 估時（推論，未量）

| 項目 | 3a（欄） | 3b（`DeferredChanges`，在第一個非玩家批內） |
|---|---|---|
| Engine 模組與 CMake | 1～1.5 h | 1～1.5 h |
| common 測試、連結與字詞檢查、突變目錄與執行 | 2.5～3.5 h | 1.5～2 h |
| pvp 換上（別名、`EntityKey` 特化、測試拆分、突變 2 筆搬家） | 1.5～2.5 h | 由該批的 E-12 實作承擔 |
| 驗收：兩樹 build、42 情境 compare `--scale 1`／`10`、golden、L1 | 2～3 h | 由該批的驗收承擔 |
| 文件（產品契約註記、dev_log、HANDOFF；architecture.md 由圖書管理員） | 1～1.5 h | 0.5 h |
| 審查 | 1 h | 0.5～1 h |
| 合計 | **9～13 h** | **3.5～5 h** |

選 §7 決定 2 (b) 約 12.5～18 h；選決定 7 (b) 時 3b 另加 `ResolveActions`／`Decide` 的改寫（未估）。超過估時 1.5 倍時停下回報。

## 9. 推論與風險

- 估時全是推論；沒有實作 spike。
- 讀鍵（審查 M1）：
  - (i) 同時有 `id` 與 `playerId` 的型別：現行兩個同名多載＋兩個獨立 concept 會 ambiguous；改成 `EntityKey` 特化後由特化勝出。契約 Guard 第 10 列以合成型別經 `EntityOf` 與欄函數確認。
  - (ii) `fps::pvp` 以外的 `playerId` 型別（例：`EntityColumnsTests.cpp:11-17` 匿名命名空間的 `Legacy`）：ADL 看不到 using-directive，所以不能靠 ADL；產品以 concept 限定的部分特化涵蓋，與命名空間無關。
  - (iii) 特化必須在每個用到該欄的地方之前可見，否則程式 ill-formed（不要求診斷）：產品轉接標頭要在型別定義旁或所有欄使用處之前被 include；batch 3 以編譯與 Guard 第 10 列確認。
  - 產品 `HasEntityId` 版的 `EntityOf` 多載必須刪除（否則與 Engine 的 `EntityOf` 重名並存）。
- 同名並存（審查 S3）：using 宣告後，`fps::pvp` 內會並存 Engine 的 `EraseEntity(std::vector<T>&, EntityId)`、產品的 `EraseEntity(WorldSnapshot&, PlayerId)`（`PvpMatch.hpp:45`，`MatchRuntimeHost.cpp:760` 呼叫的是它）與成員 `PvpMatch::EraseEntity(EntityId)`；以參數型別區分。`PvpMatch.cpp:87-96` 現在以 `::fps::pvp::` 限定呼叫，改裝後仍需要；限定名查找找得到 using 宣告是依 C++ 規則的推論，batch 3 以編譯確認。
- 3b 的 `Apply` 與 entity-model §5.4 digest 的 `spawned`／`destroyed` 紀錄如何對應（同一同步點生成又刪除的實體要不要出現在兩份清單），由第一個非玩家批決定。
- 突變 runner 是否把「編譯失敗」算 killed（Guard 第 9、10 列的部分突變會編譯失敗）：未確認；依記憶規則只在預期的測試失敗時算 killed，batch 3 要讓這些突變以 configure 檢查或 `requires` 測試的失敗呈現。

## 10. 審查的處置

審查者 Opus 5.5／high（唯讀），2026-11-05；報告存主 checkout `build/evidence/entity-store/delta-review-20261105.md`（git 管理外）。每條引用在本工作樹以 grep 重核（`tickShapes_.emplace_back` 實為 `:634`）。

| 項目 | 處置 |
|---|---|
| M1 讀鍵無法實作 | 採：契約改成 `EntityKey<T>` trait（特化優先、預設 `id`、不靠 ADL）；Guard 第 10 列改為經 hook 測「兩欄位都有」；§1 第 4 項、§2、§9 改寫，明記刪除產品 `HasEntityId` 多載 |
| S1 Engine 文件引用產品 | 採：契約刪掉產品路徑、行號、G／M 名與「Linux Match」，只留 plan 連結；每條 DI 加一句不依產品的理由；RC 前文改「measured by a consumer on its target platform」；產品對應移到本文件 §4 |
| S2 壓力分類 | 採：§1 第 2 項重分類（決定／決定的結果／觀測一項且註明不屬 §2 列舉種類／預測），刪「重複 ≥5 處」列；第 7 項明寫「技術上可留在產品，根據只有決定」 |
| S3 行數與 760 | 採：33→30 行並列出排除的 3 行；刪 `MatchRuntimeHost.cpp:760` 作根據；§9 記同名並存 |
| S4 突變目錄 | 採：契約刪「catalog entries stay」；§1 第 4 項寫 M1、M2 改 `file`／`expect` 搬到 Engine，M3～M6（含 M5b）留產品 |
| S5 結構守衛的選擇 | 採：§7 決定 7 列 (a) 消費端 Assert（建議）、(b) 只能記錄的 handle、(c) 並用與取捨 |
| S6 產品 DI 變更 | 採：§6 加 E-5 守衛欄、E-12 措辭（凍結區間＝Tick 扣掉 `Apply`）兩列，註明需同意；併入決定 7 |
| S7 生成先的取捨 | 採：契約加「生成的 id 已在欄中＝callback 的 `InsertEntity` assert」與「刪 X 再生 X 不可能」；決定 4 列三案 |
| S8 Guard 第 3 列 | 採：加比較次數上限測試（可殺線性搜尋），其餘「無側表」標 review |
| S9 RC 5 | 採：加「消費端 RC-4 觸發且原因是交集」、定義「same」與計數者，「Mirrors」改「Derived from」 |
| S10 architecture.md 時機 | 採：§5 每段加「3a 版」；§7 決定 8 |
| S11 決定 2 與驗收事前宣告 | 採：決定 2 中立化並寫 3a 的真正理由；決定 9 寫驗收內容，註明 batch 3 開始時宣告並取得承認 |
| S12 懸空參照 | 採：契約刪除該參照（產品守衛段落改成指向 Delta） |
| N1 行號 | 採：模組表 `:102-125`（資料列 `:104-125`）、突變 entry 從 `:8834` |
| N2 B2 的 survey 判定 | 採：改為「registry 列是改裝後採；多欄型別未判定」 |
| N3 B1 的理由方向 | 採：改用 Engine 側理由（消費端一個、無觀測到的繞過） |
| N4 「沒有 Engine 模組依賴它」 | 採：從契約刪除（不進 DI，無守衛） |
| N5 `static` 檢查的定義 | 採：Guard 第 5、6、8 列寫明排除 `static_cast`／`static_assert`／`static constexpr`／static 成員函數，並附 self-test |
| N6 Apply 的例外與巢狀 | 採：契約寫明 callback 擲出後清單清空、狀態重設；同一物件的巢狀 `Apply` 是 Programmer Error；Guard 第 4 列加測試與突變 |
| N7 RC 14 的引信 | 採：限「test 或 incident 發現的缺陷」，整個重建或整欄複製的快取不算繞過（§4 末段列產品行號） |
| N8 RC 4 的數值 | 採：寫明由消費端量，「majority」定義為過半的存活實體 |
| N9 退案說明循環 | 採：改成「requested refactoring with one consumer」與技術理由 |
| N10 DI 形式 | 不改（審查者判定可接受） |
| N11 語言 | 無需處置 |
