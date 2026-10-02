# 01 A1 在低幀率（30／40 FPS）造成 Held 與重設

狀態：已解決。2026-10-02。Owner：`object_fps_pvp`。
**2026-10-02更新**：本守門已隨A1一起由持續相位追蹤取代並刪除（見[08](./08-a1-clock-drift.md)）；token bucket worker使30／40 FPS對齊後不再Held，本文保留作為歷史紀錄。
相關：A1 由 PR #2（`claude/project-thread-lv6bg5`，`d74830c`，合併為 `ff11ee3`）引入；
本修正（防護 v3）經 PR #3（`claude/pvp-v5-start-phase-guard`）合併為 `9cd7f26`。
第03批仍未結案：計次GUI可見延遲輪次與25案矩陣依使用者決定未執行（MVP技術驗證）；
本修正不代表結案，第04批維持暫停。
另見 [第03批計畫](../03-v5-gameplay-and-delivery.md)、[交接](../HANDOFF.md)、
[v5契約](../../../protocol-v5.zh-Hant.md)、[A1 dev_log](../../../../dev_logs/2026_10_01_pvp_v5_start_phase.zh-Hant.md)、
[02 停頓重新播種取消A1](./02-a1-cancelled-by-stall-reseed.md)、[修正索引](./README.md)。

## 問題成因

A1：Host 回報 epoch 首窗口「收到→執行 Tick」等待 w，Client 把固定步相位移
w＋首窗口已過時間－4ms（`MovementStartPhaseTargetSeconds`），之後每個命令到 Host
時只留約 4ms 餘裕。60 FPS 以上每幀至多一個固定步，這個 just-in-time 設計成立。

30 FPS 時每幀跑兩個固定步：

- 同一幀產生、發布、送出兩個命令（n、n+1）；Host 每 Tick 執行一個。
- 相位調整只能落在整幀：命令被移到較晚的一幀產生。
- 每對中較舊的命令（n）到達餘裕約為 q1＋w－Q。q1：首窗口的「產生→送出」延遲；
  Q：該命令自己的「產生→送出」延遲。
- Q 由 worker 發送相位與幀時鐘的相對位置決定。兩者只要相對漂移，Q 就會掃過一個
  發送週期；Q＞q1＋w 時命令 n 錯過它的 Tick → Host 以 Held 替代，連續缺命令再觸發
  starvation 重設。w 4–8ms 時餘裕最薄。

產品中確實存在的漂移來源：

| 來源 | 機制 |
|---|---|
| worker 重定基準 | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp:525–527`：`nextInputSendAt=sentAt+period`，每次以實際送出時刻加週期，喚醒延遲逐次累積；實機（本 Mac）量測每次＋0.33–0.37ms（58.7–58.8Hz 而非 60Hz）。只在完整 ACK（`:471–476`）或 epoch 變更（`:464–466`）時清除 |
| 幀時鐘漂移 | 顯示時鐘對 steady clock 的 ppm 偏差；模擬＋1000ppm 即可觸發 |
| 幀節奏抖動 | 每幀 ±0.5–2ms 即產生同樣效果 |

CI／原 CPU 模擬沒抓到的原因：

- 原模擬 worker 相位鎖定：準時在 nextSend 醒來並以該時刻為基準，發送格點永不漂移，
  q1 與 Q 固定，所以 30 FPS「全部 Actual」。
- 2026-10-01 dev_log「鎖頻幀率下…模擬中全部Actual」只在此假設下成立，已被推翻；
  該 dev_log 的乾淨模擬基線也退化：30 FPS 實際收益約 21ms，而非 29ms。
- 現行模擬（`tests/object_fps_pvp/MovementRecoveryTests.cpp`）已補 `ClockOptions`／
  `ProductClock`：worker 每次喚醒 0.175–0.525ms overshoot 並以實際送出重定基準
  （`:201–204`、`:385–394`）、幀漂移 ppm、幀抖動、vsync refresh pattern、隨機掉
  refresh、每 N 幀掉一個 refresh、啟動卡頓。

## 影響

模擬，產品式 worker，每列 36 次啟動×10 秒量測：

| 幀率 | A1（無防護）Held | 未對齊 |
|---|---|---|
| 30 FPS，RTT 20／40 | 10–14%（最差單次 30–36%）＋starvation 重設 | 0% |
| 30 FPS，依 w 分桶 | w 4–8ms：21–33% | 0% |
| 30 FPS，RTT 0（loopback） | 多數 0；144 次中 1 次 Actual＜99% | 0% |
| 40 FPS | 約 1.7% | 0% |
| 45 FPS | 約 0.14% | 0% |
| ≥50 FPS | ≈0 | 0% |
| 60／144 FPS | 不受影響 | — |

- 受影響：鎖在 30／40 FPS，或 vsync 掉到約 45–54 FPS 的 Client，且 RTT 20／40ms。
  症狀是該 Client 的移動命令被權威以 Held 替代（本機預測被修正）與 epoch 重設。
- 不受影響：60 FPS 以上（含 59.94Hz、144 FPS）、Host 計時、wire 格式。
- 第03批 GUI 守門以名目 60 FPS 量測，不直接觸發；但本 Mac GUI 實際約 57–58 FPS，
  這決定了 cut 不能設在字面 60（見下）。
- 實機未重現：GUI harness 要求名目 ≥60 FPS（`run_timing.py --gui` 拒絕 `--fps` 30）；
  回歸需 RTT 20／40，loopback 在模擬中也幾乎不出現。實機證據只有 worker overshoot
  量測（臨時探針，未保留於證據目錄）與 GUI 幀節奏。

## 如何復現

全部是 CPU 模擬，任何平台皆可，不需 GPU／GUI。先在 repository root 以
`cmake --preset test` 設定 `build/target/_build/test`。

### A. 現行防護的回歸測例（工作樹，防護 v3）

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests
build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_tests \
  -tc="PvP below about 54.5 FPS the start phase stays unaligned under drifting product clocks"
ctest --test-dir build/target/_build/test -R '^object_fps_pvp\.cpu$' --output-on-failure
```

該測例：穩定 30／40 FPS、vsync 50／48／45 FPS pattern、隨機掉 refresh 50／46 FPS，
×RTT 0／20／40×三個 worker 相位×六個 Authority 相位；斷言 cut 以下從不套用調整、
Held／延遲／重設與未對齊相同。其餘相關測例見「驗證」。

### B. 拿掉防護看到回歸（臨時變異，不提交）

在 `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp` 最後一個 `#include` 之後暫時加入：

```cpp
#define MovementStartPhaseMaximumFrameSeconds 1e9 // SCRATCH: disable the A1 low-FPS guard
```

- 必須放在 include 之後；放在前面會替換 `Movement.hpp` 的常數名稱而無法編譯。
- 效果：cut 永不超過，任何幀率都套用調整（仍等 8 個間隔才決定，其餘即 PR #2 的 A1）。
- 重建後執行 A 的測例，預期 `shiftedBelowCut == 0`、`FrameRateBelowTick` 與
  Held／重設等同未對齊的斷言失敗；`PredictionTests.cpp` 的
  "PvP start phase waits for a full frame window and is not applied below about 54.5 FPS" 也會失敗。
  模擬的 cut 判定在測試端依文件定義獨立計算，不受此變異影響。
- 結束後手動刪除該行，或以 `git restore` 還原該檔（防護已隨 PR #3 提交）。
- 同一手法的舊版紀錄：證據目錄 `below60/scratch_src/LocalPlayerPrediction.cpp`
  （`GYO_A1_EXPERIMENT_NO_GUARD`）與 `below60/noguard_*.txt`（v1 時期）。

### C. 原始實驗（無防護的 `ff11ee3`＋worker 模型掃描）

實驗 diff 只改 `tests/object_fps_pvp/MovementRecoveryTests.cpp`（基底 blob `afed74b`，
即 `ff11ee3`／`d74830c`），產品碼不動；在獨立 worktree 套用，勿套在本工作樹：

```bash
git worktree add ../GYO-Engine-a1-exp ff11ee3
cd ../GYO-Engine-a1-exp
git apply <PR工作樹>/build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/a1-30fps-experiment.diff
cmake --preset test
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests
build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_tests -tc="SCRATCH A1*"
```

| 測例（`-tc`） | 內容 | 已存輸出 |
|---|---|---|
| `SCRATCH A1 sweep clean wire` | 30／60／144 FPS×RTT 0／20／40×worker 模型 | `scratch_all_final.txt` |
| `SCRATCH A1 sweep extra frame rates` | 40／50 FPS | 同上 |
| `SCRATCH A1 sweep jitter` | 封包抖動 2／10ms | 同上 |
| `SCRATCH A1 held versus start wait` | 依 w 分桶的 Held | 同上 |
| `SCRATCH A1 loopback held runs` | RTT 0 | 同上 |
| `SCRATCH A1 emulated frame-aware cap` | 整幀限制候選 | 同上 |
| `SCRATCH A1 frame drift` | 幀漂移＋1000ppm | `frame_drift.txt` |

worker 模型：`locked`＝原相位鎖定；`prodX`＝產品式，平均 overshoot X ms
（`prod0.35` 對應實機量測）；`grid1.0`＝deadline 固定格點；`new1.0`／`newcv1.0`＝
新窗口立即送出（／並喚醒）。掃描量大，耗時明顯長於一般 CPU 測試。

## 解決方案

只改 Client 預測（`LocalPlayerPrediction`）：量測幀率，低於 cut 不套用或撤回 A1 調整。
Host、wire、`ClientConnection`、Engine 不變；不新增依賴，不是 Architecture Delta。

### 設計（v3，現行）

| 項目 | 值／行為 | 位置 |
|---|---|---|
| 幀率估計 | 最近 32 個正幀間隔的簡單平均；每個間隔上限 2 Tick（`StartPhaseFrameIntervalLimit`）；至少 8 個（`StartPhaseFrameEvidence`）才決定 | `LocalPlayerPrediction.cpp` `FramePeriodSeconds()`、`Advance()` |
| cut | 平均＞`MovementStartPhaseMaximumFrameSeconds`＝1.1 Tick（18.33ms，≈54.5 FPS）即不套用／撤回 | `Movement.hpp:47` |
| 恢復 | 平均 ≤`MovementStartPhaseRestoreFrameSeconds`＝cut－0.04 Tick＝1.06 Tick（17.67ms，≈56.6 FPS），且維持整個 32 幀窗口（dwell） | `Movement.hpp:52–53` |
| 決定時機 | 首個 epoch 在 8 個間隔後（60 FPS 約 133ms）；之後的 epoch 立即決定（幀間隔跨 reseed 保留，`Reset()` 清除） | `Reconcile()` |
| 可逆 | cut 以下為「已武裝但撤回」；幀率回升後套用 | `armedPhaseShiftSeconds_`、`startPhaseWithdrawn_` |
| 撤回 | 沿用每幀 25% slew（`StartPhaseSlewFraction`），顯示不倒退；降到 30 FPS 約 4 幀內撤回，vsync 50 FPS 約 16–20 幀 | `Advance()` |
| 追趕上限 | 負向 slew 限制在 `FixedTickRuntime` 剩餘追趕容量（`CatchUpSteps`＝5）內，4–5 Tick 長幀中撤回不丟整步 | `Advance()` |
| 診斷 | `StartPhaseSkip{HostLate, FrameRateBelowTick, CancelledByReseed}`；`LocalMovementObservation::startPhaseSkip` | `LocalPlayerPrediction.hpp` |

- 簡單平均而非截尾：vsync 顯示掉的是整個 refresh，丟棄最長間隔正好看不到它。
- 上限 2 Tick：一次長卡頓只算一個掉 refresh；vsync 掉的每個 refresh 仍全數計入。
- 遲滯＋dwell：cut 附近不來回切換；低幀率中短暫的整幀片段（隨機掉幀）不會恢復調整。
- `CancelledByReseed` 只是診斷，行為與 v2 相同，見 [02](./02-a1-cancelled-by-stall-reseed.md)。
- 驗收器紀錄：`build/acceptance/object_fps_pvp/start_phase_record.hpp`／`start_phase_evidence.py`
  的 `shift_withdrawn_below_cut`、`shift_armed_after_below_cut_decision`、
  `skipped_frame_rate_below_tick` 與量測窗內 `shift_applied_seconds`／`unaligned_seconds`。

### 為何 cut≈54.5 FPS 而非字面 60

- 使用者決定「低於 60 FPS 時不對齊」；實作為 1.1 Tick，並經使用者明確確認。
- 實機：本 Mac GUI harness 中位幀長 17.18–17.6ms（≈57–58 FPS）；字面 60 會讓驗收中的 A1
  全部關閉。59.94Hz 顯示與一般抖動也落在 60 以下。
- 回歸只在一幀含多個固定步時出現，≥50 FPS 已≈0；10% 寬限保留 59.94Hz、一般抖動與
  約 58 FPS 的桌面幀。
- 恢復門檻須高於 GUI 約 1.03–1.06 Tick 的節奏；1.03 會在此 Mac 永不恢復（測例已釘住）。

### 迭代

| 版本 | 設計 | 結果 |
|---|---|---|
| v1 | 最近 8 間隔的 IQM＋鎖存 | 漏判 vsync 掉 refresh 的 48–54 FPS（RTT 20／40 每 18 次啟動 14–15 次重設，未對齊 0–1）；啟動卡頓鎖住整個 epoch；55 FPS 附近約每秒翻轉 9 次 |
| v2 | 32 間隔 clamp 平均＋可逆＋遲滯／dwell | vsync 45–54 FPS pattern 與隨機掉幀 p≥0.15 與未對齊相同；59.94／60／75–240 FPS 與 `d74830c` A1 逐位元相同；啟動卡頓後恢復（約 98% 時間套用）；以實機 GUI 記錄的幀間隔重播（模擬）0 額外重設 |
| v3 | v2＋`CancelledByReseed` 診斷＋追趕上限＋測例 | 診斷不改行為；追趕上限只影響 4 Tick 以上長幀中的撤回 |

修補檔：證據目錄 `a1-below60-guard.patch`（v1）、`-v2.patch`、`-v3.patch`；
v3 與目前工作樹的產品／測試 diff 相同。

### 不採用的方案

| 方案 | 原因 |
|---|---|
| 新窗口立即送出（`new1.0`／`newcv1.0`） | 模擬中消除 Held，但破壞獨立 60Hz 發送政策，75–100 FPS 時超過 120 pps 輸入上限 |
| deadline 固定格點（`grid1.0`） | 乾淨線路有效；幀漂移／抖動下不足（30 FPS RTT 20＋1000ppm：Held 3.12%，36 次 38 次重設） |
| 調整限制為整幀（frame-aware cap） | 收益大半消失（30 FPS RTT 0 中位數約 22→39ms），10ms 封包抖動下仍有 Held |
| 字面 60 FPS cut | 本機 GUI 驗收中 A1 全關，見上 |
| 截尾估計（IQM）／鎖存 | v1 的問題，見上表 |

不變的政策：lead 2、獨立 60Hz worker、60Hz 權威／snapshot、1 Tick 插值、4ms 目標、
starvation fuse、各門檻、120 pps、wire 格式（不改 proto）。

檔案：`apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp`、
`.../Pvp/LocalPlayerPrediction.hpp`、`apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp`、
`tests/object_fps_pvp/MovementRecoveryTests.cpp`、`tests/object_fps_pvp/PredictionTests.cpp`。

## 驗證

- `gyo_object_fps_pvp_tests`：133 cases／1,418,939 assertions 通過；CTest
  `object_fps_pvp.cpu` 7.87 秒（逾時 30 秒）；`ctest -L pvp` 16／16。
  紀錄：`build/target/_build/test/logs/pvp-v5-batch03-integration3-20261002-015053/`
  （`cpu.log`、`pvp-ctest.log`；當時為 `d74830c`＋防護 v3，與目前工作樹相同內容）。
- 變異測試全數被測例抓到：IQM-8、鎖存、無 clamp、無遲滯、無 dwell、恢復 1.03、窗口 16。
- 新增／修改測例：

| 檔案 | 測例 |
|---|---|
| `MovementRecoveryTests.cpp` | "PvP start phase alignment removes the start lottery at 60 FPS and above without losing Actual commands"（30 FPS 改為斷言 `FrameRateBelowTick` 且與未對齊相同） |
| 同上 | "PvP below about 54.5 FPS the start phase stays unaligned under drifting product clocks" |
| 同上 | "PvP an epoch armed during startup hitches aligns once frames recover" |
| 同上 | "PvP after startup hitches a 58 FPS GUI cadence restores the start phase within 64 frames" |
| 同上 | "PvP at 59.94 60 and 144 FPS the start phase still aligns under drifting product clocks" |
| 同上 | "PvP a 60 Hz display dropping one refresh in 12-30 frames keeps the start phase state it took" |
| `PredictionTests.cpp` | "PvP start phase waits for a full frame window and is not applied below about 54.5 FPS" |
| 同上 | "PvP start phase frame rate counts every dropped refresh and a long hitch as one" |
| 同上 | "PvP a drop below about 54.5 FPS withdraws the start phase shift mid-slew and restores it on recovery" |
| 同上 | "PvP a start phase armed during startup hitches is applied once frames recover" |
| 同上 | "PvP start phase near the cut keeps its state and a short recovery does not restore it" |
| 同上 | "PvP a withdrawal slew in 4.0-4.4 tick frames never makes the runtime drop a step" |
| 同上 | "PvP start phase skip reasons are cleared by Reset"；兩個 stall reseed 測例見 [02](./02-a1-cancelled-by-stall-reseed.md) |

- 三次本機 GUI 整合 smoke（report-only）中移動方的對齊都被 reseed 取消（[02](./02-a1-cancelled-by-stall-reseed.md)），
  未驗證到本問題；不計次、不作為驗收證據。30 FPS 實機未執行。
- 證據（git 忽略，僅本機，`build/` 預計移出工作區，須保留）：PR 工作樹
  `build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/` 的
  `a1-30fps-experiment.diff`、`scratch_all_final.txt`、`frame_drift.txt`、`below60/`、
  `rv2/`、`rv3/` 與三份防護修補檔。

## 殘留風險與後續

前兩項另列為已知問題（[03](./03-a1-missed-frame-starvation.md)），此處只摘要。

- 已知問題（使用者決定暫不處理）：cut 以上 A1 仍是 just-in-time。60 FPS 每 2 秒一個
  33ms 幀 → RTT 20／40 每 18 次 4–5 次 starvation 重設，未對齊 0；每 0.5 秒一個為 15／14
  對 0／1。實機 GUI 幀間隔重播（模擬）0。
- 60Hz 每 12–30 幀掉一個 refresh（55–58 FPS）：留在 cut 內；是否保留對齊取決於起始時機
  （前約 9 個間隔有掉幀即撤回，N≤31 時該 epoch 不再恢復，約 9.5／N 的啟動）；保留時
  重設與 `d74830c` A1 相同（RTT 20：76 次 17 次重設，未對齊 0）。
- 5 Tick 以上長幀：`FixedTickRuntime` 丟整步，相位差只在模一 Tick 意義下保留。
- GUI 節奏下恢復餘裕薄：smoke-3 平均約 1.056 Tick 對 1.06 門檻，約 12% 窗口可能維持撤回。
- cut 以下的 Client 回到 A1 前的未對齊延遲（30 FPS 失去約 21ms 收益），不比 PR #2 之前差。
- Code smell：追趕上限在產品碼內複製 `FixedTickRuntime` 預覽以重算 Engine 追趕算術；
  日後可考慮由 Engine 提供 accessor（屬 Engine 公開介面變更，需另評估，未做）。
- 若要實機確認 30 FPS：需非 GUI 真 socket＋RTT 延遲注入，或放寬 GUI harness 名目
  FPS 下限；皆未規劃。
