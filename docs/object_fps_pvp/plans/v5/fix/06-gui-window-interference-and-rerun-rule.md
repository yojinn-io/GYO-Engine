# 06 GUI 延遲測試的視窗干擾、視窗配置與補跑規則

狀態：已解決（驗收器修正與補跑規則已實作；2026-10-02第03批結案驗收首次在計次輪運用）。2026-10-02。Owner：`object_fps_pvp`。
相關：PR #2（A1，已合併為`ff11ee3`，不含本修正）；本修正經PR #3（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。
範圍：只改產品自有驗收器`build/acceptance/object_fps_pvp/`（GUI probe、分析器、runner）；Engine `SdlPlatform`、
產品Client／Gateway／Match、wire契約與延遲門檻都不變。撰寫時計次GUI短測未執行、補跑規則未在真實計次輪用過；
2026-10-02第03批結案驗收首次在計次輪運用（第3輪依本規則補跑一次），見「驗證」。索引與其他問題見[本目錄總覽](README.md)。

## 問題成因

三個相關缺口：觀察方視窗可能被完全蓋住；視窗干擾沒有紀錄；補跑沒有事前規則。

### 1. macOS上兩個probe視窗完全重疊

| 位置 | 行為 |
|---|---|
| `engine/platform/sdl/src/SdlPlatform.cpp:34` | `SDL_CreateWindow`只給尺寸（1280×720）不給位置；兩個GUI probe開在同一個平台預設位置（macOS為置中） |
| `build/acceptance/object_fps_pvp/gui_main.cpp:418` | 雙方入場後，移動方（`create`）寫完事件計畫即`SDL_RaiseWindow`，疊到觀察方（`join`）上方 |
| `build/acceptance/object_fps_pvp/presentation_evidence.py` `_analyze_latency` | 可見交越取自`join-presentation.csv`（觀察方畫面上的遠端位置），延遲以觀察方的呈現時間計 |

macOS會對完全被遮蔽的視窗節流（`gui_main.cpp` `LatencyWindow`註解）。兩窗同尺寸同位置時觀察方被完全蓋住，
量到的可見延遲可能包含節流，而不只是網路／預測／插值。這是由程式路徑推得的風險：
本機沒有保存修正前的重疊跑次，也沒有量化節流幅度。

### 2. 視窗狀態干擾沒有紀錄

- v4只在[手動指南](../../v4/MANUAL_ACCEPTANCE.md)要求「量測時讓兩個視窗持續呈現」，不要拖曳、切換、最小化或遮蔽；
  harness不記錄是否真的做到。
- 產品在`FOCUS_LOST`／`MOVED`／`RESIZED`／`MINIMIZED`時釋放滑鼠指標
  （`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:191`，`HandleNativeEvent`），這些事件會改變受測Client的輸入狀態。
- 結果：受干擾的一輪與真正的延遲退化，在證據上分不開。

### 3. 補跑沒有事前規則

沒有事前規則時，「這輪受干擾所以重跑」可以在看過延遲分數後才決定，等同挑分數；也違反既有
「失敗保留、不以另一次通過覆蓋」的原則（例：v4第05批GUI首輪P50 52.02 ms與外部建置重疊，仍記為未通過，
見[v4驗收狀態](../../v4/ACCEPTANCE_STATUS.md)）。

## 影響

- 受影響：probe延遲模式（`--latency`／`--latency-short`）的雙GUI量測，即`run_timing.py --gui`（含`--short`、`--combat`）
  與`run_weapon_short.py`的`latency`案例。門檻為每輪可見交越P50 ≤50 ms、P95 ≤80 ms（[第05批計畫](../05-short-validation-and-acceptance.md)）。
- 時機：第03批結案需要的計次GUI可見延遲輪。本機GUI中位幀間隔約17.2–17.6 ms（≈57–58 FPS）；
  三次不計次冒煙的可見P50為47.4／38.3／46.5 ms，兩次距50 ms只剩約3 ms，觀察方節流或一次外部操作就足以讓一輪越線。
- 誤判有兩個方向：受干擾輪被當成產品退化；或受干擾輪被事後挑掉、只留好分數。
- 不受影響：產品執行期（Client／Gateway／Match）、wire、Engine、延遲計算與門檻；CPU／headless測試；
  25案網路矩陣（`run_network.py`可選的`--gui-probe`走`--move`模式，不建立`LatencyWindow`，也不套gate）；
  `run_weapon_short.py`的`weapon30`／`weapon60`／`weapon144`／`capture`案例。
- 摘要表格的`Disturbed`欄仍是既有移動命令證據的干擾；視窗狀態另列在`Round N window:`行，兩者不混用。

## 如何復現

環境：實機、需顯示器與GPU；macOS需Xcode Metal工具鏈（`xcodebuild -downloadComponent MetalToolchain`）。
實測：Intel MacBook Pro 2019（x86_64）、macOS 26.7.1。機器保持閒置、一次只跑一輪GUI；在repo根目錄執行。
方法A為單元測試（模擬輸入），B–D為實機。

方法A（判定與gate，不需GUI，數秒）：

```bash
python3 build/acceptance/object_fps_pvp/test_run_timing.py -v -k window -k disturbed -k wayland
python3 build/acceptance/object_fps_pvp/test_presentation_evidence.py -v -k window
ctest --preset test -R '^object_fps_pvp\.(timing_runner|presentation_evidence)$' --output-on-failure
```

撰寫本文時（2026-10-02）在本機確認：前兩行分別選中8項與10項，全數OK。

B–D的前置建置：

```bash
cmake --preset test
cmake --build --preset test
cmake --build --preset test --target gyo_object_fps_pvp-gateway
```

方法B（視窗配置與事件計數；report-only冒煙，約25秒）：

```bash
PVP_WIN="build/target/_build/test/logs/pvp-v5-window-smoke-$(date +%Y%m%d-%H%M%S)"
python3 build/acceptance/object_fps_pvp/run_timing.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --output "$PVP_WIN" \
  --gui --short --report-only --rounds 1 --duration 16 --events 20 --fps 60
grep '^Round 1 window:' "$PVP_WIN/summary.md"
grep -H '^window_' "$PVP_WIN"/round-1/create-report.txt "$PVP_WIN"/round-1/join-report.txt
```

本機預期（座標隨顯示器與縮放設定而變）：

```text
Round 1 window: no window-state interference detected (create usable_bounds_top_left at (0, 62); join usable_bounds_bottom_right at (512, 400))
```

`round-1/round.json`的`presentation.window_overlap`是兩窗各自的可見比例（本機`0.6817`）。

方法C（干擾偵測，只限report-only）：執行方法B的命令，在量測期間（雙方入場約2秒後起，持續16秒＋結尾2秒）
做一件事：點選其他App、拖曳或最小化任一probe視窗，或把`join`完全蓋住。預期視窗行變成：

```text
Round 1 window: window interference (...): <role>: <N> OS <kind> event(s) during measurement -> report-only: flagged_window_disturbed, not enforced
```

report-only輪的`status`仍由門檻決定，只多一個標記。本次未實跑此方法；預期輸出依程式與單元測試
（`test_disturbed_window_events_are_flagged_without_changing_latency_verdict`、`test_window_gate_invalidates_only_counted_rounds`）。
不得在計次輪刻意干擾。

方法D（修正前的重疊；依程式推得，本次未實跑）：在另一個worktree checkout `ff11ee3`，執行
`cmake --preset test`與`cmake --build --preset test --target gyo_object_fps_pvp_gui_probe`，再以方法B的命令把`--probe`
換成該worktree的`build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe`。
預期兩窗同處置中、移動方在上；舊probe的報告沒有`window_*`鍵，視窗行為`unknown`，原因
`window evidence absent: ... has no window evidence (probe predates it)`，結尾`-> report-only: flagged_window_evidence_missing, not enforced`。
不要在目前的工作樹切換版本。

## 解決方案

只改產品自有驗收器。Ownership全在`object_fps_pvp`；沒有新的跨owner相依，Engine與產品執行期不變。

### 1. macOS對角配置（`gui_main.cpp:239` `LatencyWindow`；在`RunLatency`連線前建構，`:365`）

- 只在`__APPLE__`：移動方`create`放在顯示器可用範圍（`SDL_GetDisplayUsableBounds`，扣除選單列與Dock）左上，
  觀察方`join`放右下；`SDL_SetWindowPosition`後以`SDL_SyncWindow`等待生效。
- 視窗尺寸1280×720不變，只改位置；移動方仍會`SDL_RaiseWindow`，重疊區由它覆蓋。
- 在連線前配置：配置造成的`MOVED`在Lobby階段就排空，不落入量測。
- 報告（`<role>-report.txt`）記錄`window_placement`、`window_usable_bounds`、`window_requested_position`、
  `window_sync`（`succeeded`／`timed_out`／`not_requested`）、`window_position_after_sync`、`window_final_position`、
  `window_size`、`window_borders`。
- `presentation_evidence.py` `_window_overlap`以客戶區計算兩窗各自可見比例（`window_overlap`，只記錄、不參與gate）；
  macOS回報邊框為0，標題列不在此幾何內。
- 本機三次冒煙一致：可用範圍`0,30,1792,1090`；`create`要求`(0,30)`、同步後`(0,62)`（下移32 px，標題列位於客戶區上方）；
  `join`為`(512,400)`；重疊768×382，兩窗各自可見約68.2 %。部分可見即不是完全遮蔽。

### 2. OS視窗事件計數（全平台；`LatencyWindow::Watch`／`Report`）

- 以`SDL_AddEventWatch`登記回呼：SDL在事件入佇列時同步呼叫，回呼只遞增計數，不另行排空產品的事件佇列。
- watcher在配置之後才加入：建立視窗與配置本身的事件不計。
- 8類事件：`occluded`、`exposed`、`hidden`、`minimized`、`focus_gained`、`focus_lost`、`moved`、`resized`；
  分別計總數（`window_os_events`）與量測期間（`window_os_events_during_measurement`）。
- 量測期間＝雙方入場2秒後起，到`duration + 2`秒為止（含結尾2秒；`gui_main.cpp:424`）。
- probe為注入按鍵而自送的`FOCUS_GAINED`（`PushMovement`）另計`window_synthetic_events`，不算干擾；本機`create`為20，等於事件數。
- 旗標快照：`window_flags_at_measurement_start`、`window_flags_at_end`（`occluded`／`hidden`／`minimized`／`input_focus`）。

### 3. 干擾判定（`presentation_evidence.py:256` `window_evidence`、`:303` `_windows`）

| 項目 | 是否算干擾 |
|---|---|
| 量測期間的`occluded`／`hidden`／`minimized`／`focus_gained`／`focus_lost`／`moved`／`resized`（`DISTURBING_WINDOW_EVENTS`） | 是。焦點變化代表外部操作；moved／resized會讓產品釋放指標 |
| 量測開始時旗標為`occluded`／`hidden`／`minimized`（`DISTURBING_WINDOW_FLAGS`），或開始狀態未觀測 | 是 |
| `exposed` | 否，只作資訊 |
| `input_focus`旗標 | 否，只記錄；觀察方通常沒有焦點（本機`join`為0） |

- 兩個角色都有有效紀錄，才把`window_disturbed`判為`True`／`False`；任一角色缺失或格式錯誤即為`None`（unknown），永不升格為乾淨。
- 視窗行寫`no window-state interference detected`而不寫「clean」：只檢查已記錄的事件與旗標，沒偵測到不等於沒受干擾。
  smoke-1的摘要仍是改名前的`clean`字樣。

### 4. 計次輪gate（`run_timing.py:55` `gate_window_evidence`；`execute_rounds`）

| 視窗證據 | 計次輪狀態 | report-only狀態 |
|---|---|---|
| 兩角色完整、未偵測到干擾 | `clean`，照門檻判定 | `clean` |
| 偵測到干擾 | `invalid_window_disturbed` | `flagged_window_disturbed` |
| 缺失（probe未寫視窗證據，或沒有`window`紀錄） | `invalid_window_evidence_missing` | `flagged_window_evidence_missing` |
| 無效（格式錯誤、重複鍵） | `invalid_window_evidence_invalid` | `flagged_window_evidence_invalid` |
| 無法歸類（沒有角色狀態能解釋） | `invalid_window_evidence_unknown` | `flagged_window_evidence_unknown` |

- `invalid_*`：該輪`passed: false`；`results.json`保留獨立狀態（不併入`failed`）、`window_reasons`、`thresholds_passed`
  與`underlying_status`（`passed`／`failed`／`unknown`）；`round.json`有`window_gate`。完整GUI輪的乾淨跑次要求
  （`require_clean_gui_round`）也算在gate前結果內。
- `summary.md`：表格列為`invalid_window_disturbed (thresholds passed)`；視窗行結尾
  `-> counted round invalid_window_disturbed (underlying threshold result: passed)`；另寫`runner-failure.txt`，runner以非零結束。
- `flagged_*`：`enforced: false`，不改該輪狀態；視窗行結尾`-> report-only: flagged_..., not enforced`。
- 原始證據不刪不覆寫：`--output`必須是新目錄或空目錄。
- 既有fail-fast不變：第一個未通過的輪之後，其餘宣告輪標`not_run`。

### 5. 已核准的補跑規則（使用者決定，第03批收尾期間）

> 1. 計次輪若harness記錄到視窗狀態干擾（量測期間被遮蔽／隱藏／最小化／失焦／移動／縮放，或量測開始時已被遮蔽／隱藏／最小化），
>    或其視窗證據缺失／無效，即標為無效（`invalid_window_disturbed`／`invalid_window_evidence_missing`／`invalid_window_evidence_invalid`），
>    不算通過，並保留紀錄（永不刪除）；gate前的門檻結果保留為`underlying_status`。
> 2. 是否補跑只依該視窗證據決定，絕不依該輪的延遲分數。
> 3. 每個受干擾輪最多補跑一次；補跑若再受干擾，保留它並停止，交由使用者決定。
> 4. 總輪數與本規則須在執行前宣告；report-only輪只標記。

執行方式與實作對照：

- runner不自動補跑，也不判斷該不該補跑，只產生視窗證據與狀態。補跑是手動作業：保留原目錄，以新的`--output`重跑同一命令。
- 判斷只看`window_gate.status`與`window_reasons`（或視窗行），不看`Visible P50 / P95`欄。
- 因fail-fast，同一次呼叫裡無效輪之後的宣告輪為`not_run`；補跑後如何接續剩餘輪數，須在執行前與總輪數一併宣告。
- 規則中的「失焦」在實作上涵蓋量測期間任何OS焦點變化（`focus_gained`與`focus_lost`）；
  `invalid_window_evidence_unknown`屬「證據無效」一類。
- 服務啟動失敗等非視窗原因記為`failed`，不適用本規則（見[05](./05-match-gateway-startup-race.md)）。

### 6. `run_weapon_short.py`

- `latency`案例經`gate_latency_case`（第83行）套用同一個gate，一律視為計次輪（此runner沒有report-only）。
- `weapon30`／`weapon60`／`weapon144`／`capture`不走延遲模式，沒有視窗證據，不套gate。
- 選了`latency`案例且處於Wayland工作階段時，開始前輸出警告。
- 新增同owner內的相依：`run_weapon_short.py`→`run_timing.py`（`gate_window_evidence`、`WINDOW_INTERFERENCE_NOTE`、`wayland_warning`）。

### 7. Linux／Windows／Wayland

- 不做配置，保留平台預設位置，兩窗可能重疊；視窗行附`[platform-default placement (non-macOS): ...]`。
- SDL的X11後端只在最小化時回報`OCCLUDED`；Wayland compositor可能暫停被蓋住的視窗並回報`OCCLUDED`，
  此時計次輪即使門檻通過也是`invalid_window_disturbed`。
- 在Wayland工作階段（`WAYLAND_DISPLAY`，或`XDG_SESSION_TYPE=wayland`）啟動計次GUI輪前，`wayland_warning`輸出警告；
  `summary.md`與`results.json`附`WINDOW_INTERFERENCE_NOTE`。
- Linux／Windows未實機驗證，只有單元測試的報告形狀（`test_linux_shaped_window_reports_are_recorded_and_may_overlap`）。

### 設計取捨

| 做法 | 不採用原因 |
|---|---|
| 在Engine `SdlPlatform`或產品Client決定視窗位置 | 兩窗配置是驗收量測條件，不是產品行為；Engine不應知道PvP probe的角色 |
| 縮小視窗以避免重疊 | 改變1280×720呈現尺寸，與既有量測條件不可比 |
| probe另行排空事件佇列來計數 | 會與產品自己的事件處理競爭；同步watcher只計數、不消費事件 |
| 刪除受干擾輪，或以補跑結果覆蓋 | 違反保留紀錄；無效輪以獨立狀態保存 |
| 依延遲分數決定補跑 | 等同挑分數 |
| runner內自動補跑 | 規則要求再受干擾時停下交使用者決定；目前維持手動作業，未實作 |

## 驗證

- 自動測試（整合第3輪`build/target/_build/test/logs/pvp-v5-batch03-integration3-20261002-015053/`）：
  `ctest -L pvp` 16／16通過（`timing_runner` 0.34秒、`presentation_evidence` 19.23秒）；`python-tests.log`共165項OK。
- 相關測試：
  - `test_run_timing.py`：`test_window_gate_invalidates_only_counted_rounds`、
    `test_window_gate_names_invalid_and_unknown_window_evidence_apart_from_missing`、
    `test_counted_disturbed_round_keeps_explicit_status_and_evidence`、`test_counted_round_without_window_evidence_is_invalid_not_failed`、
    `test_run_round_records_counted_disturbed_round_with_threshold_result`、`test_summary_names_start_phase_and_window_state_per_round`、
    `test_wayland_warning_is_printed_only_for_counted_gui_rounds_in_wayland_sessions`、
    `test_weapon_short_latency_case_takes_the_counted_window_gate`。
  - `test_presentation_evidence.py`：`test_window_positions_and_clean_run_are_recorded`、
    `test_disturbed_window_events_are_flagged_without_changing_latency_verdict`、`test_missing_window_evidence_is_unknown_not_clean`、
    `test_truncated_window_position_marks_only_that_role_invalid`、`test_duplicate_window_key_marks_only_that_role_invalid`、
    `test_window_overlap_never_raises_on_malformed_geometry`、`test_linux_shaped_window_reports_are_recorded_and_may_overlap`。
- 實機冒煙：3次report-only GUI短測（16秒／20事件／60 FPS，Intel Mac），配置與上述座標一致；兩窗量測期間OS事件皆為0；
  `create`的`window_synthetic_events`皆為20；可見P50 47.4／38.3／46.5 ms。**不計次、不作為驗收證據。**
  證據（git-ignored，只在本機）：`build/target/_build/test/logs/pvp-v5-start-phase-evidence-20261002/integration-smoke-{1,2,3}/`
  （`summary.md`、`round-1/{create,join}-report.txt`、`round-1/round.json`）。
- 未驗證：
  - 2026-10-02第03批結案驗收（事前宣告3輪）：第3輪量測中create視窗出現OS `focus_lost`／`focus_gained`各1次，
    兩個probe同時約122ms卡頓，判`invalid_window_disturbed`（底層門檻因配對19／20而失敗，可見P50 36.9ms）；
    依規則只看視窗證據補跑一次，補跑PASS。焦點來源未確認。證據：`build/target/_build/test/logs/pvp-v5-batch03-closure-20261002/gui-round-3{,-rerun}/`。
  - `LatencyWindow`（C++）沒有單元測試，只由實機冒煙涵蓋。
  - 方法C、D未實跑；Linux／Windows未實跑。

## 殘留風險與後續

- 偵測只涵蓋SDL回報的視窗事件與旗標。沒有對應事件的干擾（例如其他程式造成的CPU／GPU負載）不會記為視窗干擾，
  需搭配`Round N movement:`行的幀間隔統計判讀。
- 可用範圍越小重疊越大；寬高都不超過1280×720時兩窗完全重疊。`window_overlap`不參與gate，完全遮蔽時要靠OS的`occluded`事件判為干擾。
- 開窗後的啟動卡頓發生在量測開始前，不在本gate範圍；它們取消了A1啟動相位，見[02](./02-a1-cancelled-by-stall-reseed.md)。
  視窗行無干擾不代表啟動階段未受影響。
- 補跑仍是手動作業；恢復計次輪且確認反覆操作後，才考慮依「手動→Script」順序工具化。未排程，不構成承諾。
- Linux／Windows計次可能需要比照macOS配置；未實作、未排程。
