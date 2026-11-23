# 姿態的父子傳播（SDL 線子批 X2，`v8-arch-gyo2-sdl-transform-hierarchy`）

更新：2026-11-23（新頁）。Owner：架構（GYO2 主會話開的任務卡「開X2、Object_Connect 移植準備的卡」）。一頁短說明：設計取捨、和別家的對照、後果。條文在 [transform](transform.zh-Hant.md)（TF-1、TF-3、TF-5，RC-TF1）。

讀的版本：`origin/gyo2` 7be54ccb（KR #295、X1 #298、R1a #299 都已合併）；Bevy 6a911653f；Unity Entities（`~/Code/GitHome/com.unity.entities`）。

## 1. 先用遊戲講

Object_Connect：一個節點下面掛著它的標籤與連接點，是節點的子。拖節點只改節點的 `Transform`；同一幀的 `PostUpdate`，標籤與連接點的 `GlobalTransform` 都跟上（父的 `GlobalTransform` × 子的 `Transform`）。把連接點從一個節點拿下來掛到另一個（插入新的 `ChildOf`），它跟著新的父；拿掉 `ChildOf`，它就變成根，`GlobalTransform` 等於自己的 `Transform`。

PvP：武器是角色的子。回滾讓預測中被打死的角色復活時，角色與武器一起放回（`Restore` 給了兩者的 `ChildOf`）；放回的那一刻武器的 `GlobalTransform` 是局部矩陣，下一次傳播就在角色手上。判定不讀 `GlobalTransform`（CCR-12）。

## 2. 做了什麼

`transform` Module 的同步 System 換成一個 `PropagateTransforms`（取代 X1 的 `SyncTransforms`，同一個 set、同一個預設的 stage）：

- **根**（帶 `Transform`、`GlobalTransform`、沒有 `ChildOf`）：`Transform` 被寫過、`GlobalTransform` 剛加上、或上一次跑之後失去 `ChildOf`（孤兒）時重寫成 `ToMatrix(transform)`。
- **根以下**：照 `Children` 走（深度優先、前序、子照 `Children` 的順序；用自己的堆疊，不遞迴）。父被重寫過、或自己的 `Transform`／`ChildOf` 被寫過、或 `GlobalTransform` 剛加上時，重寫成 `Multiply(父的 GlobalTransform, ToMatrix(transform))`。
- **孤兒**：System 的 `Local` 留著上一次跑時帶 `ChildOf` 的實體（升冪），這次的差集就是孤兒。

## 3. 和別家的對照

| | Bevy（主參考） | Unity Entities | GYO（本批） |
|---|---|---|---|
| 沒有父子的實體 | `sync_simple_transforms`：`Or<(Changed<Transform>, Added<GlobalTransform>)>`、`Without<ChildOf>`、`Without<Children>`，加孤兒（`bevy_transform/src/systems.rs:42-79`） | 世界空間的 job，chunk 的 `DidChange` 當早退（`LocalToWorldSystem.cs:46-47`、`:165-171`） | 同一個 System 的根那一段 |
| 有子的根與傳播 | `propagate_parent_transforms`：根是 `With<Children>`、`Without<ChildOf>`；`changed = transform.is_changed() \|\| global_transform.is_added() \|\| 孤兒`，往下傳 `changed \|\| child_of.is_changed()`（`systems.rs:334-391`、`:352`、`:381`）；子 `changed \|= transform.is_changed() \|\| global_transform.is_added()`，`parent.mul_transform(transform)`（`:406-480`、`:449-451`） | 根以下遞迴，`updateChildrenTransform` 往下傳；子的 `LocalTransform` 變了就算；`math.mul(parentLocalToWorld, localTransform.ToMatrix())`（`LocalToWorldSystem.cs:82-128`）；根看 `Child` 或 `LocalToWorld` 的 `DidChange`（`:132-143`） | 同 Bevy 的條件與乘法 |
| 孤兒 | `RemovedComponents<ChildOf>`（`systems.rs:52`、`:72-78`、`:345-350`） | `PreviousParent` 元件（`ParentSystem.cs:21-23`、`:236-244`、`:274-289`） | System 的 `Local`：上一次帶 `ChildOf` 的實體 |
| 靜態樹的略過 | `mark_dirty_trees` ＋ `TransformTreeChanged`（`systems.rs:111-151`；只有多執行緒版的傳播用它，`:532`、`:715-717`） | 沒有（chunk 早退） | 不做（RC-TF1） |
| 環與不一致 | 子的 `ChildOf` 不指回父就 panic（`systems.rs:358-361`、`:458-461`） | — | 不檢查（下面） |
| 排程 | `PostStartup`、`PostUpdate`，`mark_dirty_trees → propagate → sync_simple` 串起來（`plugins.rs:27-46`） | `TransformSystemGroup`，`ParentSystem` 之後 | `Main`／`PostUpdate`，set `TransformPropagate`（X1 不變） |

### 3.1 偏離與理由

1. **兩個 System 併成一個**：Bevy 分兩個是為了多執行緒（`par_iter_mut`）；兩個都要孤兒，GYO 的 Kernel 沒有每個讀者各自游標的「拿掉的元件」紀錄，分開就要兩份孤兒狀態。併成一個只掃一次根。看得到的差別：只有一個 System 名（set 不變，讀者排在 set 之後照舊）。
2. **孤兒用 `Local`（上一次帶 `ChildOf` 的實體），不用 Kernel 的紀錄或 hook**：
   - 加 Kernel 的「拿掉的元件」紀錄（Bevy 的 `RemovedComponents`）＝動 Kernel，一個 Module 的需要不夠（Kernel RC 26 的「換父的通知」同類）。
   - 掛 `ChildOf` 的 OnRemove hook 補一個標記：Host 的 `World::Remove<ChildOf>` 不跑 hook（KR 5.7），會漏；而且多一個寫者。
   - `Local` 比的是實際狀態，同步點、當場改、`Restore`、Host 的直接放、父被刪（子的 `ChildOf` 被 Kernel 拿掉）都抓得到。它記的是「上一次傳播看到的」，和 `GlobalTransform` 是同一個視角（`GlobalTransform` 不回滾），所以 `Restore` 之後仍然對。誤報（例：同一個編號被別的實體重用）只會多重算一次，值仍然對。Unity 的 `PreviousParent` 是同一個想法，只是存成元件。
3. **不查環、不查不一致**：Kernel 在每一次套用之後維護 `ChildOf` 與 `Children` 兩邊一致（DI-KS 30），Bevy 的 assert 防的是不一致。唯一的例外是 `Restore` 照給的放回 `Children`、不維護（DI-KS 33）：作者給的 `Children` 和 `ChildOf` 不合時（違反回滾 R-2 的前提「成對登記」），一個子可能被兩個父列到（後走到的根勝，仍然確定），根可走到的 `Children` 成環時傳播不會結束（Bevy 在這裡 panic）。這是前提被破壞，照 AGENTS §3 不加檢查；開發版的存取驗證在邊界報不一致（DI-KS 33）。環（Kernel 不查環，KR 5.5）裡的實體都有 `ChildOf`，不是根；一致的前提下，根的子樹也走不進環（進環的那個實體的 `ChildOf` 只有一個、指著環裡的前一個）。所以環裡的實體永遠不被走到，`GlobalTransform` 留著舊值，走訪一定結束——和 Bevy 單執行緒版一樣（Bevy 的測試 `panic_when_hierarchy_cycle` 是用另一個 World 造出不一致）。不加檢查（AGENTS §3）。
4. **`GlobalTransform` 剛加上（`Added`）照 Bevy 算進去**：GYO 的 `GlobalTransform` 跟著 `Transform` 出現（初始值、hook），那時 `Transform` 也被寫過；`Added` 多抓的是作者自己放 `GlobalTransform` 的情形，便宜，照 Bevy。

## 4. 後果（寫進 transform 說明頁）

- **有父的實體，`GlobalTransform` 出現的那一刻是局部矩陣**（初始值與 hook 只看 `Transform`；Bevy 是單位矩陣），同一幀的 `PostUpdate` 之後才對。沒有父的照 TF-3，當下就對。
- **父沒有 `Transform`（或 `GlobalTransform`）**：走訪停在那裡，它的子樹不更新、留舊值（同 Bevy：`transform_query` 拿不到就 return）。
- **`Changed<GlobalTransform>`** ＝「自己或某個祖先的 `Transform` 或 `ChildOf` 被寫過、`GlobalTransform` 剛加上，或成為孤兒」（重寫的旗標往下傳，同 Bevy `systems.rs:381`、`:473`；同值也算，CCR-8）。
- **看的是已套用的關係**：同一幀 `PostUpdate` 之前記下、還沒套用的 `ChildOf` 紀錄，到它套用之後的那一次傳播才算。
- **`Children` 的順序不影響值**，只定走訪的先後。
- **回滾**：`GlobalTransform` 不登記回滾（TF-2）；父子要回滾時作者成對登記 `ChildOf` 與 `Children`（R-2 前提，KR 5.8）。`Restore` 寫了 `Transform`／`ChildOf` 就打 stamp，下一次傳播跟上；`Restore` 拿掉 `ChildOf` 的由孤兒抓。World 摘要不變（`GlobalTransform` 雜湊空；`Local` 不在摘要裡）。
- **非等比縮放**：父的非等比縮放配上子的旋轉會出現剪切（矩陣照樣相乘，同 Bevy `GlobalTransform`）。

## 5. 量測與預算（RC-TF1）

每次傳播的成本：所有根各看一次（X1 的 `SyncTransforms` 本來就掃全部帶 `Transform` 的實體），加上所有在某個根子樹裡的實體各走一次（沒變的只讀不寫），加上帶 `ChildOf` 的實體的差集。深的階層不會用完堆疊（`H-7`：兩萬層的鏈）。

本批**不量測**：幀時間預算由 R2b 宣告，還沒有，沒有可比的數字。R2b 定了預算之後，「深的父子階層或大量實體讓傳播超過預算」照 RC-TF1 停下交使用者；候選是 Bevy 的 `mark_dirty_trees` ＋ `TransformTreeChanged`（每個實體多一個元件，換掉沒變的子樹整棵略過）。

## 6. 測試與突變

ctest `transform.module`（`tests/common/transform/TransformTests.cpp`，「H-」開頭）：

| 案例 | 看什麼 | 標記 |
|---|---|---|
| H-1 | 三層的鏈：子＝父 × 局部；`Update` 生成的子同一幀就在父下面 | `[tf-h-root]`、`[tf-h-child]`、`[tf-h-grandchild]`、`[tf-h-spawned-child]` |
| H-2 | 移動根，整棵子樹跟上；沒寫的幀不寫任何 `GlobalTransform`；只移動子，子與它的子樹跟上、父不寫 | `[tf-h-follow-parent]`、`[tf-h-changed]`、`[tf-h-own-transform]`、`[tf-h-own-transform-count]` |
| H-3 | 換父 | `[tf-h-reparent]` |
| H-4 | 孤兒（System 的紀錄、Host 的 `World::Remove`），孤兒的子跟上 | `[tf-h-orphan]` |
| H-5 | 父沒有 `Transform` 時子樹不動；環不卡住 | `[tf-h-no-transform-parent]`、`[tf-h-cycle]` |
| H-6 | `Restore` 放回被連帶刪除的整棵樹；`Restore` 沒放回父、子留下（Kernel 拿掉子的 `ChildOf`，子的 `Transform` 沒被寫）時子成為根 | `[tf-h-restore]`、`[tf-h-restore-orphan]` |
| H-7 | 兩萬層的鏈 | `[tf-h-deep]` |

突變（`scripts/transform_mutations.json`，`tf-h-`）見該檔；X1 的 `tf-sync-*` 改指 `PropagateTransforms`。

## 7. 不在這一批

- 從 `Transform` 與 `ChildOf` 算全域姿態的輔助（Bevy `TransformHelper::compute_global_transform`，`bevy_transform/src/helper.rs`）：CCR-12 要判定自己算，目前沒有判定讀姿態；有點名的判定需要時再加。
- 靜態樹的略過：RC-TF1。
- 內插要的上一步姿態：X1 第 9 節已回報主會話。
