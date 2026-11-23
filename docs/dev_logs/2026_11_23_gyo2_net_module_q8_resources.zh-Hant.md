# 2026-11-23 網路 Module：Resource 的複製與狀態機的 `RequestSilent`（Q8 (b)；第 2 步的補完）（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（gyo2 併入 f69a6385、宣告第 19 節補結果 24929bb7 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md) 第 9 節 Q8（含「(b) 的生效時點」）、第 17 節（使用者 2026-11-22「照建議，全部核准」：Q8 (b)）。狀態機的 silent 規則照修訂 7（分支 `claude/gyo2-state-machine-520d19` 的宣告第 5.1 節「兩種要求」、第 7 節判定 1 的 silent 各列、第 8.2 節 DI 6）。
- **實作者**：Opus／high。

## 1. 做了什麼

### 網路（`engine/modules/net`）

- `NetEntry::ReplicateResource<R, V = R>(名字, ResourceOptions<R, V>{codec, read, apply})`（`Replication.hpp`）。
  - `V` 是上 wire 的值。`V`＝`R` 時 `read`、`apply` 可以不給（預設 apply＝`SetResource<R>`：用 codec 把現值編碼、和收到的位元組比，相同就不寫，CCR-8）。
  - `V`≠`R` 時兩個都要給：狀態機的值型別封閉，Client 造不出 `StateMachine<S>`，所以上 wire 的是 `S`，伺服器用 `read`（`Current()`）讀、Client 用 `apply`（狀態機的套用函式）寫。
  - `apply` 的型別：`Kernel::SystemResult(Kernel::ExclusiveWorld<>&, const V&)`。
- **送**：`ReplicateOut` 每 tick 讀每個 Resource 的變更戳記（`ExclusiveWorld::ResourceStamp<R>()`）與編碼後的值，所有 Peer 共用；帳本 `PeerLedger::resources` 記每個 Resource 確認過的 World 戳記，比它新或還沒確認過就放進第 0 則值訊息。
- **wire**：每則值訊息在實體之前多一段 `u8` 個數＋（`u16` wire id、`u32` 長度、值）。只有第 0 則有內容，其他則是個數 0（每則多 1 byte）。協議雜湊加 `r:名字:版本`，版本字串升到 `gyo.net.registrations/3`。
- **確認**：和實體同一條（`PendingPart::resources`）。
- **收**：`ApplyValues` 先讀 Resource 段：比寫過的 tick 舊就 `StaleValue`；否則呼叫登記的 `apply`，Err 往上傳（World 停）。
- **計數**：`PeerLedger::resourceValuesSent`；`NetReplicaState::resourceValuesApplied`、`resourceValuesUnchanged`、`resourcesWritten`。

### 狀態機（`engine/modules/state_machine`）

- `StateMachineRequest<S>`：`RequestSilent(值)`、`PendingKind()`、種類欄（`RequestKind::Normal`／`Silent`）。後寫蓋前寫時值與種類一起蓋，計數照算；雜湊收種類；取走時種類回到 `Normal`。
- 套用的 System（`RegisterStateMachine.hpp` 的 `Apply`）：取走前先讀每種狀態的種類。
  - 父 silent、換到別的值：存在的子不跑離開、記成不存在 → 改父的值 → 新父值底下的子設成初始值。都不跑進入與離開。
  - 父 silent、換成同一個值：什麼都不寫，子不動。
  - 子 silent：子存在且值不同才改值，不跑離開與進入；子不存在就丟掉。
  - 驗值（SM-E1）、取走的時機、計數和 `Request` 相同。
- `AdoptValue.hpp`：`AdoptByRequest<S>(world, 值)`、`AdoptByRequestSilent<S>(world, 值)`。只用 Kernel 的型別。狀態已經要去那個值時什麼都不寫（CCR-8），否則寫 `Request`／`RequestSilent`。「已經要去」看有待處理的要求時它的值與種類，沒有時看現在的值。

### 文件

- [`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md)：第 12 節（Resource 的複製）、NT-1、NT-3、NT-10。
- [`state-machine.zh-Hant.md`](../architecture/target/v2/state-machine.zh-Hant.md)：第 1 節（不互相依賴）、第 2 節的表、DI 6（新）、DI 7 的括號拿掉、第 3 節 (c) 區、第 4 節限界。
- 兩份的 Design Invariants 改動都標「實作，合併前核准定稿」。

## 2. 和宣告、卡片不同的地方

- **`ReplicateResource` 多一個型別參數 `V` 與 `read`**：卡片寫 `ReplicateResource<T>(name, {codec, apply})`。狀態機的 `StateMachine<S>` 型別封閉（狀態機 DI 1），Client 的 codec 造不出它，所以上 wire 的值要能和 Resource 不同。`V` 預設＝`R`，計分這種 Resource 的寫法和卡片相同。
- **帳本記戳記，不記 tick**：卡片寫「帳本記 Resource 上次確認的 tick」。實體的帳本記的是 World 戳記（`confirmedStamp`），送不送是比 Resource 的變更戳記，同一個尺度才比得了，所以照實體記戳記。
- **協議的版本字串 `/2` → `/3`**：值訊息的格式變了（每則多 1 byte），沒登記 Resource 的兩端也要擋新舊混連。
- **G-8 的位元組變了**：每則值訊息多 1 byte（Resource 個數）。G-8 的判定（兩次執行逐 tick 相同）照舊過；數字見第 4 節。

## 3. 自己判的細節（揭露）

- **名字**：`ReplicateResource`、`ResourceOptions`、`ResourceEntry`、`RequestSilent`、`RequestKind`、`PendingKind`、`AdoptByRequest`、`AdoptByRequestSilent`（動詞片語，和 `Request`／`RequestSilent` 同一組）。
- **Resource 放在第 0 則、算進那一則的大小**：一個 tick 的 Resource 一起確認。
- **作者的 `apply` 回 Err 時 World 停**（和元件的 `Failed` 同）。
- **`AdoptBy…` 的「已經要去」看待處理的要求**：同一幀收到兩筆相同的值（重送）時，第二筆不再寫要求（不加被蓋掉的計數）。
- **網路測試連 `GYO::StateMachine`**：`statesync:` 的案例像產品一樣把兩者接起來。`gyo_net` 本身不連狀態機；狀態機的連結集守衛（DI 8：只有 Kernel 與編排層）照舊，所以狀態機也不連網路。
- **修訂 7 與修訂 8 的 silent 規則**：對照過，沒有衝突。
  - 子的存在：修訂 7 的「子照新的父值靜靜地出現成初始值或消失」和修訂 8 的 DI 5 相同。
  - `keepOnParentReenter`：修訂 7 的「只管一般的 `Request`」照做（silent 同值時子本來就不動）。
  - 第一次套用不取走任何要求：修訂 8 的 DI 4，silent 也一樣。

## 4. 驗證

- 本機（macOS x86_64，Intel i9-9980HK，`core` preset＝RelWithDebInfo）：
  - 全部的 ctest 185 個全過（gyo2 併入後 183 個＋`net.resource`、`net.statesync`）；
  - `gyo_net_tests` 66 案全過；
  - `gyo_state_machine_tests` 37 案全過（silent 與套用函式新加 7 案）。
- **Resource 複製**（`net.resource`）：
  - G-5 的方法：計分 `Score`，伺服器每 3 tick 改，K1 Client 用預設 apply。有損的 Link（2 成遺失、1 成重複、25% 延後）。結果：觀測 168 個 tick（391 個中）、不一致 0；排空之後相等（99 幀），計分 66／67；Resource 送 332、套用 251、其中值相同沒寫 121；確認後 10 幀沒有再送。
  - 同值重送：確認 8 成遺失的 Link；計分在 tick 120、160 各改一次，跑 240 幀。Resource 送 44、套用 44、值相同沒寫 42；第一次送在 tick 80（改值之前）；Client 的 `Changed()` 為真 3 次＝準備放的初始值＋兩次真的改值。
  - 協議雜湊：一端登記複製、另一端沒有 → `ProtocolMismatch`。
- **狀態機**（`net.statesync`，`Sky`：Clear／Fog，伺服器在 tick 30 `Request(Fog)`）：
  - `AdoptByRequest`：Client 跑了 `Exit.Clear`、`Enter.Fog`。
  - `AdoptByRequestSilent`：Client 只有初始的 `Enter.Clear`，值換成 Fog，`Changed()` 為真一次。
  - 同值重送（確認 8 成遺失）：兩種都沒有多餘的離開與進入；`Changed()` 只在換值那一幀為真；被蓋掉的計數 0。
  - **生效時點**：值換成 Fog 的那一幀，`PreUpdate` 的 System 看到 Clear 與待處理的 Fog（`First` 收到、寫了要求），`Update` 的 System 看到 Fog；前一幀沒有待處理的要求。兩種都一樣。
- **狀態機的行為**（`state_machine.behaviour`，修訂 7 判定 1 的 silent 各列）：
  - 不跑離開與進入的要求；
  - silent 讓子消失；
  - silent 換成同一個值（子不動、狀態的戳記不變）；
  - 子的 silent（存在、同值不寫、不存在丟掉）；
  - `Request` 被 `RequestSilent` 蓋掉（計數 1、照後寫的種類、套用後回到一般）；
  - 不在對照表的值用 `RequestSilent`（SM-E1）；
  - `AdoptByRequest`／`AdoptByRequestSilent` 經要求生效、同值不寫。
- **G-2～G-9**：照舊通過。G-8 的數字：兩次執行逐 tick 相同（判定成立）；合計 44 078 bytes（第 3c 步 43 950，多 128＝每則值訊息多 1 byte），每 tick 最大 626、平均 339.1。G-5 (1) 觀測 164 個 tick、不一致 0（第 3c 步 168：位元組變了，有損 Link 的抽樣跟著變）；G-6、G-9 的數字和第 3c 步相同。G-12 的量測程式不在 ctest，這次沒跑。

## 5. 突變

- **新加 10 個**（都寫了 `guards`；各自只捕自己那一個決定，`tests/common/mutation/README.md` 的 1、3、9）：
  - 狀態機 6：`sm-silent-runs-transitions`、`sm-silent-same-resets-children`、`sm-silent-same-writes`、`sm-silent-child-runs-transitions`、`sm-kind-not-overwritten`、`sm-adopt-writes-same`。
  - 網路 4：`nt-resource-sent-again`、`nt-resource-default-writes-same`、`nt-resource-hash-missing`、`nt-resource-apply-not-called`。
- **find 跟著改**：`sm-same-value-skipped` 的 find 前面多了 `} else`（silent 的分支放在前面），replace 照舊，守的性質不變。其他既有的 find 都沒有變。
- **跑的範圍**（D184）：狀態機全清單（改了它的兩個標頭）、網路清單裡 file 是 `Replication.cpp`／`Replication.hpp` 的全部。
- **結果**：
  - 狀態機 70／70 killed。
  - 網路第一次 24／25：`nt-resource-default-writes-same` 是 wrong_failure。測試的前提「同一個計分又到了」原本用 `resourceValuesUnchanged > 0` 判，突變拿掉比較之後這個計數也變 0，失敗停在前提，不是要的那一條。前提改成「套用的次數比計分的 3 個值多」（和結果無關的量），`resourceValuesUnchanged > 0` 改成一般的 CHECK。判定沒有放寬。重跑 killed，網路 25／25。
- **沒加的**：Resource 的舊值丟掉（`StaleValue`）沒有另外加突變，和實體的值共用同一個規則（`nt-stale-value-applied` 捕實體那一行，Resource 這一行是同一個規則的另一份）。這是細節，等觀測到漏掉時再加（README 的「細部」）。

## 6. 證據

主 checkout `build/evidence/gyo2-net-module/q8/`（git 不追蹤）：
- `mutations-state-machine.log`、`mutations-net.log`；
- `ctest-all.log`、`net-messages.txt`（網路測試印出的數字，含 G-8 的數列）。

## 之後要接的

- 第 4 步：K3 經回滾的入口收 Resource 的權威值（rollback 限界「Resource 本批只從歷史還原」要改）。
- 合併前核准：net 第 12 節、狀態機 DI 6 的條文定稿。
