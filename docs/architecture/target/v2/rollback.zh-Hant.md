# 回滾 Module（`engine/modules/rollback`）

更新：2026-11-21（新頁，`v8-arch-gyo2-authority-rollback` 第 4 步）。Owner：架構。程式：`engine/modules/rollback/`（`GYO::Engine::Rollback`，target `GYO::Rollback`，登記單位 `RollbackModule`）。行為測試：`tests/common/rollback/`（ctest `rollback.rollback_registrations`、`rollback.rollback_strategy`、`rollback.rollback_checks`，標籤 `cpu`）；突變清單 [`scripts/rollback_mutations.json`](scripts/rollback_mutations.json)（`rb-`）。

由來：[回滾份](rollback-module-declaration.zh-Hant.md)（修訂 6，已核准）第 5、9 節；[第 4 批宣告](authority-rollback-declaration.zh-Hant.md)第 5、6 節與第 11.3 節（方向使用者 2026-11-20 核准：「DI 文字方向照建議」）。**Design Invariants 的最終文字在合併前核准時定稿**；和回滾份字面不同的地方在條末標「實作」。

## 一個例子

PvP 射擊的 Client：玩家按前進，Client 不等伺服器，當場預測第 100 步角色在 (10, 0)。伺服器的權威值晚 6 步才到：網路那一側把「第 100 步 `Health` 是 7」寫進 `RollbackInbox`，再說「第 100 步以前都收齊了」。下一幀開頭，官方策略在第 100 步逐實體逐元件比對權威值和自己記下的歷史，不同就把 World 放回第 100 步（權威值蓋在歷史上）、退回 `Tick`、把第 101～106 步的輸入一步一步寫回、重跑六次固定步，最後放下「重跑中」。單機的時間倒流只用回滾、不用權威；格鬥遊戲的 P2P 目前是佔位（啟動時報錯）。Kernel 與編排層都不知道「回滾」「重跑中」。

## 1. 位置與依賴

```text
權威 Module ──→ 回滾 Module ──→ 時間 Module
                    │
                    ├──→ 編排層（登記單位、ModuleBuffer）──→ Kernel
                    └──→ Kernel（原語 1～4、唯讀登記表、ExclusiveWorld）
```

- 登記檔 `config/modules.csv` 的 `rollback` 列（`world_time` 之後、`authority` 之前），`default=yes`：每個 World 預設帶，沒選策略就沒有作用（宣告 4.1、5.1）。
- 標頭：`Registrations.hpp`（公開登記、裁決、凍結進 World 的 `RollbackRegistry`）；`RollbackStrategy.hpp`（策略介面、情境物件、`RollbackOptions`、P2P 佔位）；`StateRollbackStrategy.hpp`（官方策略）；`History.hpp`（官方策略的歷史）；`Inbox.hpp`（權威值的入口）；`Runtime.hpp`（Module 的 System 與「放棄回滾」的訊息）；`RollbackModule.hpp`。

## 2. 公開登記與入口

| 登記（`RollbackEntry`） | 意思 |
|---|---|
| `Component<T>(options)`、`Resource<T>(options)` | 可回滾（比較函式預設 `==`；沒有 `==` 的型別要給比較函式）。型別由別的入口登記進 Kernel |
| `Derived<T>()` | 每步在讀者之前由立即寫者重算；不記錄、不還原（R-7） |
| `StepSchedule(name, recordStage)` | 回滾的單位；有策略的 World 在登記當下把記錄 System 掛到那個 stage |
| `NestedSchedule(name, stepSchedule)` | 從步的 schedule 巢狀跑的 schedule；重跑時會跑到，只進檢查的範圍 |
| `SkipDuringResimulation(options)` | 在 `AddSystem` 之前給它的每個掛載加執行條件（讀時間 Module 的 `Resimulation`） |
| `Input<T>(capacity)` | 官方策略替 `T` 保留按 tick 的歷史；不登記 `T` |

`RollbackInbox`（所有策略共用、World 的 Resource）：`Value<T>(entity, tick, value)`、`Removed<T>(entity, tick)`、`ConfirmedThrough(tick)`；`PartialValue`、`NotSent`、`Entered`、`Exited`、`FromEvent` 本批回 `RollbackFailure`（NotImplemented）。

## Design Invariants

<a id="r-1"></a>
**R-1 契約不變**：`RollbackStrategy` 的掛點與語意是契約——`Name`、`Check`（啟動）與執行時的 `Record`、`Decide`、`Restore`、`Resimulate`、`Correct`，每個掛點只收一個自己的情境物件（之後要給更多東西時加在情境物件上、不改簽名；實作）；官方實作的內部不保證不變。守衛：測試裡一個不用官方實作、從頭實作介面的策略，通過同一個等價案（`rollback strategy: R-1`）。

<a id="r-2"></a>
**R-2 回滾等價**：從狀態 s 直接跑 k 步，與「跑到現在、退回第 T 步、重跑回現在」，可回滾的欄、Resource、實體存活集合逐位元相同（stamp 除外），前提：
1. 步的 schedule 與巢狀 schedule 上的 System 只讀：可回滾的資料、Derived 的資料、輸入歷史、時間 Module 的 tick 與固定步時間。
2. 那些 System 不用 `Local`。
3. 那些 System 不讀沒退回的時間（可縮放的時間、時鐘）。
4. `Message` 在同一步內寫完讀完；有副作用的 observer 讀「重跑中」。
5. 可回滾的資料在兩個步之間，不被步的 schedule 與巢狀 schedule 以外的 System 寫（還原與權威值的入口除外）。
6. 步的 schedule 上有生成時，配發器登記成可回滾。
7. 輸入的入口登記成「重跑時不跑」。

前提是作者的責任，回滾 Module 不加檢查。守衛：等價測試（`rollback strategy: R-2…`，兩案：只有寫值、有生成刪除與副作用；比 World 摘要）；突變：漏退 tick、漏退存活集合、「重跑中」沒立、「重跑中」在還原之後才立、重跑完沒放下、輸入沒寫回、只記新出現的格。

<a id="r-3"></a>
**R-3 不認得權威、網路、狀態機**：回滾 Module 不 include 它們。守衛：判定 4 的一次性方向掃描；常設的依賴掃描延到第 5 批（宣告 9.5）。

<a id="r-4"></a>
**R-4 沒選就沒有**：產品不選回滾 Module 時，Kernel 與編排層裡沒有回滾的狀態或分支；World 沒選回滾策略時，沒有歷史（型別也不登記）、不掛記錄 System。守衛：`rollback strategy: R-4`。

<a id="r-5"></a>
**R-5 歷史不跨世代**：歷史在 World 的 Resource 裡；換 World 之後新 World 沒有舊歷史。守衛：`rollback strategy: R-5`。

<a id="r-6"></a>
**R-6 公開的登記可讀、穩定**：六種登記經 `RollbackRegistry` 讀得到，啟動檢查之後不變。`Build` 時追加、帶來源（作者或代為）；凍結後由擁有者對紀錄的純函式 `Resolve` 裁決：作者的蓋過代為的、不論先後，同一來源的兩筆 options 不同是 RB-E1。裁決結果在準備函式裡放進那個 World 的 Resource `RollbackRegistry`（實作：宣告 5.1）。守衛：`rollback registrations:` 各案。

<a id="r-7"></a>
**R-7 Derived 的順序條件**（取代 Kernel 的 D-E16）：步的 schedule 與巢狀 schedule 裡，讀 Derived 元件的每個 System 之前，唯讀登記表的順序圖上有同一張 schedule 的立即寫者（階段、宣告、同步點的邊；登記先後的平手不算）；違反是 RB-E7。hook 與 observer 讀 Derived 不在啟動時看得到的範圍，作者負責。守衛：`rollback checks: R-7…` 兩案；突變：不檢查、把延後寫算成立即寫、不看先後、每張 schedule 都查。

<a id="r-8"></a>
**R-8 入口形狀**（宣告 5.5；使用者 2026-11-20 重新核准）：
- 比對與還原以（實體、元件、tick）為單位；入口帶這樣的權威值與「第 C 步以前收齊」的標記。
- 官方策略的「判斷」用全域的確認點 C（MC-5 的 (a)）：C 前進時在 C 比，不同就回滾到 C；作者覆寫「判斷」可以做別的規則（只回滾收到新值的實體、回到最早的不一致），不另加掛點。
- 晚到的權威值不靜默丟（MC-1）：照 lightyear，記進確認歷史，在 C 用「C 當時或之前最後的值」比。超過最大深度時放棄這次回滾，發 `RollbackAbandoned`（宣告 10.3：E3 用 World 的訊息告知，形狀照 `TimeDiscarded`）。
- 權威值有三態：有值、確定刪除、這份沒帶；實體的有效期間（進入、退出）由呼叫者明說，沒明說的一律當遊戲的生成與刪除（MC-3、SL-1、SL-2）。本批「這份沒帶」與明說的進入、退出回 NotImplemented。
- 事件來源的權威值可寫進歷史，由作者選（MC-2）；本批回 NotImplemented。
- 「確認點不得晚於還沒解開的差分」是給網路 Module 的需求（MC-4），不在本頁。

守衛：`rollback strategy:` 的權威值、一致不回滾、深度各案；`rollback checks: the four entries…`。

**跨 schedule 的訊息**（回滾份 7.1，`time-plan-two` (5) 兩個方向）：官方策略在啟動時查——步的 schedule 與巢狀 schedule 裡寫的 `Message` 不能有外面的讀者，讀的不能有外面的寫者（RB-E8）；只看 System 的掛載，hook 與 observer 由作者負責。

### Replacement Criteria

- 職責清單要多加一項，或判不出某樣東西屬回滾還是別層。
- Kernel 為了回滾要多加原語 1～4 以外的原語；P2P 實作時發現原語不夠。
- **回滾等價破裂**（由 Kernel 的 RC 11 移來）：R-2 的等價測試在 L1 平台因回滾 Module 以外的原因（Math、編譯器）失敗——停下，交使用者選「R-2 的『逐位元相同』改寫」或「官方實作支援的範圍縮小」，不在回滾 Module 裡繞過。
- **記錄要並行**（宣告 11.3、15.4）：記錄是獨占 System；記錄要改成一般 System 才能並行時（獨占 System 成為並行的牆），重看記錄的形狀。

## 3. 本批的限界

- 修正（`Correct`）的官方實作本批什麼都不做：比較函式以外的修正函式還沒有登記入口，畫面修正等 Client 那一批（實作）。
- Resource 本批只從歷史還原，不收權威值（`RollbackInbox` 只有元件）。
- 「這份沒帶」不參加比對（不推成「沒變」）的規則已寫進條文，本批的入口回 NotImplemented。
