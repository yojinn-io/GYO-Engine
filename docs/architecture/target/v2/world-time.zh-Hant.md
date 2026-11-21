# 時間 Module（`engine/modules/world_time`）

更新：2026-11-22（`v8-arch-gyo2-authority-rollback` 第 7 步：WT-1 補「`Resimulation` 每次準備都放成 false，`CarryOver` 不帶它」並隨區拿掉「(b) 區」的字樣，WT-5 補伺服器權威策略檢查 `TickWriters()` 名單外的寫者，第 2 節刪第 3 批的 G4 註記；型別 2、型別 3、WT-4、第 1～4 節與 Design Invariants 的開頭照逐型別雜湊與第 1 步立的守衛改寫；照[事前宣告](authority-rollback-declaration.zh-Hant.md)第 11.4 節，方向使用者 2026-11-20 核准，最終文字在合併前核准）；2026-11-20（新頁，`v8-arch-gyo2-time-events` 第 6 步；同日第 6 步 C 補 `WorldTimeOptions::fixedStep`：沒有固定步的 World 用 250 ms 的上限，宣告第 3.2 節、`v8-arch-gyo2-t1-three-items`；同日第 7 步依換模型審查：放回與帶過去的步長以這次的設定為準（S1、S4）、放回的時點（S5）、倍率 -0 寫成 +0（S7），型別 2、型別 3、WT-1、WT-3 改寫）。Owner：架構。程式：`engine/modules/world_time/`（`GYO::Engine::WorldTime`，target `GYO::WorldTime`，登記單位 `WorldTimeModule`）。行為測試：`tests/common/world_time/`（ctest `world_time.types`、`world_time.module`、`world_time.sampling`、`world_time.fixed`、`world_time.restore`，標籤 `cpu`）；突變清單 [`scripts/world_time_mutations.json`](scripts/world_time_mutations.json)（`wt-`）。

由來：[事前宣告](time-events-declaration.zh-Hant.md)（`v8-arch-gyo2-time-events`，2026-11-19 動工前核准，`v8-arch-gyo2-time-events-declaration-approved`）第 3 節、第 10.3、10.4 節；遊戲時間全部放這裡、`engine/core/time` 只留時間基準（使用者 2026-11-19，`v8-arch-gyo2-game-time-in-world-time`）。本頁照第 2～5 步的實作寫；**Design Invariants 的最終文字在合併前核准時定稿**。和宣告文字不同的地方在每條末尾標「實作」。

## 一個例子

2D 橫版的單機遊戲，畫面每幀、模擬固定步 64 Hz。每幀開頭 `AdvanceTime` 取一次樣，算出這一幀的真實時間與可縮放的時間（暫停時 0、慢動作時乘 0.5）；卡住 1 秒時固定步最多補 5 步，多的整步從可縮放的時間扣掉、累計進 `VirtualTime.discarded` 並發一筆 `TimeDiscarded`。`RunFixedSteps` 把這一幀的量加進累加器，夠幾步就巢狀跑幾次 `FixedUpdate`；每跑一步 `Tick` 加 1，固定步裡的 System 讀 `ActiveTime` 得到（步長、固定步的累計），跑完換回每幀的值。工具的預覽只有每幀的 World，關掉固定步（`fixedStep=false`），一幀最多前進 250 ms、多的扣掉並記下來。PvP 的 Match 不登記 `TimeControl`，沒人能暫停；Client 的回滾策略（第 4 批）用 `RewindFixedSteps` 把 `Tick` 與固定步的累計一起退回，再用 `RunOneFixedStep` 重跑。Kernel 與編排層都不知道「每幀」「固定步」「暫停」「tick」是什麼。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Time ◄── GYO::WorldTime ──► GYO::Orchestration ──► GYO::Kernel
```

- `engine/core/time` 不動：只有時間基準（`MonotonicClock`、`Waiter`、`LateWakeStats`），連 Base，並私有地連 `GYO::Os`（等待由 OS 層的 `WakeSignal` 做，`v8-arch-gyo2-os-wait`；[`time.md`](../../time.md) DI 10）。三家（Bevy、flecs、Unity）都把「現在幾點、睡到幾點」放在 OS 介面那一層、把遊戲時間另成一處並和 World 綁在一起。
- 登記檔 `config/modules.csv` 的 `world_time` 列：依賴 `logging`（2026-11-21 起 core、ecs、orchestration 不登記，`base`、`kernel`、`orchestration`、`time` 的 link 寫在 `engine/modules/world_time/CMakeLists.txt`，`v8-arch-gyo2-core-not-registered`；之前寫 `base;kernel;logging;orchestration;time`）（`logging` 由 `v8-arch-gyo2-logging` 加，描述器 `TimeDiscardedLog` 放在訊息旁邊，日誌宣告第 13 節第 1 題 D，2026-11-20 動工前核准），`library_only=no`、`default=yes`（每個 World 預設帶、產品可關，O-6）。第一個真的進 World 的 Engine Module。
- 標頭：`TimeTypes.hpp`（純型別、算術與逐欄的雜湊函式，只 include `engine/kernel/Hash.hpp` 與 `<cstdint>`）；`FixedStep.hpp`（跑一步、退回、放回；用 Kernel 的 `ExclusiveWorld`，不讀時間基準）；`Sampling.hpp`（**唯一** include `MonotonicClock.hpp` 的標頭，`authority_scan` ZS1：判定的 System 要能讀 `Tick` 而不碰時鐘）；`WorldTimeModule.hpp`。實作：宣告的 `TickAccumulator.hpp` 併進 `TimeTypes.hpp`。
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
| `TimeConfig` | `limits`（一幀的兩個上限）、`fixedSchedule`（固定步那一張的名字，至多 63 位元組）、`fixedStep`（這個 World 有沒有固定步） | 時間 Module，建 World 時寫一次（每次準備都覆寫）。實作：宣告沒有這一列——System 沒有捕獲、名字不能用 `std::string`（當時摘要照原始位元組雜湊；第 4 批起逐型別雜湊，欄位形狀照舊），所以設定放進 Resource |

另登記保留式訊息 `TimeDiscarded`（`amount`、`realDelta`、`cause`：`StepLimit`／`FrameLimit`／`NotDriven`）。

| System 與函式 | 做什麼 |
|---|---|
| `AdvanceTime`（一般 System，掛 `Main` 的 `First`） | 依 `TimeSampling` 取一次樣、更新 `RealTime` → 上一幀的 `pending` 若沒被取走，累計進 `discarded`、發 `TimeDiscarded`（`NotDriven`），`VirtualTime.elapsed` 不扣回 → `AdvanceVirtual` 算這一幀的量與扣掉的量 → 寫 `VirtualTime`、`ActiveTime`（每幀的值）、`FixedTime.pending`；扣掉的量大於 0 時發 `TimeDiscarded`。`pausable=false` 時用不讀 `TimeControl` 的另一個簽名（沒登記的型別當參數是 RE4）。`fixedStep=false` 時呼叫 `AdvanceVirtual` 不給累加器（一幀最多 `maxFrameDelta`，扣掉的原因 `FrameLimit`），不寫 `FixedTime.pending`（所以也沒有 `NotDriven`） |
| `RunFixedSteps`（`ExclusiveWorld<Writes<Tick, FixedTime, ActiveTime>>`，掛 `Main` 的 `FixedMainLoop`） | 取走 `pending`、`Add` 進累加器得 k 步（`AdvanceVirtual` 已保證 k ≤ 上限）、呼叫 k 次 `RunOneFixedStep`；作者可以給它 run 條件（`WorldTimeOptions::fixedStepConditions`，跳過的那一幀由下一次 `AdvanceTime` 記成 `NotDriven`）。`fixedStep=false` 時不掛。實作：`fixedStepConditions` 是宣告沒有的選項 |
| `RunOneFixedStep(world, schedule)`（公開樣板，參數是任何 `ExclusiveWorld<…>`） | `Tick`+1、`FixedTime.elapsed`+步長 → `ActiveTime`＝（步長、`FixedTime.elapsed`）→ 巢狀跑固定步那一張 → Ok 時 `ActiveTime` 換回每幀的值；Err 或例外直接往上傳、不換回（World 已中斷，D2 之後 Product 結束） |
| `RewindFixedSteps(world, k)`（公開樣板） | `Tick`−k、`FixedTime.elapsed`−k×步長；累加器不動；k 超過 `Tick` 是斷言。之後呼叫 k 次 `RunOneFixedStep` 重跑（回滾宣告 7.2） |
| `RestoreFixedTime(world, elapsed, overstep)`（`World&` 與 `ExclusiveWorld` 兩個多載） | 讀檔放回固定步的狀態：`elapsed` 與累加器的餘數；步長以這個 World 的設定為準。存檔的餘數 ≥ 這個 World 的步長時（例：32 Hz 存、64 Hz 讀），留 `餘數 % 步長`，多的整步記進 `VirtualTime.discarded`（不發 `TimeDiscarded`：放回不在 System 裡）；負的餘數是斷言。放回的時點見 WT-3。實作：第 7 步之前餘數 ≥ 步長是斷言中止（審查 S1） |
| `WorldTimeModule::TickWriters()` | 驅動 `Tick` 的 System 名單＝`{"RunFixedSteps"}`（L27；對時 System 在 Client 那一批加） |
| `WorldTimeModule::Check` | 固定步那一張不存在時回報 `WT-E1`（`Check` 只在 Kernel 沒有啟動錯誤時跑，O-2）；`fixedStep=false` 時不需要那一張、不回報 |

**掛在哪**：時間 Module 不宣告任何 schedule，只往編排層的預設骨架（O-4）上掛兩格並巢狀跑 `FixedUpdate`。作者關掉骨架、自己宣告時，用 `WorldTimeOptions` 的 `frameSchedule`、`advanceStage`、`fixedStepStage`、`fixedSchedule` 換名字：掛載的目標不存在是 Kernel 的 D-E1，固定步那一張不存在是 `WT-E1`。沒有固定步的 World（`fixedStep=false`）只掛 `AdvanceTime`，不需要固定步那一張。

**設定**（`WorldTimeOptions`，經 `WorldDescription` 的選項包給；沒給用預設）：`step`（`StepLength::FromHz(double)` 四捨五入成整數奈秒一次，或 `FromNanoseconds`；預設 64 Hz＝15,625,000 ns）、`limits`（`maxStepsPerFrame` 預設 5、`maxFrameDelta` 預設 250 ms）、`pausable`（預設 true）、`fixedStep`（預設 true；false 時沒有固定步：不掛 `RunFixedSteps`、一幀最多 `maxFrameDelta`；`FixedTime`、`Tick` 照登記、值不動，照 Bevy `TimePlugin` 一律登記 `Time<Fixed>`）、四個名字、`fixedStepConditions`。

## 3. 取樣（過渡）

時間設計的形狀是「runner 在交換點寫取樣的時間點」，交換點在編排層第 5 條。在那之前照 Bevy `TimeUpdateStrategy`，取樣放在 `TimeSampling`：

- `TimeBase`（預設）：`AdvanceTime` 讀 `::Engine::Time::NowNs()`；建好 World 後第一次取樣 `delta`＝0。
- `ManualInstant`：`value` 是這一幀的時間點；第一次取樣 `delta`＝0。
- `ManualDelta`：每幀前進 `value`，**第一幀就套用**，沿用到被改（Bevy `ManualDuration`）。

三個後果：(1) 多個 World 的產品各自取樣，同一幀不同 World 的 `RealTime.now` 不同——「一次醒來只取樣一次」在交換點做出來之前只對每個 World 成立；(2) Ownership 和時間設計「取樣歸 runner」不同，是過渡，職責清單第 (2) 項註明；(3) 手動模式沿用到被改。`AdvanceTime` 不是判定的 System；K.3 第 11 項只禁判定的 System 自己讀時鐘（區隨第 4 批拿掉，這一條現在是產品的規則，`determinism.md` DI 2）。

## 4. 和讀檔、回滾、權威的交界

| 要求（出處） | 回答 |
|---|---|
| 時間的 Resource 能被讀檔放回（讀檔 5.5） | **可以存與放回**：`Tick`、`VirtualTime`（`elapsed`、`discarded`、`fraction`；`delta` 下一幀會被蓋）、`FixedTime` 的 `elapsed` 與累加器的餘數（`RestoreFixedTime`）、`TimeControl`。**不放回**：`RealTime`（時間基準的時間點跨行程沒有意義，新 World 從頭開始）、`FixedTime` 的步長（以這次的設定為準）、`Resimulation`（恆 0）、`TimeConfig`（每次準備都照設定覆寫）。準備時已經在的值不覆蓋（O-7）；`FixedTime` 帶過去時留 `elapsed` 與餘數、步長換成這次的設定，餘數 ≥ 新步長時同 `RestoreFixedTime`（第 7 步依審查 S4；之前帶過去的步長是舊 World 的）。帶過去的情形由 `world_time.restore` 的 carry-over 案守。存檔結構不再要求沒有 padding：摘要逐型別雜湊（Kernel DI-KS 27），第 4 批第 1 步拿掉 `static_assert` |
| `Tick` 是回滾策略能寫的 Resource、用 `ExclusiveWorld` 退（回滾 7.2 第 1 條） | 公開型別；不擋任何寫者；`RewindFixedSteps` 是建議的退法 |
| 重跑時 System 讀到的固定步時間等於第一次（7.2 第 2 條） | `RewindFixedSteps`＋`RunOneFixedStep`：步長在 World 跑起來之後不變，所以重跑第 T+1 步時 `Tick`、`elapsed`、`ActiveTime` 與第一次相同 |
| 「重跑中」由時間 Module 登記、恆在（5.7、7.2 第 3 條） | `Resimulation`；沒有時間 Module 的 World 沒有它 |
| 公開 tick 型別、驅動者（L27、權威 5.4） | `Tick`；`TickWriters()`；`RunFixedSteps` 帶 `Writes<Tick>` |
| 時間設計 DI 2「各只有一個寫者」 | 改成約定（WT-5），Kernel 沒有啟動檢查（Kernel 不數寫者、作者自己寫驅動也做得到）；選了伺服器權威策略的 World 由 AU-E5 檢查 `Tick` 的寫者 |
| tick 的 Z6 例外、「tick 只有一個來源」（統合 16、權威 5.4） | 第 4 批已做，都在伺服器權威策略（[權威頁](authority.zh-Hant.md)第 3 節）：`Tick` 寫在可回滾 schedule 上不算違反 Z6（AU-E6 的 `Tick` 例外）；`TickWriters()` 名單外的寫者是 AU-E5 |
| 訊息要 tick 的自己在內容帶（游標宣告 3.2） | 寫的 System 讀 `Res<Tick>` |

## Design Invariants

守衛欄：行為測試（`world_time.*` 的 ctest 群組、突變 `wt-`）照常是守衛；scenario 第 2 段（`scenario.segment2`，本批第 6 步）通過之後也是。**綁實作方式的掃描在第 4 批第 1 步立了**（`v8-arch-gyo2-guards-after-mechanism-stable`；第 3 批宣告第 6.3 節「新」那一列延下來的三條，`v8-arch-gyo2-authority-rollback` 宣告 9.5）：`kernel_guard` K9 加時鐘的識別字（ctest `kernel.guard`，突變 `ks-k9-clock-dropped`）、`naming_guard` 加 `engine/modules/world_time`（ctest `kernel.naming_guard`，突變 `wt-guard-naming-root-dropped`、`wt-guard-real-n1-other-namespace`）、`world_time` 的 `gyo_expect_links`（configure 時檢查，突變 `wt-links-more`）。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）「實作」標的是照實作而和宣告第 10.3、10.4 節文字不同的地方。

### 時間的型別（宣告第 10.3 節）

<a id="types-1"></a>
**型別 1. 累加器只做整數的算術。** 不讀時間基準、不記 tick、不丟時間；從上一次 `Reset` 起，加進去的總量恆等於步數×步長＋餘數；`Add` 之後餘數不足一步；步數超過呼叫者給的上限是斷言。`Reset` 回傳清掉的量，本批不接線（執行中換步長沒有點名的 Feature）。守衛：`world_time.types`（累加、餘數、不丟時間、上限的斷言、`Reset`）；突變 `wt-add-*`、`wt-until-next-*`、`wt-reset-not-cleared`。

<a id="types-2"></a>
**型別 2. 超過上限的量只在 `AdvanceVirtual` 一處算。** 有固定步時只扣超過 `maxStepsPerFrame` 的整步（扣完的餘數和不扣時相同）；沒有固定步時一幀最多 `maxFrameDelta`；兩個上限不同時作用。暫停時這一幀的量是 0、零頭保留。倍率是唯一的浮點，每幀乘一次，零頭帶到下一幀；寫進 `VirtualTime.scale` 時 -0 寫成 +0（第 7 步依審查 S7；當時摘要照位元組雜湊，第 4 批起逐型別雜湊、浮點逐位元，-0 與 +0 照樣不同）。頻率換算成整數奈秒一次、四捨五入，不是有限的正數是斷言。時間 Module 裡「有沒有固定步」由 `WorldTimeOptions::fixedStep` 選：開著（預設）時 `AdvanceTime` 給累加器、只扣整步；關掉時不給、一幀最多 `maxFrameDelta`（第 6 步 C；之前一律給累加器，250 ms 的上限在時間 Module 裡永遠不作用）。守衛：`world_time.types`（扣整步、步數不超過上限、暫停、倍率、零頭、沒有固定步的 250 ms、步長的設定）；`world_time.module`「fixedStep = false: a frame advances at most 250 ms …」「fixedStep = false: the cap is TimeLimits::maxFrameDelta …」；突變 `wt-no-fixed-passes-accumulator`、`wt-no-fixed-flag-not-set`、`wt-no-fixed-limits-default`、`wt-scale-negative-zero-kept`、`wt-max-steps-default`、`wt-step-limit-cause-wrong`、`wt-frame-limit-cause-wrong`、突變 `wt-cut-*`、`wt-elapsed-before-cut`、`wt-frame-cap-*`、`wt-no-frame-cap`、`wt-fraction-*`、`wt-pause-*`、`wt-resume-catches-up`、`wt-scale-*`、`wt-hz-*`、`wt-default-step-not-64hz`、`wt-negative-scale-allowed`。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）

<a id="types-3"></a>
**型別 3. 純型別與算術的標頭只引用 Kernel 的雜湊客製點、不 include `MonotonicClock.hpp`；每個型別帶雜湊函式。** 純型別與算術在 `TimeTypes.hpp`（只 include `engine/kernel/Hash.hpp` 與 `<cstdint>`，為了在型別旁邊寫逐欄的 `HashValue`；其餘引用 Kernel 的是同一個 Module 裡的函式與 System：`FixedStep.hpp`、`WorldTimeModule.hpp`；讀時間基準只在 `Sampling.hpp`）。時間 Module 放進 World 的每個型別都有逐欄的雜湊函式（World 摘要逐型別雜湊，Kernel DI-KS 27；第 4 批宣告 3.3），不再要求沒有 padding：第 4 批第 1 步拿掉大小的 `static_assert`，留下的保留欄位是摘要格式 2 的遺留、不雜湊。浮點逐位元雜湊：`VirtualTime.scale` 照舊把 -0 寫成 +0（型別 2）；作者寫的 `TimeControl.scale` 等，`±0` 不同就是不同，要相等由寫入的一方正規化（第 4 批宣告第 12 節第 2 件）。實作：宣告的 `TickAccumulator.hpp` 併進 `TimeTypes.hpp`。守衛：`world_time.module`「module: every time type has a hash function: a World with only the time Module has a summary」與突變 `wt-hash-resimulation-missing`；include 的分法靠審查（第 4 批第 1 步立的三條守衛不含它）。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）

**Replacement Criteria（型別）**：時間 Module 要正確必須知道累加器的內部狀態；或 `ActiveTime` 需要第三種來源。

### 時間 Module（宣告第 10.4 節；WT 是宣告加的代號）

<a id="wt-1"></a>
**WT-1 登記。** 建進一個 World，就登記 `RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`Tick`、`TimeSampling`、`Resimulation`、`TimeConfig`、訊息 `TimeDiscarded`，`pausable` 時加 `TimeControl`；在第一次 Run 之前放初始值，已經在的值不覆蓋（O-7；帶過去的 `FixedTime` 只換步長，見第 4 節），`TimeConfig` 每次照設定覆寫。`Resimulation` 恆在，每次準備（啟動與每次替換）都放成 false，`CarryOver` 不帶它（作者標了帶過去也照樣放成 false：替換打斷的回滾不留下「重跑中」）；除此之外時間 Module 不寫它。兩個 System 掛進設定的名字（預設是編排層的骨架）；掛載的目標不存在是 Kernel 的 D-E1，固定步那一張不存在由 `Check` 回報 `WT-E1`。`fixedStep=false`（預設 true）時不掛 `RunFixedSteps`、不需要固定步那一張（`Check` 不回報）；`FixedTime`、`Tick` 照登記。實作：多 `TimeConfig` 一列；`WT-E1` 是時間 Module 自己的碼。守衛：`world_time.module`「every time Resource is registered and put in …」「pausable = false registers no TimeControl …」「the step length comes from the settings …」「a fixed-step schedule that does not exist is reported by Check (WT-E1)」「a mount target that does not exist is the kernel's D-E1」「fixedStep = false mounts no RunFixedSteps …」；「by default AdvanceTime is mounted in Main's First …」「an author who turns the skeleton off names its own schedules …」「maxStepsPerFrame comes from the settings …」「a 64-byte fixed-step schedule name …」；`world_time.restore`「carry-over: …」；`WorldTimeModuleTests.cpp`「carry-over: Resimulation is false in the new World, carried over or not」；突變 `wt-resimulation-carried`、`wt-no-fixed-driver-mounted`、`wt-no-fixed-check-reports`、`wt-fixed-step-default-off`、`wt-check-silent`、`wt-resimulation-not-put`、`wt-control-always-registered`、`wt-unpausable-reads-control`、`wt-step-setting-ignored`、`wt-carried-value-overwritten`、`wt-config-kept-when-carried`、`wt-carried-step-kept`、`wt-carried-excess-not-discarded`、`wt-fixed-stage-option-ignored`、`wt-frame-schedule-option-ignored`、`wt-fixed-schedule-option-ignored`、`wt-max-steps-setting-ignored`、`wt-fixed-stage-default`、`wt-advance-stage-default`、`wt-pausable-default-off`、`wt-schedule-name-length-unchecked`。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）

<a id="wt-2"></a>
**WT-2 取樣與推進。** 每個 World 每幀只取一次樣（`AdvanceTime`，依 `TimeSampling`；`TimeBase` 與 `ManualInstant` 的第一次取樣 `delta`＝0，`ManualDelta` 第一幀就套用）；`RealTime` 不受暫停、倍率、上限影響；扣掉只在 `AdvanceVirtual` 算（型別 2）；扣掉的量（有固定步時 `StepLimit`、沒有時 `FrameLimit`）與上一幀沒被 `RunFixedSteps` 取走的量（`NotDriven`）都累計進 `VirtualTime.discarded`，每一種大於 0 時各發一筆 `TimeDiscarded`（一幀最多兩筆）；`NotDriven` 的量只記進 `discarded`，`VirtualTime.elapsed` 不扣回；`discarded` 只由 `AdvanceTime` 寫。實作：`ManualDelta` 第一幀套用、`NotDriven` 不扣回 `elapsed`、一幀可能兩筆。守衛：`world_time.sampling` 各案；`world_time.fixed`「a frame that falls behind …」「a frame whose RunFixedSteps a run condition skips …」；`world_time.module`「fixedStep = false: a frame advances at most 250 ms …」；突變 `wt-no-fixed-pending-written`、`wt-real-stops-while-paused`、`wt-sampled-twice`、`wt-sampling-default-manual`、`wt-not-driven-elapsed-taken-back`、`wt-sample-*`、`wt-real-first-delta`、`wt-manual-first-frame-zero`、`wt-not-driven-*`、`wt-cut-no-message`、`wt-cut-not-recorded`、`wt-discarded-overwritten`、`wt-pending-*`、`wt-control-ignored`、`wt-active-real-time`、`wt-conditions-dropped`。

<a id="wt-3"></a>
**WT-3 tick 與固定步。** World 跑起來之後，`Tick` 與 `FixedTime.elapsed` 只經 `RunOneFixedStep`（前進：跑之前加 1 與一個步長）與 `RewindFixedSteps`（退回 k 與 k 個步長）一起改；讀檔放回（`RestoreFixedTime` 與放回 `Tick`）在準備時機做（新 World 第一次 Run 之前的 prepare 函式）；存檔的餘數 ≥ 這個 World 的步長時留 `餘數 % 步長`、多的整步記進 `VirtualTime.discarded`。`ExclusiveWorld` 的多載只能放在這一幀的 `RunFixedSteps` 之後到下一幀的 `AdvanceTime` 之前（例：`Main` 的 `Last`）；放在同一幀的 `AdvanceTime` 與 `RunFixedSteps` 之間是 Programmer Error——`AdvanceTime` 照舊的餘數算出這一幀准跑的步數，放大的餘數會讓 `RunFixedSteps` 的步數上限斷言中止（第 7 步依審查 S5 寫明，程式不改）。固定步執行中 `ActiveTime`＝（步長、`FixedTime.elapsed`），Ok 時換回每幀的值，Err 或例外時不換回、直接往上傳；k 步裡第 j 步出錯，之後的不跑。累加器不因退回而倒退；步長只在建 World 時給，執行中不換（`Reset` 不接線）。`RunFixedSteps` 一幀的步數不超過 `maxStepsPerFrame`。沒有固定步的 World（`fixedStep=false`）：`Tick` 與 `FixedTime` 不動，`ActiveTime` 一律是每幀的值。守衛：`world_time.fixed`「k steps a frame, Tick + k …」「RewindFixedSteps then RunOneFixedStep k times …」「the j-th of k steps returns Err …」；`world_time.restore`；`world_time.restore`「a remainder saved under a longer step …」（prepare 函式與獨占 System 兩案）；`world_time.fixed`「RewindFixedSteps may not take more steps back than Tick …」；突變 `wt-run-one-step-no-tick`、`wt-tick-after-step`、`wt-one-step-a-frame`、`wt-fixed-elapsed-kept`、`wt-active-not-fixed`、`wt-active-not-restored`、`wt-restored-on-err`、`wt-run-not-err`、`wt-rewind-*`、`wt-restore-no-overstep`、`wt-restore-no-elapsed`、`wt-restore-remainder-clamped`、`wt-restore-excess-not-discarded`、`wt-restore-exclusive-noop`、`wt-restore-exclusive-excess-dropped`、`wt-stages-swapped`。

<a id="wt-4"></a>
**WT-4 依賴。** `world_time` 只依賴 `orchestration`、`kernel`、`time`、`base`，以及為了描述器 `TimeDiscardedLog` 的 `logging`（2026-11-20 日誌宣告第 13 節第 1 題 D 核准；`logging` 不認得 ECS 與時間 Module，方向是 `world_time` → `logging`）（對 `logging` 的 link 由登記檔 `modules.csv` 的那一列產生；對 core、ecs、orchestration 的寫在 CMakeLists）；Kernel、編排層不認得它（不 include、不按它的名字分支）；程式裡的 `Engine::` 一律全限定；只有 `Sampling.hpp` include `MonotonicClock.hpp`。守衛：`cmake.UnitRegistry`（登記檔與產生的 link）；`gyo_orchestration_include_order`（各種順序編得過；突變 `wt-include-order-unqualified`，限 darwin）；Kernel 與編排層的 include 掃描（`kernel_guard`、`orchestration.guard`）擋它們 include 時間 Module。`world_time` 自己的 `gyo_expect_links`（`tests/common/world_time/CMakeLists.txt`，configure 時檢查；突變 `wt-links-more`）與 `naming_guard` 的目錄對應（ctest `kernel.naming_guard`；突變 `wt-guard-naming-root-dropped`）在第 4 批第 1 步立了。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）

<a id="wt-5"></a>
**WT-5 寫者的約定。** `RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`TimeConfig` 只由時間 Module 寫；`TimeControl`、`TimeSampling` 由產品（與測試）寫；`Resimulation` 只由回滾策略寫；`Tick` 另外可由回滾策略寫、讀檔時由作者放回——**約定，不是啟動檢查**（Kernel 不數寫者，`-kernel-is-ecs-no-restrictions`）。`TickWriters()` 公開驅動者名單、`RunFixedSteps` 帶 `Writes<Tick>`；選了伺服器權威策略的 World，由策略在啟動時檢查 `TickWriters()` 名單外的 `Tick` 寫者（System、hook、observer，`Writes<Tick>` 標註也算；AU-E5，[權威頁](authority.zh-Hant.md)第 3 節；權威份 5.4），沒選的 World 照舊只是約定。實作：加 `TimeConfig`。守衛：突變 `wt-tick-writers-empty`、`wt-driver-no-writes-tick`（`world_time.module`「TickWriters names RunFixedSteps …」）；`authority.authority_checks`「authority checks: AU-E5: only the time Module's drivers write the tick」「authority checks: AU-E5 sees an ExclusiveWorld<Writes<Tick>> annotation, a hook and an observer」；其餘靠審查。（草稿，`v8-arch-gyo2-authority-rollback` 合併前核准）

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
