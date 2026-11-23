# 2026-11-23 網路 Module 第 3 步後半（3b）：對時；ECS 元件的 W-8 修正（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（3a 222148b3 之後；W-8 的修正 eddd7b04 先推）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md) D-9、第 6 節（CPU）、第 18 節（使用者 2026-11-23「照建議，選 A」）。
- **實作者**：Opus／high。

## 1. W-8 的修正（先做，eddd7b04）

- windows-x64 的 L1（run 45037365171，第 2 步起）在 `Registrar::Component<PeerLedger>` 撞上 Kernel 的 W-8 斷言（元件要 nothrow 可搬移）。MSVC 的 `std::map` 搬移建構不是 noexcept；libc++、libstdc++ 是，所以 macOS 與 Linux 沒報。
- 元件裡的容器改成排序的 `std::vector` 的 `FlatMap`（`engine/modules/net/include/engine/net/FlatMap.hpp`）：`PeerLedger::entities`、`PeerLedger::pending`、`PeerInput<T>::slots`。Kernel 的斷言沒改。
- 查過的：網路登記的元件是 `Peer`、`PeerLedger`、`PeerControls`、`PeerInput<T>`；Resource（`NetReplicaState`、`NetWorldLink`、`ClockSync` 等）在 Kernel 沒有這個斷言，不改。回滾的 `ComponentInputHistory<T>` 在 Resource `RollbackHistory` 裡，不是元件。
- 每個元件旁放 `static_assert(kStorable<…>)`。**它在 macOS、Linux 上驗不出 MSVC 的 `std::map`**（那兩家的 `std::map` 本來就 nothrow），實際的守衛是 L1 的 windows-x64；`net.zh-Hant.md` 第 11 節寫了「元件裡不放節點式容器」這條規則。
- 行為不變：G-5、G-6、G-8、G-9 的數字和修正前相同，全部 ctest 180 個通過。

## 2. 3b：對時

### 做了什麼

- **時間 Module**（`engine/modules/world_time`）：
  - Resource `TimeSyncScale{scale = 1}`：每個 World 都登記、每次準備都放成 1（理由同 `Resimulation`：別的 Module 寫、恆在、沒人寫時是 1，`AdvanceTime` 只要一個簽名，`pausable` 的兩個版本各加一個參數）。`AdvanceTime` 用 `TimeControl.scale × TimeSyncScale.scale`，沒有 `TimeControl` 時當作 1。
  - `JumpFixedSteps(world, k)`（`FixedStep.hpp`）：`Tick` 與 `FixedTime.elapsed` 一起改 k 步，k 可正可負，不跑步的 schedule、累加器不動，`Tick` 不得小於 0（GYO_ASSERT，同 `RewindFixedSteps`）。
  - `TickWriters` 可登記：時間 Module 的 `Build` 在那次建 World 的 `ModuleBuffer` 放 `TickWriterRegistrations{"RunFixedSteps"}`；別的單位用 `WorldTimeModule::AddTickWriter(setup, 名字)` 加；`TickWriters(buffer)` 讀。原本的 `TickWriters()` 留著（時間 Module 自己的驅動者）。
- **權威 Module**：AU-E5 讀 `WorldTimeModule::TickWriters(context.World().Buffer())`。只呼叫時間 Module 的公開函式、不讀它的內部（DI-A 9），改動一行。
- **網路**：
  - Client 的 World（K1 也有）：Resource `ClockSync`，System `SyncClock`（`ExclusiveWorld<Writes<Tick, FixedTime, TimeSyncScale>>`，`Main`／`Last`，在 `SendInputs` 之前）。它登記進 `TickWriters`，有權威策略時標 `OneEndOnly`。
  - `Connection` 加 jitter（RTT 偏差的 EWMA 1/6、jitter＝偏差的一半，lightyear 的 estimator）與最新封包到達的時間；`ClientSession` 記 `Accepted` 到達的時間。
  - 量測、目標、調整照 lightyear（`net.zh-Hant.md` 第 10 節）：剛連上與 |誤差| > 10 tick 時跳；|誤差| > 1 tick 連續 3 次同方向時倍率 1.05 或 1/1.05；|誤差| ≤ 1 tick 時回到 1。
  - 3a 的暫時做法拿掉：G-6 的 Client 以自己對時後的 `Tick`＋1 為鍵放輸入。
  - `MemoryLink` 加固定延遲（`fixedDelaySteps`）與改變減損的 `SetOutgoingImpairment`（測試用）。
- **測試**：
  - `world_time`：`TimeSyncScale` 的登記與相乘（有／沒有 `TimeControl`）、`JumpFixedSteps` 的正負與下限、`AddTickWriter`／`TickWriters(buffer)`。
  - `authority`：AU-E5 認得別的單位登記的寫者。
  - `net.clock`（`ClockTests.cpp`）：Match 在第 5000 步、Client 從 0 開始；單程固定 3 步（約 47 ms）＋三分之一的封包多延後 1～2 步。連上後跳到目標，之後誤差保持在 ±2 tick 內；延遲 3 → 5 步時倍率先調、再回到 1。
- **CPU 量測**：`tests/common/net/NetBudget.cpp`（`gyo_net_budget`，不是 ctest）。
- **文件**：
  - [`world-time.zh-Hant.md`](../architecture/target/v2/world-time.zh-Hant.md)：WT-1、WT-3、WT-5、WT-6 第 (3)(4) 項。
  - [`authority.zh-Hant.md`](../architecture/target/v2/authority.zh-Hant.md)：AU-E5 列。
  - [`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md)：第 10 節（對時）、第 11 節（W-8），第 9 節拿掉暫時做法，NT-1、NT-4。
  - 三份的 Design Invariants 改動都標「實作，合併前核准定稿」。
  - WT-6 不多一項：倍率在 (3)，跳 k 步與可登記的名單在 (4)。

### 和宣告不同的地方

- **倍率固定 1.05**：lightyear 的增益隨誤差加大（`1＋0.05×誤差÷10×2`，`sync.rs:255-283`）。這裡照主會話的卡片用固定的 1.05、1/1.05。
- **遠端的估計用 lightyear 的 `RemoteTimeline` 的平滑**：主會話寫的「最後收到的 Match tick＋（經過的時間＋RTT/2）÷步長」直接用，在有 jitter 的 Link 上誤差會跳到 2 tick 以上（第一次跑：最大 2.2 tick、多跳一次）。改成 lightyear 的平滑位移（`remote.rs:109-221`：EMA 的權重隨 jitter 0.10～0.02）。lightyear 等 3 個延遲樣本才開始估；這裡照卡片「剛連上用握手的 Match tick 直接跳」，所以 RTT 還不準時可能再跳一次（測試裡跳了 1 次）。
- **測試的牆鐘**：每幀前進一個 64 Hz 的步長（15.625 ms）。第一次用 16 ms 時，Match 的 tick 與牆鐘的速率差 2.4%，平滑的估計因此偏約一個 tick；這是測試的設定，不是 Module 的。

### 自己判的細節（L17 的範圍，揭露）

- **名字**：`TimeSyncScale`、`JumpFixedSteps`、`TickWriterRegistrations`、`AddTickWriter`、`TickWriters(buffer)`、`ClockSync`、`ClockSyncSettings`、`SyncClock`、`FlatMap`。
- **死區 1 tick**：lightyear 的 `error_margin`（`SyncConfig` 的預設 1.0）；目標裡也加了這一個 tick，所以在死區裡輸入仍會在 Match 之前到。
- **目標的邊際**（共 3 tick）：jitter 邊際的固定一個 tick（`jitter_margin` 1.0）＋輸入自己的一個 tick＋誤差邊際一個 tick（`input.rs:628-629`）；jitter 的倍數 4（`jitter_multiple`）。
- **`SyncClock` 放在 `Main`／`Last`**：`JumpFixedSteps` 不動累加器，放哪裡都不會讓 `RunFixedSteps` 的步數上限出錯；放在 `SendInputs` 之前，送的輸入就用跳過之後的 tick。
- **`SyncClock` 帶 `Writes<Tick>` 標註**：AU-E5 看得到它寫 `Tick`，靠登記的名單放行（突變 `nt-clock-not-tick-writer` 證明沒登記就是 AU-E5）。

### 驗證

- 本機（macOS x86_64，Intel i9-9980HK，`core` preset＝RelWithDebInfo）：
  - `gyo_net_tests` 59 案全過；
  - 相關 ctest 79 個全過；
  - 全部的 ctest 181 個全過；
  - 網路測試兩次執行，印出的數字逐項相同（RTT 的那行除外）。
- **對時**：
  - Match 5000、Client 0：連上後第一次跳 5000，之後 300 幀內誤差最大 0.88 tick，Client 領先 Match 8 tick，沒有再跳。
  - 延遲 3 → 5 步：倍率升到 1.05，最後回到 1，最後的誤差 −0.62 tick，沒有跳。
- **G-6**（判定照舊，拿掉暫時做法之後）：
  - 對時讓輸入提前到達，沿用的步從 3a 的 39 降到 1，`nt-input-not-held`、`nt-held-fires-again` 因此 survived（G-6 幾乎看不到「沿用」）。測試加了「每 50 tick，按下之後的兩個 tick 不給輸入」（Client 這兩格不放），讓沿用與單次觸發的規則一定被走到；比的範圍也包含這些沒給的 tick（期望值＝沿用、觸發清掉）。判定（每 tick 等於送的或沿用、件數）沒變。
  - 比的 tick 404：等於 Client 送的 383，沿用 21（其中沒給的 16），都不是的 0。
  - 單次觸發：送 40、看到 40、沒送卻觸發的 0。
  - 件數：收到的格 1 558＝收下 383＋重複 1 082＋已消耗 92＋窗外 1；收下 383＝消耗 383＋前面 0；步 415＝消耗 383＋缺 32；訊息 送 410＝讀 328＋遺失 82。
- **G-9**：照舊通過（觀測 47、不相等 0、刪掉的 10 個不在 Client、舊連線的 datagram 3 個被丟並計數）。
- **CPU**（宣告第 6 節，記錄、不判定）：
  - 情境：Match 1、Client 4（K1），MemoryLink 不掉包；Match 400 個實體（Position、Health）每 tick 都動，另有地圖的門與兩個玩家；一幀一個 tick；量 2 000 個 tick（暖機 300）。
  - 量法：trace receiver 用單調時鐘計 Kernel 每個 System 的 span，每幀加總網路的 System。
  - Match（ReceiveNet＋ReceiveInputs＋ReplicateOut＋SendNet）：平均 8.19 ms／tick，p50 7.43 ms，p99 17.66 ms，最大 30.9 ms。其中 `ReplicateOut` 平均 8.03 ms，`SendNet` 0.12 ms，`ReceiveNet` 0.04 ms，`ReceiveInputs` 0.01 ms。
  - Client 一個（ReceiveNet＋ApplyReplication＋SyncClock＋SendInputs＋SendNet）：平均 0.66 ms，p50 0.45 ms，p99 6.17 ms。其中 `ApplyReplication` 0.62 ms。
  - **發現**：`ReplicateOut` 每個 Peer 每 tick 約 2 ms（400 個實體的值）。取樣（macOS `sample`）看到時間大多花在記憶體配置與釋放：每個實體、每個元件的值都用一個暫時的 `std::vector` 編碼（`Components()`），快照用 `std::map`。這是之後優化的對象（重用緩衝、快照用排序的 vector），本步沒改。64 Hz 的一步是 15.6 ms，8 個 Peer 時會超過。宣告第 6 節要的是「量、記錄，擴到 Windows Client、Linux Match 之前交使用者對照」。
- **突變**（各清單照 §14.5，都寫了 `guards`）：
  - world_time 加 4：`wt-sync-scale-ignored`、`wt-jump-tick-only`、`wt-jump-below-zero-allowed`、`wt-tick-writer-not-added`。
  - authority 加 1：`au-tick-writers-fixed-list`。
  - net 加 5：`nt-clock-no-jump`、`nt-clock-speed-reversed`、`nt-clock-speed-kept`、`nt-clock-not-tick-writer`、`nt-clock-unsmoothed`。
  - 新加的 10 個第一次就全部 killed。全清單的結果在「突變的結果」。
  - 沒加的：`TimeSyncScale` 每次準備放成 1（拿掉它，`AdvanceTime` 讀不到值就斷言中止，失敗訊息認不出是哪個突變）；目標的邊際（改小只會讓 G-6 多幾個「缺」，G-6 不判「缺」的多寡）。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step3b/`，git 不追蹤）：
  - `net-all-run1.log`、`net-all-run2.log`：對時、G-6、G-9 等的數字；
  - `cpu-budget.log`：CPU；
  - `ctest-selected.log`、`ctest-all.log`、`ctest-all-w8fix.log`；
  - `mutations-new.log`：新的 10 個；
  - `mutations-all.log`：net、world_time 全清單與 authority 的 Tick 相關。

## 突變的結果

- 新加的 10 個：10／10 killed。
- 全清單（3b 的程式之後）：net 49／51、world_time 95／96，authority 的 Tick 相關 6／6。沒過的三個：
  - `nt-input-not-held`、`nt-held-fires-again` survived：對時之後 G-6 幾乎沒有「沿用」的步（上面 G-6 一條）。測試加了沒給輸入的 tick 之後，連同其他輸入相關的 4 個重跑：6／6 killed。
  - `wt-control-ignored` stale：`AdvanceTime` 多了 `TimeSyncScale` 的參數，find 那一行變了。find 跟著改（多一個 `*sync`），replace 照舊把 `TimeControl` 換成預設，守的性質不變；重跑 killed。
- 合計：net 51／51、world_time 96／96、authority 的 Tick 相關 6／6（`mutations-all.log` 的最後是重跑）。

## 之後要接的

- `ReplicateOut` 的效能（上面的發現）。
- 第 4 步：K3（`RollbackInbox`、確認點）。
- L1：W-8 的修正（eddd7b04）與 3b 的 windows-x64。
