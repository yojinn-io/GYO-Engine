# 時間 Module（`engine/world_time`）

更新：2026-11-20（新頁，`v8-arch-gyo2-time-events` 第 6 步）。Owner：架構。程式：`engine/world_time/`（`GYO::Engine::WorldTime`，target `GYO::WorldTime`，登記單位 `WorldTimeModule`）。行為測試：`tests/common/world_time/`（ctest `world_time.types`、`world_time.module`、`world_time.sampling`、`world_time.fixed`、`world_time.restore`，標籤 `cpu`）；突變清單 [`scripts/world_time_mutations.json`](scripts/world_time_mutations.json)（`wt-`）。

由來：[事前宣告](time-events-declaration.zh-Hant.md)（`v8-arch-gyo2-time-events`，2026-11-19 動工前核准，`v8-arch-gyo2-time-events-declaration-approved`）第 3 節、第 10.3、10.4 節；遊戲時間全部放這裡、`engine/time` 只留時間基準（使用者 2026-11-19，`v8-arch-gyo2-game-time-in-world-time`）。本頁照第 2～5 步的實作寫；**Design Invariants 的最終文字在合併前核准時定稿**。和宣告文字不同的地方在每條末尾標「實作」。

## 一個例子

2D 橫版的單機遊戲，畫面每幀、模擬固定步 64 Hz。每幀開頭 `AdvanceTime` 取一次樣，算出這一幀的真實時間與可縮放的時間（暫停時 0、慢動作時乘 0.5）；卡住 1 秒時固定步最多補 5 步，多的整步從可縮放的時間扣掉、累計進 `VirtualTime.discarded` 並發一筆 `TimeDiscarded`。`RunFixedSteps` 把這一幀的量加進累加器，夠幾步就巢狀跑幾次 `FixedUpdate`；每跑一步 `Tick` 加 1，固定步裡的 System 讀 `ActiveTime` 得到（步長、固定步的累計），跑完換回每幀的值。PvP 的 Match 不登記 `TimeControl`，沒人能暫停；Client 的回滾策略（第 4 批）用 `RewindFixedSteps` 把 `Tick` 與固定步的累計一起退回，再用 `RunOneFixedStep` 重跑。Kernel 與編排層都不知道「每幀」「固定步」「暫停」「tick」是什麼。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Time ◄── GYO::WorldTime ──► GYO::Orchestration ──► GYO::Kernel
```

- `engine/time` 不動：只有時間基準（`MonotonicClock`、`Waiter`、`LateWakeStats`），只連 Base＋Threads（[`time.md`](../../time.md) DI 10）。三家（Bevy、flecs、Unity）都把「現在幾點、睡到幾點」放在 OS 介面那一層、把遊戲時間另成一處並和 World 綁在一起。
- 登記檔 `engine/config/modules.csv` 的 `world_time` 列：依賴 `base;kernel;orchestration;time`，`library_only=no`、`default=yes`（每個 World 預設帶、產品可關，O-6）。第一個真的進 World 的 Engine Module。
- 標頭：`TimeTypes.hpp`（純型別與算術，只 include `<cstdint>`）；`FixedStep.hpp`（跑一步、退回、放回；用 Kernel 的 `ExclusiveWorld`，不讀時間基準）；`Sampling.hpp`（**唯一** include `MonotonicClock.hpp` 的標頭，zone_scan ZS1：判定的 System 要能讀 `Tick` 而不碰時鐘）；`WorldTimeModule.hpp`。實作：宣告的 `TickAccumulator.hpp` 併進 `TimeTypes.hpp`。
- 程式裡的 `Engine::` 一律全限定（`::Engine::Base::`、`::Engine::Time::`）；`gyo_orchestration_include_order` 涵蓋 `world_time` 的公開標頭與編排層、Kernel 標頭的各種順序。

## 2. Resource、System、函式

| Resource | 欄位（整數奈秒） | 寫者 |
|---|---|---|
| `RealTime` | `now`、`delta`、`elapsed`、`sampled` | `AdvanceTime` |
| `VirtualTime` | `delta`、`elapsed`、`discarded`（累計扣掉的）、`scale`（這一幀套用的）、`fraction`（倍率乘完的零頭）、`paused` | `AdvanceTime` |
| `FixedTime` | `accumulator`（步長＋餘數）、`elapsed`（已跑的固定步累計）、`pending`（`AdvanceTime` 交給 `RunFixedSteps` 的這一幀的量） | `AdvanceTime`（`pending`）、`RunFixedSteps`（取走 `pending`、`Add`）、`RunOneFixedStep`／`RewindFixedSteps`／`RestoreFixedTime`（`elapsed`） |
| `ActiveTime` | `delta`、`elapsed` | `AdvanceTime`（每幀的值）、`RunOneFixedStep`（固定步的值，跑完換回） |
| `Tick` | `value`（`uint64`）：已經開始的固定步數，第 n 步執行中與跑完之後都是 n | `RunOneFixedStep`、`RewindFixedSteps`；回滾策略；讀檔放回 |
| `TimeControl` | 想要的 `paused`、`scale` | 產品。`pausable=false` 時不登記 |
| `TimeSampling` | `mode`（`TimeBase`／`ManualDelta`／`ManualInstant`）、`value` | 產品、測試 |
| `Resimulation`（「重跑中」） | `active`，初始 0 | 只有回滾策略（第 4 批）；時間 Module 只登記、放初始值 |
| `TimeConfig` | `limits`（一幀的兩個上限）、`fixedSchedule`（固定步那一張的名字，至多 63 位元組） | 時間 Module，建 World 時寫一次（每次準備都覆寫）。實作：宣告沒有這一列——System 沒有捕獲、名字不能用 `std::string`（摘要照原始位元組雜湊），所以設定放進 Resource |

另登記保留式訊息 `TimeDiscarded`（`amount`、`realDelta`、`cause`：`StepLimit`／`FrameLimit`／`NotDriven`）。全部是 (b) 區（第 4 批以前 (c) 區讀不到 `Tick`，宣告 G4）。

| System 與函式 | 做什麼 |
|---|---|
| `AdvanceTime`（一般 System，掛 `Main` 的 `First`） | 依 `TimeSampling` 取一次樣、更新 `RealTime` → 上一幀的 `pending` 若沒被取走，累計進 `discarded`、發 `TimeDiscarded`（`NotDriven`），`VirtualTime.elapsed` 不扣回 → `AdvanceVirtual` 算這一幀的量與扣掉的量 → 寫 `VirtualTime`、`ActiveTime`（每幀的值）、`FixedTime.pending`；扣掉的量大於 0 時發 `TimeDiscarded`。`pausable=false` 時用不讀 `TimeControl` 的另一個簽名（沒登記的型別當參數是 RE4） |
| `RunFixedSteps`（`ExclusiveWorld<Writes<Tick, FixedTime, ActiveTime>>`，掛 `Main` 的 `FixedMainLoop`） | 取走 `pending`、`Add` 進累加器得 k 步（`AdvanceVirtual` 已保證 k ≤ 上限）、呼叫 k 次 `RunOneFixedStep`；作者可以給它 run 條件（`WorldTimeOptions::fixedStepConditions`，跳過的那一幀由下一次 `AdvanceTime` 記成 `NotDriven`）。實作：`fixedStepConditions` 是宣告沒有的選項 |
| `RunOneFixedStep(world, schedule)`（公開樣板，參數是任何 `ExclusiveWorld<…>`） | `Tick`+1、`FixedTime.elapsed`+步長 → `ActiveTime`＝（步長、`FixedTime.elapsed`）→ 巢狀跑固定步那一張 → Ok 時 `ActiveTime` 換回每幀的值；Err 或例外直接往上傳、不換回（World 已中斷，D2 之後 Product 結束） |
| `RewindFixedSteps(world, k)`（公開樣板） | `Tick`−k、`FixedTime.elapsed`−k×步長；累加器不動；k 超過 `Tick` 是斷言。之後呼叫 k 次 `RunOneFixedStep` 重跑（回滾宣告 7.2） |
| `RestoreFixedTime(world, elapsed, overstep)`（`World&` 與 `ExclusiveWorld` 兩個多載） | 讀檔放回固定步的狀態：`elapsed` 與累加器的餘數；步長以這個 World 的設定為準 |
| `WorldTimeModule::TickWriters()` | 驅動 `Tick` 的 System 名單＝`{"RunFixedSteps"}`（L27；對時 System 在 Client 那一批加） |
| `WorldTimeModule::Check` | 固定步那一張不存在時回報 `WT-E1`（`Check` 只在 Kernel 沒有啟動錯誤時跑，O-2） |

**掛在哪**：時間 Module 不宣告任何 schedule，只往編排層的預設骨架（O-4）上掛兩格並巢狀跑 `FixedUpdate`。作者關掉骨架、自己宣告時，用 `WorldTimeOptions` 的 `frameSchedule`、`advanceStage`、`fixedStepStage`、`fixedSchedule` 換名字：掛載的目標不存在是 Kernel 的 D-E1，固定步那一張不存在是 `WT-E1`。

**設定**（`WorldTimeOptions`，經 `WorldDescription` 的選項包給；沒給用預設）：`step`（`StepLength::FromHz(double)` 四捨五入成整數奈秒一次，或 `FromNanoseconds`；預設 64 Hz＝15,625,000 ns）、`limits`（`maxStepsPerFrame` 預設 5、`maxFrameDelta` 預設 250 ms）、`pausable`（預設 true）、四個名字、`fixedStepConditions`。

## 3. 取樣（過渡）

時間設計的形狀是「runner 在交換點寫取樣的時間點」，交換點在編排層第 5 條。在那之前照 Bevy `TimeUpdateStrategy`，取樣放在 `TimeSampling`：

- `TimeBase`（預設）：`AdvanceTime` 讀 `::Engine::Time::NowNs()`；建好 World 後第一次取樣 `delta`＝0。
- `ManualInstant`：`value` 是這一幀的時間點；第一次取樣 `delta`＝0。
- `ManualDelta`：每幀前進 `value`，**第一幀就套用**，沿用到被改（Bevy `ManualDuration`）。

三個後果：(1) 多個 World 的產品各自取樣，同一幀不同 World 的 `RealTime.now` 不同——「一次醒來只取樣一次」在交換點做出來之前只對每個 World 成立；(2) Ownership 和時間設計「取樣歸 runner」不同，是過渡，職責清單第 (2) 項註明；(3) 手動模式沿用到被改。`AdvanceTime` 是 (b) 區、不是判定的 System；K.3 第 11 項只禁 (c) 區的 System 自己讀時鐘。

## 4. 和讀檔、回滾、權威的交界

| 要求（出處） | 回答 |
|---|---|
| 時間的 Resource 能被讀檔放回（讀檔 5.5） | **可以存與放回**：`Tick`、`VirtualTime`（`elapsed`、`discarded`、`fraction`；`delta` 下一幀會被蓋）、`FixedTime` 的 `elapsed` 與累加器的餘數（`RestoreFixedTime`）、`TimeControl`。**不放回**：`RealTime`（時間基準的時間點跨行程沒有意義，新 World 從頭開始）、`FixedTime` 的步長（以這次的設定為準）、`Resimulation`（恆 0）、`TimeConfig`（每次準備都照設定覆寫）。準備時已經在的值不覆蓋（O-7；帶過去的情形沒有測試）。存檔結構沒有 padding（`static_assert`） |
| `Tick` 是回滾策略能寫的 Resource、用 `ExclusiveWorld` 退（回滾 7.2 第 1 條） | 公開型別；不擋任何寫者；`RewindFixedSteps` 是建議的退法 |
| 重跑時 System 讀到的固定步時間等於第一次（7.2 第 2 條） | `RewindFixedSteps`＋`RunOneFixedStep`：步長在 World 跑起來之後不變，所以重跑第 T+1 步時 `Tick`、`elapsed`、`ActiveTime` 與第一次相同 |
| 「重跑中」由時間 Module 登記、恆在（5.7、7.2 第 3 條） | `Resimulation`；沒有時間 Module 的 World 沒有它 |
| 公開 tick 型別、驅動者（L27、權威 5.4） | `Tick`；`TickWriters()`；`RunFixedSteps` 帶 `Writes<Tick>` |
| 時間設計 DI 2「各只有一個寫者」 | 改成約定（WT-5），沒有啟動檢查（Kernel 不數寫者、作者自己寫驅動也做得到） |
| tick 的 Z6 例外、「tick 只有一個來源」（統合 16、權威 5.4） | 第 4 批 |
| 訊息要 tick 的自己在內容帶（游標宣告 3.2） | 寫的 System 讀 `Res<Tick>` |

## Design Invariants

守衛欄：行為測試（`world_time.*` 的 ctest 群組、突變 `wt-`）照常是守衛；scenario 第 2 段（`scenario.segment2`，本批第 6 步）通過之後也是。**綁實作方式的掃描延到第 4 批開頭**（`v8-arch-gyo2-guards-after-mechanism-stable`；宣告第 6.3 節「新」那一列：Kernel 常駐掃描加時鐘的識別字、`naming_guard` 加 `engine/world_time`、`world_time` 的 `gyo_expect_links`），這段期間靠本頁與審查守。「實作」標的是照實作而和宣告第 10.3、10.4 節文字不同的地方。

### 時間的型別（宣告第 10.3 節）

<a id="types-1"></a>
**型別 1. 累加器只做整數的算術。** 不讀時間基準、不記 tick、不丟時間；從上一次 `Reset` 起，加進去的總量恆等於步數×步長＋餘數；`Add` 之後餘數不足一步；步數超過呼叫者給的上限是斷言。`Reset` 回傳清掉的量，本批不接線（執行中換步長沒有點名的 Feature）。守衛：`world_time.types`（累加、餘數、不丟時間、上限的斷言、`Reset`）；突變 `wt-add-*`、`wt-until-next-*`、`wt-reset-not-cleared`。

<a id="types-2"></a>
**型別 2. 超過上限的量只在 `AdvanceVirtual` 一處算。** 有固定步時只扣超過 `maxStepsPerFrame` 的整步（扣完的餘數和不扣時相同）；沒有固定步時一幀最多 `maxFrameDelta`；兩個上限不同時作用。暫停時這一幀的量是 0、零頭保留。倍率是唯一的浮點，每幀乘一次，零頭帶到下一幀。頻率換算成整數奈秒一次、四捨五入，不是有限的正數是斷言。實作：時間 Module 的 World 一律有 `FixedTime`，`AdvanceTime` 一律以固定步的方式呼叫，所以 250 ms 的上限只在直接呼叫 `AdvanceVirtual` 而不給累加器時作用。守衛：`world_time.types`（扣整步、步數不超過上限、暫停、倍率、零頭、沒有固定步的 250 ms、步長的設定）；突變 `wt-cut-*`、`wt-elapsed-before-cut`、`wt-frame-cap-*`、`wt-no-frame-cap`、`wt-fraction-*`、`wt-pause-*`、`wt-resume-catches-up`、`wt-scale-*`、`wt-hz-*`、`wt-default-step-not-64hz`、`wt-negative-scale-allowed`。

<a id="types-3"></a>
**型別 3. 純型別與算術的標頭不引用 Kernel、不 include `MonotonicClock.hpp`；型別沒有 padding。** 純型別與算術在 `TimeTypes.hpp`（同一個 Module 裡的函式與 System 才引用 Kernel：`FixedStep.hpp`、`WorldTimeModule.hpp`；讀時間基準只在 `Sampling.hpp`）。每個型別的大小等於逐欄大小的加總（`static_assert`；`has_unique_object_representations_v` 對浮點一律回 false，不用）。浮點的 +0／-0、NaN 在原始位元組雜湊下不同，本批不處理（逐型別雜湊在第 4 批，`-summary-padding-a-then-c`）。實作：宣告的 `TickAccumulator.hpp` 併進 `TimeTypes.hpp`。守衛：`TimeTypes.hpp` 的 `static_assert`（建置）；include 的分法靠審查（掃描延到第 4 批）。

**Replacement Criteria（型別）**：時間 Module 要正確必須知道累加器的內部狀態；或 `ActiveTime` 需要第三種來源。

### 時間 Module（宣告第 10.4 節；WT 是宣告加的代號）

<a id="wt-1"></a>
**WT-1 登記。** 建進一個 World，就登記 `RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`Tick`、`TimeSampling`、`Resimulation`、`TimeConfig`、訊息 `TimeDiscarded`，`pausable` 時加 `TimeControl`，全部 (b) 區；在第一次 Run 之前放初始值，已經在的值不覆蓋（O-7），`TimeConfig` 每次照設定覆寫。`Resimulation` 恆在、初始 0，時間 Module 自己不寫它。兩個 System 掛進設定的名字（預設是編排層的骨架）；掛載的目標不存在是 Kernel 的 D-E1，固定步那一張不存在由 `Check` 回報 `WT-E1`。實作：多 `TimeConfig` 一列；`WT-E1` 是時間 Module 自己的碼。守衛：`world_time.module`「every time Resource is registered and put in …」「pausable = false registers no TimeControl …」「the step length comes from the settings …」「a fixed-step schedule that does not exist is reported by Check (WT-E1)」「a mount target that does not exist is the kernel's D-E1」；突變 `wt-check-silent`、`wt-resimulation-not-put`、`wt-control-always-registered`、`wt-unpausable-reads-control`、`wt-step-setting-ignored`。

<a id="wt-2"></a>
**WT-2 取樣與推進。** 每個 World 每幀只取一次樣（`AdvanceTime`，依 `TimeSampling`；`TimeBase` 與 `ManualInstant` 的第一次取樣 `delta`＝0，`ManualDelta` 第一幀就套用）；`RealTime` 不受暫停、倍率、上限影響；扣掉只在 `AdvanceVirtual` 算（型別 2）；扣掉的量（`StepLimit`／`FrameLimit`）與上一幀沒被 `RunFixedSteps` 取走的量（`NotDriven`）都累計進 `VirtualTime.discarded`，每一種大於 0 時各發一筆 `TimeDiscarded`（一幀最多兩筆）；`NotDriven` 的量只記進 `discarded`，`VirtualTime.elapsed` 不扣回；`discarded` 只由 `AdvanceTime` 寫。實作：`ManualDelta` 第一幀套用、`NotDriven` 不扣回 `elapsed`、一幀可能兩筆。守衛：`world_time.sampling` 各案；`world_time.fixed`「a frame that falls behind …」「a frame whose RunFixedSteps a run condition skips …」；突變 `wt-sample-*`、`wt-real-first-delta`、`wt-manual-first-frame-zero`、`wt-not-driven-*`、`wt-cut-no-message`、`wt-cut-not-recorded`、`wt-discarded-overwritten`、`wt-pending-*`、`wt-control-ignored`、`wt-active-real-time`、`wt-conditions-dropped`。

<a id="wt-3"></a>
**WT-3 tick 與固定步。** World 跑起來之後，`Tick` 與 `FixedTime.elapsed` 只經 `RunOneFixedStep`（前進：跑之前加 1 與一個步長）與 `RewindFixedSteps`（退回 k 與 k 個步長）一起改；讀檔放回（`RestoreFixedTime` 與放回 `Tick`）在新 World 第一次 Run 之前，或由作者的獨占 System 做。固定步執行中 `ActiveTime`＝（步長、`FixedTime.elapsed`），Ok 時換回每幀的值，Err 或例外時不換回、直接往上傳；k 步裡第 j 步出錯，之後的不跑。累加器不因退回而倒退；步長只在建 World 時給，執行中不換（`Reset` 不接線）。`RunFixedSteps` 一幀的步數不超過 `maxStepsPerFrame`。守衛：`world_time.fixed`「k steps a frame, Tick + k …」「RewindFixedSteps then RunOneFixedStep k times …」「the j-th of k steps returns Err …」；`world_time.restore`；突變 `wt-run-one-step-no-tick`、`wt-one-step-a-frame`、`wt-fixed-elapsed-kept`、`wt-active-not-fixed`、`wt-active-not-restored`、`wt-restored-on-err`、`wt-run-not-err`、`wt-rewind-*`、`wt-restore-no-overstep`、`wt-stages-swapped`。

<a id="wt-4"></a>
**WT-4 依賴。** `world_time` 只依賴 `orchestration`、`kernel`、`time`、`base`（登記檔 `modules.csv` 的那一列產生 link）；Kernel、編排層不認得它（不 include、不按它的名字分支）；程式裡的 `Engine::` 一律全限定；只有 `Sampling.hpp` include `MonotonicClock.hpp`。守衛：`cmake.UnitRegistry`（登記檔與產生的 link）；`gyo_orchestration_include_order`（各種順序編得過；突變 `wt-include-order-unqualified`，限 darwin）；Kernel 與編排層的 include 掃描（`kernel_guard`、`orchestration.guard`）擋它們 include 時間 Module。`world_time` 自己的 `gyo_expect_links` 與 `naming_guard` 的目錄對應延到第 4 批開頭。

<a id="wt-5"></a>
**WT-5 寫者的約定。** `RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`TimeConfig` 只由時間 Module 寫；`TimeControl`、`TimeSampling` 由產品（與測試）寫；`Resimulation` 只由回滾策略寫；`Tick` 另外可由回滾策略寫、讀檔時由作者放回——**約定，不是啟動檢查**（Kernel 不數寫者，`-kernel-is-ecs-no-restrictions`）。`TickWriters()` 公開驅動者名單、`RunFixedSteps` 帶 `Writes<Tick>`，供權威策略檢查（第 4 批）。實作：加 `TimeConfig`。守衛：突變 `wt-tick-writers-empty`、`wt-driver-no-writes-tick`（`world_time.module`「TickWriters names RunFixedSteps …」）；其餘靠審查。

<a id="wt-6"></a>
**WT-6 職責清單。** 時間 Module 的職責是：(1) 把時間的 Resource 登記進它被建進的每個 World；(2) 取樣（照 `TimeSampling`；交換點做出來後這一項移給 runner，Resource 的形狀不變）；(3) 真實時間與可縮放時間的推進（暫停、倍率、上限、扣掉的記錄）；(4) 固定步的累加與驅動、`Tick`、退回 k 步、放回固定步的狀態；(5) `ActiveTime` 的切換；(6) 登記「重跑中」並放初始值（回滾 5.7）；(7) 發 `TimeDiscarded`。不在清單上的：回滾的判斷、還原與重跑（回滾 Module）；tick 只有一個來源的檢查（權威）；schedule 的骨架（編排層 O-4）；runner 的等待與交換點（編排層）；狀態（狀態機）。不另發「固定步跑了一步」「暫停、恢復」的事件：被跑到就是通知、連續的狀態讀 Resource（`v8-arch-gyo2-event-delivery-rule`）。守衛：審查；RC (d)。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」（L17 的形式：不數次數，出現就停）。

- **(a)** 要正確必須讓 Kernel 或編排層認得時間。
- **(b)** 回滾或讀檔需要時間 Module 以外的地方推 tick、退 tick 或換 `ActiveTime`。
- **(c)** 某個產品的時間組法（例：固定步在別的執行緒的 World）用這些 Resource 與函式拼不出來。
- **(d)** 職責清單（WT-6）要多一項，或判不出屬時間 Module 還是別的 Module。
- **(e)** 需要執行中換步長。
- 型別的兩條見上面「Replacement Criteria（型別）」。
