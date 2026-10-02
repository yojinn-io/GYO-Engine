# 07 啟動相位的觀測與記錄（w、調整量、撤回、取消）

狀態：已解決（工作樹修正，尚未提交）。2026-10-02。Owner：`object_fps_pvp`。
相關：A1 由 PR #2（合併為 `ff11ee3`）引入；本修正在提交前的工作分支
`claude/pvp-v5-start-phase-guard`，尚未 commit／開 PR。
第03批仍未結案：本文件只解決「能記錄、能判讀」，計次 GUI 輪次與 25 案矩陣依使用者決定未執行。
索引見 [本目錄總覽](README.md)；相關問題見
[01 低幀率守門](./01-a1-low-fps-regression.md)、[02 停頓重新播種取消A1](./02-a1-cancelled-by-stall-reseed.md)。

## 問題成因

**1. 交接的要求沒有工具可做。**
`ff11ee3` 的 HANDOFF 要求結案前重跑 GUI 短測，「並在結果記錄每輪的w與相位調整量」。
當時的專用驗收器（`build/acceptance/object_fps_pvp/`）沒有任何地方讀取 Host 的
`PlayerState.epochStartWaitMicros` 或 Client 的 `LocalMovementObservation::startPhaseShiftSeconds`
（`git grep epochStartWait ff11ee3 -- build/acceptance/` 為空）。
GUI probe 只輸出延遲、命令與視窗證據；一輪通過或失敗時說不出 w 多少、有沒有對齊。

**2. 只記「第一次決定」會與實際相反。** A1 決定之後狀態還會變：

- 低幀率守門（[01](./01-a1-low-fps-regression.md)）在最新 32 幀平均幀期 >1.1 Tick 時撤回已武裝的調整量，
  回到 ≤1.06 Tick 並維持 32 幀才恢復；在切點以下做的決定可能稍後才武裝。
- stall reseed（`LocalPlayerPrediction::SeedLead`）清掉 pending 或已武裝的相位，
  該 epoch／life 其餘時間不再對齊（[02](./02-a1-cancelled-by-stall-reseed.md)）。
- `ff11ee3` 的 observation 沒有原因欄位。守門 v1／v2 加了 `startPhaseSkip`（HostLate／FrameRateBelowTick），
  但 reseed 後仍保留舊的 `startPhaseShiftSeconds`；pending 被取消時則永遠停在「未決定」。

第一版記錄器只存第一次決定；第二版加了狀態歷史，但產品仍說不出「被 reseed 取消」。

**3. 實例（不計次的 report-only 冒煙；移動方為 create）。**

| 冒煙 | 產品／記錄器 | 原摘要 | 以目前讀取器重新分析 |
|---|---|---|---|
| smoke-1 | 守門 v1＋第一版 | create `shift_armed`：Host w 2.606ms、調整 −0.912ms | 決定後 0.533s 被 stall reseed（中立序號 42 起）取消，比量測開始早 1.825s；16s 量測全程未對齊 |
| smoke-2 | 守門 v2＋第二版 | create `not_armed_or_invalidated`，量測期間 `undecided`；另有一筆 `player_id 0` 的 `host_wait_absent` | 決定前即被 reseed（序號 7）取消，早 2.192s；量測期間對齊 0.000s／未對齊 16.000s；player 0 紀錄另列排除 |

`player_id 0` 的來源：GUI probe 用 `Update` 之前的連線狀態配對 Client 觀察；
同一幀的 `Update` 可能剛加入並啟用預測，這一幀就被記到尚無 id 的玩家。

**4. 失敗輪無法歸因。** 摘要沒有 Host movement 重設的原因，也沒有各角色量測期間的幀間隔，
分不出是低幀率、單次卡頓還是網路。

## 影響

- 交接要求的「每輪 w 與調整量」無法交付，結案證據不完整。
- 錯誤的 `shift_armed` 會讓人以為「已對齊仍只有 47.4ms」，據此誤判 A1 收益或去調門檻；
  實際上三次 GUI 冒煙的移動方都未對齊（[02](./02-a1-cancelled-by-stall-reseed.md)）。
- 只影響診斷與證據判讀：記錄器與讀取器不改任何延遲或玩法判定；產品新增的
  `StartPhaseSkip::CancelledByReseed` 只是 observation 欄位，行為與守門 v2 逐位元相同；wire／契約不變。
- 舊證據不改寫：smoke-1／2 原 `summary.md` 保留，更正結果另存 `reanalysis-integration-smoke-1／2/`。

## 如何復現

### A. 舊紀錄說反話（重新分析；不需顯示器／GPU）

需求：本機保存的證據（git-ignored，只在本機）
`build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/`；Python 3（本機 3.14）。
`presentation_evidence.py` 與 `command_evidence.py` 會把結果寫回目標目錄，**先複製**，不在原證據上執行。
從 repository root 執行：

```bash
E=build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002
W=$(mktemp -d)
grep 'start phase' "$E/integration-smoke-1/summary.md"   # 原摘要：create shift_armed
cp -R "$E/integration-smoke-1/round-1" "$W/smoke1"
cp -R "$E/integration-smoke-3/round-1" "$W/smoke3"
python3 build/acceptance/object_fps_pvp/presentation_evidence.py --short "$W/smoke1" > "$W/smoke1.json"
python3 build/acceptance/object_fps_pvp/presentation_evidence.py --short "$W/smoke3" > "$W/smoke3.json"
python3 build/acceptance/object_fps_pvp/command_evidence.py --report-only "$W/smoke3" > "$W/smoke3-commands.json"
python3 - "$W" <<'PY'
import json, sys
W = sys.argv[1]
for name in ("smoke1", "smoke3"):
    for role, phase in json.load(open(f"{W}/{name}.json"))["start_phase"].items():
        e = phase["measured_epoch_record"]; w = e["measurement_window"] or {}
        print(name, role, e["status"], e.get("recorded_status"), (e["cancelled_by_reseed"] or {}).get("source"),
              w.get("status"), w.get("shift_applied_seconds"), w.get("unaligned_seconds"))
c = json.load(open(f"{W}/smoke3-commands.json"))
print(c["reset_reasons"], c["presentation_frame_intervals"]["roles"]["create"])
PY
```

預期（2026-10-02 在複本上確認）：

| 跑次／角色 | status | recorded_status | 取消來源 | 量測期間 |
|---|---|---|---|---|
| smoke1 create | `cancelled_by_reseed` | `shift_armed` | `client_trace_fallback`（序號 42，決定後 0.533s） | `not_recorded`（舊記錄器無狀態歷史），另標量測前已取消 |
| smoke1 join | `shift_armed` | — | — | `not_recorded` |
| smoke3 create | `cancelled_by_reseed` | — | `product`（第 12 幀，決定於第 9 幀） | 對齊 0.000s／未對齊 16.000s |
| smoke3 join | `cancelled_by_reseed` | — | `product`（第 8 幀，決定前） | 對齊 0.000s／未對齊 16.000s |

smoke-3 的 `reset_reasons` 為空（0 次）；create 量測期間 908 個幀間隔、>1.1 Tick 165 個、≥2 Tick 0 個。

完整 `summary.md` 行由 `run_timing.py` 在跑次結束時產生（`start_phase_note`／`movement_note`）。
保存的 `$E/reanalysis-integration-smoke-1/`、`-2/` 是以目前讀取器對複本重跑
`run_timing.execute_rounds` 的結果（臨時腳本，未納入 repo），可直接與原 `summary.md` 對照。

### B. 單元層級的「舊產品」行為

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_start_phase_record_tests --parallel 2
build/target/_build/test/tests/object_fps_pvp/gyo_object_fps_pvp_start_phase_record_tests \
  -tc='Start-phase record names a reseed cancellation of an armed shift only when the product reports it'
python3 build/acceptance/object_fps_pvp/test_presentation_evidence.py \
  -k test_older_product_reseed_after_the_decision_is_cancelled_from_the_client_trace
```

前者以真實 `LocalPlayerPrediction` 製造 stall reseed。目前工作樹的產品已有 `CancelledByReseed`，
所以它走 `ReportsCancellation(...)` 分支，斷言 `cancelled_by_reseed`（修正後行為）；
斷言紀錄仍讀 `shift_armed` 的舊行為分支，只在對著沒有 `CancelledByReseed` 的產品
（守門 v1／v2 或 `ff11ee3`）建置時才執行。後者以合成 trace 重現 smoke-1 型誤判，並驗證 fallback 的更正。
因此在本工作樹上，舊誤判只由後者與 A 節重現。

### C. 產生新紀錄（實機 GUI；不計次）

需求：有顯示器與 GPU（macOS 為 Metal；本機為 Intel MacBook Pro 2019、Xcode 26.6＋Metal toolchain）；
Client／Gateway／Match／probe 與部署資產為同一批建置；閒置機器、一次一輪，兩視窗不操作、不遮蔽。

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_gui_probe --parallel 2
OUT="build/target/_build/test/logs/pvp-v5-start-phase-smoke-$(date +%Y%m%d-%H%M%S)"
python3 build/acceptance/object_fps_pvp/run_timing.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$OUT" --gui --short --report-only --rounds 1 --duration 16 --events 20 --fps 60
grep -E '^Round 1 (start phase|movement):' "$OUT/summary.md"
ls "$OUT"/round-1/*-start-phase.json
```

`--report-only` 只標示、不判門檻；這是冒煙，不計次、不作為驗收證據。
計次輪須依 [06](./06-gui-window-interference-and-rerun-rule.md)（視窗干擾與補跑規則）事前宣告總輪數與補跑規則。

## 解決方案

三層各有 owner，全部屬於 `object_fps_pvp`；Engine 與公共 Gateway 不變。

### 1. 產品：observation 跟上每個狀態變化

| 位置 | 內容 |
|---|---|
| `include/RetroFPS/Pvp/LocalPlayerPrediction.hpp` | `enum class StartPhaseSkip { HostLate, FrameRateBelowTick, CancelledByReseed }`；`LocalMovementObservation::startPhaseSkip` |
| `src/Pvp/LocalPlayerPrediction.cpp` `Reconcile`（決定處） | 取 w 時寫 `epochStartWaitSeconds`；w > `MovementStartPhaseMaximumWaitSeconds`（1 Tick＋2ms）記 `HostLate`；切點以下的決定記 `FrameRateBelowTick`；否則寫 `startPhaseShiftSeconds` ＝ w＋首窗口已過時間－4ms |
| 同檔 `Advance`（撤回／恢復） | 撤回時清 shift、記 `FrameRateBelowTick`；恢復時寫回 shift、清原因 |
| 同檔 `SeedLead` | pending 或已武裝時清 shift、記 `CancelledByReseed`；已取的 w 保留；HostLate epoch 沒有武裝，維持 `HostLate` |

- 純診斷：控制流不讀 observation；加入 `CancelledByReseed` 後行為與守門 v2 逐位元相同。
- `Reset()` 清空所有原因。

### 2. 記錄器：`build/acceptance/object_fps_pvp/start_phase_record.hpp`

`StartPhaseRecorder`，header-only，只由本產品的 probe 與其測試 include。

- 一筆紀錄＝（`player_id`, `movement_epoch`, `life_generation`）；最多 64 筆（`Capacity`，事先 reserve，
  迴圈內不配置），超出只計 `dropped_observations`。
- 每幀 `Observe()`：Host 端取 probe 自己 Snapshot 中該玩家的 `epochStartWaitMicros`
  （首次值、幀、steady_ns；之後出現不同值計 `host_wait_conflicting_frames`）；
  Client 端記第一次決定（w／shift／原因）、第一次武裝、第一次取消、最後狀態、撤回幀數、
  `withdrawals`／`restorations`，以及最多 32 筆狀態變化（`StateChangeCapacity`；超出仍計 `client_state_changes_dropped`）。
- 量測迴圈內只比較與複製；JSON 在量測結束後才建立。
- 以 `requires` 偵測欄位，不猜：產品沒有欄位 → `supported:false`；沒有原因 → `skip_reason_supported:false`；
  原因不能表達取消 → `cancel_reason_supported:false`。
- `PendingFrameAllowance = 8 + 2`：Client 啟用不超過 10 幀仍未決定，算「還在收集幀間隔」。
- 不歸屬 player 0：`gui_main.cpp` 改用 `Update` 之後的連線狀態配對；仍無 id 的幀只計 `unattributed_client_frames`。
- 呼叫端：`gui_main.cpp`（延遲 GUI probe；量測後寫 `<role>-start-phase.json`，report 加 `start_phase_record=`）、
  `gameplay_action.hpp`（矩陣 action probe；每個 Session 一份，寫入 `action-client.json` 的 `clients[i].start_phase`）。

Client 每幀狀態（`client_state_changes`）：

| 狀態 | 意義 |
|---|---|
| `undecided` | 尚未取 w |
| `shift_armed` | 調整量已套用；唯一算「對齊」的狀態 |
| `withdrawn_below_cut` | 曾武裝，現因低於切點撤回 |
| `skipped_below_cut` | 在切點以下決定，尚未武裝 |
| `skipped_host_late` | Host 遲到，不採用 |
| `cancelled_by_reseed` | stall reseed 取消；該 epoch／life 終局 |
| `skipped_unknown` | 未設調整量且原因無法辨識 |

每筆紀錄的 `status`（`start_phase_evidence.START_PHASE_STATUSES`）：

| status | 判定 |
|---|---|
| `shift_armed` | 已武裝，觀察期間從未撤回 |
| `shift_withdrawn_below_cut` | 武裝後曾撤回；看 `withdrawn_frames`、撤回／恢復次數、`last_client_state` |
| `shift_armed_after_below_cut_decision` | 切點以下決定，之後才武裝 |
| `skipped_frame_rate_below_tick` | 切點以下決定，從未武裝 |
| `skipped_host_late` | Host 遲到 |
| `cancelled_by_reseed` | 產品回報（或舊產品由 trace 推得）被 stall reseed 取消；終局 |
| `pending_frame_window` | 有 Host w，Client 啟用 ≤10 幀仍在收集幀間隔 |
| `not_armed_or_invalidated` | 有 Host w，但從未取用 |
| `host_wait_absent` | 沒看到 Host w |
| `rejected_or_skipped_unknown` | 有決定但原因無法辨識 |

### 3. 讀取器與摘要

- `start_phase_evidence.py`（GUI 延遲與玩法讀取器共用，兩者互不依賴）：`summarize_start_phase` 檢查型別，
  拒絕 NaN／Infinity／溢位數字；缺檔、壞檔、舊產品分別為 `absent`／`invalid`／`unsupported`，
  不拋例外、不變成數值；不改任何延遲或玩法判定。
- 量測的 epoch（`presentation_evidence._measured_epoch`）：create 取 `latency-events.csv` 第一個事件的
  `movement_epoch`；join 取量測開始時 `join-presentation.csv` 的 `local_epoch`。
- 量測窗口（`measurement_window`，計畫的量測區間）：每個狀態從首次出現持續到下一個變化，
  最後一個到最後觀察幀。`shift_applied_seconds` ＝ `shift_armed` 的秒數；
  `unaligned_seconds` ＝ 其他所有觀察到的狀態；兩者和為 `observed_seconds`，
  Client 有未觀察到的時段時小於 `window_seconds`。
  窗口狀態：單一狀態名／`mixed`／`withdrawn_below_cut`／`cancelled_by_reseed`／`client_not_observed_in_window`；
  舊記錄器為 `not_recorded`，狀態歷史被截斷為 `state_history_truncated`。
- `run_timing.py` 摘要每輪兩行：
  - `Round N start phase:`：每角色量測 epoch 的 status、Host w、Client w、shift、原因、撤回、
    取消來源與距量測開始秒數、量測期間對齊／未對齊秒數。
  - `Round N movement:`：Host movement 重設次數與原因（trace `reset_reason`：
    `backlog`／`starvation`／`sequence_exhausted`／`life_respawn`；schema 1 沒有原因，記 `unrecorded`），
    及各角色量測期間幀間隔 count／median／p95／max／>1.1 Tick（`START_PHASE_FRAME_CUT_SECONDS`＝18.33ms）／≥2 Tick
    （`command_evidence.frame_interval_statistics`）。
- 玩法矩陣：`gameplay_evidence.start_phase_by_player` 逐 Session 摘要；沒有延遲計畫，量測欄位為明確 null。

### 4. 舊產品的 trace fallback

只在紀錄的 `cancel_reason_supported` 不為 true 時使用，讀同角色的 `<role>-commands.jsonl`
（矩陣為共用的 `clients-commands.jsonl`）：

- stall reseed ＝ 同（player, epoch, life）一段連續 `seeded_neutral` 的 generated 事件，
  且序號 > `INITIAL_COMMAND_LEAD`（2）；epoch 開始的 seed 不算。
- 已決定的紀錄取決定幀之後第一個 reseed；未決定的取第一個 reseed。
- 摘要中 `status` 改為 `cancelled_by_reseed`，原值保留在 `recorded_status`，
  `cancelled_by_reseed.source` ＝ `client_trace_fallback`；原始 JSON 不改寫。
- `runtime_gap` 事件不單獨使用：被覆蓋的幀間隙會丟時間但不 reseed，已武裝的調整量仍保留。

已知與 v3 產品回報不一致的三種情形（只影響舊證據的重新分析；新產品不走 fallback）：

| # | 情形 | v3 產品 | fallback |
|---|---|---|---|
| 1 | 已決定為 HostLate 的 epoch 之後發生 reseed | 維持 `skipped_host_late` | 改判 `cancelled_by_reseed` |
| 2 | 首窗口後丟棄時間（`LocalPlayerPrediction.cpp` `Advance` 約 304 行）已靜默取消 pending，之後才 reseed | 不回報原因，讀作 `not_armed_or_invalidated` | 歸因於該 reseed |
| 3 | epoch 開始時從 `lastResolvedCommand` > 0 播種，送出序號 > 2 | 不是 reseed，也不建立 pending | 誤判為 stall reseed |

因此新舊證據不可當作等價比較。

### 設計取捨

| 做法 | 處理 |
|---|---|
| 只記第一次決定（第一版記錄器） | 已取代：撤回、恢復、取消後仍報 `shift_armed`，smoke-1 說反話 |
| 只靠 trace 推論取消、不改產品 | 只留作舊證據的 fallback：有上表三種不一致；產品自己知道狀態，回報最直接 |
| 量測迴圈內組 JSON 或寫檔 | 不做：迴圈內只比較與複製，避免增加每幀工作、干擾量測時序 |
| 欄位不存在時推測 | 不做：明確寫 `supported`／`skip_reason_supported`／`cancel_reason_supported` 為 false |

Architecture：新增 CTest `object_fps_pvp.start_phase_record`（標籤 `cpu;pvp;acceptance`），
以真實 `LocalPlayerPrediction` 驅動記錄器；獨立 target，避免 domain 測試 target 多出 JSON 依賴。
產品 observation 多一個欄位；wire、契約、Engine、公共 Gateway 不變；移除產品時一併移除。

## 驗證

- C++ 記錄器：`object_fps_pvp.start_phase_record` 13 cases／524 assertions 通過
  （`build/target/_build/test/logs/pvp-v5-batch03-integration3-20261002-015053/start-phase-record.log`）。
  涵蓋：每 epoch 一次武裝與首幀時間、HostLate、pending 與未武裝的區分、撤回與恢復、切點以下的決定、
  每玩家／epoch／life 分開、原因映射、狀態歷史上限、不支援的產品、容量上限、
  產品回報的取消（終局）、舊產品不回報取消、player 0 不歸屬。
- 產品：`gyo_object_fps_pvp_tests`（133 cases）中的
  `PvP a stall reseed cancels an armed or withdrawn start phase without moving the phase`、
  `PvP a stall reseed inside the frame window cancels the pending start phase and the next epoch starts afresh`、
  `PvP start phase skip reasons are cleared by Reset`。
- Python：`test_presentation_evidence.py`（start_phase／reseed／window_state／unattributed 相關 9 項）、
  `test_run_timing.py`（摘要行 4 項）、`test_command_evidence.py`（重設原因與幀間隔）、
  `test_gameplay_evidence.py`（逐 Session 與共用 trace 的取消歸屬）。
- 整體：CTest `-L pvp` 16／16、Python 165 項通過（同上 integration3 目錄）。

```bash
T=build/target/_build/test/tests/object_fps_pvp
$T/gyo_object_fps_pvp_start_phase_record_tests -tc='Start-phase record*'
$T/gyo_object_fps_pvp_tests -tc='PvP a stall reseed*,PvP start phase skip reasons*'
python3 build/acceptance/object_fps_pvp/test_presentation_evidence.py -k start_phase -k reseed -k window_state -k unattributed
python3 build/acceptance/object_fps_pvp/test_run_timing.py -k start_phase -k reseed -k withdrawal
python3 build/acceptance/object_fps_pvp/test_command_evidence.py -k reset
python3 build/acceptance/object_fps_pvp/test_gameplay_evidence.py -k start_phase
ctest --test-dir build/target/_build/test -L pvp --output-on-failure
```

不計次冒煙（report-only；不計次、不作為驗收證據）：

- smoke-3（守門 v3＋目前記錄器）：產品自己回報取消。create：Host w 7.913ms、調整 13.379ms、
  第 9 幀決定、第 12 幀取消；join：Host w 3.596ms、第 8 幀在決定前取消；
  兩者量測期間對齊 0.000s／未對齊 16.000s，比量測開始早約 1.8–1.9s。
- 矩陣單案 `upstream-250ms`：兩個 Session 的紀錄皆為 `shift_armed`（來源 product）。
- smoke-1／2 以目前讀取器重新分析，結果見「如何復現」A 段。
- 撰寫本文件時（2026-10-02）在複本上重跑 A 段命令與上列篩選測試，結果與保存紀錄一致。

## 殘留風險與後續

不影響「已解決」；列出以免誤讀：

- trace fallback 與產品的三種不一致（上表）只影響舊證據；`TRACE_FALLBACK_SCOPE` 說明文字尚未列出這三點。
- 同一幀內「決定」與「reseed」：記錄器看不到該決定（`client_wait_seconds` 為 null）；
  同一幀內 Reset＋reseed 也看不到，產品未回報取消時讀作 `not_armed_or_invalidated`。
- 首窗口後丟棄時間使 pending 靜默失效，產品不給原因，讀作 `not_armed_or_invalidated`。
- 調整量的 slew（每幀最多經過時間的 25%）是否完成不可觀測。
- 說明文字仍寫「below 60 FPS」（`run_timing.SHIFT_ARMED_NOTE`、記錄器 `status_scope`），
  實際切點約 54.5 FPS（1.1 Tick）；只是文字，不影響判定。
- `gui_main.cpp` 每幀多一次 `ClientConnection::State()`（mutex＋快照複製），成本小但增加量測迴圈工作。
- 證據目錄未進 git；`build/` 計畫移出工作區，搬移時須保留 `pvp-v5-start-phase-evidence-20261002/`。
- 本文件只提供工具。若恢復結案，計次輪仍須實際執行並記錄每輪 w／調整量／撤回／取消，
  見 [02](./02-a1-cancelled-by-stall-reseed.md) 與 [06](./06-gui-window-interference-and-rerun-rule.md)；目前未排程，不代表承諾。
