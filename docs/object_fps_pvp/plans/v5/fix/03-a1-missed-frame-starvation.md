# 03 A1 對偶發掉幀敏感（Starvation 重設；55–58 FPS vsync）

狀態：**已知問題**（未修正；使用者決定暫不處理：「目前沒有影響到那麼嚴重」）。2026-10-02。Owner：`object_fps_pvp`。
相關：A1 由 PR #2（`claude/project-thread-lv6bg5`，`d74830c`，合併為 `ff11ee3`）引入；
低幀率守門 v3 與下文引用的測例在提交前的工作分支 `claude/pvp-v5-start-phase-guard`，尚未 commit／開 PR。
第03批仍未結案（計次 GUI 可見延遲輪與 25 案矩陣依使用者決定未執行），第04批暫停。
下列方案只是評估與 scratch 原型，沒有進產品，也不代表已排程。
另見 [01 低幀率守門](./01-a1-low-fps-regression.md)、[02 停頓重新播種取消A1](./02-a1-cancelled-by-stall-reseed.md)、
[06 視窗干擾與補跑規則](./06-gui-window-interference-and-rerun-rule.md)、[07 啟動相位診斷](./07-start-phase-diagnostics.md)、
[修正索引](./README.md)、[A1 dev_log](../../../../dev_logs/2026_10_01_pvp_v5_start_phase.zh-Hant.md)。

本文涵蓋同一根源（A1 的 just-in-time 餘裕）或同一守門參數造成的四種情況。
數字未標「實機」者皆為 CPU 模擬。

| 代號 | 情況 | 來源 |
|---|---|---|
| S1 | cut 以上的偶發掉幀（例：60 FPS 每 2 秒一個 33ms 幀）引發 starvation 重設 | A1 本身；守門不改變 |
| S2 | 60Hz vsync 每 12–30 幀掉 1 個 refresh（55–58 FPS）：是否對齊取決於啟動時機，保留時同 S1 | A1＋守門的 8 間隔決定 |
| S3 | 5 Tick 以上長幀：固定步丟整步，相位差只在一個 Tick 的模數內保留 | A1＋`FixedTickRuntime` |
| S4 | 本機 GUI 節奏下恢復餘裕很薄（smoke-3 平均約 1.056 Tick，恢復線 1.06） | 守門 v2／v3 |

## 問題成因

### 共同機制：just-in-time 餘裕＋starvation fuse

1. A1：Host 回報 epoch 首窗口等待 w（`PlayerState.epochStartWaitMicros`），Client 把固定步相位移
   w＋首窗口已過時間－4ms（`MovementStartPhaseTargetSeconds`＝0.004，`Movement.hpp:34`；
   `LocalPlayerPrediction::Reconcile`，`LocalPlayerPrediction.cpp` 約 192–222 行）。
   之後每個命令在 lead 之外只剩約 4ms 餘裕。review 模擬：對齊時約 25% 的裁決之後 Host 佇列為空，未對齊約 6%。
2. 60 FPS 掉一個 refresh＝一幀約 33ms：這一幀本該產生的固定步晚一個 Tick，和下一步在同一幀產生，
   再由獨立 60Hz worker 送出。16.7ms 的延後遠大於 4ms 餘裕，較舊的命令錯過它的 Tick，
   Host 以 Held 替代（`PvpMatch::Tick`）。
3. Starvation fuse（`apps/object_fps_pvp/src/Pvp/PvpMatch.cpp:420–433`）：距上次重設已滿
   `MovementResetCooldownTicks`（60 Tick），且下列同時成立：
   - 最近 `MovementBacklogSampleTicks`（30）個裁決後的連續待執行數總和為 0（`backlogSum == 0`）；
   - 其中至少一個不是 Actual（`fallbackCount > 0`）；
   - 當下沒有任何排隊命令（`commands.empty()`）。

   成立則下一 Tick 以 `MovementResetReason::Starvation` 執行 `ResetMovementEpoch`（`:334`）。
   程式註解寫明 `All-Actual just-in-time traffic must never trigger this fuse`：
   全 Actual 的 just-in-time 不觸發，但只要混進一次 Held 就觸發。
4. 未對齊時餘裕是啟動抽籤（w 0–16.7ms），佇列較常有命令，「30 Tick 全為 0」很少成立；
   同樣的掉幀多半只多一次 Held，不重設。

### 守門為何不管（設計取捨）

- [01](./01-a1-low-fps-regression.md) 的守門看最近 32 個正幀間隔（每個上限 2 Tick，`StartPhaseFrameIntervalLimit`）
  的平均，＞1.1 Tick（`MovementStartPhaseMaximumFrameSeconds`，`Movement.hpp:47`）才撤回。
- 32 幀內有 k 個掉 refresh 時平均為（32＋k）／32 Tick：k＝1 為 1.031、k＝3 為 1.094，都在 cut 內；
  k＝4（1.125）才撤回。一次長卡頓也只算一個掉 refresh。
- 這是刻意的：59.94Hz、一般抖動與本機約 58 FPS 的 GUI 幀必須保持對齊；cut≈54.5 FPS 經使用者確認。

### S1：cut 以上的偶發掉幀

上述機制直接成立。守門不反應，這些幀型下的結果與 `d74830c` A1 逐位元相同
（證據 `rv2/out/g_sparse_base.txt` 與 `g_sparse_v2.txt` 中各 sparse 列的 A 值一致）。

### S2：55–58 FPS vsync（每 N 幀掉 1 個 refresh，N＝12–30）

- 窗口滿 32 個間隔後最多含 ⌈32／N⌉ 個掉 refresh，平均 ≤約 1.095 Tick（含 59.94Hz），在 cut 內。
- 但決定只等 8 個間隔（`StartPhaseFrameEvidence`＝8，`LocalPlayerPrediction.cpp:28`）：
  - 第一個掉 refresh 落在前 9 個計數間隔內：決定時平均 1.11–1.13 Tick＞cut，以 `FrameRateBelowTick` 撤回；
  - 落在第 10 個：正好在 cut 上（隨抖動約各半，測例不釘選）；
  - 落在第 11 個以後：套用並一直保留，之後與 S1 相同。
- 撤回後須平均 ≤1.06 Tick（`MovementStartPhaseRestoreFrameSeconds`，`Movement.hpp:52–53`）維持 32 幀才恢復；
  含兩個掉 refresh 的 32 幀窗口為 34／32×1.001≈1.064 Tick＞1.06，N≤31 時 dwell 永遠湊不齊，該 epoch 一直不對齊。
- 結果：啟動時機隨機時約 9.5／N 的乾淨啟動不對齊（N＝12 約 80%、20 約 48%、30 約 32%），
  其餘對齊並承受 S1 的重設。同一台機器、同一顯示，每次啟動的結果可能不同。
- `Movement.hpp:44–46` 的註解（`stays within it once the window holds more than ten intervals`）
  容易讀成這類顯示一定對齊；實際行為以釘選測例（見「如何復現」A）為準。

### S3：5 Tick 以上長幀

- `FixedTickRuntime`（`engine/runtime/include/engine/runtime/FixedTickRuntime.hpp`）每次 `Advance` 最多執行
  `CatchUpSteps`＝5 步（`LocalPlayerPrediction.hpp:88`），多出的整步丟棄（`droppedSeconds`），只保留不足一步的餘數。
- A1 調整的是不足一個 Tick 的相位。丟整步後餘數仍在，但命令序號與 Host Tick 的對應少了整步：
  相位差只在一個 Tick 的模數內保留。若同時觸發 stall reseed，該 epoch 其餘時間的對齊被取消（[02](./02-a1-cancelled-by-stall-reseed.md)）。
- 守門 v3 的追趕上限：撤回時的負向 slew 夾在本幀剩餘追趕容量內（`StartPhaseCatchUpMargin`，
  `LocalPlayerPrediction.cpp` 約 306–327 行），4–5 Tick 幀中的撤回本身不會多丟一步；
  本來就丟步的幀上暫停，幀變短後完成。正向 slew 不受影響。
- Code smell（記錄，不在本次處理）：該夾限重算了 `FixedTickRuntime` 的追趕算式，日後可考慮由 Engine 提供存取器。

### S4：GUI 節奏下的恢復餘裕

恢復線 1.06 Tick（約 17.67ms，≈56.6 FPS）由 cut 推出（cut－0.04 Tick），必須高於 GUI 的幀節奏。
實機（不計次冒煙的量測期間；`summary.md` 的 `Round 1 movement` 行）：

| 冒煙 | create 中位 | join 中位 | ＞1.1 Tick 間隔 | ≥2 Tick 間隔 | 32 幀平均 |
|---|---|---|---|---|---|
| smoke-1 | 17.228ms | 17.231ms | 0 | 0 | 1.034–1.036 Tick（smoke-1／2） |
| smoke-2 | 17.271ms | 17.264ms | 0 | 0 | 同上 |
| smoke-3 | 17.566ms | 17.616ms | 165／908、165／907 | 0 | 約 1.056–1.057 Tick |

- smoke-3：27–29% 的 32 幀窗口高於 1.06，最大窗口 1.08；最長合格連續段 104–108 幀，仍長於 32 幀 dwell，
  所以可以恢復；但以其幀間隔重播（模擬），撤回後（例如啟動卡頓之後）約 12% 的時間停在撤回狀態。
- 現有測例只釘選 1.031／1.036 Tick，沒有 1.055 Tick 左右的測例。
- 恢復線不能單獨調：它跟著 cut；把 cut 往字面 60 FPS 下修會讓恢復線落到 GUI 節奏之下，撤回後幾乎不再恢復。

## 影響

### 誰、何時、代價

- 對齊中的 epoch，且 RTT＞0（模擬 RTT 20／40ms）。模擬中 RTT 0 時對齊沒有多出重設。
- 顯示在 cut 以上偶發掉幀（S1），或 60Hz vsync 在 55–58 FPS 規律掉 refresh、且啟動時機落在「保留」那邊（S2）。
- 一次 starvation 重設：Host 清空該玩家的排隊命令、`movementEpoch`＋1，新 epoch 的 seq1 到達前不推進其移動；
  Client 丟棄未確認命令並重新播種，本機預測被修正。新 epoch 重新量測 w，幀率正常時會再次對齊。
- 驗收：[05 計畫](../05-short-validation-and-acceptance.md)要求乾淨跑次「零非預期恢復重設」；
  計次輪內出現 starvation 重設，該輪就不是乾淨跑次。驗收器每輪 `summary.md` 的 `Round N movement` 行
  已列出 Host 重設原因與各角色幀間隔統計（＞1.1 Tick、≥2 Tick），可據此歸因。

### 數字（模擬）

review 模擬，產品式 worker；每格 18 次啟動（3 個 worker 相位×6 個 Authority 相位）×11 秒（自第 1 秒起計）。
值依序為 RTT 0／20／40。A＝對齊（守門 v2；這些幀型下 v3 行為相同），U＝未對齊。

| 幀型 | 平均 FPS | 重設 A | 重設 U | Held% A | Held% U |
|---|---|---|---|---|---|
| 60Hz，約每 0.5 秒一個 33ms 幀 | 58.1 | 0／15／14 | 0／0／1 | 0.019／0.780／0.688 | 0.000／0.537／0.472 |
| 60Hz，約每 1 秒一個 | 59.0 | 0／9／8 | 0／0／1 | 0.000／0.426／0.371 | 0.000／0.324／0.204 |
| 60Hz，約每 2 秒一個 | 59.5 | 0／4／5 | 0／0／0 | 0.000／0.213／0.185 | 0.000／0.157／0.102 |
| 60Hz，約每 5 秒一個 | 59.8 | 0／2／2 | 0／0／0 | 0.000／0.093／0.074 | 0.000／0.056／0.056 |
| 60Hz，每 2 秒連續掉 3 個 refresh | 58.5 | 0／3／5 | 0／0／0 | 0.009／0.463／0.482 | 0.009／0.417／0.305 |
| 60Hz，隨機掉 refresh p＝0.02 | 58.8 | 0／8／8 | 0／1／1 | 0.000／0.352／0.677 | 0.000／0.343／0.370 |
| 60Hz，隨機掉 refresh p＝0.05 | 57.1 | 0／8／3 | 0／0／0 | 0.009／0.927／0.973 | 0.000／0.861／0.685 |
| 120Hz＋每秒一次 50ms 卡頓 | ≈115 | 27／35／0 | 28／11／0 | 2.546／10.220／1.990 | 0.762／2.590／1.111 |

- 這些幀型的平均都在 cut 以上，守門保持對齊（套用時間多數 96–100%；p＝0.05 為 51–63%）。
- 120Hz 列的卡頓本身在 RTT 0 就讓兩邊都重設；對齊多出的部分在 RTT 20（35 對 11）。
- 收益仍在：約每 2 秒一個 33ms 幀時「命令產生→執行」中位數 A 30.4／39.4／50.3ms、U 38.4／47.6／60.7ms。

S2：

- 釘選測例的設定（4 秒 run；N＝12–30；兩組 worker／Authority 相位）：保留那邊（首個掉 refresh 在第 11 個或第 N 個間隔）
  RTT 20 共 76 次 run，對齊 17 次重設、108 個 Held，未對齊 0／0。撤回那邊與未對齊逐位元相同；
  同條件下無守門的 `d74830c` A1 有 21 次重設、252 個 Held（守門在這一邊反而改善）。
- review 模擬（只含保留那邊，18 次×11 秒）：N＝12／20／30 時對齊重設 RTT 20 為 14／12／13、RTT 40 為 13／15／16，
  未對齊 0–1。

### 不受影響

- cut 以下：守門使結果與未對齊逐位元相同（[01](./01-a1-low-fps-regression.md)）。
- 穩定 59.94／60（±1／±2ms 抖動）／75–240 FPS：與 `d74830c` A1 逐位元相同、零重設。
- wire、契約、Host、門檻、lead、60Hz 政策不變。

### 實機（不計次、不作為驗收證據）

- 三次 report-only GUI 冒煙量測期間 Host 移動重設 0 次、≥2 Tick 間隔 0 個；本機 GUI 幀集中在 17.2–17.6ms，
  沒有 60Hz 掉 refresh 的型態，S1／S2 未在本機出現。三次冒煙的移動方都已被啟動卡頓取消對齊
  （[02](./02-a1-cancelled-by-stall-reseed.md)），所以也沒有量到對齊狀態下的掉幀。
- 以冒煙記錄的真實幀間隔重播（模擬；smoke-1／2 的 create／join×4 個起點×RTT 0／20／40）：
  對齊沒有增加重設（各格與未對齊相同或差 1 次）。
- 使用者評估：目前影響不大，列為已知問題。

## 如何復現

### 環境

- 全部 CPU 模擬，任何能建置 `gyo_object_fps_pvp_tests` 的平台皆可，不需 GPU／GUI；文中數字取自本機 Intel Mac。
- 使用本工作分支（守門 v3）。`ClockOptions` 幀模型只存在本分支的 `tests/object_fps_pvp/MovementRecoveryTests.cpp`
  （`ClockOptions` `:71–82`、`ProductClock` `:385–394`）。
- review 模擬程式（scratch）未保存，只留輸出（見 D）；B 以產品測試框架重做，數字可能與上表不同（亂數與時鐘模型不同）。

### A. 釘選現行行為（通過＝行為未變；不輸出重設數）

在 repository root：

```bash
cmake --preset test          # 首次設定 build/target/_build/test
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests --parallel 2
T=build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_tests
$T -tc="PvP a 60 Hz display dropping one refresh in 12-30 frames keeps the start phase state it took"
$T -tc="PvP start phase frame rate counts every dropped refresh and a long hitch as one"
$T -tc="PvP a withdrawal slew in 4.0-4.4 tick frames never makes the runtime drop a step"
$T -tc="PvP after startup hitches a 58 FPS GUI cadence restores the start phase within 64 frames"
```

| 測例 | 對應 | 斷言 |
|---|---|---|
| `...12-30 frames keeps the start phase state it took`（`MovementRecoveryTests.cpp:610`） | S2 | N＝12–30×首個掉 refresh 在第 1／9／11／N 個間隔×RTT 0／20／40×兩組相位；第 11 個以後一直套用，第 9 個以前撤回且與未對齊逐位元相同 |
| `...counts every dropped refresh and a long hitch as one`（`PredictionTests.cpp:1229`） | 守門取捨 | 250ms 卡頓、16 幀掉 1 個 refresh 仍對齊；掉 2 個或 vsync 50 FPS 撤回 |
| `...4.0-4.4 tick frames never makes the runtime drop a step`（`PredictionTests.cpp:1395`） | S3 | 撤回 slew 不造成丟步 |
| `...58 FPS GUI cadence restores...`（`MovementRecoveryTests.cpp:537`） | S4 | 1.031／1.036 Tick 節奏在 64 幀內恢復 |

### B. 計數重設（暫時測例，不提交；撰寫本文時未建置、未執行）

`ClockOptions`／`RunRecovery` 旋鈕：

| 欄位 | 意義 | 本問題用法 |
|---|---|---|
| `refreshPattern` | 每幀顯示幾個 refresh，循環 | S1：N 個 1 中放一個 2（N＝30／60／120／300≈每 0.5／1／2／5 秒一個 33ms 幀） |
| `droppedRefreshEvery`／`firstDroppedRefresh` | 從第 first 個計數間隔起，每 N 個多一個 refresh | S2：N＝12–30；first≤9 撤回、≥11 保留 |
| `missedRefreshPerMille` | 隨機掉 refresh（千分率） | p＝0.02 → 20 |
| `startupHitches`／`startupHitch` | 啟動後最初幾幀的固定長度 | S4（與 `frameDriftPpm` 併用） |
| `ProductClock(driftPpm, jitter, seed, runUntil)` | 產品式 worker（每次晚醒 0.175–0.525ms 並以送出時刻重設期限）、顯示時鐘漂移、幀抖動 | S1：0ppm、±0.5ms、11 秒 |
| `RunRecovery(..., hostStartWait, clock)` | `hostStartWait=false` 為未對齊對照 | — |
| `RecoveryResult::resets`／`held`／`resolved` | `resets` 計任何 epoch 變更（不分原因） | — |

S1（貼在 `tests/object_fps_pvp/MovementRecoveryTests.cpp` 檔尾，用完刪除）：

```cpp
TEST_CASE("SCRATCH A1 sparse missed refresh resets") {
    for (const std::size_t every : {std::size_t(30), std::size_t(60), std::size_t(120), std::size_t(300)})
        for (const int rtt : {0, 20, 40}) {
            unsigned aligned{}, unaligned{};
            for (const Time worker : {Time(0), Time(4000), Time(8000)})
                for (Time authority = 0; authority < Tick; authority += Tick / 6) {
                    auto clock = ProductClock(0, Units / 2000,
                        std::uint32_t(worker * 31 + authority * 7 + rtt + 1), 11 * Units);
                    clock.refreshPattern.assign(every, 1);
                    clock.refreshPattern[every / 2] = 2; // one 33 ms frame per cycle
                    aligned += RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, true, clock).resets;
                    unaligned += RunRecovery(60, rtt, 0, false, worker, authority, 0.0, 0, false, clock).resets;
                }
            MESSAGE("every ", every, " frames rtt ", rtt, ": resets aligned ", aligned, " unaligned ", unaligned);
        }
}
```

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests --parallel 2
$T -tc="SCRATCH A1 sparse*"
```

- 預期方向（依 review 模擬）：RTT 20／40 對齊重設多於未對齊，RTT 0 皆為 0；確切數字未驗證。
- S2：把兩行 `refreshPattern` 換成 `clock.droppedRefreshEvery = n; clock.firstDroppedRefresh = first;`，
  對 n＝12–30、first∈{11, n}、（worker, authority）∈{(0, 0), (8000, Tick / 2)} 累計 `resets` 與 `held`，
  時鐘與 seed 照抄釘選測例（`ProductClock(1000, Units / 2000, <同式 seed>, 4 * Units)`）。
  實作者以相同設定量得 RTT 20：對齊 17 次重設／108 個 Held，未對齊 0／0。
- S4：複製 `PvP after startup hitches a 58 FPS GUI cadence...`，把 `frameDriftPpm` 換成 55000–57000（1.055–1.057 Tick），
  看 `firstShifted` 是否出現、距 `firstWithdrawn` 幾幀。結果未知。

### C. 實機（不計次、不作為驗收證據）

- 需要在 60Hz vsync 下會偶發掉 refresh 的顯示環境；本機 Intel Mac 沒有出現（見「影響」）。
  macOS 需 Xcode＋Metal toolchain；在閒置機器上一次只跑一輪。
- report-only GUI 冒煙（路徑取自冒煙的 `run-manifest.json`）：

```bash
python3 build/acceptance/object_fps_pvp/run_timing.py --gui --short --report-only --duration 16 \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output build/target/_build/test/logs/<新的證據目錄>
```

- 判讀 `summary.md`：`Round 1 movement` 行的 starvation 次數與 `>=2 ticks` 個數，`Round 1 start phase` 行的對齊狀態。
  GUI probe 在視窗建立後立即加入，對齊常被啟動卡頓取消（[02](./02-a1-cancelled-by-stall-reseed.md)），此時量不到 S1。
  計次輪的規則見 [06](./06-gui-window-interference-and-rerun-rule.md)。

### D. 已存證據（git 忽略，只在本機）

`build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/rv2/out/`：

- `g_sparse_base.txt`（base＝`d74830c`）、`g_sparse_v2.txt`、`g_vsync_v2.txt`、`g_high_v2.txt`：S1。
- `g_nmiss_v2.txt`：S2，但其 pattern 從第 0 幀起算，N≥14 時首個掉 refresh 都落在第 11 個間隔之後，只代表保留那邊。
- `g_replay2_v2.txt`：冒煙幀間隔重播；`h_*_m1.txt`／`h_*_m2.txt`：下述原型 B／B′。
- 冒煙：`integration-smoke-1`／`-2`／`-3`、`reanalysis-integration-smoke-1`／`-2` 的 `summary.md`。

## 解決方案

未實作。使用者決定列為已知問題。以下每一項都會改變已批准的參數或政策，須使用者批准，不代表已排程。

### 評估過的方案

| 方案 | 做法 | 效果 | 代價／風險 | 評估程度 |
|---|---|---|---|---|
| A 加大目標 | 提高 `MovementStartPhaseTargetSeconds`（現 4ms，使用者批准的 A1 參數） | 推論：要扛住一個掉 refresh 需＞16.7ms，約等於最差的未對齊啟動，A1 收益歸零；中間值只降低機率 | 每個對齊啟動都少收益；改已批准參數 | 未模擬 |
| B 掉 refresh 判準（review 原型 m1） | 窗口內任一間隔 ≥1.5 Tick 即撤回；窗口內沒有這種間隔且平均 ≤1.06 Tick 維持 32 幀才恢復；不改 header | S2 重設降到與未對齊相同（0／0–1／1）；約每 0.5 秒一個 33ms 幀 15／14→0／1；p＝0.02 8／8→2／2；穩定 59.94／60／75–240 FPS 結果與 `d74830c` 相同；冒煙重播重設數不變 | 見下 | scratch 原型＋模擬（以守門 v2 為基礎） |
| B′ m2 | 同 B，但窗口內需 ≥2 個長間隔 | N＝12–16 與 m1 相同；N≥18 開始有切換，N＝20–30 重設略多（RTT 20 最多 4 次，m1 為 0–1） | 不優於 m1 | scratch 原型＋模擬 |
| C 慢幀後撤回到 epoch 結束 | 出現一個慢幀（例如 ≥1.5 Tick）後，該 epoch 不再套用 | 推論：避開 S1 重設 | 一次掉幀就失去整個 epoch（可能整條生命）的收益，比 B 更保守 | 未原型、未模擬 |
| D 調整 starvation fuse | 例如容忍單次 fallback | — | fuse 是固定政策，牽動所有恢復路徑；RTT 20／40 時 starvation 重設也是 stall reseed 取消對齊後重新量測的實際路徑（[02](./02-a1-cancelled-by-stall-reseed.md)） | 未評估 |

B（m1）相對現行 v2 的改動與代價（模擬）：

```cpp
constexpr double MissedRefreshSeconds = 1.5 * MovementTickSeconds;   // K = 1（m1）、2（m2）
// missed = 窗口內 >= MissedRefreshSeconds 的間隔數
// Reconcile 決定：startPhaseWithdrawn_ = mean > cut || missed >= K;
// Advance 未撤回：withdrawn = mean > cut || missed >= K;
// Advance 恢復條件：mean <= restore && missed == 0（再加 32 幀 dwell）
```

- 等於把「不對齊」從 cut 以下擴大到偶發掉幀，是政策變更：S2 全撤回（N＝12、RTT 20 中位 39.2→47.3ms，即未對齊值）；
  p＝0.02 套用時間 96–98%→28–31%、每秒切換約 0.6 次；約每 2 秒一個 33ms 幀時套用 47%、每秒切換 1 次。
- 1.5 Tick＝25ms 是絕對時間：75Hz 顯示掉一個 refresh（26.7ms）也算；75Hz p＝0.10 幾乎全撤回
  （RTT 20 中位 41.3→56.3ms），而 v2 在該處零重設（Held 0.6–0.7%）。採用前需另行處理。
- 冒煙重播套用時間 75.7–100%（v2 96.6–100%）。
- `PredictionTests.cpp:1229` 測例中兩個斷言（250ms 卡頓、16 幀掉 1 個仍對齊）編碼現行政策，需一併改；
  S2 釘選測例亦需改寫。原型只在 v2 上跑過，未在 v3 上驗證。

### 若日後恢復處理（建議，非承諾）

1. 先請使用者做政策決定：cut 以上的偶發掉 refresh 與 55–58 FPS vsync，要「對齊並接受偶發重設」、
   「不對齊並放棄收益」，或維持現狀（由啟動時機決定）。
2. 若選不對齊：以 B（m1）為起點在 v3 上重做；處理 75Hz 誤判；改上述斷言與 S2 釘選測例；
   補 75Hz／144Hz 掉 refresh 與 1.055 Tick 節奏的測例；重跑全套與突變檢查。
3. 若選對齊：維持現狀；需要時再評估 A／D。
4. 不論哪個，先確認實機發生頻率：需要一台會掉 refresh 的 60Hz vsync 環境，
   用 `Round N movement` 行分出 starvation 與 ≥2 Tick 間隔。

## 驗證

- 本問題未修正，沒有修正驗證。
- 現行行為由「如何復現」A 的四個測例釘選，屬於 `gyo_object_fps_pvp_tests`
  （133 cases／1,418,939 assertions 全通過；CTest `object_fps_pvp.cpu`；`ctest -L pvp` 16／16）。
- 數字來源：review 模擬（`rv2/out/`）；S2 的 17 次重設／108 個 Held 為實作者在釘選測例設定下的計數。
  B 段暫時測例未執行。
- 實機：三次 report-only GUI 冒煙（不計次、不作為驗收證據）量測期間 Host 移動重設 0、≥2 Tick 間隔 0，
  且移動方未對齊；這不能證明 S1／S2 不存在，只說明本機沒有觀察到。

## 殘留風險與後續

- 會掉 refresh 的 60Hz vsync 顯示（包括其他機器）上，對齊啟動可能偶發 starvation 重設；
  計次 GUI 輪若在這類環境執行，可能因此失去乾淨跑次資格。
- S2 讓同一環境的結果隨啟動時機變化；跨輪比較可見延遲時，要看每輪量測窗口的
  `shift_applied_seconds`／`unaligned_seconds`（[07](./07-start-phase-diagnostics.md)）。
- S4：smoke-3 的節奏貼近恢復線；較慢的機器或負載下，撤回後可能長時間不恢復。可補 1.055 Tick 節奏測例（review 建議，未做）。
- `Movement.hpp:44–46` 的註解可補一句「前 9 個計數間隔內掉 refresh 會撤回，N≤31 時該 epoch 不再恢復」
  （review 建議，未做；屬產品碼變更，不在本文件範圍）。
- S3 追趕上限重算 Engine 算式（code smell），見上。
