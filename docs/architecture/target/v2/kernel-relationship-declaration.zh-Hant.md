# 事前宣告：Kernel 的關係與父子（`v8-arch-gyo2-kernel-relationship`，子批 KR）

更新：2026-11-23（修訂 1）。Owner：Kernel（GYO2 主會話開出的任務卡，使用者「開 1、2、3」的第 1 張）。**狀態：使用者 2026-11-23 在本卡動工前核准（原話「Q1～Q4 照建議，其他一起同意，開始」）：Q1＝(A′) 附掛先後、Q2＝維護在套用步驟（5b 在 insert 之後、初始值之前）、Q3＝本階段明寫的關係紀錄優先於連帶、Q4＝深度優先前序；第 10.1 節末的條文與判定變更（DI-KS 9、12、15，R-2 前提，讀檔宣告 C-10，第 12.5 節的 golden）一起同意。實作開始。**

經過：
- 修訂 0（4b5340e2）：照卡片寫。
- 敵對式檢查：**Opus／high，和作者同一個模型**（原定 Fable／high；使用者 2026-11-23 經主會話「現在fable用量不夠了，暫時用Opus」；照 `agents/navigator.md`「別のモデルを用意できないとき」寫明）。blocker 5、should 11、nit 12，全部採用。報告：主 checkout `build/evidence/gyo2-kernel-relationship/declaration-r0-adversarial-opus.md`。對照在第 16 節。
- **修訂 1（這一份）**：最大的改動——(1) Q1 的建議從「EntityId 升冪的索引」(B) 改成「附掛先後」(A′)：檢查指出 bevy_text 的 span 與 bevy_ui 的排版都靠 `Children` 的順序，SDL 計畫已點名 T、U 兩批，(B) 一開始就會碰到自己寫的 RC 27(a)（S6），而且 (B)「不必改已核准條文」的決勝理由不成立（B3）。(2) 第 5b 步移到 insert 之後、初始值之前（B2）。(3) 新增 Q3「同一階段先拆再刪」（B1）、Q4「連帶刪除的先後」（S3）。(4) 要改的已核准條文與判定逐條列出、交使用者同意（B3、B4、B5、S4）。(5) 主會話 2026-11-23 排的第 13 節四件。

根據：
- HANDOFF `v8-arch-gyo2-hierarchy-bevy-and-primary-reference`（使用者 2026-11-23「照建議」）：父子照 Bevy 進 Kernel（含連帶刪除、從子查父），`transform` 只放 `Transform`、`GlobalTransform` 與傳播；Kernel 職責清單多一項「關係」（使用者同意）；ECS 機制以 Bevy 為主參考，偏離要寫衝突點與膠水量。
- HANDOFF `v8-arch-gyo2-transform-camera-physics-modules`（同日）：Transform、相機、物理放 Module。KR 排在 X2（讀 `ChildOf` 的階層傳播）之前的出處是 SDL 宣告第 10.1 節（「KR 之後：X2」）。
- SDL 宣告（分支 `claude/gyo2-sdl-declaration-7e7a1f`，修訂 18 de73b4cd，還沒核准）第 6.5 節：統合只寫了需求 1～5 與衝突點 K1～K9，「K3、K6、K7（以及 K1、K2 選改條文的路時）在 KR 的宣告交使用者」。本宣告逐條回答（第 6 節）。
- 批次表〈每批的設計審查〉第 5 項（主參考 Bevy）。

讀的版本：`origin/gyo2` b5ef9f87（`engine/`、`tests/` 和修訂 0 讀的 0ed6ea10 相同）；Bevy 本機 `6a911653f`（`bevy_ecs` 0.20.0-dev）；bevy_replicon、lightyear、flecs `09790b5f7`、Unity Entities 本機鏡像。行號都是這些版本的。

---

## 1. 這一批做什麼（先用遊戲講）

- **武器掛在角色手上**：生成武器時帶 `ChildOf{角色}`。同一個同步點之後，角色的 `Children` 就有武器。
- **角色被刪，武器跟著刪**：只刪角色；同一個同步點裡 Kernel 把角色的子、孫……都刪掉。
- **角色死時掉落武器**：同一個 System 先拿掉武器的 `ChildOf`、再刪角色，武器留下（Q3）。
- **道具換到另一個角色身上（換父）**：對道具再插入 `ChildOf{新角色}`。舊角色的 `Children` 少掉它、新角色的多了它（排在最後）；舊角色沒有子了，`Children` 那一格拿掉。
- **UI 面板裡按鈕的先後、一段文字裡各 span 的先後**：照掛上去的先後；讀檔時照存檔的順序生成就能重現（Q1）。
- **回滾讓預測中被打掉的道具回來**：回滾 Module 放回 `ChildOf`、`Children` 與存活集合，和直跑逐位元相同。

`transform` 的階層傳播（X2）、UI 的節點樹（U）、文字的 span（T）、物理的碰撞體掛剛體（avian 的 `ColliderOf`）都會積在這上面，所以是基盤（前提 P6）。

---

## 2. 前提表

| # | 前提 | 誰定的／出處 | 掛在上面的決定 |
|---|---|---|---|
| P1 | 父子照 Bevy 放進 Kernel，當「關係」；`transform` 只放 Transform 與傳播 | 使用者 2026-11-23（`-hierarchy-bevy-and-primary-reference` (1)） | 全部 |
| P2 | Kernel 職責清單多一項「關係」 | 使用者同意（同上） | DI-KS 15（第 7.2 節） |
| P3 | ECS 機制以 Bevy 為主參考；偏離要寫衝突點與膠水量；Unity、flecs 只當對照、不當混搭的來源 | 使用者 2026-11-23（同上 (2)） | 第 6 節；Q1～Q4 |
| P4 | hook 只有 OnAdd、OnRemove；hook 記的變更屬下一窗 | DI-KS 9、11（使用者 2026-11-22 核准）；hook 清空那一批 | Q2 |
| P5 | `Restore` 照給的內容放回、不推導資料；通知照跑 | DI-KS 12（使用者 2026-11-22 選 (a2)） | 第 5.8 節；Q1 |
| P6 | 基盤／應用的判準：上面會積 X2、T、U、物理、網路複製、讀檔 | AGENTS §2；SDL 宣告第 6.5 節、第 10.1 節 | 用予見的 Pressure 設計（T、U 要子的順序）；RC 先寫 |
| P7 | 讀檔一律在新的 World，用一般的 `Spawn`／`SpawnWithId` 放回（或 C-10 的直接放）；Kernel 不認得讀檔，沒有「讀檔中」的旗標 | 讀檔宣告 5.1、5.3、C-10（`restore-entry-declaration.zh-Hant.md:139`、`:400`） | 第 5.9 節；Q1 |
| P8 | 刪除實體時一個做完才做下一個、依 EntityId 升冪；每個實體先全部通知再銷毀 | 使用者 2026-11-16（`-despawn-notify-all-then-destroy`、`-despawn-entity-by-entity`，`HANDOFF.md:116`、`:118`） | Q4 |
| P9 | `Changed<T>` 是「被寫過」；同值同步的寫者先比再寫（CCR-8） | DI-KS 28；CCR-8 | 第 5.11 節 |
| P10 | 底層不替誤用加守衛；守衛只守寫明的契約 | 使用者 2026-11-21／22 | 第 5.2 節 |
| P11 | 規格 F.4：hook 只讀自己實體的元件（Kernel 沒擋） | `plans/kernel/spec.zh-Hant.md:710`；`HANDOFF.md:118` | 測試的寫法（第 12 節用 observer 驗跨實體的讀） |
| P12 | 順序只取決於 id、Plan、登記序與宣告（互不衝突的 System 在 Plan 裡用 registrationSeq 定平手） | DI-KS 3；`Scheduler.cpp:15-25` | Q1 (A′) 的同階段先後 |

---

## 3. Feature → Contract → Architecture

| Feature（誰用） | Contract | Architecture |
|---|---|---|
| 從子查父、從父查子（X2、T、U、物理、遊戲邏輯） | `ChildOf{parent}` 在子上、`Children` 在父上；一般的 `Query<Read<…>>` 讀 | Kernel 自帶兩個元件型別（第 5.1 節） |
| 兩邊一致（作者不必自己維護反向表） | 每一次套用之後兩邊一致；OnAdd hook 與 observer 看到一致的狀態（DI-KS 30） | 維護內建在 Kernel 的套用步驟（Q2） |
| 子的先後（T 的 span、U 的排版） | `Children` 照附掛先後；讀檔照存檔順序生成就重現（DI-KS 30、33） | Q1 (A′) |
| 刪父連帶刪子；掉落武器 | despawn 展開、遞迴到底；本階段明寫拆開的子不連帶（DI-KS 32） | Q3、Q4 |
| 回滾等價（PvP Client） | `ChildOf`、`Children` 都是狀態；`Restore` 給了 `Children` 就照給的放回（DI-KS 33） | Q1 |
| 讀檔 | 存 `ChildOf`（要順序時存的先後就是生成的先後） | Q1 |
| 網路複製 | 目標不在時那一筆不成立、留明細 | 交網路複製那批（第 5.13 節） |
| 物理的 `ColliderOf`（avian） | 通用的關係登記入口：形狀定下、不實作 | RC 27 |

---

## 4. Bevy 怎麼做（主參考）與對照

### 4.1 Bevy

| 項目 | Bevy | 出處 |
|---|---|---|
| 形 | 來源側（`Relationship`，例 `ChildOf(Entity)`）存一個目標；目標側（`RelationshipTarget`，例 `Children(Vec<Entity>)`）存來源的集合；derive 時互指 | `relationship/mod.rs:28-34`、`hierarchy.rs:105-107`、`:148-152` |
| 可不可改 | **來源側**一律 `Immutable`，只能重新插入（`macro_logic/src/component.rs:310-312` 只對 `relationship.is_some()`）；**目標側是 `Mutable`**（`mod.rs:270`），集合欄位私有，`collection_mut_risky` 註明作者不要用（`:296`），但作者可以 `Query<&mut Children>` 用 `sort_by`、`swap` 等重排（`hierarchy.rs:154-240`） | 同左 |
| 維護 | hook：`on_insert` 把來源加進目標側（經 commands，「Deferring is necessary for batch mode」，`mod.rs:195`）；`on_discard` 當場從目標側拿掉，空了經 commands 拿掉那一格（`:219-262`）；目標側的 `on_discard` 拿掉所有來源的關係（`:307-327`） | 同左 |
| 不合法的目標 | 目標不存在、或指向自己（預設不准）：`warn!`＋拿掉關係 | `:166-178`、`:205-214`；`ALLOW_SELF_REFERENTIAL` `:127` |
| 連帶刪除 | 目標側標 `linked_spawn`（`Children` 有）：`on_despawn` 對每個來源 `try_despawn`；每次 despawn 之後 flush、巢狀 flush 只跑新排的 → **深度優先的前序**；要避開就「先拆、再刪」 | `:270-278`、`:275-276`、`:332-338`；`world/entity_access/world_mut.rs:1700-1702`；`world/mod.rs:3151-3190` |
| 環 | 不查；`iter_ancestors` 等有環會無限迴圈（文件寫明） | `relationship_query.rs:41`、`:58` |
| 順序 | `Vec` 的先後＝插入先後；可插在指定位置、可排序 | `relationship_source_collection.rs:137-200`；`hierarchy.rs:292-298` |
| 同值再插入 | 只有一個子時：`on_discard` 排「空了就拿掉」、`on_insert` 排「加回」→ `Children` 先 Remove 再 Add；多個子時被移到最後 | `mod.rs:242-257`、`:194-205` |
| 讀檔 | `DynamicWorld` 連 `Children` 也存；放回元件時 `RelationshipHookMode::Skip` | `bevy_world_serialization/src/dynamic_world.rs:148-156`；`relationship/mod.rs:403-410` |
| 順序的使用者 | bevy_ui 的排版照 `Children` 的先後（`bevy_ui/src/layout/mod.rs:165-166`、`:288`、`:352`）；bevy_text 的 span 用 `Children` 走訪（`bevy_text/src/text_access.rs:20`） | 同左 |
| 複製（生態） | bevy_replicon 只複製 `ChildOf`，`Children` 靠插入時的 hook；父比子晚到時 lightyear 另做 `PendingChildOf` | `bevy_replicon/src/lib.rs:255-259`；`lightyear/crates/replication/replication/src/hierarchy.rs:44-56`、`:358-376` |
| 回滾（生態） | 本機 lightyear 的 prediction 沒有回滾 `ChildOf` 的例子（`git grep -i childof crates/prediction` 0 筆；正例：同指令在 `crates/replication` 命中） | — |
| 物理（生態） | avian 的 `ColliderOf` 是關係，允許指向自己 | `lightyear/crates/integration/avian/src/plugin.rs:428-429`、`:517` |

Bevy 的「立即」是「那一次操作 flush 之後」；observer 可能看到還沒 flush 的狀態（lightyear `hierarchy.rs:542-544`）。

### 4.2 對照（只查證，不混搭）

- **flecs**：`EcsChildOf` 在核心；子的順序預設不保證，父加 `EcsOrderedChildren` 才保證穩定順序（`include/flecs.h:1851-1853`、`:4415-4428`）——「要順序時保留插入先後」，和 (A′) 同向。
- **Unity Entities**：`Parent`＋`Child` 緩衝，`ParentSystem` 只保證每次更新之後一致；連帶刪除另一套 `LinkedEntityGroup`。使用者 2026-11-23 不選。

---

## 5. 設計（Contract）

### 5.1 型別與登記（K9）

**前提**：Engine 的事（P1）。

- Kernel 自帶 `ChildOf{EntityId parent}` 與 `Children`。名字照 Bevy（命名規範：取別家已通用的名字時照搬）。
- **`Children` 的表示**：大小在三個 L1 平台與 Debug／Release 都相同（`sizeof` 進每份 Registry 指紋，`Registry.hpp:331-332`、`Fingerprint.cpp:35-37`；J-18 的 golden 是跨平台守衛）。所以不直接放 `std::vector`（MSVC Debug 的迭代器除錯會改它的大小），用 Kernel 自己的固定寬度表示（指標＋32 位元的個數與容量），對外只給只讀的走訪（`begin`／`end`、`size`、`operator[]`、`Contains`）。
- **每個 World 都有**：Kernel 在 `Registry` 建立時自己登記，和 `KernelSettings` 同一處（`Registry.cpp:649` 的「kernel's own registrations come first」），owner 是 Kernel。`ChildOf` 是 ComponentId 0、`Children` 是 1，作者登記的往後排、不隨作者的數量移動。照 Bevy：`ChildOf` 在 `bevy_ecs`，不必加 Plugin 就能用。
- 作者再登記 `ChildOf`／`Children` 是既有的 R-E2。權威要當判定資料時用 `Mark<ChildOf>()`。
- **不加 `StartupCode`**：只有 Kernel 自帶的一對，沒有作者的登記會錯（SDL K9 問的）。
- **指紋與摘要的版本不升**：Registry 指紋與摘要的格式都沒變，值會變（多兩個欄，第 12.5 節的判定變更）。「每份 Registry 帶兩個 Kernel 元件」會照常出現在指紋的元件表裡。
- **通用的登記入口只定形、不實作**（AGENTS §2 薄起步；SDL 需求 4）：`Registrar::Relationship<R>(RelationshipOptions)`——`R` 宣告 `using Target = …;` 與一個 `EntityId` 欄位，選項 `linkedDespawn`、`allowSelf`；一對一、自訂集合、連帶複製也只定形。Kernel 內部照「一張關係表」寫（`ChildOf` 是唯一的一列）。第一個點名的使用者是物理（avian 的 `ColliderOf`），那一批再開（RC 27 (b)）。

### 5.2 誰能寫（K2）

**前提**：Engine 的事。擋的是「就地改 `ChildOf` 讓兩邊不一致」，守的是 DI-KS 30 的契約本身，不是擋誤用（P10）。Bevy 對來源側也在型別層擋。

編譯期拒（`static_assert`，訊息帶各自的 `GYO_KERNEL_REJECT_RELATIONSHIP_…` 標記；負向編譯樣本照 #286 的共用判法）：

| 入口 | `ChildOf` | `Children` |
|---|---|---|
| 讀（`Read<T>`、`With`／`Without`／`Changed`／`Added`、`World::Get`／`Keys`／`Stamps`、`ReadColumn`、`SetRead`） | 可 | 可 |
| 就地寫（`Write<T>`（`Query::GetMut` 跟著擋）、`Mut`、`World::GetMut`、`ExclusiveWorld::Modify`） | **拒** | **拒** |
| 插入（`Commands::Insert`、`CommandsFor<Inserts<…>>`、`Spawn`／`SpawnWithId` 的 bundle、`ExclusiveWorld` 的當場插入與生成、Host 的 `World::Insert`） | 可（再插入＝換父） | **拒** |
| 拿掉（`Commands::Remove`、`CommandsFor<…, Removes<…>>`、`ExclusiveWorld` 的當場拿掉、Host 的 `World::Remove`） | 可（＝脫離父） | **拒** |
| `RestoreSet::Column<T>` | 可 | 可（第 5.8 節） |
| `Registrar::InitialValue<T, …>` 的目標 | 照既有規則：目標由登記它的 owner 宣告，owner 是 Kernel → 作者宣告是 R-E4（`Registry.cpp:757-762`，實作時以案例確認） | **拒** |
| `Registrar::InitialValue<X, Fn(const T&)>` 的來源 | 可 | 可（5b 在初始值之前，第 5.3 節） |

和 Bevy 的差（C5）：Bevy 的 `Children` 可就地重排、可 `remove::<Children>()`（等於拆掉所有子）；這裡都拒。重排的入口只定形（Kernel 的一種紀錄「把子移到第 n 位」），T 或 U 那一批有 Feature 時開（RC 27 (a)）。脫離所有子要逐個拿掉 `ChildOf`。

### 5.3 維護的時點（K1、K3）——Q2

**前提**：Engine 的事。作者在 Kernel 外做兩邊一致只能用 hook（下一窗才一致、換父沒有 hook，P4）或每幀比對（Unity 的延遲）。

**建議：維護內建在 Kernel 的套用步驟，不經 hook。** 先例：`InitialValue`（DI-KS 9 的第 5a 步）也是 Kernel 在同一個同步點、OnAdd 之前做的。

在每一個套用階段（hook 佇列的第 0 步、窗的第 1～7 步各是一個階段；當場改是一個階段）裡：

| 時點 | 做什麼 |
|---|---|
| despawn、remove 階段：一格 `ChildOf` 被銷毀之後 | 當場從父的 `Children` 拿掉它（OnRemove hook 在銷毀之前跑，看到的父的 `Children` 還含自己）。`Children` 空了先不拿掉那一格，等 5b |
| spawn 階段：bundle 裡的 `ChildOf` | 照常寫進格；目標還不檢查（父可能在這個階段稍後才生成，例：子的編號比父小） |
| insert 階段：`ChildOf` 的插入紀錄 | 寫之前檢查目標（這時本階段的刪除與生成都已套用，目標活不活著已確定）。不合法：第 5.5 節。合法的覆寫：當場從舊父的 `Children` 拿掉 |
| **第 5b 步**（insert 之後、5a 初始值之前） | (1) 檢查 spawn 階段寫進的 `ChildOf` 的目標（不合法的那一格拿掉，它還沒被任何 hook 看到）；(2) 把本階段新出現與換父的關係加進新父的 `Children`，順序照 Q1；沒有 `Children` 那一格就新建，新格進 OnAdd 的名單、也進 5a 的第一輪；(3) 對本階段 `Children` 變空的父**看淨差**：最後仍然沒有子的，先跑那一格的 OnRemove（與 `Remove<Children>` observer），再拿掉；同一階段又有子加入的不拿掉、不通知 |
| 5a、OnAdd、resource、觸發 | 照常；這時兩邊已一致 |

為什麼 5b 在 5a 之前（B2）：`ChildOf`／`Children` 不可能是 5a 的目標（第 5.2 節），但可能是 5a 的來源；5b 在前，從 `ChildOf` 推的初始值不會根據一筆之後被拒的關係，從 `Children` 推的也觸發得到。

**一致的時點**（DI-KS 30）：每一次套用之後（同步點的每個階段、清空輪的每一輪、當場改、`Restore`、Host 的直接放）兩邊一致。OnAdd hook 與 observer 看到套用之後的一致狀態。OnRemove hook 看到拿掉之前的狀態，例外有四種，條文逐條寫明：
1. 連帶刪除中的子：父已不在（同 Bevy，第 5.6 節）。
2. `Restore` 刪了父、子留下時：子的 `ChildOf` 的 OnRemove 看到指向已刪的實體（第 5.8 節）。
3. 同一階段裡，一格 `ChildOf` 銷毀之後到 5b 之前，父的 `Children` 可能是空的（Bevy 也是：空了經 commands 才拿掉）。
4. 5b 裡 `Children` 的 OnRemove 看到的是空的 `Children`（同 Bevy `mod.rs:242-257`）。

**和 Bevy 的差**：機制不同；看得到的結果相同、OnAdd 與 observer 的保證更嚴（C1、C2）。

**沒選的路**：
- (ii) 加 hook 種類（Bevy 的 `on_insert`／`on_discard`）＋讓它們記的變更當場套用：改 DI-KS 9、11、18；「hook 記的變更當場套用」是清空輪設計的例外，作者自己的 hook 也會要。
- (iii) 照現行 hook：第一個子加上後到下一個同步點之前父沒有 `Children`；換父沒有 hook；等於 Unity 的延遲。
- (iv) 不存 `Children`、改成查詢時的索引（檢查 S7；flecs 用 `(ChildOf, parent)` 查詢列子）：只在 Q1 選 (B) 時成立；偏離 Bevy 的 `Query<&Children>`、`With<Children>`（avian 用 `With<Children>` 找剛體，`lightyear/crates/integration/avian/src/plugin.rs:920`），而且沒有地方放順序。

**換父沒有 hook**：GYO 的 hook 只有兩種（DI-KS 11），覆寫不觸發；換父看得到的是 `Changed<ChildOf>` 與兩個父的 `Changed<Children>`。Bevy 有 `Insert`／`Discard` 事件（C3）。有 Module 需要時是 RC 26。

### 5.4 `Children` 的內容與順序（K5）——Q1

**前提**：Engine 的事（只有 Kernel 能定 `Children` 的順序）。

| | **(A′) 附掛先後，`Children` 是狀態（建議）** | (B) EntityId 升冪，`Children` 是索引 | (D) 不存 `Children`，查詢時列出 |
|---|---|---|---|
| 內容與順序 | 新掛上的子排在最後。同一個階段裡依附掛那筆紀錄的 (planPos, recSeq)（spawn 紀錄或 insert 紀錄；hook 佇列那一階段照它的 recSeq）；`Restore` 沒給 `Children` 時依 EntityId；Host 的直接放依呼叫先後。同值再插入不動（C15） | 依 EntityId 升冪；`ChildOf` 與存活集合的純函式 | 同 (B)，不是元件 |
| 和 Bevy | 同「插入先後」；同一階段的先後用 GYO 的確定鍵（P12：跨 System 的先後跟 Plan 走，互不衝突時平手照 registrationSeq——和現行 insert「同一格後寫的勝」同一種依賴） | 偏離（flecs 的「不保證」也不是升冪） | 偏離（沒有 `Children` 元件） |
| T、U（點名的批） | 直接用 `Children` 的先後（bevy_text、bevy_ui 的做法） | 兩個 Module 各要一個排序鍵 → RC 27(a) 一開始就觸發 | 同 (B) |
| 重排 | 入口只定形，T／U 要時開（Kernel 的一種紀錄） | 不能 | 不能 |
| 回滾（K6） | `ChildOf` 與 `Children` 都登記回滾（前提寫進 R-2）；`Restore` 給了 `Children` 就照給的放回、不維護，沒給就維護 | 只登記 `ChildOf`；`Restore` 維護 | 只登記 `ChildOf` |
| 讀檔（K7） | 存 `ChildOf`；要保留順序時，讀檔的 System 照存檔的 `Children` 順序在同一個階段生成（或插入 `ChildOf`），recSeq 就重現順序。不需要 Bevy 的 `Skip`（P7 不變） | 存 `ChildOf`；照一般路放回 | 同 (B) |
| 要改的已核准條文 | DI-KS 12（補句）、R-2 前提（成對登記）、讀檔宣告 C-10（Host 路的三件）、說明頁 | DI-KS 12（補句）、R-2 前提 1（`Children` 可讀）、C-10、說明頁 | DI-KS 12、R-2 前提 1、C-10，加 Kernel 參數 |
| 摘要 | 進摘要（順序也進） | 進摘要 | 不在欄裡 |
| 網路兩端 | 先後跟附掛紀錄走 | 兩端編號不同時順序不同 | 同 (B) |

建議 (A′)：真正的差別在 T、U 已點名而且照 Bevy 靠 `Children` 的先後（P6：基盤用點名 Feature 預見的 Pressure 設計）；(A′) 的讀檔仍走一般路、不需要「讀檔中」的旗標；代價是回滾要成對登記（作者的責任，和 R-2 其他前提同類，回滾 Module 不加檢查）。三個選項都要改 DI-KS 12、R-2 的前提與 C-10，「不必改已核准條文」不是決勝理由（修訂 0 的寫法不成立，B3）。

不列 flecs 式（預設升冪、父可選保留順序）：兩套順序並存，是主參考之外的混搭（P3）。

### 5.5 不合法的目標

**前提**：Engine 的事（只有套用時看得到目標活不活著）。

照 Bevy（目標不存在、指向自己就把關係拿掉）：
- **目標不活著**（這個階段之前已刪、或這個 World 沒有這個實體）；**指向自己**（`ChildOf` 不准；`allowSelf` 只定形）。
- spawn 階段寫進的（新格、還沒被 hook 看到）：5b 拿掉，不跑 OnAdd、OnRemove。
- insert 階段的新格：不寫。insert 階段的覆寫：當成拿掉——OnRemove 照跑（看到的是原本合法的值），從舊父拿掉。結果和 Bevy 相同：實體沒有 `ChildOf`。
- 留一筆 Kernel 自己的診斷紀錄 `RejectedRelationship` 與總數（`World::RejectedRelationships()`），不是錯誤（Bevy 是 `warn!`；Kernel 不寫日誌，用診斷紀錄告訴外面）。欄位：實體、目標、原因（TargetNotAlive／SelfReference）、記錄者與是否 hook、schedule、步、套用它的同步點、同步點序號、路（同步點／當場改／`Restore`／Host）。Host 的直接放：schedule 空、步 0、記錄者 `Host`；`Restore`：記錄者是呼叫 `Restore` 的獨占 System、路是 `Restore`。
- `ExclusiveWorld` 的當場插入 `ChildOf` 被拒時回 false（和實體不在相同；原因在診斷紀錄）。

不查環（Bevy 也不查）。Kernel 只保證自己的走訪會結束（連帶刪除跳過已不活著的實體，`World.cpp:697-698` 已有）；X2 要不要偵測環，照 `bevy_transform`（`systems.rs:360`「Malformed hierarchy」）在 X2 定。

### 5.6 連帶刪除（K4）——Q3、Q4

**前提**：Engine 的事。

- despawn 階段處理一個被刪的實體時，把它的 `Children` 裡的子接在它後面一起刪，遞迴到底。每個實體照 P8 完整處理（全部 OnRemove → 全部銷毀 → 移出存活集合）才處理下一個；已不活著的跳過。
- 不經 hook 輪，不受 `hookRoundLimit`（照現行 hook 做會一層一輪、超過 8 層 World 中斷）。
- 被刪的子的 OnRemove 跑時父已不在（同 Bevy）。
- **先後（Q4）**：建議**深度優先的前序**（照 Bevy）：父 → 第一個子 → 它的整棵子樹 → 第二個子……，子照 `Children` 的順序；明寫的 despawn 紀錄之間照舊依 EntityId 升冪（P8）。另一個選項：先展開全部子孫，再和明寫的一起依 EntityId 升冪處理——合 P8 的字面（子的編號比父小時，子的 OnRemove 看得到父），和 Bevy 不同。P8 定的是兩筆明寫的 despawn，沒有區分連帶的，所以交你。
- **同一階段的拆與刪（Q3）**：建議——
  - 本階段有 `ChildOf` 的拿掉或覆寫紀錄的子，**不連帶**（它的那筆紀錄照常在 remove、insert 階段套用）。這是 Bevy 文件寫的「先拆、再刪」（`mod.rs:275-276`）；GYO 的同步點不看記錄先後（DI-KS 9），所以改成「本階段明寫的關係紀錄優先於連帶展開」，不論誰先記。
  - 同一階段刪了父、又有子掛到它（生成時帶 `ChildOf{父}` 或插入）：父先刪，子的關係不成立（第 5.5 節），子活著、沒有父，留 `RejectedRelationship`。
  - 另一個選項：照現狀（連帶在 despawn 階段先展開），說明頁寫「要先拆再刪就跨一個同步點」。
- **目標被刪、來源留著**時（`Restore`，以及之後的非連帶關係），來源的關係在同一步拿掉（OnRemove 照跑）。

### 5.7 Host 的直接放（C-10 那條路）

`World::Insert`／`Remove`／`Despawn`（Host、Ready）照舊不跑 hook、不補初始值、不碰配發器；但**照樣維護 `Children`、照樣連帶刪除、照樣拒絕不合法的 `ChildOf`**——維護不是通知，是 DI-KS 30 的契約。這改了讀檔宣告 C-10 已核准的「三個差別」，列在第 7.4 節交你同意。
- 一次放一格，所以 `ChildOf` 被放進來時父要已經活著：照存檔放回時先放父、再放子（說明頁寫；`[kr-host-load-order]` 驗）。
- Host 路改到 `Children` 那一欄時照 World API 的寫入標記（`MarkApiWritten`），存取驗證打開時下一次 Run 的邊界比對不誤報（`World.cpp:1065-1084`）。

### 5.8 回滾（K6）

**前提**：Engine 的事只到「`Restore` 之後兩邊一致」；要不要回滾 `ChildOf` 是作者的選擇。

以 Q1 選 (A′) 寫：
- `ChildOf` 與 `Children` 都是狀態。作者回滾父子時兩個都登記（`rollback.Component<ChildOf>()`、`rollback.Component<Children>()`）；R-2 前提補「`ChildOf` 登記回滾時 `Children` 也登記」（作者的責任；回滾 Module 不加檢查，同 R-2 的其他前提）。不用 `rollback.Derived<Children>()`：`Derived` 要一個每步重算的立即寫者 System（R-7），`Children` 的寫者是 Kernel，而且順序是歷史、算不出來。
- **`Restore`**：`RestoreSet` 給了 `Children` 就照給的放回、這次不維護 `Children`；沒給就照 DI-KS 30 維護（順序依 EntityId）。**連帶刪除不展開**（給的存活集合是完整的；展開會刪掉「之後才掛上某個父、但當時活著」的子）。
- `Restore` 還會做兩件和「照給的放回」不同的事，寫進 DI-KS 12 的補句（第 7.2 節，交你同意；B4）：
  - (a) `Restore` 刪了父、子留下、而子的 `ChildOf` 指著那個父時，Kernel 拿掉子的 `ChildOf`（就算 `RestoreSet` 沒提到 `ChildOf`）。不拿就破 DI-KS 30；作者登記了 `ChildOf` 時，這和 `RestoreSet` 給的拿掉相同，之後那筆變成無作用。
  - (b) `RestoreSet` 給的 `ChildOf` 指向不活著的實體（例：權威 inbox 給的值，`History.hpp:95-108` 優先用 inbox）時，照第 5.5 節不成立、留 `RejectedRelationship`；放回的結果和給的不同。
- SDL K6 的三種情形：換父的回滾（給了 `Children` 照放；沒給則維護處理覆寫）、順序變過的回滾（`Children` 照放）、預測死亡的復活（spawn 先復活、5b 才維護；給了 `Children` 照放）。
- 回滾的歷史在 `History.hpp:95-108` 的虛擬函式裡用 `RestoreSet::Column<T>`；(A′) 下 `Column<Children>` 是允許的。

選 (B) 時這一節改成「只登記 `ChildOf`、`Restore` 一律維護」，R-2 前提 1 補「`Children` 可讀」。

### 5.9 讀檔（K7）

以 Q1 選 (A′) 寫：
- 存 `ChildOf`；讀檔照讀檔宣告的路放回，`Children` 自己長回來。要保留子的順序時，讀檔的 System 照存檔的 `Children` 順序生成（同一個階段裡 recSeq 決定先後）。`Children` 本身放不回（插入是拒的）。
- 父要在子的同一個同步點或更早放回；子的 `ChildOf` 指向還沒放回的父，那一筆不成立並留 `RejectedRelationship`。C-10 的直接放要先放父（第 5.7 節）。
- 重新配號時作者的對照表改寫 `ChildOf::parent`（讀檔宣告 5.1、案例 2 的 `ChaseTarget` 寫法）。Bevy 有 `MapEntities`（`#[entities]`），GYO 的 Kernel 沒有編號映射，維持讀檔宣告的決定（C12）。
- 讀檔宣告 5.3 的「將來加關係時照 Bevy 再看」：不需要 Bevy 的 `Skip`，P7 不變。說明頁讀檔一節補這一段。

### 5.10 World 摘要、權威的 digest

- **摘要**（DI-KS 27，版本 4）：`ChildOf`、`Children` 和其他欄一樣進摘要（`Children` 的順序也進）。Kernel 替兩者寫 `HashValue`（`ChildOf`：父的編號；`Children`：個數與依序的每個編號）。
- **digest**（權威 DI-A 12）：`ChildOf` 要當判定資料時用 `authority.Mark<ChildOf>()`；值裡有父的編號，兩端編號要相同——和其他存實體編號的元件同一個限制，交網路複製那批。
- 權威的「啟動時看得到的寫」：`Children` 沒有作者的寫者；AU-E6 不受影響。

### 5.11 `Changed<T>`／CCR-8

- `ChildOf` 被插入（含同值再插入）就是被寫過（P9）。同步 `ChildOf` 的寫者照 CCR-8 先比再寫。
- `Children` 只在內容或順序變時打 changed stamp；新建那一格時 added＝changed＝那次套用的 stamp。同值再插入不動 `Children`（和 Bevy 不同，C15）。

### 5.12 事件游標、observer

`Add<…>`、`Remove<…>` 的 observer 照一般元件跑（DI-KS 10），跑的時候兩邊已一致。事件游標不相關。

### 5.13 複製（K8）

交網路複製那批（主會話 2026-11-23 排定；照 lightyear 的 `PendingChildOf`，父比子晚到時由網路自己等）。Kernel 這一側就是第 5.5 節的行為。

### 5.14 範圍的限界（本批不做）

- 通用的 `Registrar::Relationship<R>`、一對一、`allowSelf`、自訂集合、連帶複製、重排：只定形。
- 走訪的輔助（祖先、子孫、根）：X2 需要時在 X2 判放 transform 還是 Kernel。
- 「生成時一起帶子」的便利寫法：`Spawn::New` 當下就回傳編號，同一個 System 先生成父、再帶 `ChildOf{父}` 生成子即可。
- 換父的通知：RC 26。
- **效能的預算延後**（AGENTS §2 要和性能有關的 Feature 寫預算）：本批沒有點名的預算；X2 或 U 有預算時量（RC 27 (c)）。這是對 §2 的延後，寫明。

---

## 6. 和 Bevy 的對照表（衝突點與膠水量；回答 SDL K1～K9）

| # | 項目 | Bevy | 本宣告 | 衝突點 | 膠水（誰寫、多少） |
|---|---|---|---|---|---|
| C1 | 維護的機制（K1、K3） | hook＋commands | 套用步驟（5b、銷毀與覆寫的當場維護） | P4 | Kernel 內部一個步驟；作者 0 |
| C2 | 一致的時點（K3） | flush 之後（observer 可能看到 flush 前） | 每次套用之後；OnAdd 與 observer 一定一致 | 無（更嚴） | 0 |
| C3 | 換父的通知（K1） | `Insert`／`Discard` | 沒有；看 `Changed<…>` | DI-KS 11 | 需要的 Module 自己比 `Changed<ChildOf>`；RC 26 |
| C4 | 來源側不可就地改（K2） | `Immutable` | 編譯期拒 | 無 | 0 |
| C5 | 目標側可重排、可整個拿掉 | 可（`Mutable`） | 拒；重排入口只定形 | Kernel 要維持 DI-KS 30 | T／U 要重排時開入口（Kernel 一種紀錄） |
| C6 | 順序（K5） | 插入先後 | (A′) 附掛先後；同一階段用 (planPos, recSeq) | DI-KS 9 不看記錄時間先後 | 0 |
| C7 | 連帶刪除（K4） | `on_despawn` 遞迴 | despawn 階段展開 | hook 輪上限 | 0 |
| C8 | 連帶刪除的先後 | 深度優先前序 | Q4 建議同 Bevy | P8 | 0 |
| C9 | 先拆再刪 | commands 照順序 | Q3：本階段明寫的關係紀錄優先 | DI-KS 9 | 0 |
| C10 | 不合法的目標 | `warn!`＋拿掉 | 拿掉＋`RejectedRelationship` | Kernel 不寫日誌 | 0 |
| C11 | 讀檔（K7） | 存兩邊、`Skip` | 存 `ChildOf`、照順序生成 | P7 | 讀檔的 System 照存檔順序生成（一般寫法） |
| C12 | 編號映射 | `MapEntities` | 作者的對照表 | 讀檔宣告 5.1 | 作者改寫 `parent` |
| C13 | 回滾（K6） | 生態沒有例子 | 兩邊都登記；`Restore` 給了就照放 | DI-KS 12、R-2 | 作者多登記一行 |
| C14 | 登記（K9） | 用到時自動登記 | `Registry` 建立時（ComponentId 0、1） | DI-KS 4 | 0；golden 改一次（第 12.5 節） |
| C15 | 同值再插入 | `Children` Remove＋Add（多個子時移到最後） | 不動 | 無（GYO 少一次通知） | 0 |
| C16 | 通用入口 | 公開 | 只定形 | 薄起步 | 物理那一批開 |
| C17 | 父比子晚到（K8） | lightyear `PendingChildOf` | 同形，網路做 | 無 | 網路一處 |

GYO 自己的設計和 Bevy 的接合點：編排層、權威、回滾都只看到兩個普通元件，不知道關係的維護（回滾 R-3、權威 DI-A 8 不變）。

---

## 7. Design Invariants 與相關文件的改動（草稿；最終文字在合併前核准時定）

條號：DI-KS 29～33（SDL 宣告第 10.1 節分配 29～34，34 保留）；RC 26、27（分配的 RC 25 已被摘要那一批用掉；主會話 2026-11-23 同意改號）。

### 7.1 新條文（`docs/architecture/target/kernel.zh-Hant.md`）

- **DI-KS 29 關係的形。** 來源側元件存一個目標實體；目標側元件存來源的集合，只由 Kernel 維護。Kernel 自帶 `ChildOf`（來源側）與 `Children`（目標側，連帶刪除，大小各平台相同），在 `Registry` 建立時以 Kernel 為 owner 登記，每個 World 都有。來源側不可就地改，只能插入（換目標）或拿掉；目標側作者讀得到，寫、插、拿都不行，只有 `RestoreSet` 能放回——都在編譯期擋。通用的登記入口、重排只定形。守衛：負向編譯樣本每個入口一個；`relationship:` 的形的案例。
- **DI-KS 30 兩邊一致。** 每一次套用之後，對每個活著的實體：有 `Children` ⇔ 至少一個活著的實體的 `ChildOf` 指向它；內容是那些實體，依附掛的先後〔Q1 (A′)〕。銷毀與覆寫當場從舊目標拿掉；新出現與換目標的在 insert 之後、初始值之前（第 5b 步）一起加上；目標側變空的看淨差、在 5b 先 OnRemove 再拿掉。OnAdd hook 與 observer 看到套用之後的一致狀態；OnRemove hook 看到拿掉之前的狀態，例外四種（第 5.3 節）。目標側只在內容或順序變時打 changed stamp。守衛：`relationship:` 的一致案例。
- **DI-KS 31 不合法的目標。** 目標不活著、或指向自己時，那一筆關係不成立：還沒通知過的新格拿掉或不寫；覆寫的當成拿掉（OnRemove 照跑）。留 `RejectedRelationship` 明細與總數，不是錯誤。Kernel 不查環。守衛：兩種原因、覆寫、spawn 階段、Host、`Restore` 各一案；明細的欄位。
- **DI-KS 32 連帶刪除。** despawn 階段處理一個實體時，它的 `Children` 裡的子接在它後面一起刪，遞迴到底〔Q4〕，不經 hook 輪、不受輪數上限；每個實體完整處理才處理下一個；已不活著的跳過。本階段有 `ChildOf` 拿掉或覆寫紀錄的子不連帶〔Q3〕。目標被刪而來源留著時，來源的關係在同一步拿掉。`Restore` 不展開。守衛：孫、深度大於輪數上限、先後、環、先拆再刪、刪父又掛子、`Restore` 不展開。
- **DI-KS 33 回滾、讀檔、摘要裡的關係。** 〔Q1 (A′)〕兩邊都是狀態：進摘要（順序也進）、可以登記回滾；`Restore` 給了目標側就照給的放回、這次不維護，沒給就維護。存檔只需要來源側；子的順序由放回時的附掛先後重現。守衛：`Restore` 換父、預測死亡的復活與直跑的摘要相同；讀檔照順序生成後 `Children` 和存檔前相同。

（DI-KS 34 保留。）

### 7.2 改寫的條文（交你同意，Architecture Delta）

- **DI-KS 9**：套用順序補「第 5b 步：關係（insert 之後、5a 之前）」與「despawn 展開連帶刪除」；「每階段依 id」補「連帶的子接在帶它的實體之後〔Q4〕；本階段明寫的關係紀錄優先於連帶〔Q3〕」。
- **DI-KS 12**：「`Restore` 照給的內容放回，不推導資料」之後補：「關係是例外的兩件：`RestoreSet` 沒給目標側時 Kernel 照 DI-KS 30 維護它；被刪的實體留下的來源，關係在同一步拿掉；給的來源側指向不活著的實體時不成立（DI-KS 31）。連帶刪除不展開（給的存活集合是完整的）」。
- **DI-KS 15**：職責清單加 **(14) 關係**——兩側的形、兩邊一致的維護、連帶刪除、`ChildOf`／`Children`、`RejectedRelationship`。同一個變更改邊界統合宣告的 Kernel 職責清單。
- DI-KS 11、18 不改。

### 7.3 Replacement Criteria（寫在積上去之前）

- **RC 26 維護的路不夠**：有點名的 Module 需要換父的通知才能做對，或維護需要第三種以上的例外路（同步點、當場改、`Restore`、Host 以外）——停下來交使用者「加 hook 的種類（改 DI-KS 11）」或「改維護的形」。
- **RC 27 關係的形擋路**：(a) T 或 U 要重排、或兩個以上的 Module 各自寫排序的膠水；(b) 通用入口開放時，關係的種類需要 Kernel 認得它的意思（一張表描述不了）；(c) X2 或 U 有預算後，維護或連帶刪除的成本超出預算（計測）——停下來交使用者「修還是重建」。

### 7.4 其他文件（已核准的契約有改的標 ★，交你同意）

| 文件 | 改什麼 |
|---|---|
| ★ `rollback.zh-Hant.md` R-2 前提 | 補「`ChildOf` 登記回滾時 `Children` 也登記」〔(A′)〕；在 KR 的實作 PR 裡改（主會話 2026-11-23：和網路 PR-2 誰後合併誰 merge gyo2，兩邊內容都保留） |
| ★ `restore-entry-declaration.zh-Hant.md` C-10 | 直接放的三個差別之外，加「維護 `Children`、連帶刪除、拒絕不合法的 `ChildOf`；先放父再放子」 |
| `system-contract-page.zh-Hant.md` | 新增一節「父子與關係」（例子、一致的時點與四個例外、換父、先拆再刪、連帶刪除、不合法的目標、回滾成對登記、讀檔照順序生成、重新配號改寫 `parent`、Host 直接放）；讀檔一節補第 5.9 節。節號照合併時的 gyo2 |
| `kernel-app-boundary-declaration.zh-Hant.md` | Kernel 職責清單加「關係」 |
| 規格 F.4 | 不改；測試用 observer 驗跨實體的讀（P11） |

---

## 8. Architecture Delta（AGENTS §3 的七項）

1. **Feature**：父子（X2、T、U、物理、遊戲邏輯）、子的先後、刪父連帶刪子、回滾與讀檔裡的父子。
2. **現在的問題**：Kernel 沒有實體之間的關係；作者只能自己存反向表，兩邊一致只能靠 hook（下一窗、換父沒有 hook）或每幀比對；連帶刪除走 hook 會撞清空輪的上限。
3. **變的邊界**：Kernel 公開介面多兩個元件型別與一個診斷紀錄型別；套用步驟多第 5b 步、despawn 展開連帶刪除；職責清單多一項；`Restore` 與 Host 直接放的行為多關係的那幾件。
4. **影響**：kernel（程式、條文、golden）；回滾頁 R-2 前提、讀檔宣告 C-10、說明頁、邊界統合宣告（文字）；X2、T、U、物理、網路複製、讀檔的寫法（之後的批）。編排層、權威、回滾、時間的程式不改。
5. **依賴方向**：不變。
6. **Ownership**：父子與兩邊一致歸 Kernel；回滾要不要記父子歸作者；父比子晚到的等待歸網路複製那批。
7. **更小的變更為什麼不行**：只加型別不維護＝作者自己維護；用現行 hook＝下一窗才一致；不存 `Children`＝沒有地方放 T、U 要的順序、偏離 Bevy；不在 Kernel＝推翻 P1。

---

## 9. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的選項 | 決勝理由 | 要它的 Feature／Product |
|---|---|---|---|---|
| 維護在套用步驟（Q2） | Bevy 的語意＋GYO `InitialValue` 的位置 | 加 hook 種類＋當場套用；照現行 hook；不存 `Children` | 不改 DI-KS 11、18 而 OnAdd 與 observer 看到一致狀態；保留 Bevy 的 `Children` 元件 | X2、T、U、物理（PvP、Object_Connect、工具） |
| `Children` 附掛先後（Q1 (A′)） | Bevy 的插入先後＋GYO 的確定鍵（DI-KS 3、9） | (B) 升冪索引；(D) 不存；flecs 式可選 | T、U 已點名而照 Bevy 靠子的先後；讀檔不需要旗標 | T、U、回滾、讀檔 |
| 編譯期擋 | Bevy（來源側 `Immutable`） | 執行期檢查；不擋 | 守 DI-KS 30；零執行成本 | 同上 |
| Kernel 自己登記 | Bevy（`bevy_ecs` 自帶）＋GYO `KernelSettings` 的位置 | 編排層骨架登記；用到的 Module 登記（R-E2） | 每個 World 都能用、id 不隨作者移動 | 同上 |
| 連帶刪除在 despawn 展開、深度優先（Q4） | Bevy | 走 hook 輪；先展開再依 id | 不受上限；先後同 Bevy | 同上 |
| 先拆再刪（Q3） | Bevy 文件的避開法 | 照現狀（跨同步點） | 掉落武器是常見寫法 | PvP、2D 遊戲 |
| 不合法＝拿掉＋明細 | Bevy＋GYO `RejectedSpawn` | 保留舊關係；Err | 結果同 Bevy；告訴外面發生了什麼 | 讀檔、網路複製 |
| 通用入口只定形 | Bevy 的形 | 本批開放 | 薄起步；物理那一批才有使用者 | 物理 |

不以「既有測試不必改」當理由；golden 的代價照列（第 12.5 節）。

---

## 10. 要使用者定的，與本卡照 L17 判的

### 10.1 要使用者定的

**Q1 `Children` 的內容與順序（K5、K6、K7；和 Bevy 分歧的程度）**：(A′) 附掛先後、`Children` 是狀態（**建議**）／(B) EntityId 升冪、`Children` 是索引／(D) 不存 `Children`。第 5.4 節。

**Q2 維護與連帶刪除放在哪（K1、K3、K4）**：Kernel 的套用步驟、5b 在 insert 之後初始值之前（**建議**）／加 hook 種類並當場套用／照現行 hook。第 5.3 節。

**Q3 同一階段先拆再刪（和 Bevy 的記錄先後不同）**：本階段明寫的 `ChildOf` 拿掉或覆寫優先於連帶、刪父又掛子時子不成立並留紀錄（**建議**）／照現狀、跨同步點才拆得開。第 5.6 節。

**Q4 連帶刪除的先後（P8 的延伸）**：深度優先前序、照 Bevy（**建議**）／先展開再和明寫的一起依 EntityId 升冪。第 5.6 節。

**核准時一起同意的已核准條文與判定**（AGENTS §3、§14.4）：DI-KS 9、12、15 的改寫（第 7.2 節）；★ 回滾 R-2 前提與讀檔宣告 C-10（第 7.4 節）；golden 與寫死值的改值（第 12.5 節的 1～5）。

### 10.2 本卡照命名規範、Bevy 原始碼、職責清單判的（有異議再改）

1. 來源側編譯期擋就地改（Bevy）；目標側只有 `RestoreSet` 能放（第 5.2 節）。
2. `ChildOf`／`Children` 由 Kernel 在 `Registry` 建立時登記、每個 World 都有；`Children` 用固定寬度的表示；不加 `StartupCode`；指紋與摘要的版本不升（第 5.1 節）。
3. 不合法的目標＝拿掉＋`RejectedRelationship`、欄位第 5.5 節；當場插入被拒回 false；不查環。
4. Host 直接放也維護（第 5.7 節；C-10 的改寫交你同意）。
5. 同值再插入不動 `Children`（C15）。
6. 通用入口、一對一、`allowSelf`、重排、走訪輔助、換父通知不在本批。
7. RC 用 26、27（主會話同意）。
8. 存取驗證打開時，`StructureProblem`（`World.cpp:999-1016`）加查 DI-KS 30（開發版的檢查，只報 Kernel 自己的不變量；檢查 N12）。

### 10.3 `LogWorldLabel`（代碼審計 N2）

不併進 KR，另開小批（主會話 2026-11-23 同意，批次表由主會話改）。

---

## 11. 子批與檔位

上段（`agents/navigator.md`）：Kernel 的公開介面、Design Invariants、職責清單。

```text
KR  kernel-relationship
├─ 第 0 步  跟上 gyo2、基準建置與 kernel 測試全過、基準突變                    ｜Opus／medium
├─ 第 1 步  型別（固定寬度的 Children）、Kernel 自己登記、編譯期擋、HashValue、
│           負向樣本、golden 改值                                             ｜Opus／high
├─ 第 2 步  套用步驟的維護（銷毀與覆寫的當場維護、5b、淨差、不合法目標、
│           RejectedRelationship、hook 佇列階段、清空輪、當場改、Host、
│           StructureProblem 的檢查）                                          ｜Opus／high
├─ 第 3 步  連帶刪除（Q3、Q4）；Restore 的規則；回滾與讀檔的組合案             ｜Opus／high
├─ 第 4 步  條文、說明頁、回滾頁 R-2 前提、讀檔宣告 C-10、邊界統合清單；
│           突變；dev_log                                                      ｜Opus／medium
└─ 第 5 步  設計審查（唯讀）→ 合併前核准頁                                     ｜Fable／high（不夠用時同主會話的指示，用 Opus 並寫明）
```

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 實作者 | Opus／high | 上段不用小模型；卡片指定 |
| 審查者（code review） | Opus／high | 卡片指定 |
| 設計審查 | Fable／high | 批次表〈每批的設計審查〉要和實作者不同模型；Fable 用量不夠時照主會話 2026-11-23 的指示用 Opus、報告寫明同模型 |
| §14.4 的驗證者 | Opus／high | 第 12.5 節的改值要另一個 Agent 用實際資料確認 1～5 |
| 宣告的敵對式檢查 | Opus／high（已做，同模型） | 使用者 2026-11-23 |

第 2、3 步屬決定性的邏輯；照使用者 2026-11-19（xhigh 只在要調查時用）用 high，碰到停止條件停下回報。

---

## 12. 測試、判定（先宣告；結果出來之後不放寬）

### 12.1 案例（新檔 `tests/common/kernel/RelationshipTests.cpp`，ctest `kernel.*`，標籤 `cpu`）

跨實體的讀（子的事件讀父的 `Children`）一律用 observer 驗（P11）；hook 只驗自己實體上的格。

| 標記 | 案例 | 判定 |
|---|---|---|
| `[kr-spawn]` | 生成帶 `ChildOf{p}` 的子 | 同步點之後 p 的 `Children`＝{子}、子的 `parent`＝p |
| `[kr-same-phase-small-id]` | 同一個 System 生成父、再 `SpawnWithId` 一個編號比父小的子 | 關係成立 |
| `[kr-reparent]` | 對子再插入 `ChildOf{q}` | p 少掉子（空了沒有那一格）、q 有且排最後 |
| `[kr-detach]`、`[kr-child-despawn]` | 拿掉 `ChildOf`；刪子 | p 少掉子 |
| `[kr-order]` | 三個子跨兩個同步點、同一個 System 裡依序掛上；再把舊實體掛上 | 依附掛先後；同一階段依 recSeq〔(A′)〕 |
| `[kr-order-two-systems]` | 兩個互不衝突的 System 同一階段各掛一個子 | 依 Plan 先後；換登記順序時兩份 World 各自確定（記下實際先後，不跨登記順序比） |
| `[kr-observer-add]` | `Add<ChildOf>` observer 讀父的 `Children` | 已含自己 |
| `[kr-observer-remove]` | `Remove<ChildOf>` observer 讀父的 `Children` | 還含自己 |
| `[kr-children-hooks]` | 父自己的 `Children` 的 OnAdd（第一個子）、OnRemove（最後一個子離開；看到空的） | 各一次 |
| `[kr-children-observers]` | `Add<Children>`、`Remove<Children>` observer | 各一次 |
| `[kr-net-diff]` | 同一階段 C1 離開 P、C2 加入 P；C1<C2 與 C2<C1 各一份 | 兩份都沒有 `Children` 的 OnRemove／OnAdd，內容相同 |
| `[kr-children-changed]` | 同值再插入 `ChildOf{p}` | `Changed<ChildOf>` true；p 的 `Changed<Children>` false |
| `[kr-initial-from-childof]` | `InitialValue<X, Fn(const ChildOf&)>`，子的目標不合法 | 沒有 X |
| `[kr-initial-from-children]` | `InitialValue<X, Fn(const Children&)>` | 父第一個子掛上的那個同步點就有 X |
| `[kr-dead-target]`、`[kr-self]` | 指向已刪的實體；指向自己 | 不成立；`RejectedRelationship` 一筆，**每個欄位**逐一驗 |
| `[kr-spawn-dead-target]` | 生成時帶指向已刪實體的 `ChildOf` | 沒有 `ChildOf`、沒有 OnAdd／OnRemove、一筆紀錄 |
| `[kr-reparent-dead]` | 有父的子改指向已刪的實體 | 沒有 `ChildOf`；舊父少掉它；`Remove<ChildOf>` observer 讀到舊值 |
| `[kr-linked]` | 刪父 | 子、孫一起刪 |
| `[kr-linked-deep]` | 深度 20（> 預設上限 8） | 全刪、沒有 `HookRoundLimit` |
| `[kr-linked-order]` | 父 P（編號 50），子 A（10）、B（30），A 的子 A1（40）、B 的子 B1（20）；另一筆明寫刪 E（25） | OnRemove 的先後：E 與 P 依編號（E 先），P 之後 A → A1 → B → B1〔Q4 建議〕 |
| `[kr-linked-cycle]` | A、B 互為父子後刪 A | 兩個都刪、結束 |
| `[kr-detach-then-despawn]` | 同一個 System：拿掉武器的 `ChildOf`、刪角色 | 武器活著、沒有 `ChildOf`〔Q3〕 |
| `[kr-reparent-then-despawn]` | 同一個 System：武器換到新角色、刪舊角色 | 武器活著、在新角色的 `Children`〔Q3〕 |
| `[kr-despawn-and-attach]` | 同一階段刪 P、生成帶 `ChildOf{P}` 的 C | C 活著、沒有 `ChildOf`、一筆紀錄〔Q3〕 |
| `[kr-hook-queue-phase]` | hook 記的 `ChildOf` 在下一個同步點的第 0 步階段套用 | 那一階段之後一致 |
| `[kr-hook-round]` | 最後一個同步點的 hook 記的 `ChildOf` 在清空輪套用 | 那一輪之後一致 |
| `[kr-immediate]` | `ExclusiveWorld` 當場插入（合法、被拒各一） | 呼叫回來時一致；被拒回 false |
| `[kr-host]` | Host 的 `Insert<ChildOf>`、`Despawn(父)`；存取驗證打開 | 一致、連帶刪除；不跑 hook；下一次 Run 不報 `BoundaryChange` |
| `[kr-host-load-order]` | Host 先放子再放父 | 子的關係不成立、一筆紀錄（說明頁的要求有案例） |
| `[kr-restore-reparent]` | `Restore` 給 `ChildOf` 與 `Children`，放回換父之前 | 兩邊照給的；摘要＝直跑 |
| `[kr-restore-order]` | `Restore` 放回順序變過的 `Children` | 順序照給的 |
| `[kr-restore-revive]` | `Restore` 復活被連帶刪除的子 | 摘要＝直跑 |
| `[kr-restore-no-expand]` | `Restore` 刪父、存活集合留著之後才掛上去的子 | 子活著、沒有 `ChildOf` |
| `[kr-restore-unregistered-childof]` | `RestoreSet` 只給存活集合（沒給 `ChildOf`），刪了父 | 子的 `ChildOf` 被拿掉（DI-KS 12 補句 (a)） |
| `[kr-restore-rejected]` | `RestoreSet` 給的 `ChildOf` 指向不活著的實體 | 不成立、紀錄的路是 `Restore`（補句 (b)） |
| `[kr-restore-maintain]` | `RestoreSet` 給 `ChildOf`、不給 `Children` | `Children` 照 DI-KS 30 維護 |
| `[kr-summary]` | 同一組操作兩個 World | 摘要相同 |
| `[kr-hash]` | `HashValue(ChildOf)`、`HashValue(Children)` 的位元組（`HashTests.cpp` 式） | 和手算的位元組相同（順序不同的兩個 `Children` 雜湊不同） |
| `[kr-load]` | 生成期間照存檔的 `Children` 順序放回（照存檔編號、重新配號各一） | `Children` 的內容與順序和存檔前相同；重新配號改寫 `parent` |
| `[kr-structure-check]` | 存取驗證打開，`Restore` 給了互相矛盾的 `ChildOf` 與 `Children` | `StructureProblem` 報出 DI-KS 30 |
| `[kr-kernel-owned]` | 作者宣告 `InitialValue<ChildOf, …>` | R-E4 |

回滾 Module 的整合案（兩邊都登記，預測換父、預測死亡後回滾，R-2 等價）：加在 `tests/common/rollback/`，在 KR 的實作 PR 裡（主會話 2026-11-23 排定）。

### 12.2 負向編譯樣本（每個入口一個、各自的標記；照 #286 的共用判法）

`Write<ChildOf>`、`Write<Children>`、`World::GetMut<ChildOf>`、`World::GetMut<Children>`、`ExclusiveWorld::Modify<ChildOf>`、`ExclusiveWorld::Modify<Children>`、`Commands::Insert<Children>`、`Commands::Remove<Children>`、`CommandsFor<Inserts<Children>, Removes<>>`、`CommandsFor<Inserts<>, Removes<Children>>`、`Spawn<Bundle<Children>>`、`SpawnWithId<Bundle<Children>>`、`ExclusiveWorld` 的當場插入、拿掉、生成帶 `Children`、`World::Insert<Children>`、`World::Remove<Children>`、`Registrar::InitialValue<Children, …>`。第 5.2 節的表與這份清單在實作第 1 步逐條對齊，dev_log 寫對照。

### 12.3 突變（AGENTS §14.5；放 `docs/architecture/plans/kernel/scripts/mutations.json`，前綴 `kr-`；D184：每步只跑相關的與新加的）

| id | 改什麼 | 預期失敗 | 它獨有的誤 | 守的 |
|---|---|---|---|---|
| `kr-no-add` | 5b 不加進目標側 | `[kr-spawn]` | 新關係沒進 `Children` | DI-KS 30 |
| `kr-validate-at-spawn` | 在 spawn 當下檢查目標 | `[kr-same-phase-small-id]` | 小編號的子被拒 | DI-KS 30 |
| `kr-move-keeps-old` | 覆寫不從舊父拿掉 | `[kr-reparent]` | 換父留在舊父 | DI-KS 30 |
| `kr-empty-kept` | 變空的 `Children` 不拿掉 | `[kr-detach]` | 空格留著 | DI-KS 30 |
| `kr-erase-no-drop` | 銷毀子時不維護 | `[kr-child-despawn]` | 刪子留在父 | DI-KS 30 |
| `kr-order-by-id` | 5b 依子的編號插入 | `[kr-order]` | 順序不是附掛先後 | DI-KS 30〔(A′)〕 |
| `kr-after-initial` | 5b 移到 5a 之後 | `[kr-initial-from-children]` | 從 `Children` 推的初始值不觸發 | DI-KS 30 |
| `kr-after-onadd` | 5b 移到 OnAdd 之後 | `[kr-observer-add]` | observer 看到不一致 | DI-KS 30 |
| `kr-drop-before-onremove` | 先從父拿掉再跑子的 OnRemove | `[kr-observer-remove]` | OnRemove 看到拿掉後 | DI-KS 30 |
| `kr-no-net-diff` | 不看淨差 | `[kr-net-diff]` | 通知依編號而變 | DI-KS 30 |
| `kr-children-stamp-always` | 目標側每次都打 stamp | `[kr-children-changed]` | 同值再插入讓 `Children` 變 | DI-KS 30 |
| `kr-dead-kept` | 不查目標活不活著 | `[kr-dead-target]` | 指向死的實體 | DI-KS 31 |
| `kr-self-kept` | 不查指向自己 | `[kr-self]` | 自己是自己的父 | DI-KS 31 |
| `kr-spawn-unchecked` | 5b 不檢查 spawn 寫進的 | `[kr-spawn-dead-target]` | 生成路漏查 | DI-KS 31 |
| `kr-reparent-dead-keeps-old` | 覆寫不合法時保留舊關係 | `[kr-reparent-dead]` | 結果和 Bevy 不同 | DI-KS 31 |
| `kr-linked-off` | 不展開連帶刪除 | `[kr-linked]` | 子留下 | DI-KS 32 |
| `kr-linked-one-level` | 只展開一層 | `[kr-linked]`（孫） | 孫留下 | DI-KS 32 |
| `kr-linked-breadth` | 廣度優先 | `[kr-linked-order]` | 先後不同 | DI-KS 32〔Q4〕 |
| `kr-linked-by-id` | 連帶的子依編號混進明寫的序列 | `[kr-linked-order]`（A 的編號 10 比 P 小、E 夾在中間） | 先後不同 | DI-KS 32〔Q4〕 |
| `kr-detach-ignored` | 展開時不看本階段的拆開紀錄 | `[kr-detach-then-despawn]` | 掉落的武器被刪 | DI-KS 32〔Q3〕 |
| `kr-reparent-ignored` | 展開時不看本階段的覆寫 | `[kr-reparent-then-despawn]` | 換父的武器被刪 | DI-KS 32〔Q3〕 |
| `kr-restore-expands` | `Restore` 也展開 | `[kr-restore-no-expand]` | 活著的子被刪 | DI-KS 32、12 |
| `kr-restore-maintains-given` | `Restore` 給了 `Children` 仍維護 | `[kr-restore-order]` | 順序被重算 | DI-KS 33、12 |
| `kr-restore-no-maintain` | `Restore` 沒給 `Children` 時不維護 | `[kr-restore-maintain]` | 兩邊不一致 | DI-KS 33、12 |
| `kr-restore-keeps-dangling` | `Restore` 不拿掉指向被刪父的 `ChildOf` | `[kr-restore-unregistered-childof]` | 懸空 | DI-KS 12 補句 |
| `kr-host-no-maintain` | Host 路不維護 | `[kr-host]` | 直接放破一致 | DI-KS 30 |
| `kr-host-no-mark` | Host 路不標寫入 | `[kr-host]`（`BoundaryChange`） | 驗證誤報 | DI-KS 30 |
| `kr-hash-children-empty` | `Children` 的 `HashValue` 不寫內容 | `[kr-hash]` | 摘要看不到順序 | DI-KS 33 |
| `kr-hash-children-unordered` | 雜湊前排序 | `[kr-hash]` | 摘要看不到順序 | DI-KS 33 |
| `kr-structure-unchecked` | `StructureProblem` 不查 | `[kr-structure-check]` | 開發版漏報 | DI-KS 30 |

不立的：`kr-linked-no-visited`（step 2 已有「不活著就跳過」，`World.cpp:697-698`，有環時重新排進來的實體已死，突變是等價的；修訂 0 列過，撤回）。`[kr-linked-deep]`、`[kr-hook-queue-phase]`、`[kr-hook-round]`、`[kr-immediate]`、`[kr-restore-revive]`、`[kr-load]`、`[kr-order-two-systems]` 守組合；抓到的誤和上表重疊時不另立（§14.5），實作時逐條確認，寫進 dev_log。

### 12.4 受け入れ判定

- `kernel.*` 全部通過；新案例全部通過；上表突變全部 killed（預期失敗）；負向樣本全部以各自的標記失敗。
- 相關的既有突變（find 在 `World.cpp` 的套用步驟、`Registry.cpp` 的 Kernel 登記、`Params.hpp` 的入口、引用 golden 的 `mutations.json:1833`）重跑，killed 數不少於第 0 步的基準。
- L1 三平台通過（draft PR 手動 dispatch L1）。
- 判定不看計時；沒有計測。

### 12.5 判定的變更（AGENTS §14.4；Design Invariants 的守衛，要你同意）

Kernel 自己多登記兩個元件，下列判定的期待值要改（第 0 步跑出新值，第 1 步改）：

| 判定 | 位置 |
|---|---|
| J-18 Registry 指紋 golden（三平台相同） | `RegistryTests.cpp:41` |
| J-18 的 ComponentId（0、1、2 → 2、3、4） | `RegistryTests.cpp:51-53` |
| 同檔的 ComponentId 與元件數 | `RegistryTests.cpp:72-74`、`:90-91`、`:148`、`:150` |
| Registry 指紋（Plan 的 golden 不受影響） | `SchedulerTests.cpp:138`、`:140`、`:142` |
| J-54 | `HookFlushTests.cpp:935` |
| 初始值的寫死值 | `InitialValueTests.cpp:656` |
| J-32 摘要 golden（三平台相同） | `WorldTests.cpp:519` |
| DI-KS 4 的元件數 | `RegistrationRoundTests.cpp:254`、`:264`、`:696` |

1. **守的性質**：Registry 指紋、World 摘要的值只取決於登記與宣告、三個平台相同；id 依登記順序（DI-KS 3、4、27）。
2. **緩的面**：沒有。新的期待值一樣是一個固定值，任何其他改動（型別大小、順序、平台差）照舊讓判定失敗。
3. **締的面**：沒有。
4. 性質由同一批判定繼續守（同一個案例、同一個三平台 L1）。
5. **根據與證據**：新值是 Kernel 多兩個欄的必然結果；第 0 步在三平台 L1 取得新值、與本機相同，寫進 dev_log；§14.4 的驗證者（Opus／high，另一個 Agent）用實際輸出確認 1～5。

改值之後，引用 `kJ18RegistryGolden` 的突變（`mutations.json:1833`）重跑。

---

## 13. 協調（主會話 2026-11-23 排定）

| 對象 | 什麼 | 決定 |
|---|---|---|
| 網路 PR-2 卡（`local_a19de0ec`） | KR 要改回滾頁 R-2 前提的文字、在 `tests/common/rollback/` 加整合案；不改回滾 Module 的程式 | 留在 KR 的實作 PR；不擋 KR 的宣告與其他實作；兩個 PR 誰後合併誰把 gyo2 merge 進自己的分支，兩邊內容都保留，不 rebase |
| 網路複製那批 | K8：父比子晚到由網路自己等（lightyear `PendingChildOf`）；digest 裡 `ChildOf` 是原始編號 | 主會話轉給網路卡、記成網路複製那批的待辦 |
| SDL 宣告卡（`local_2cc7e494`） | K1～K9 的回答（第 6 節）；X2 等 KR；Q1 的結果影響 T、U；RC 號 26、27 | 經主會話 |
| R1a、R1b | 都改 `World.cpp`、`Executor.cpp` | 誰後合併誰跟上 gyo2（merge，不 rebase） |
| 主會話 | 核准後照四樣記 HANDOFF；`LogWorldLabel` 另開小批 | 已同意 |

---

## 14. 停止條件（AGENTS §14 冒頭）

- 維護需要改 DI-KS 11 或 18 → 停下回報（RC 26）。
- 既有 `kernel.*` 案例因 KR 失敗、原因不在第 12.5 節的清單 → 停下回報（回歸）。
- 範圍超出：要改回滾 Module、編排層、權威的程式 → 停下回報。
- 第 12.5 節的新值在三平台不一致 → 停下回報（`Children` 的表示沒做到各平台相同）。

---

## 15. 檢查與核准

- 修訂 0（4b5340e2）→ 敵對式檢查（Opus／high，同模型）→ 修訂 1（本稿）。
- 交主會話轉使用者做動工前核准：Q1～Q4 與第 10.1 節末「一起同意的」。核准前不動 `engine/`、`tests/`。

---

## 16. 修訂 0 的檢查怎麼處理

| 條 | 處理 | 改在 |
|---|---|---|
| B1 先拆再刪、刪父又掛子 | 採用：新 Q3（建議本階段明寫的關係紀錄優先）；三個案例 | 第 5.6 節、10.1、12 |
| B2 5b 在 5a 之後 | 採用：5b 移到 insert 之後、5a 之前；`Children` 可當初始值的來源；兩個案例 | 第 5.2、5.3 節、DI-KS 30 |
| B3 決勝理由自相矛盾 | 採用：三個選項各要改的條文列進表；決勝理由改成 T、U 的順序與讀檔不需旗標；建議改 (A′) | 第 5.4、9 節 |
| B4 Restore 的兩件 | 採用：DI-KS 12 補句寫明 (a)(b)、交你同意；兩個案例 | 第 5.8、7.2 節 |
| B5 golden 是守衛 | 採用：清單與 1～5 寫進宣告、交你同意 | 第 12.5 節 |
| S1 OnRemove 的三個例外 | 採用：覆寫在 insert 寫之前檢查（OnRemove 看到原本的值）；例外改成四種逐條寫 | 第 5.3、5.5 節 |
| S2 依編號而變的通知 | 採用：淨差 | 第 5.3 節、`[kr-net-diff]` |
| S3 深度／廣度、P8 | 採用：新 Q4，建議深度優先前序（Bevy） | 第 5.6 節 |
| S4 C-10 那條路 | 採用：先放父的要求與案例；C-10 改寫交你同意；Host 路標寫入 | 第 5.7、7.4 節 |
| S5 Bevy 事實 | 採用：目標側在 Bevy 是 `Mutable`；同值再插入的差（C15）；flecs 系譜改正 | 第 4.1、4.2、6 節 |
| S6 少了選項、膠水算少 | 採用：(A′) 成為建議；flecs 式寫明不列的理由 | 第 5.4 節 |
| S7 更小的變更（不存 `Children`） | 採用：列成 (D) 與第 5.3 節 (iv)，寫退掉的理由；第 9 節「唯一的路」拿掉 | 第 5.3、5.4、9 節 |
| S8 突變與案例 | 採用：雜湊改單元案；`kr-linked-no-visited` 撤回；`kr-linked-by-id` 的案例改成子的編號比父小；缺的案例補上；負向樣本逐入口列 | 第 12 節 |
| S9 規格 F.4 | 採用：跨實體的讀用 observer 驗；F.4 不改 | 第 12.1 節、P11 |
| S10 `Children` 的表示、當場插入的回傳 | 採用：固定寬度的表示；被拒回 false | 第 5.1、5.5 節 |
| S11 明細欄位 | 採用：Host、`Restore` 的值寫明 | 第 5.5 節 |
| N1 讀的版本 | 改成 b5ef9f87，註明 `engine/` 相同 | 文首 |
| N2 KR 在 X2 之前的出處 | 改成 SDL 宣告第 10.1 節 | 文首 |
| N3 `dynamic_world.rs:192-200` | 拿掉 | 第 4.1 節 |
| N4 `Registry.cpp:649` | 改 | 第 5.1 節 |
| N5 版本要不要升 | 不升，理由寫明 | 第 5.1 節 |
| N6 突變放哪 | 寫明 | 第 12.3 節 |
| N7 虛擬函式的實例化 | (A′) 下 `Column<Children>` 允許，這一句不再需要 | 第 5.8 節 |
| N8 檢查者的模型 | 寫明 Opus 同模型 | 文首、第 15 節 |
| N9 SDL K9 的兩問 | `StartupCode` 不加；J-32 golden 列進第 12.5 節 | 第 5.1 節、12.5 |
| N10 R-2 補句的條件、不用 `Derived` | 寫明 | 第 5.8 節 |
| N11 預算的延後 | 寫明是對 §2 的延後 | 第 5.14 節 |
| N12 `StructureProblem` 加查 | 採用（開發版） | 第 10.2 節、`[kr-structure-check]` |
