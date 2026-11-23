# 回滾 Module（`engine/modules/rollback`）

更新：2026-11-23（`v8-arch-gyo2-net-module` 第 3 步前半：`Input<T>` 也收元件，R-6 多一種登記；網路宣告第 5 節，使用者已核准；**實作，最終文字在合併前核准定稿**）；2026-11-22（`v8-arch-gyo2-batch4-followups`：R-3 本文改成「不依賴」的寫法（使用者核准）與常設守衛）；2026-11-21（新頁，`v8-arch-gyo2-authority-rollback` 第 4 步）。Owner：架構。程式：`engine/modules/rollback/`（`GYO::Engine::Rollback`，target `GYO::Rollback`，登記單位 `RollbackModule`）。行為測試：`tests/common/rollback/`（ctest `rollback.rollback_registrations`、`rollback.rollback_strategy`、`rollback.rollback_checks`，標籤 `cpu`）；突變清單 [`scripts/rollback_mutations.json`](scripts/rollback_mutations.json)（`rb-`）。

由來：[回滾份](rollback-module-declaration.zh-Hant.md)（修訂 6，已核准）第 5、9 節；[第 4 批宣告](authority-rollback-declaration.zh-Hant.md)第 5、6 節與第 11.3 節（方向使用者 2026-11-20 核准：「DI 文字方向照建議」）。Design Invariants 的最終文字使用者 2026-11-22 核准（[合併前核准](authority-rollback-merge-approval.zh-Hant.md)）；和回滾份字面不同的地方在條末標「實作」。

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
| `StepSchedule(name, recordStage)`、`StepSchedule()` | 回滾的單位；有策略的 World 在登記當下把記錄 System 掛到那個 stage。預設跟著時間 Module（回滾份 2.3）：不帶參數＝`WorldTimeOptions::fixedSchedule`，`recordStage` 預設 `Last`。選了官方策略卻沒有步的 schedule 是 RB-E10（什麼都不記、權威值永遠不採用） |
| `NestedSchedule(name, stepSchedule)` | 從步的 schedule 巢狀跑的 schedule；重跑時會跑到，只進檢查的範圍 |
| `SkipDuringResimulation(options)` | 在 `AddSystem` 之前給它的每個掛載加執行條件（讀時間 Module 的 `Resimulation`）。「重跑時跑不跑」逐個掛載看條件（`RunsWhileResimulating`）：之後才加的掛載會重跑 |
| `Input<T>(length, kind)` | 官方策略替 `T` 保留按 tick 的紀錄，重跑每一步前寫回（不是回滾狀態）；不登記 `T`。`kind`：`DataKind::Resource`（預設）或 `DataKind::Component`——元件的輸入每個實體按 tick 一份（玩家實體上的輸入元件，網路宣告 D-11；照 lightyear 的 `InputBuffer`），重跑每一步前逐格寫回，那一步沒有這個元件的實體不寫；保留與放回（`Keep`／`PutBack`）也逐格〔實作，合併前核准定稿〕。長度用檔位 `InputHistoryLength`（`StrategyDepth`＝策略的深度、預設；`Short` 20、`Medium` 32、`Long` 64）或直接寫步數，不攔。檔位的系譜：`Short` 是預算 P1、P3 的深度與 lightyear 的預設回滾深度，`Medium` 是 P2 的深度，`Long` 是時間 Module 預設 64 Hz 的一秒。容量小於深度照舊是 RB-E9；預設長度不會小於深度。伺服器權威的判定輸入（`authority.Resource<T>({.input = true})`、`authority.Component<T>({.input = true})`）由權威代為登記（預設長度），作者不必寫這行（2026-11-22 使用者選 (c)）；只有選了回滾策略的 World 才建紀錄，沒選的零成本（R-4） |

同一型別同時登記成可回滾與 `Derived`：作者的蓋過代為的（跨兩種也一樣）；兩筆都是作者的是 RB-E1。

`RollbackOptions::frameSchedule`（決定、還原、重跑與修正掛的 frame schedule）空字串＝時間 Module 的 `WorldTimeOptions::frameSchedule`（沒有時間的選項時是預設骨架的）。

重跑的每一步外面有一段 trace（經 Base 的門面，category `rollback`、name `resimulate`；回滾份 10.4）：祖先裡有 `span.rollback` 的段是重跑，第一次跑沒有這一層。不依賴 trace Module，不讀寫 World 的值。

`RollbackInbox`（所有策略共用、World 的 Resource）：`Value<T>(entity, tick, value)`、`Removed<T>(entity, tick)`、`ConfirmedThrough(tick)`；〔實作，合併前核准定稿；網路 PR-2 閘口 1 的 G〕`Entered(entity, tick)`、`Exited(entity, tick)`、`ResourceValue<T>(tick, value)` 實作；`NotSent` 網路第 5 步；`PartialValue`、`FromEvent` 照舊回 `RollbackFailure`（NotImplemented；網路宣告第 13 節：不用）。

## Design Invariants

<a id="r-1"></a>
**R-1 契約不變**：`RollbackStrategy` 的掛點與語意是契約——`Name`、`Check`（啟動）與執行時的 `Record`、`Decide`、`Restore`、`Resimulate`、`Correct`，每個掛點只收一個自己的情境物件（之後要給更多東西時加在情境物件上、不改簽名；實作）；另有一個有預設實作的唯讀掛點 `DefaultInputLength()`（輸入紀錄的預設長度：官方策略回它的深度，預設 0；第 8 步隨使用者選 (c) 加）；官方實作的內部不保證不變。守衛：測試裡一個不用官方實作、從頭實作介面的策略，通過同一個等價案（`rollback strategy: R-1`）。

<a id="r-2"></a>
**R-2 回滾等價**：從狀態 s 直接跑 k 步，與「跑到現在、退回第 T 步、重跑回現在」，可回滾的欄、Resource、實體存活集合逐位元相同（stamp 除外），前提：
1. 步的 schedule 與巢狀 schedule 上的 System 只讀：可回滾的資料、Derived 的資料、輸入歷史、時間 Module 的 tick 與固定步時間。
2. 那些 System 不用 `Local`。
3. 那些 System 不讀沒退回的時間（可縮放的時間、時鐘）。
4. `Message` 在同一步內寫完讀完；有副作用的 observer 讀「重跑中」。
5. 可回滾的資料在兩個步之間，不被步的 schedule 與巢狀 schedule 以外的 System 寫（還原與權威值的入口除外）。
6. 步的 schedule 上有生成時，配發器登記成可回滾。
7. 輸入的入口登記成「重跑時不跑」。

Kernel 的 `Restore` 照給的內容放回、不補 `InitialValue`（DI-KS 12；第 8 步審查 A3 發現它原本會補、等價破掉，使用者選 (a2) 從源頭修），所以「請求在、目標當時不在」也照樣等價，不必當成前提。守衛：`rollback strategy: R-2: a restore puts back a request…`、`…an entity a predicted death took comes back…`。

前提是作者的責任，回滾 Module 不加檢查；只有輸入歷史的長度由官方策略在啟動時查（容量小於深度是 RB-E9；實作）。守衛：等價測試（`rollback strategy: R-2…`，兩案：只有寫值、有生成刪除與副作用；`rollbacks every frame at the depth's edge…`：每幀在深度邊界回滾一次、連續 18 幀，讀前幾次重跑改寫過的歷史；`R-2: a message written and read within the step leaves the summary of running straight`：前提 4 的訊息，重跑再寫一次，比的時候已經修剪掉，摘要和直跑相同（還有讀者拉著重跑寫的紀錄時，R-2 成立、摘要仍不同：kernel DI-KS 10，使用者選 (b) 時連帶接受）〔`v8-arch-gyo2-kernel-summary-messages`，摘要版本 4〕；比 World 摘要）；突變：漏退 tick、漏退存活集合、漏退 Resource（配發器）、「重跑中」沒立、「重跑中」在還原之後才立、重跑完沒放下、輸入沒寫回、重跑後輸入沒放回這一幀的值、只記新出現的格、不記被拿掉的格、重跑時不丟被取代的歷史（欄與存活集合各一）、修剪連窗口起點也丟、永不修剪、深度邊界差一。

<a id="r-3"></a>
**R-3 不認得權威、網路、狀態機**：回滾 Module 不依賴它們：不連結，也不用它們的型別與名字。守衛：「不連結」由 `gyo_rollback` 的連結集（`tests/common/rollback/CMakeLists.txt` 的 `gyo_expect_links`，整個專案 configure 完才查，別的目錄後來加的連結也看得到）。它釘的是整個集合，比本條嚴：加的依賴不屬權威、網路、狀態機時，改登記列與那一行，本條不變。權威另有登記檔的順序（authority 排在 rollback 之後）與 O6（`engine/` 裡手寫連結登記的 target）。「不用它們的型別與名字」沒有機械守衛，交每批的設計審查（`v8-arch-gyo2-include-bypass-to-review`）：不經連結的 include（相對路徑、另加標頭目錄）、在回滾裡抄一份它們的型別或欄位表、用名字字串去找它們的東西，都是違反。突變：`rb-r3-links-more`、`rb-r3-authority-from-later-directory`。

<a id="r-4"></a>
**R-4 沒選就沒有**：產品不選回滾 Module 時，Kernel 與編排層裡沒有回滾的狀態或分支；World 沒選回滾策略時，沒有歷史（型別也不登記）、不掛記錄 System。守衛：`rollback strategy: R-4`。

<a id="r-5"></a>
**R-5 歷史不跨世代**：歷史在 World 的 Resource 裡；換 World 之後新 World 沒有舊歷史。守衛：`rollback strategy: R-5`。

<a id="r-6"></a>
**R-6 公開的登記可讀、穩定**：七種登記（〔實作，合併前核准定稿〕元件的輸入紀錄是第七種：`InputRecord` 多一欄 `kind`，Resource 照舊；R-1 的掛點不改）經 `RollbackRegistry` 讀得到，啟動檢查之後不變。`Build` 時追加、帶來源（作者或代為）；凍結後由擁有者對紀錄的純函式 `Resolve` 裁決：作者的蓋過代為的、不論先後，同一來源的兩筆 options 不同是 RB-E1。裁決結果在準備函式裡放進那個 World 的 Resource `RollbackRegistry`（實作：宣告 5.1）。守衛：`rollback registrations:` 各案；元件的輸入：`rollback strategy: R-2 with a component input…`（`[rb-component-input-r2]`、`[rb-component-input-kind]`）、`…a component input's entry that runs before the rollback…`（`[rb-component-input-put-back]`），突變 `rb-component-input-not-written-back`、`rb-component-input-not-put-back`、`rb-component-input-as-resource`。

<a id="r-7"></a>
**R-7 Derived 的順序條件**（取代 Kernel 的 D-E16）：步的 schedule 與巢狀 schedule 裡，讀 Derived 元件的每個 System 之前，唯讀登記表的順序圖上有同一張 schedule 的立即寫者（階段、宣告、同步點的邊；登記先後的平手不算）；違反是 RB-E7。hook 與 observer 讀 Derived、以及獨占 System（`ExclusiveWorld`）的讀寫，不在啟動時看得到的範圍：作者負責（用獨占 System 重算 Derived 的作者，讀者要宣告在它之後的順序，或改用一般 System；實作）。守衛：`rollback checks: R-7…` 四案；突變：不檢查、把延後寫算成立即寫、不看先後、每張 schedule 都查、只看直接邊、登記先後的平手算順序、漏掉巢狀 schedule。

<a id="r-8"></a>
**R-8 入口形狀**（宣告 5.5；使用者 2026-11-20 重新核准）：
- 比對與還原以（實體、元件、tick）為單位；入口帶這樣的權威值與「第 C 步以前收齊」的標記。
- 官方策略的「判斷」用全域的確認點 C（MC-5 的 (a)）：C 前進時在 C 比，不同就回滾到 C；作者覆寫「判斷」可以做別的規則（只回滾收到新值的實體、回到最早的不一致），不另加掛點。
- 晚到的權威值不靜默丟（MC-1）：照 lightyear，記進確認歷史，在 C 用「C 當時或之前最後的值」比。超過最大深度時放棄這次回滾，發 `RollbackAbandoned`（宣告 10.3：E3 用 World 的訊息告知，形狀照 `TimeDiscarded`）。
  - 這條的前提（實作，第 4 步審查 1-1；〔實作，合併前核准定稿；網路 PR-2 閘口 1 的 B〕）：寫入的一方在說「第 C 步以前收齊」之前，把每個實體**在 C 當時的值**寫進入口（從它上一筆寫入以來有變的才要寫，`Removed` 同）；沒寫的就當成「從上一筆以後沒變」（lightyear 的增量複製也是這個前提）。**確認點只落在寫入的一方送了值的 tick 上**：中間沒送的 tick 不單獨確認，由下一個 C 一起涵蓋。只送一次、之後不再送的實體，在每個 C 都會拿最後那筆比。
  - 比對與「權威值蓋在歷史上」只看 C 當時存活的實體（實作，審查 1-2）。〔實作，合併前核准定稿；網路 PR-2 閘口 1 的 A〕**呼叫者明說了進入、退出的實體照它算**：進入的 tick ≤ C、而且沒在 C 以前退出，就算存活。還原到 C 時，這樣的實體照入口的值放回，不在的就生成（用同一個編號）；在 C 以前退出的刪掉。沒明說的實體照遊戲的生成與刪除走：指向這個 World 沒有的實體的值不當回滾的理由。明說的存活和記下的存活集合不同，本身就是回滾的理由（實作）；還原時 C 的存活集合換成明說之後的（實作）。
  - 入口在 C 前進而不回滾時（一致、放棄、模式 Check 或 Disabled）也修剪，每個實體留「C 當時或之前最後的值」（實作，審查 1-3）。
- 權威值有三態：有值、確定刪除、這份沒帶；實體的有效期間（進入、退出）由呼叫者明說，沒明說的一律當遊戲的生成與刪除（MC-3、SL-1、SL-2）。〔實作，合併前核准定稿；網路 PR-2 閘口 1 的 C〕明說的進入、退出（`Entered`、`Exited`）第 4 步實作；「這份沒帶」（`NotSent`）第 5 步實作。
- 事件來源的權威值可寫進歷史，由作者選（MC-2）；本批回 NotImplemented。
- 「確認點不得晚於還沒解開的差分」是給網路 Module 的需求（MC-4），不在本頁。

守衛：`rollback strategy:` 的權威值、C 之前的值、`Removed`、一致不回滾、Disabled、入口修剪、不存活的實體、C 超前時等待、深度各案；`rollback checks: the entries not made…`。〔實作，合併前核准定稿〕A：`rollback strategy: R-8 A:` 四案（進入的放回並在之後的回滾留著、C 之後才進入的不在、退出的在 C 刪掉、修剪留 C 當時或之前最後一筆）；F：`rollback strategy: R-8 F:`（不同就回滾、一致不回滾、C 之前的值與修剪）。突變：只認同一個 tick、入口不修剪、比對不看存活、超前不等、Disabled 無效；`rb-entered-not-restored`、`rb-lifecycle-not-a-difference`、`rb-entered-at-any-tick`、`rb-lifecycle-prune-drops-c`、`rb-resource-authority-ignored`、`rb-resource-not-adopted`。

**跨 schedule 的訊息**（回滾份 7.1，`time-plan-two` (5) 兩個方向）：官方策略在啟動時查——步的 schedule 與巢狀 schedule 裡寫的 `Message` 不能有外面的讀者，讀的不能有外面的寫者（RB-E8）；只看 System 的掛載，hook 與 observer 由作者負責。守衛：`rollback checks: a message…`、`messages: the nested schedules…`；突變：兩個方向各一、巢狀 schedule 不算在內、hook 的寫算進來。

### Replacement Criteria

- 職責清單要多加一項，或判不出某樣東西屬回滾還是別層。
- Kernel 為了回滾要多加原語 1～4 以外的原語；P2P 實作時發現原語不夠。
- **回滾等價破裂**（由 Kernel 的 RC 11 移來）：R-2 的等價測試在 L1 平台因回滾 Module 以外的原因（Math、編譯器）失敗——停下，交使用者選「R-2 的『逐位元相同』改寫」或「官方實作支援的範圍縮小」，不在回滾 Module 裡繞過。
- **記錄要並行**（宣告 11.3、15.4）：記錄是獨占 System；記錄要改成一般 System 才能並行時（獨占 System 成為並行的牆），重看記錄的形狀。

## 3. 本批的限界

- 修正（`Correct`）的官方實作本批什麼都不做：比較函式以外的修正函式還沒有登記入口，畫面修正等 Client 那一批（實作）。
- 存活集合整份放回（回滾份 5.5，7.4 說要寫進限制）：步外生成、只帶本機元件的實體，回滾到它出生之前會被刪掉且不會再生；步外刪掉的實體會以沒有元件的空殼回來。〔實作，合併前核准定稿；閘口 1 的 D〕網路寫進來的生成與刪除走 `Entered`、`Exited`（R-8 的明說），回滾不會刪掉它們。**本機**的步外生成與刪除照舊整份放回。
- 只重跑第一張步的 schedule；步的 schedule 和時間 Module 的固定步 schedule 不同也不檢查（實作；目前只有一張）。
- 記錄 System 掛在 `recordStage`，同一個 stage 裡比它晚跑的寫者、或那個 stage 的延後命令，會記到下一步：`recordStage` 要選步裡最後的 stage（預設 `Last`）。
- Plan 的版本在一個 World 裡不變，換 Plan 就是換 World（R-5 的新歷史）；回滾份 7.5「Plan 版本變了歷史作廢」由此滿足，不另檢查。
- C 早於這個 World 的第一筆記錄（剛換 World）時，也當「超過深度」放棄並發 `RollbackAbandoned`。
- 〔實作，合併前核准定稿；閘口 1 的 F〕入口收 Resource 的權威值：`ResourceValue<T>(tick, value)`。比對、「蓋在歷史上」、晚到、修剪的規則同元件，只是沒有實體。
- 〔實作，合併前核准定稿；閘口 1 的 B〕確認點只落在送了值的 tick 上：作者覆寫「判斷」、想找「最早的不一致」時，看不到中間 tick 的權威值。
- 「這份沒帶」不參加比對（不推成「沒變」）的規則已寫進條文，本批的入口回 NotImplemented。
