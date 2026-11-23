# 事前宣告：Kernel 的關係與父子（`v8-arch-gyo2-kernel-relationship`，子批 KR）

更新：2026-11-23（修訂 0）。Owner：Kernel（GYO2 主會話開出的任務卡，使用者「開 1、2、3」的第 1 張）。**狀態：草稿。敵對式檢查之前；使用者動工前核准之前不改 `engine/` 與 `tests/`。**

根據：
- HANDOFF `v8-arch-gyo2-hierarchy-bevy-and-primary-reference`（使用者 2026-11-23「照建議」）：父子關係照 Bevy 進 Kernel（含連帶刪除、從子查父），`transform` 只放 `Transform`、`GlobalTransform` 與傳播；Kernel 職責清單多一項「關係」（使用者同意）；ECS 相關機制以 Bevy 為主參考，偏離要寫衝突點與膠水量。
- HANDOFF `v8-arch-gyo2-transform-camera-physics-modules`（同日）：Transform、相機、物理放 Module；KR 排在子批 X（X2：讀 `ChildOf` 的階層傳播）之前。
- SDL 宣告（分支 `claude/gyo2-sdl-declaration-7e7a1f`，修訂 18 de73b4cd，還沒核准）第 6.5 節：統合只寫了 KR 的需求 1～5 與衝突點 K1～K9，「K3、K6、K7（以及 K1、K2 選改條文的路時）在 KR 的宣告交使用者」。本宣告逐條回答（第 5 節、第 6 節）。
- 批次表〈每批的設計審查〉第 5 項（主參考 Bevy）。

讀的版本：`origin/gyo2` 0ed6ea10；Bevy 本機 `6a911653f`（`bevy_ecs` 0.20.0-dev）；bevy_replicon、lightyear、flecs `09790b5f7`、Unity Entities 本機鏡像。行號都是這些版本的。

---

## 1. 這一批做什麼（先用遊戲講）

- **武器掛在角色手上**：作者生成武器時帶 `ChildOf{角色}`。同一個同步點之後，角色的 `Children` 就有武器；武器的 `ChildOf` 就是「從子查父」。
- **角色被刪，武器跟著刪**：作者只刪角色；同一個同步點裡，Kernel 把角色 `Children` 裡的每一個（以及它們的子、孫……）都刪掉。
- **道具換到另一個角色身上（換父）**：作者對道具再插入一次 `ChildOf{新角色}`。舊角色的 `Children` 少掉它、新角色的 `Children` 多了它；舊角色沒有子了，`Children` 那一格就拿掉。
- **回滾讓預測中被打掉的道具回來**：回滾 Module 放回道具的 `ChildOf` 與存活集合；角色的 `Children` 跟著回來，和直跑逐位元相同。
- **讀檔**：存檔只存 `ChildOf`；讀檔時照一般的 `Spawn` 放回，`Children` 自己長回來。

`transform` 的階層傳播（X2）、UI 的節點樹、物理的碰撞體掛剛體（avian 的 `ColliderOf`）都會積在這個機制上，所以它是基盤（第 2 節前提 P6）。

---

## 2. 前提表

每一條前提寫「誰定的」與「哪些決定掛在它上面」。換掉一條前提，掛在上面的決定要重看（`premises-before-decisions`）。

| # | 前提 | 誰定的／出處 | 掛在上面的決定 |
|---|---|---|---|
| P1 | 父子關係照 Bevy 放進 Kernel，當「關係」；`transform` 只放 Transform 與傳播 | 使用者 2026-11-23（`-hierarchy-bevy-and-primary-reference` (1)） | 全部 |
| P2 | Kernel 職責清單多一項「關係」 | 使用者同意（同上） | DI-KS 15 的改寫（第 7.2 節） |
| P3 | ECS 機制以 Bevy 為主參考；偏離要寫衝突點與膠水量；Unity、flecs 只當對照 | 使用者 2026-11-23（同上 (2)） | 第 6 節的對照表；Q1、Q2 |
| P4 | GYO 的 hook 只有 OnAdd、OnRemove；hook 記的變更屬下一窗（下一個同步點的第 0 步，或最後一個同步點之後的清空輪） | DI-KS 9、11（使用者 2026-11-22 核准）；hook 清空那一批（`-hook-queue-flush-at-run-end`） | Q2（維護放哪）：照現行 hook 做，`Children` 要等下一窗才一致 |
| P5 | `Restore` 照給的內容放回、不推導資料；通知照跑 | DI-KS 12（使用者 2026-11-22 選 (a2)） | 第 5.8 節回滾；Q1 |
| P6 | 基盤／應用的判準：上面會積 X2、UI、物理、網路複製、讀檔 | AGENTS §2；SDL 宣告第 6.5 節 | 用予見的 Pressure 設計；Replacement Criteria 要先寫 |
| P7 | 讀檔一律在新的 World，用一般的 `Spawn`／`SpawnWithId` 放回；Kernel 不認得讀檔，沒有「讀檔中」的旗標 | 讀檔宣告 5.1、5.3（使用者 2026-11-17 核准；`restore-entry-declaration.zh-Hant.md:139`） | 第 5.9 節；Q1 的 (A) 會撞到它 |
| P8 | 刪除實體時「一個實體做完才做下一個」、每個實體先全部通知再銷毀 | 使用者 2026-11-16（`-despawn-notify-all-then-destroy`、`-despawn-entity-by-entity`） | 第 5.6 節連帶刪除的先後 |
| P9 | `Changed<T>` 的意思是「被寫過」；同值同步的寫者先比再寫（CCR-8） | DI-KS 28；CCR-8 | 第 5.11 節 |
| P10 | 底層不替誤用加守衛；守衛只守寫明的契約 | 使用者 2026-11-21／22（`no-guards-against-misuse`、`design-review-not-guards`） | 「不可就地改」是守契約（兩邊一致）還是擋誤用：本卡判為守契約（第 5.2 節） |

每一題的「Engine 還是作者」（`premise-engine-or-author`）寫在第 5 節各小節的開頭。

---

## 3. Feature → Contract → Architecture

| Feature（誰用） | Contract | Architecture |
|---|---|---|
| 從子查父、從父查子（X2 傳播、UI、物理、遊戲邏輯） | `ChildOf{parent}` 在子上、`Children` 在父上；任何 System 用一般的 `Query<Read<…>>` 讀 | Kernel 自帶兩個元件型別（第 5.1 節） |
| 兩邊永遠一致（作者不必自己維護反向表） | 每一次套用之後兩邊一致；OnAdd hook 與 observer 看到的是一致的狀態（DI-KS 30 草案） | 維護內建在 Kernel 的套用步驟（Q2） |
| 刪父連帶刪子（PvP 武器、UI 面板、2D 遊戲的節點） | despawn 階段展開、遞迴到底、不受清空輪上限（DI-KS 32 草案） | 同上 |
| 回滾等價（PvP Client） | 來源側是狀態、目標側是索引；放回來源側即可（DI-KS 33 草案） | Q1 |
| 讀檔（有存檔的產品） | 只存來源側即可重建 | Q1 |
| 網路複製（之後） | 目標不在時那一筆不成立、留明細；網路 Module 自己做等待（lightyear 的 `PendingChildOf`） | 不在本批 |
| 物理的 `ColliderOf`（之後，avian） | 通用的關係登記入口：形狀定下來、不實作 | 第 5.1 節、RC 27 |

---

## 4. Bevy 怎麼做（主參考）與對照

### 4.1 Bevy

| 項目 | Bevy | 出處 |
|---|---|---|
| 形 | 來源側元件（`Relationship`，例 `ChildOf(Entity)`）存一個目標；目標側元件（`RelationshipTarget`，例 `Children(Vec<Entity>)`）存來源的集合；derive 時兩個型別互指 | `relationship/mod.rs:28-34`、`hierarchy.rs:105-107`、`:148-152` |
| 不可就地改 | 關係元件一律 `Immutable`，只能重新插入 | `macro_logic/src/component.rs:310-312` |
| 目標側誰寫 | 集合欄位私有，只有 `collection_mut_risky` 可以改（註明作者不要用）；作者可用 `Children::sort_by` 等重排 | `relationship/mod.rs:296`；`hierarchy.rs:154-240` |
| 維護 | hook：`on_insert` 把來源加進目標側（經 commands；註解「Deferring is necessary for batch mode」）；`on_discard` 當場從目標側拿掉，空了就經 commands 拿掉目標側那一格；目標側的 `on_discard` 拿掉所有來源的關係 | `relationship/mod.rs:150-215`、`:219-262`、`:307-327` |
| 不合法的目標 | 目標不存在、或指向自己（預設不准）：`warn!` 並把關係拿掉 | `:166-178`、`:205-214`；`ALLOW_SELF_REFERENTIAL` `:127` |
| 連帶刪除 | 目標側標 `linked_spawn`（`Children` 有）：目標的 `on_despawn` 把每個來源 `try_despawn`；遞迴到底；有環時 `iter_ancestors` 等會無限迴圈（文件寫明，不查環） | `:270-278`、`:332-338`；`relationship_query.rs:41`、`:58` |
| 順序 | `Vec` 的先後＝插入的先後；可插在指定位置、可排序 | `relationship_source_collection.rs:137-200`；`hierarchy.rs:292-298` |
| 讀檔 | `DynamicWorld` 連 `Children` 也存；放回時用 `RelationshipHookMode::Skip` 跳過關係的 hook | `bevy_world_serialization/src/dynamic_world.rs:148-156`、`:192-200`；`relationship/mod.rs:403-410` |
| 複製（生態） | bevy_replicon 只複製 `ChildOf`，`Children` 靠插入時的 hook 長回來；父比子晚到時 lightyear 另做 `PendingChildOf` | `bevy_replicon/src/lib.rs:255-259`；`lightyear/crates/replication/replication/src/hierarchy.rs:44-56`、`:358-376` |
| 回滾（生態） | 本機 lightyear 的 prediction 沒有回滾 `ChildOf` 的例子（`git grep -i childof crates/prediction` 0 筆；正例：同指令在 `crates/replication` 命中） | — |
| 物理（生態） | avian 的 `ColliderOf` 是關係（碰撞體→剛體），允許指向自己 | `lightyear/crates/integration/avian/src/plugin.rs:428-429`、`:517` |

**Bevy 實際保證到哪裡**：`on_insert` 經 commands，到那一次結構操作 flush 之後才看得到；lightyear 為「Add 的 hook 與 observer 之間沒有 flush」踩過坑（SDL 宣告第 6.5 節引 `hierarchy.rs:542-544`）。所以 Bevy 的「立即」是「那一次操作結束時」，observer 看到的不一定一致。

### 4.2 對照（只查證，不混搭）

- **flecs**：`EcsChildOf` 在核心；刪除父的行為由 `OnDeleteTarget` 這類 trait 決定；子的順序預設不保證，父加 `EcsOrderedChildren` 才有穩定順序（`include/flecs.h:1851-1853`、`:4415-4428`）。
- **Unity Entities**：`Parent`＋父的 `Child` 緩衝，`ParentSystem` 只保證「每次更新之後」一致（主會話 2026-11-23 查，`ParentSystem.cs:13-24`）；連帶刪除另一套 `LinkedEntityGroup`。使用者 2026-11-23 因延遲與兩套機制而不選。

---

## 5. 設計（Contract）

### 5.1 型別與登記（K9）

**前提**：Engine 的事——P1 已定進 Kernel。

- Kernel 自帶 `ChildOf{EntityId parent}` 與 `Children`（內容：來源的 `EntityId`，只讀的走訪介面；建構與修改只有 Kernel）。名字照 Bevy（命名規範：取別家已通用的名字時照搬）。
- **每個 World 都有**：Kernel 在 `Registry` 建立時自己登記，和 `KernelSettings` 同一處（`Registry.cpp:650-660` 的「kernel's own registrations come first」），owner 是 Kernel。所以 `ChildOf` 是 ComponentId 0、`Children` 是 1，之後作者登記的往後排、不隨作者的數量移動。照 Bevy：`ChildOf` 在 `bevy_ecs`，不必加 Plugin 就能用。
- 作者再登記 `ChildOf` 或 `Children` 是既有的 R-E2（登記兩次）。權威要把 `ChildOf` 當判定資料時用 `Mark<ChildOf>()`（權威頁第 2 節：別的單位已登記的型別只記下、不再登記）。
- **代價**（照實寫，不當決勝理由，AGENTS §4）：所有 World 多兩個空欄；寫死欄數或 ComponentId 的測試要改（`RegistryTests.cpp:150` 等，實作第 1 步列全）；Registry 指紋與 World 摘要的值都會變（格式不變，摘要仍是版本 4）。
- **通用的登記入口只定形、不實作**（AGENTS §2 薄起步；SDL 宣告需求 4）：形狀是 `Registrar::Relationship<R>(RelationshipOptions)`——`R` 宣告 `using Target = …;` 與一個 `EntityId` 欄位，選項有 `linkedDespawn`、`allowSelf`；一對一、自訂集合、連帶複製也只定形。Kernel 內部的維護照「一張關係表」寫（`ChildOf` 是表裡唯一的一列），之後開放入口時變動局部。第一個點名的使用者是物理 Module（avian 的 `ColliderOf`），那一批再開（RC 27）。

### 5.2 不可就地改（K2）

**前提**：Engine 的事。這不是擋誤用：就地改 `ChildOf` 會讓 `Children` 和它不一致，破的是第 5.3 節的契約本身（P10 的「守寫明的契約」）。Bevy 也在型別層擋（`Immutable`）。

編譯期拒（`static_assert`，訊息帶 `GYO_KERNEL_REJECT_RELATIONSHIP_…` 標記，負向編譯樣本照 #286 的共用判法）：

| 入口 | `ChildOf` | `Children` |
|---|---|---|
| 讀（`Read<T>`、`With`／`Without`／`Changed`／`Added`、`World::Get`、`ReadColumn`） | 可 | 可 |
| 就地寫（`Write<T>`、`Mut`、`World::GetMut`、`ExclusiveWorld::Modify`） | **拒** | **拒** |
| 插入（`Commands::Insert`、`CommandsFor<Inserts<…>>`、`Spawn`／`SpawnWithId` 的 bundle、`ExclusiveWorld` 當場插入、Host 的 `World::Insert`） | 可（再插入＝換父） | **拒** |
| 拿掉（`Commands::Remove`、`CommandsFor<…, Removes<…>>`、當場拿掉、Host 的 `World::Remove`） | 可（＝脫離父） | **拒** |
| `RestoreSet::Column<T>` | 可 | **拒**（第 5.8 節） |
| `Registrar::InitialValue<T, …>` 的目標 | 照既有規則（目標由登記它的 owner 宣告；owner 是 Kernel，作者不能宣告；實作時確認 `CheckInitialValues` 的 R-E4 擋得到） | **拒** |

和 Bevy 的差：Bevy 允許 `remove::<Children>()`（等於拿掉所有子的 `ChildOf`）與 `Children::sort_by`；這裡都拒（`Children` 的內容由 Q1 決定，見第 5.4 節）。脫離所有子要逐個拿掉 `ChildOf`。

### 5.3 維護的時點（K1、K3）——Q2

**前提**：Engine 的事。作者在 Kernel 外做「兩邊一致」，只能用 hook 或每幀比對，前者照 P4 要等下一窗，後者就是 Unity 的延遲（使用者已不選）。

**建議做法：維護內建在 Kernel 的套用步驟，不經 hook。** 先例：`InitialValue` 也是 Kernel 在同一個同步點、OnAdd 之前補的（DI-KS 9 的第 5a 步）。

在 DI-KS 9 的套用順序裡（hook 佇列 → despawn → remove → spawn → insert → 5a 初始值 → OnAdd → resource → 觸發）：

| 時點 | 做什麼 |
|---|---|
| despawn、remove 階段：一格 `ChildOf` 被銷毀之後 | 當場從父的 `Children` 拿掉它；`Children` 空了：先跑那一格的 OnRemove（以及 `Remove<Children>` 的 observer），再拿掉 |
| spawn、insert、5a 之後的**新步驟 5b** | 對這個階段新出現或被覆寫的每一格 `ChildOf`（依子的 EntityId 升冪）：檢查目標（第 5.5 節）；合法的加進新父的 `Children`（沒有就新建那一格，新格進 OnAdd 的名單）；覆寫的先從舊父拿掉（空了照上一列） |
| OnAdd 階段 | 照常；這時兩邊已經一致 |

為什麼新增、覆寫要等到 5b 一起做、不在插入當下做：同一個階段裡，父可能和子一起生成，而 spawn 依 EntityId 升冪處理——子的編號比父小時，處理子的那一刻父還沒活（例：同一個 System 先 `Spawn` 父、再 `SpawnWithId` 一個小編號的子）。

**一致的時點**（DI-KS 30 草案）：每一次套用之後（同步點的每一個階段、清空輪的每一輪、`ExclusiveWorld` 的當場改、`Restore`、Host 的直接放），兩邊一致；OnRemove hook 看到的是**拿掉之前**的一致狀態，OnAdd hook 與 observer 看到的是**套用之後**的一致狀態。例外只有連帶刪除中的子（第 5.6 節）。

**和 Bevy 的差**：機制不同（Bevy 用 hook＋commands），看得到的結果相同、而且更嚴（Bevy 的 observer 可能看到還沒 flush 的狀態，第 4.1 節）。衝突點與膠水見第 6 節 C1～C3。

**沒選的路**：
- (ii) 加 hook 的種類（Bevy 的 `on_insert`／`on_discard`）＋讓這些 hook 記的變更當場套用：要改 DI-KS 9、11、18 三條，而且「hook 記的變更當場套用」是清空輪設計的例外，作者自己的 hook 也會問「我能不能也當場」。
- (iii) 照現行 hook（OnAdd／OnRemove＋下一窗）：第一個子加上之後，到下一個同步點之前父沒有 `Children`；換父完全沒有 hook。等於 Unity 的延遲。

**換父沒有 hook**（K1 的另一半）：GYO 的 hook 只有 OnAdd、OnRemove（DI-KS 11），覆寫不觸發任何 hook；換父看得到的是 `Changed<ChildOf>` 與兩個父的 `Changed<Children>`。Bevy 有 `Insert`／`Discard` 事件。這是 GYO 既有的差異，不在本批改；有 Module 需要換父的通知時是 RC 26。

### 5.4 `Children` 的內容與順序（K5）——Q1

**前提**：Engine 的事（`Children` 由 Kernel 維護，順序只有 Kernel 能定）。

| | (B) EntityId 升冪，`Children` 是索引（**建議**） | (A) 照 Bevy：插入先後＋可重排，`Children` 是狀態 |
|---|---|---|
| 內容 | 指向這個父、活著的子，依 EntityId 升冪；是 `ChildOf` 與存活集合的純函式 | 插入的先後（同一個同步點裡依 EntityId 升冪）；作者可以插在指定位置、排序 |
| 回滾（K6） | 只登記 `ChildOf`；`Restore` 放回 `ChildOf` 時 `Children` 照樣維護，和直跑相同（第 5.8 節） | `Children` 也要登記回滾，而且 `Restore` 不能維護（否則順序被重算）→ 回滾 Module 要「`ChildOf` 一定和 `Children` 成對登記」的檢查或代為登記；`Restore` 要分「放回」與「維護」兩種行為 |
| 讀檔（K7） | 只存 `ChildOf`；照一般的 `Spawn` 放回，`Children` 自己長回來，和存檔前相同 | 要存 `Children`；放回時要跳過維護（Bevy 的 `Skip`）→ 撞讀檔宣告的 RC（「hook 要分辨讀檔，要 Kernel 多一個記號 → 停下來交使用者」，`restore-entry-declaration.zh-Hant.md:231`）與 P7；不跳過就只能重算，順序丟掉 |
| 摘要、digest | 進摘要（值確定）；不必進 digest | 進摘要與 digest，順序也要兩端相同 |
| UI 的子順序 | UI 要另給排序鍵（例：子上的順序元件，layout 依它排）——膠水在 UI 一處 | 直接用 `Children` 的順序（bevy_ui 的做法） |
| 典型情形和 Bevy 的差 | 預設配發器單調（entity_ids EI-2），依序生成的子編號遞增 → 和 Bevy 的插入先後相同；只有「把舊實體掛到新父」或「重排」時不同 | 無 |
| 系譜 | flecs 預設不保證子順序、要穩定順序另加 `EcsOrderedChildren`（對照）；GYO 自己的「順序只來自 id 與 Plan」（DI-KS 3） | Bevy |

建議 (B) 的理由：GYO 的同步點已經是「依 id 排序、和記錄先後無關」（DI-KS 3、9），(A) 的「插入先後」要另外記一份歷史，而這份歷史會讓回滾、讀檔各多一個衝突點（上表），兩個都要改已核准的條文（DI-KS 12、讀檔宣告 5.3）。(B) 的代價集中在 UI 一處，等 UI 那一批照它的 Feature 定排序鍵。網路的兩端編號不同時，(B) 的順序也會不同；走訪 `Children` 的先後若影響判定，作者要依自己的鍵排——網路複製那一批再看（第 5.13 節）。

### 5.5 不合法的目標

**前提**：Engine 的事（只有套用時看得到目標活不活著）。

照 Bevy（目標不存在、指向自己就把關係拿掉）：5b 檢查到下列情形時，那一筆關係不成立——
- **目標不活著**（這個階段之前已刪、或這個 World 沒有這個實體）。
- **指向自己**（`ChildOf` 不准；`allowSelf` 是通用入口的選項，只定形）。

不成立時：新出現的那一格不放（它還沒被任何 hook 看到，不跑 OnAdd、也不跑 OnRemove）；覆寫的那一格連同原本的關係一起拿掉（原本的關係已存在過，跑 OnRemove，從舊父拿掉），結果和 Bevy 相同：實體沒有 `ChildOf`。留一筆 Kernel 自己的診斷紀錄 `RejectedRelationship`（實體、目標、原因、記錄者、schedule、步、同步點序號；形狀照 `RejectedSpawn`）與總數。不是錯誤（Bevy 是 `warn!`；Kernel 不寫日誌，用診斷紀錄告訴外面，`errors-inform-not-prescribe`）。

不查環：`ChildOf` 可以形成環（A 是 B 的子、B 是 A 的子），Bevy 也不查（第 4.1 節）。Kernel 只保證自己的走訪會結束（連帶刪除用已處理的名單）；X2 的傳播要不要偵測環，由 X2 照 Bevy 的 `bevy_transform`（`systems.rs:360` 的「Malformed hierarchy」）定。

### 5.6 連帶刪除（K4）

**前提**：Engine 的事。

- despawn 階段處理一個被刪的實體時，把它的 `Children` 裡的子排在它後面一起刪；子再帶它的子——**父先於子、同一層依 `Children` 的順序、遞迴到底**。每個實體照 P8 完整處理（全部 OnRemove → 全部銷毀 → 移出存活集合）才處理下一個。已經處理過的跳過（冪等，有環也會結束）。
- 不經 hook 輪，不受 `hookRoundLimit`（K4：Bevy 遞迴到底；照現行 hook 做會一層一輪、超過 8 層 World 中斷）。
- 被刪的子的 OnRemove 跑的時候，父已經不在（和 Bevy 相同：父的 `on_despawn` 排的 commands 在父刪完之後才跑）。
- 明寫的 despawn 紀錄之間照舊依 EntityId 升冪（P8）；連帶的子接在帶它的那一個後面。
- 和 P8 的接合：P8 定的是「兩筆明寫的 despawn 誰先」，沒有定連帶的子；本卡照 Bevy 判「父先」（第 10.2 節，核准時一起確認）。
- **目標被刪、來源留著**時（通用入口的非連帶關係；以及 `Restore`，第 5.8 節），來源的關係在同一步拿掉（OnRemove 照跑）。

### 5.7 Host 的直接放（C-10 那條路）

`World::Insert`／`Remove`／`Despawn`（Host、Ready）照舊不跑 hook、不補初始值；但**照樣維護 `Children` 與連帶刪除**——維護不是通知，是第 5.3 節的契約。不合法的目標同第 5.5 節（留診斷紀錄）。說明頁讀檔一節的「第三條放回的路」補一句。

### 5.8 回滾（K6）

**前提**：Engine 的事只到「`Restore` 之後兩邊一致」；要不要回滾 `ChildOf` 是作者的選擇（R-2 前提）。

以 Q1 選 (B) 寫：
- `ChildOf` 是狀態：作者（或權威代為）`rollback.Component<ChildOf>()`。
- `Children` 不能登記回滾：`rollback.Component<Children>()` 在作者編譯時就失敗（回滾的歷史在虛擬函式裡實例化 `RestoreSet::Column<T>`，`History.hpp:95-108`，碰到第 5.2 節的拒）。
- `Restore` 放回 `ChildOf` 的差異時，`Children` 照第 5.3 節維護（覆寫＝換父也一樣，不靠 hook）；**連帶刪除不展開**（`Restore` 給的存活集合是完整的；展開會刪掉「之後才掛到某個父、但當時活著」的子）；被刪的父留下的子的 `ChildOf` 在同一步拿掉，接著 `RestoreSet` 給的拿掉變成無作用。
- SDL 宣告 K6 列的三種情形：換父的回滾（維護處理覆寫）、順序變過的回滾（(B) 沒有自訂順序）、預測死亡的復活（spawn 階段先復活、5b 才維護）都成立。
- **DI-KS 12 的接合**：「照給的內容放回、不推導資料」不變。維護 `Children` 不是「從新格推導資料」（那是作者的 `InitialValue`），而是和存活集合同類的索引；(B) 下它是 `ChildOf` 的純函式，維護出來的就是「當時的樣子」。DI-KS 12 補一句寫明（第 7.2 節）。
- **回滾頁 R-2 前提 1**：「步上的 System 只讀可回滾的資料、Derived……」要補「Kernel 維護的關係目標側（`Children`）」——它是可回滾的 `ChildOf` 與存活集合的函式。這是回滾頁的文字（R-2 是 Design Invariant，改文字要使用者同意），不改回滾 Module 的程式。網路 PR-2 卡正在改回滾 Module，先後由主會話排（第 13 節）。

選 (A) 時這一節整個改寫（第 5.4 節表格的回滾列）。

### 5.9 讀檔（K7）

以 Q1 選 (B) 寫：存檔只存 `ChildOf`；讀檔照讀檔宣告的路（生成世界期間、`beforeSwap`、或準備函式直接放）放回，`Children` 自己長回來。
- 父要在子的同一個同步點或更早放回；子的 `ChildOf` 指向還沒放回的父，那一筆不成立並留 `RejectedRelationship`（分批放回時要注意，說明頁寫）。
- 重新配號時，作者的對照表照 `ChaseTarget` 的寫法改寫 `ChildOf::parent`（讀檔宣告 5.1、案例 2）。Bevy 有 `MapEntities`（`ChildOf` 的欄位標 `#[entities]`），GYO 的 Kernel 沒有編號映射的機制，維持讀檔宣告的決定。
- 讀檔宣告 5.3 寫「Bevy 的關係例外：GYO 現在沒有關係的概念，將來加關係時照 Bevy 再看」：(B) 下不需要 Bevy 的 `Skip`（`Children` 不存），P7 不變。說明頁讀檔一節補這一段。

### 5.10 World 摘要、權威的 digest

- **摘要**（DI-KS 27，版本 4）：`ChildOf`、`Children` 和其他欄一樣進摘要；Kernel 替兩者寫 `HashValue`（`ChildOf`：父的編號；`Children`：個數與每個編號）。(B) 下 `Children` 的值確定，兩份 World 的摘要比較照舊成立。格式不變，版本不升。
- **digest**（權威 DI-A 12）：`ChildOf` 要當判定資料時用 `authority.Mark<ChildOf>()`；值裡有父的編號，兩端的編號要相同才比得起來——和其他存實體編號的元件（例 `ChaseTarget`）同一個限制，網路複製的編號映射那一批處理。`Children` 不必進 digest。
- 權威的「啟動時看得到的寫」：`Children` 只有 Kernel 寫，沒有作者的寫者；AU-E6 不受影響。

### 5.11 `Changed<T>`／`Res::Changed()`／CCR-8

- `ChildOf` 被插入（含同值再插入）就是被寫過，`Changed<ChildOf>` 為 true（P9；Bevy 的同值再插入也會跑 `on_discard`＋`on_insert`）。同步 `ChildOf` 的寫者（網路的套用）照 CCR-8 先比再寫。
- `Children` 由 Kernel 寫：**只在內容變了時**打 changed stamp（Kernel 是寫者，照 CCR-8 的精神先比）；新建那一格時 added＝changed＝那次套用的 stamp。
- 只有元件，沒有 Resource，`Res::Changed()` 不相關。

### 5.12 事件游標、observer

- `Add<ChildOf>`、`Remove<ChildOf>`、`Add<Children>`、`Remove<Children>` 的 observer 照一般元件的規則跑（DI-KS 10）；跑的時候兩邊已一致（第 5.3 節）。
- 事件游標（`Message`）不相關。

### 5.13 複製（K8，不在本批）

Kernel 這一側的行為就是第 5.5 節：子的 `ChildOf` 指向這個 World 沒有的實體時不成立、留明細。父比子晚到時，網路 Module 要自己先存著、等父到了再插入——和 lightyear 的 `PendingChildOf` 同形（第 4.1 節）。經主會話轉給網路卡（第 13 節）。(B) 的順序在兩端編號不同時也不同（第 5.4 節）。

### 5.14 範圍的限界（本批不做）

- 通用的 `Registrar::Relationship<R>`、一對一、`allowSelf`、自訂集合、連帶複製：只定形（第 5.1 節）。
- 走訪的輔助（祖先、子孫、根）：Bevy 放在 `relationship_query.rs`；X2 需要時在 X2 判放 transform 還是 Kernel。
- 「生成時一起帶子」的便利寫法（Bevy 的 `with_children`、`Children::spawn`）：GYO 的 `Spawn::New` 當下就回傳編號，同一個 System 先生成父、再帶 `ChildOf{父}` 生成子即可，不另加 API。
- 換父的通知（Bevy 的 `Insert`／`Discard`）：RC 26。
- 效能：沒有點名的 Feature 給預算；不量。深樹、大量子的成本到 X2 或 UI 有預算時量（RC 27）。

---

## 6. 和 Bevy 的對照表（衝突點與膠水量）

| # | 項目 | Bevy | 本宣告 | 衝突點 | 膠水（誰寫、多少） |
|---|---|---|---|---|---|
| C1 | 維護的機制 | hook＋commands | Kernel 的套用步驟（5b 與 despawn／remove 的當場維護） | GYO 的 hook 是下一窗、沒有 `on_insert`／`on_discard`（P4） | Kernel 內部約一個步驟；作者 0 |
| C2 | 一致的時點 | 那一次操作 flush 之後（observer 可能看到 flush 之前） | 每一次套用之後；OnAdd 與 observer 一定一致 | 無（GYO 更嚴） | 0 |
| C3 | 換父的通知 | `Insert`／`Discard` 事件 | 沒有；看 `Changed<…>` | GYO 的 hook 只有兩種（DI-KS 11） | 需要換父通知的 Module 自己比 `Changed<ChildOf>`；出現第一個這樣的 Module 時 RC 26 |
| C4 | 不可就地改 | `Immutable`（型別層） | 編譯期拒（`static_assert`） | 無 | 0 |
| C5 | `Children` 可重排、可整個拿掉 | 可 | 拒 | (B) 的連帶 | UI 的排序鍵（UI 那一批） |
| C6 | 順序 | 插入先後 | (B) EntityId 升冪 | Q1 | 同上；網路兩端編號不同時作者自己排 |
| C7 | 連帶刪除 | `on_despawn` hook 遞迴 | despawn 階段展開 | GYO 的 hook 輪有上限（K4） | 0 |
| C8 | 連帶刪除的先後 | 父先（commands 在父刪完之後） | 父先、同層依 `Children` | 和 P8 的接合（第 10.2 節） | 0 |
| C9 | 不合法的目標 | `warn!`＋拿掉 | 拿掉＋`RejectedRelationship` | Kernel 不寫日誌 | 0 |
| C10 | 讀檔 | 存兩邊、`Skip` | 只存 `ChildOf`、照一般路放回 | P7（沒有讀檔旗標） | 0（(B) 下） |
| C11 | 回滾 | 生態沒有例子 | 只回滾 `ChildOf`；`Restore` 維護、不展開連帶 | DI-KS 12 的文字要補 | 回滾頁 R-2 前提 1 一句 |
| C12 | 編號映射 | `MapEntities`（`#[entities]`） | 沒有；作者的對照表 | 讀檔宣告 5.1 已定 | 作者改寫 `ChildOf::parent`（和 `ChaseTarget` 一樣） |
| C13 | 登記 | 用到時自動登記 | Kernel 在 `Registry` 建立時登記（ComponentId 0、1） | GYO 的 id 由登記順序定（DI-KS 4） | 0；測試的寫死值改一次 |
| C14 | 通用的關係入口 | 公開（derive） | 只定形 | 薄起步 | 物理那一批開放 |
| C15 | 父比子晚到（複製） | lightyear `PendingChildOf` | 同形，網路 Module 做 | 無 | 網路 Module 一處（和 lightyear 相同） |

GYO 自己的設計和 Bevy 的接合點：編排層、權威、回滾都只看得到 `ChildOf`／`Children` 兩個普通元件，不知道「關係」的維護（回滾頁 R-3、權威 DI-A 8 不變）。

---

## 7. Design Invariants 與相關文件的改動（草稿；最終文字在合併前核准時定）

條號照 SDL 宣告第 10.1 節的分配：DI-KS 29～34 給 KR。RC 的分配（25～27）和已合併的摘要那一批撞號（`v8-arch-gyo2-kernel-summary-messages` 已用 RC 25），本卡改用 **RC 26、27**，不碰 R1a、R1b 的 28、29（回報主會話，第 13 節）。

### 7.1 新條文（`docs/architecture/target/kernel.zh-Hant.md`）

- **DI-KS 29 關係的形。** 來源側元件存一個目標實體；目標側元件存來源的集合，只由 Kernel 維護。Kernel 自帶 `ChildOf`（來源側）與 `Children`（目標側，連帶刪除），在 `Registry` 建立時以 Kernel 為 owner 登記，每個 World 都有。來源側不可就地改，只能插入（換目標）或拿掉；目標側作者讀得到、寫不到、插不進、拿不掉，也不能當 `RestoreSet` 的欄或 `InitialValue` 的目標——都在編譯期拒。通用的關係登記入口只定形（`Registrar::Relationship<R>`）。守衛：負向編譯樣本各入口一個；`relationship:` 的形的案例。
- **DI-KS 30 兩邊一致。** 每一次套用之後（同步點的每一個階段、清空輪的每一輪、當場改、`Restore`、Host 的直接放），對每個活著的實體：有 `Children` ⇔ 至少一個活著的實體的 `ChildOf` 指向它；內容是那些實體，依 EntityId 升冪〔Q1 選 (B) 時〕。新出現與被覆寫的關係在 spawn、insert、初始值之後、OnAdd 之前一起套上（第 5b 步）；被銷毀的關係當場從目標側拿掉，目標側空了先 OnRemove 再拿掉。OnRemove hook 看到拿掉之前的一致狀態，OnAdd hook 與 observer 看到套用之後的一致狀態；例外：連帶刪除中的子（DI-KS 32）。目標側只在內容變時打 changed stamp。守衛：`relationship:` 的一致案例（生成、換父、脫離、子被刪、同一階段父子一起生成且子的編號較小、hook 讀到的狀態、清空輪、當場改、Host）。
- **DI-KS 31 不合法的目標。** 目標不活著、或指向自己時，那一筆關係不成立：新出現的不放（不通知），覆寫的連同原本的關係一起拿掉（OnRemove 照跑）；留一筆 `RejectedRelationship` 明細與總數，不是錯誤。Kernel 不查環。守衛：兩種原因各一案、覆寫一案、明細的欄位。
- **DI-KS 32 連帶刪除。** despawn 階段處理一個實體時，它的 `Children` 裡的子接在它後面一起刪：父先於子、同層依 `Children` 的順序、遞迴到底，不經 hook 輪、不受 hook 輪數的上限；每個實體完整處理才處理下一個；已處理的跳過（有環也會結束）。目標被刪而來源留著時，來源的關係在同一步拿掉。`Restore` 不展開連帶刪除。守衛：孫、深度大於 hook 輪上限、先後、環、`Restore` 不展開。
- **DI-KS 33 回滾、讀檔、摘要裡的關係。** 來源側是狀態：進摘要，可以登記回滾、可以存檔。目標側是 Kernel 從來源側與存活集合維護的索引：進摘要（值確定），不能登記回滾、不存檔也放不回；放回來源側就重建。〔Q1 選 (B) 時〕守衛：`Restore` 換父、預測死亡的復活與直跑的摘要相同；讀檔只放 `ChildOf` 後 `Children` 和存檔前相同。

（DI-KS 34 保留。）

### 7.2 改寫的條文

- **DI-KS 9**：套用順序的列舉補「第 5b 步：關係」與「despawn 展開連帶刪除」；「每階段依 id」那句補「連帶的子接在帶它的實體之後」。
- **DI-KS 12**：「`Restore` 照給的內容放回，不推導資料」之後補一句：「關係的目標側是索引，照 DI-KS 30 維護；連帶刪除不展開（給的存活集合是完整的）」。
- **DI-KS 15**：職責清單加 **(14) 關係**——來源側與目標側的形、兩邊一致的維護、連帶刪除、`ChildOf`／`Children`、`RejectedRelationship`。同一個變更改邊界統合宣告（`kernel-app-boundary-declaration.zh-Hant.md`）的 Kernel 職責清單（L17 的清單）。
- DI-KS 11 不改（hook 仍只有兩種）。

### 7.3 Replacement Criteria（寫在積上去之前）

- **RC 26 維護的路不夠**：有點名的 Module 需要換父的通知（Bevy 的 `Insert`／`Discard`）才能做對，或維護需要第三種套用路（同步點、當場改、`Restore`、Host 以外）——停下來交使用者「加 hook 的種類（改 DI-KS 11）」或「改維護的形」。
- **RC 27 索引的形擋路**：(a) `Children` 的升冪順序讓兩個以上的 Module 各自寫排序鍵的膠水；(b) 通用的關係入口開放時，關係的種類需要 Kernel 認得它的意思（不是一張表能描述）；(c) X2 或 UI 有預算後，維護或連帶刪除的成本超出預算（計測）——停下來交使用者「修還是重建」。

### 7.4 其他文件

| 文件 | 改什麼 |
|---|---|
| `system-contract-page.zh-Hant.md` | 新增一節「父子與關係」（遊戲例子、兩邊一致的時點、換父、連帶刪除、不合法的目標、回滾只登記 `ChildOf`、讀檔只存 `ChildOf`、重新配號要改寫 `parent`、Host 直接放也維護）；讀檔一節補第 5.9 節的段落。節號照合併時的 gyo2 |
| `rollback.zh-Hant.md` | R-2 前提 1 補 `Children`（第 5.8 節）；改條文前給使用者看（R-2 是 Design Invariant） |
| `kernel-app-boundary-declaration.zh-Hant.md` | Kernel 職責清單加「關係」 |
| `HANDOFF.md`、批次表 | 主會話記（照四樣轉） |

---

## 8. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：父子（X2 傳播、UI、物理、遊戲邏輯的從子查父）、刪父連帶刪子、回滾與讀檔裡的父子（第 3 節）。
2. **現在的問題**：Kernel 沒有實體之間的關係；作者只能自己存反向表，要兩邊一致只能靠 hook（下一窗才一致、換父沒有 hook）或每幀比對（Unity 的延遲）；連帶刪除走 hook 會撞清空輪的上限。
3. **變的邊界**：Kernel 的公開介面多兩個元件型別與一個診斷紀錄型別；套用步驟多第 5b 步、despawn 展開連帶刪除；Kernel 職責清單多一項。
4. **影響**：kernel（程式、條文）；回滾頁 R-2 前提 1（文字）；說明頁；邊界統合宣告；X2、UI、物理、網路複製、讀檔的寫法（之後的批）。編排層、權威、回滾、時間的程式不改。
5. **依賴方向**：不變。新型別在 Kernel；Module 往下依賴 Kernel。
6. **Ownership**：父子關係與兩邊一致歸 Kernel（P1、P2）；回滾要不要記 `ChildOf` 歸作者；父比子晚到的等待歸網路 Module。
7. **更小的變更為什麼不行**：只加型別、不加維護＝作者自己維護（兩邊一致是 P1 要的）；用現行 hook 維護＝下一窗才一致（P4，使用者不選 Unity 式延遲）；不在 Kernel＝推翻 P1。

---

## 9. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項 | 決勝理由 | 要它的 Feature／Product |
|---|---|---|---|---|
| 維護在套用步驟（Q2） | Bevy 的語意＋GYO `InitialValue`（第 5a 步）的位置 | 加 hook 種類＋當場套用；照現行 hook | 唯一不改 DI-KS 11、18 又讓 OnAdd 看到一致狀態的路 | X2、UI、物理（PvP、Object_Connect、工具） |
| `Children` 升冪（Q1 (B)） | flecs 的預設（對照）＋GYO DI-KS 3 | Bevy 的插入先後＋重排 | 回滾、讀檔不必改已核准的條文；膠水集中在 UI 一處 | 回滾（PvP Client）、讀檔、UI |
| 編譯期拒就地改 | Bevy `Immutable` | 執行期檢查；不擋 | 守的是兩邊一致的契約；編譯期零成本 | 同上 |
| Kernel 自己登記 | Bevy（`bevy_ecs` 自帶）＋GYO `KernelSettings` 的位置 | 編排層預設骨架登記；用到它的 Module 登記（會 R-E2） | 每個 World 都能用、id 不隨作者移動 | 同上 |
| 連帶刪除在 despawn 展開 | Bevy 的語意 | 走 hook 輪；放寬上限 | 不受上限、不多一個例外 | 同上 |
| 不合法的目標＝拿掉＋明細 | Bevy（拿掉）＋GYO `RejectedSpawn`（明細） | 保留舊關係；當成 Err | 和 Bevy 結果相同；照「告訴外面發生了什麼」 | 讀檔分批、網路複製 |
| 通用入口只定形 | Bevy 的形 | 本批就開放 | AGENTS §2 薄起步；物理那一批才有使用者 | 物理（avian `ColliderOf`） |

不以「既有測試不必改」當理由：C13 的代價照寫。

---

## 10. 要使用者定的，與本卡照 L17 判的

### 10.1 要使用者定的（和 Bevy 分歧、改已核准條文）

**Q1 `Children` 的內容與順序（K5、K6、K7）**：(B) EntityId 升冪、`Children` 是索引（**建議**）／(A) 照 Bevy 的插入先後＋可重排、`Children` 是狀態。比較在第 5.4 節。選 (A) 時第 5.8、5.9 節與 DI-KS 30、33 改寫，回滾 Module 與讀檔要各加一個機制（成對登記、跳過維護），各自再交你。

**Q2 維護與連帶刪除放在哪（K1、K3、K4）**：放在 Kernel 的套用步驟、不經 hook（**建議**；改 DI-KS 9、12 的文字，DI-KS 11、18 不改）／加 hook 種類並讓它們記的變更當場套用（改 DI-KS 9、11、18）／照現行 hook（下一窗才一致）。比較在第 5.3 節。

### 10.2 本卡照命名規範、Bevy 原始碼、職責清單判的（核准時一起看，有異議再改）

1. 不可就地改用編譯期拒（K2；Bevy `Immutable`；第 5.2 節）。
2. `ChildOf`／`Children` 由 Kernel 在 `Registry` 建立時登記、每個 World 都有（K9；第 5.1 節）。
3. 不合法的目標＝拿掉＋`RejectedRelationship`（第 5.5 節）；不查環（Bevy 也不查）。
4. 連帶刪除父先於子、同層依 `Children` 的順序（第 5.6 節）——P8 的延伸：P8 定的是兩筆明寫的 despawn，連帶的子本卡照 Bevy 判。
5. Host 的直接放也維護（第 5.7 節）。
6. `Restore` 維護目標側、不展開連帶刪除（第 5.8 節；DI-KS 12 補一句的文字要你同意，屬 Q2 的連帶）。
7. RC 改用 26、27（和已合併的 RC 25 撞號）。
8. 通用入口、一對一、`allowSelf`、走訪輔助、換父通知都不在本批。

### 10.3 批次表交來的一件：`LogWorldLabel`（代碼審計 N2）

批次表「後續小事」寫「下一次動 Kernel 的批（KR）一起看」。看了：Bevy 的 `WorldId` 是 SystemParam（`bevy_ecs/src/world/identifier.rs:65`），GYO 的 System 拿不到 World 名，world_logging 每個描述器各抄一份 Resource（`LogWorldLabel.hpp`、`Forward.hpp`）。這是 Kernel 的事（只有 Kernel 能給參數），但和關係是不同的 Feature；併進 KR 會讓 KR 的審查混進另一個公開介面（AGENTS §1、§11）。**建議另開一個小的 Kernel 批**（或併進 R1a、R1b 之一，由主會話排），不在 KR 做。

---

## 11. 子批與檔位

上段（`agents/navigator.md`）：Kernel 的公開介面、Design Invariants、職責清單。

```text
KR  kernel-relationship
├─ 第 0 步  跟上 gyo2、基準建置與 kernel 測試全過            ｜Opus／medium
├─ 第 1 步  型別、Kernel 自己登記、編譯期拒、HashValue、負向樣本｜Opus／high
├─ 第 2 步  套用步驟的維護（5b、despawn／remove 當場、不合法目標、
│           RejectedRelationship、當場改、清空輪、Host）         ｜Opus／high
├─ 第 3 步  連帶刪除；Restore 的規則；回滾與讀檔的組合案          ｜Opus／high
├─ 第 4 步  條文、說明頁、回滾頁 R-2 前提 1、邊界統合清單；
│           突變（相關的＋新加的）；dev_log                       ｜Opus／medium
└─ 第 5 步  設計審查（唯讀）→ 合併前核准頁                       ｜Fable／high
```

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus／high | 上段不用小模型；卡片指定 |
| 審查者（code review） | Opus／high | 卡片指定 |
| 設計審查 | Fable／high | 批次表〈每批的設計審查〉：和實作者不同模型 |
| §14.4 的驗證者 | Opus／high | 判定只加不鬆時不需要；寫死欄數的測試改值時用（照實作者） |
| 宣告的敵對式檢查 | Fable／high | 作者 Opus；比主會話大，**經主會話問使用者** |

第 2 步的套用順序與連帶刪除屬決定性的邏輯；照使用者 2026-11-19（xhigh 只在要調查時用），先用 high，碰到停止條件再停下回報。

---

## 12. 測試、判定（先宣告；結果出來之後不放寬）

### 12.1 案例（新檔 `tests/common/kernel/RelationshipTests.cpp`，ctest `kernel.*`，標籤 `cpu`）

| 標記 | 案例 | 判定 |
|---|---|---|
| `[kr-spawn]` | 生成帶 `ChildOf{p}` 的子 | 同一個同步點之後 `p` 的 `Children`＝{子}；子的 `ChildOf::parent`＝p |
| `[kr-same-phase-small-id]` | 同一個 System 生成父、再 `SpawnWithId` 一個編號比父小的子 | 關係成立（5b 在 spawn 之後） |
| `[kr-reparent]` | 對子再插入 `ChildOf{q}` | p 的 `Children` 少掉子（空了就沒有那一格）、q 的有 |
| `[kr-detach]` | 拿掉子的 `ChildOf` | p 的 `Children` 少掉子 |
| `[kr-child-despawn]` | 刪子 | 同上 |
| `[kr-order]` | 三個子以不同先後掛上（含把舊實體掛到新父） | `Children` 依 EntityId 升冪〔Q1 (B)〕 |
| `[kr-onadd-sees]` | `ChildOf` 的 OnAdd hook 與 `Add<ChildOf>` observer 讀父的 `Children` | 已含自己 |
| `[kr-onremove-sees]` | `ChildOf` 的 OnRemove hook 讀父的 `Children` | 還含自己 |
| `[kr-children-hooks]` | `Children` 的 OnAdd（第一個子）、OnRemove（最後一個子離開） | 各一次 |
| `[kr-children-changed]` | 同值再插入 `ChildOf{p}` | `Changed<ChildOf>` true；p 的 `Changed<Children>` false |
| `[kr-dead-target]` | 子指向已刪的實體 | 不成立、`RejectedRelationship` 一筆、原因 TargetNotAlive |
| `[kr-self]` | 子指向自己 | 不成立、原因 SelfReference |
| `[kr-reparent-dead]` | 已有父的子改指向已刪的實體 | 子沒有 `ChildOf`、舊父的 `Children` 少掉它、OnRemove 一次 |
| `[kr-linked]` | 刪父 | 子、孫一起刪 |
| `[kr-linked-deep]` | 深度 > `hookRoundLimit`（例 20 層） | 全刪、沒有 `HookRoundLimit` |
| `[kr-linked-order]` | 父、子、孫的 OnRemove 記錄先後 | 父 → 子（依 `Children`）→ 孫 |
| `[kr-linked-cycle]` | A、B 互為父子後刪 A | 兩個都刪、結束 |
| `[kr-hook-round]` | hook 記的 `ChildOf` 在清空輪套用 | 那一輪之後一致 |
| `[kr-immediate]` | `ExclusiveWorld` 當場插入 `ChildOf` | 呼叫回來時一致 |
| `[kr-host]` | Host 的 `World::Insert<ChildOf>`、`Despawn(父)` | 一致、連帶刪除；不跑 hook |
| `[kr-restore-reparent]` | `Restore` 把 `ChildOf` 放回換父之前 | 兩邊一致；摘要＝直跑 |
| `[kr-restore-revive]` | `Restore` 復活被連帶刪除的子 | 父的 `Children` 回來；摘要＝直跑 |
| `[kr-restore-no-expand]` | `Restore` 刪掉父、但存活集合留著「之後才掛上去」的子 | 子活著、沒有 `ChildOf` |
| `[kr-summary]` | 同一組操作兩個 World | 摘要相同；`Children` 的值進摘要 |
| `[kr-load]` | 生成期間只放 `ChildOf` 的讀檔（照存檔編號、重新配號各一） | `Children` 和存檔前相同；重新配號改寫 `parent` |

回滾 Module 的整合案（`rollback.Component<ChildOf>()`、預測換父與預測死亡後回滾）：要加在 `tests/common/rollback/`，和網路 PR-2 卡的檔案相鄰。**先問主會話先後**；不能加時，`[kr-restore-*]` 用 `RestoreSet` 直接驗 Kernel 這一半，回滾 Module 那一半記成範圍的限界。

### 12.2 負向編譯樣本（照 #286 的共用判法：靜態斷言失敗＋標記同一行）

`Write<ChildOf>`、`Write<Children>`、`Commands::Insert<Children>`、`Commands::Remove<Children>`、`Spawn<Bundle<Children>>`、`RestoreSet::Column<Children>`、`ExclusiveWorld::Modify<ChildOf>`、`World::GetMut<ChildOf>`。各一個樣本、各一個專用標記。

### 12.3 突變（AGENTS §14.5：每條寫它獨有的誤、守的條文；D184：只跑相關的與新加的）

| id | 改什麼 | 預期失敗 | 守的 |
|---|---|---|---|
| `kr-no-add` | 5b 不加進目標側 | `[kr-spawn]` | DI-KS 30 |
| `kr-add-at-spawn` | 在 spawn 當下維護（不等 5b） | `[kr-same-phase-small-id]` | DI-KS 30 |
| `kr-move-keeps-old` | 覆寫不從舊父拿掉 | `[kr-reparent]` | DI-KS 30 |
| `kr-empty-kept` | 目標側空了不拿掉 | `[kr-detach]` | DI-KS 30 |
| `kr-erase-no-drop` | 銷毀子時不維護 | `[kr-child-despawn]` | DI-KS 30 |
| `kr-append` | 加在尾端、不排序 | `[kr-order]` | DI-KS 30〔(B)〕 |
| `kr-after-onadd` | 5b 移到 OnAdd 之後 | `[kr-onadd-sees]` | DI-KS 30 |
| `kr-drop-before-onremove` | 先維護再跑 OnRemove | `[kr-onremove-sees]` | DI-KS 30 |
| `kr-children-stamp-always` | 目標側每次都打 stamp | `[kr-children-changed]` | DI-KS 30 |
| `kr-dead-kept` | 不查目標活不活著 | `[kr-dead-target]` | DI-KS 31 |
| `kr-self-kept` | 不查指向自己 | `[kr-self]` | DI-KS 31 |
| `kr-reparent-dead-keeps-old` | 覆寫不合法時保留舊關係 | `[kr-reparent-dead]` | DI-KS 31 |
| `kr-linked-off` | 不展開連帶刪除 | `[kr-linked]` | DI-KS 32 |
| `kr-linked-one-level` | 只展開一層 | `[kr-linked]`（孫） | DI-KS 32 |
| `kr-linked-by-id` | 連帶的子依 EntityId 混進明寫的序列 | `[kr-linked-order]` | DI-KS 32 |
| `kr-linked-no-visited` | 不記已處理 | `[kr-linked-cycle]`（逾時：用 CTest 的 TIMEOUT 判，預期失敗寫測試名） | DI-KS 32 |
| `kr-restore-expands` | `Restore` 也展開連帶刪除 | `[kr-restore-no-expand]` | DI-KS 32、12 |
| `kr-restore-no-maintain` | `Restore` 不維護 | `[kr-restore-reparent]` | DI-KS 33、12 |
| `kr-host-no-maintain` | Host 路不維護 | `[kr-host]` | DI-KS 30 |
| `kr-hash-children-skipped` | `Children` 的 `HashValue` 不寫內容 | `[kr-summary]` | DI-KS 33 |

`[kr-linked-deep]`、`[kr-hook-round]`、`[kr-immediate]`、`[kr-restore-revive]`、`[kr-load]` 守的是組合（同一段程式走不同的路）；它們抓到的誤和上表重疊時不另立突變（§14.5），實作時逐條確認，寫進 dev_log。

### 12.4 受け入れ判定

- `kernel.*` 全部通過（含既有案例；寫死欄數的改值逐條列在 dev_log，§14.4 的 1～5：判定只因 Kernel 多兩個欄而改期待值，沒有緩的面）。
- 新案例全部通過；上表突變全部 killed（預期失敗）。
- 相關的既有突變（find 在 `World.cpp` 套用步驟、`Registry.cpp` 的 Kernel 登記、`Params.hpp` 的入口）重跑，killed 數不少於基準。
- L1 三平台（linux-x64、windows-x64、macos-arm64）通過；draft PR 手動 dispatch L1。
- 判定不看任何計時；沒有計測，不需要件數的突き合わせ。

---

## 13. 協調

| 對象 | 什麼 | 怎麼做 |
|---|---|---|
| 網路 PR-2 卡（`local_a19de0ec`） | KR 不改回滾 Module 的程式；要改回滾頁 R-2 前提 1 的文字，想在 `tests/common/rollback/` 加整合案。另轉 K8：`ChildOf` 指向不在的實體時不成立，父比子晚到要網路 Module 自己等（lightyear `PendingChildOf`）；digest 裡 `ChildOf` 的值是原始編號 | 經主會話排先後、轉達 |
| SDL 宣告卡（`local_2cc7e494`） | K1～K9 的回答（第 6 節）；X2 等 KR；Q1 (B) 時 UI 要排序鍵（U 的宣告）；RC 號改 26、27 | 經主會話 |
| R1a、R1b | 都改 `World.cpp`、`Executor.cpp`；SDL 宣告建議 R1a 先合、KR 後合 | 誰後合併誰跟上 gyo2（merge，不 rebase） |
| 主會話 | 本宣告核准後照四樣記 HANDOFF；`LogWorldLabel` 的去處（第 10.3 節） | 宣告送出時一起 |

---

## 14. 停止條件（AGENTS §14 冒頭）

- 實作中發現第 5.3 節的維護需要改 DI-KS 11 或 18 → 停下回報（RC 26）。
- 既有 `kernel.*` 案例因 KR 而失敗、且原因不是「多兩個欄」→ 停下回報（回歸）。
- 範圍超出：要改回滾 Module、編排層、權威的程式 → 停下回報。

---

## 15. 檢查與核准

- 修訂 0：本稿。下一步：敵對式檢查（Fable／high，唯讀；用之前經主會話問使用者）。報告放主 checkout `build/evidence/gyo2-kernel-relationship/`。
- 檢查之後修訂，交主會話轉使用者做動工前核准（Q1、Q2 與第 10.2 節）。核准前不動 `engine/`、`tests/`。
