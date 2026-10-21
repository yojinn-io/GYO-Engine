# observability-tools：觀測工具（計畫，第 1 版）

- 狀態：**結案**（OT-1～OT-6；D152、D153；四平台 L1 #517 通過：第 1 次 macos-x64 是 `N-movement-recovery-flake` 的第 2 個實例，重跑通過）。
- 依據：README 的 `observability-tools` 列（「相位決策寫進 trace（觀測缺口 3）；late 修正改用增量計數、加寬 D44 偵測器；網路案例換不同的遺失種子；Gateway→Match 延遲注入改成正式模式」；驗收「工具的單元測試、新舊各分析一次／—／—」；估 10～13 h）、INVENTORY 的 E1-3、`N-late-count-detector-increment`、`N-d44-network-detector-narrow`、E4、`N-injection-official-mode`，以及也指向本批、但 README 那一列沒寫的 E1-6、`N-trace-overwrite-on-restart`。
- 檔位：high（實作與單元測試）；新舊分析 medium。
- file:line 以 `df04ea3` 為準。

## 1. 先講三個會影響做法的事實

1. **主 checkout 要維持「只有文件的變更」**：`d44-regression-pv7` 的 driver 每項之前都檢查主 checkout 自 `572cc39` 起有沒有文件以外的變更，也核對 `build/acceptance` 與凍結清單（`run_d44.py:118-120`、`judge_d44.py` 的 `frozen_problems`）。本批改的是凍結清單裡的工具，所以**程式放在另一個 worktree 的分支**（`claude/pvp-v71-observability-tools`，從 after-pv7 線分出），等 `d44-regression-pv7` 結案後再併回 after-pv7 線。
2. **late 計數已經是增量**：v7.1 的判定不再取最大值。`judge_c2.late_corrections` 加總 `start_phase.*.epochs[*].late_corrections`，而每個 (player, epoch, life) 的紀錄是那段的最後計數，計數器只在 `Reset()` 歸零（`LocalPlayerPrediction.cpp:63`）。在 d44 第 1 次 session 的證據上，加總與 D48 的增量式都是 121，沒有任何跑次不同（盤點時試算）。剩下的缺口：紀錄上限 64 筆，超過的觀測會丟（`dropped_observations`），目前沒有人檢查；timing／network probe 沒有這組計數器。
3. **相位決策寫進 trace（E1-3）會改 Data Contract**：要在產品 C++ 的 movement trace 新增事件。`log-contract.zh-Hant.md` R4：新增 kind 要升版；凍結的 `command_evidence.py:117-133` 只接受固定的 kind，遇到新的會丟 `unknown event kind`。所以要先決定格式（第 5 節 OT-0），本批今晚不做。

## 2. 分批

```
observability-tools
├── OT-0 trace-format-gate        ⏸｜決定 E1-3 的格式、兩個未列項目的去留（你決定）
├── OT-1 phase-decision-trace     ⏸ high｜相位決策寫進 trace（產品 C++、Data Contract）
├── OT-2 late-count-increment     high｜共用計數函式；dropped_observations>0 算工具失效
├── OT-3 network-reset-detector   high｜整場 Match trace 的非 respawn 重設分類器（加寬 E3）
├── OT-4 loss-seed-and-ipc-delay  high｜遺失種子參數化；Gateway→Match 延遲注入的正式模式
└── OT-5 old-new-analysis         medium｜在 d44 第 1 次 session 的證據上新舊各分析一次
```

| 代號 | 內容 | 驗收 | 工時 |
|---|---|---|---|
| OT-0 trace-format-gate | E1-3 的格式：(a) commands.jsonl 加新 kind、升 schema 3；(b) 另開一個 JSONL 檔；(c) Client 寫一條新的 A 級日誌行（帶 `time_ns`）。E1-6、`N-trace-overwrite-on-restart` 的去留 | 你決定 | 0.5 |
| OT-1 phase-decision-trace | `LocalPlayerPrediction` 的首次決定、視窗修正、late 修正（error、套用的 shift、路徑、樣本數）寫進 OT-0 選的格式；digest 不動（R5、`test_diagnostic_isolation.py`） | golden、隔離測試、突變 | 3.5～4 |
| OT-2 late-count-increment | `late_count.py`（新，不凍結）：增量式的共用計數（每個 (player, epoch, life) 的最後計數加總），`dropped_observations>0` 回報為工具失效；OT-1 之後改成直接數 trace 的 late 事件 | 單元測試（合成 fixture：重設、多段、dropped） | 1～1.5 |
| OT-3 network-reset-detector | `network_resets.py`（新）：掃整場 Match trace 的非 respawn 重設，「每個探針階段在模擬停下後的 starvation 1 次」之外都列為事件候選；階段邊界從 Match trace 與 `network.log` 推，不改 `network_main.cpp` | 單元測試；在 d44 的 network-r1～r7 上和 E3＋`record_flags` 並列 | 2～2.5 |
| OT-4 loss-seed-and-ipc-delay | `impaired_network.py`、`action_probe.py` 加 `--loss-seed`（預設等於現在的常數，舊行為可重現），種子寫進 evidence；Gateway→Match 的 IPC 延遲搬進 `action_probe.py` 的正式 case，`run_short_stall.py` 改用它、不再替換模組屬性 | 單元測試；預設參數下新舊工具在同一輸入上結果一致 | 2.5～3 |
| OT-5 old-new-analysis | OT-2、OT-3 在 d44 第 1 次 session 的證據上新舊各算一次並記錄 | 記錄 | 1～1.5 |

合計約 10.5～13 h，和 README 一致。今晚做 OT-2～OT-5（約 7～8.5 h）。

## 3. 凍結與之後的 L2

- OT-4 改了兩支凍結檔（`action_probe.py`、`impaired_network.py`），`run_short_stall.py` 也會改。之後的 L2 要用新的凍結清單；本批不寫新清單，由第一個用到它們的 L2 在宣告時凍結（`d44-regression-pv7` 的續跑仍用 cp 清單與原工具）。
- OT-2、OT-3 是新檔，不影響既有的凍結清單。

## 4. 不做的事

- 把短停頓升為 D44 的計數母體：那是下一次 L2 的宣告事項（D146 DR-4 只記錄）。
- 改 `network_main.cpp` 輸出階段邊界、替 network probe 加 Client trace：會改驗收用的 C++ 與 probe 雜湊；OT-3 先用現有證據推邊界，不夠時再提。
- 產品 C++ 的任何變更（OT-1）在 OT-0 之前不做。

## 5. 等你決定

- **OT-0a E1-3 的格式**：(a) commands.jsonl 加新 kind、升 schema 3，所有讀 `*-commands.jsonl` 的凍結讀者要重新凍結，也要檢查 `log-contract.zh-Hant.md` Replacement Criteria 第 3 條；(b) 另開一個 JSONL 檔（`{schema, version}`，既有讀者不受影響；建議）；(c) A 級日誌行（R4 說低頻事件應寫成日誌行，但每秒可能有數筆、要帶 `time_ns` 才能和 trace 對齊）。
- **OT-0b 兩個未列在 README 的項目**：E1-6（TimerBaseline 只在 probe 連線前量一次 3 秒，改的是驗收用 C++）與 `N-trace-overwrite-on-restart`（重新啟動 Match 會覆寫前一段的 trace，改的是產品的檔名規則，屬日誌 Data Contract）。建議 E1-6 納入本批（OT-6，約 1.5 h），`N-trace-overwrite-on-restart` 移到 `re-evaluate 之後`（L2 的 runner 每次都開新目錄，不受影響；只影響 LAN 手動測試）。
- **OT-0c 新的凍結清單從哪一次 L2 生效**：建議從 `d44-regression-pv7` 結案之後的第一個 L2 開始。

## 6. 實作（2026-10-21，D148：照建議走）

程式在分支 `claude/pvp-v71-observability-tools`（worktree `../GYO-Engine-obs`，`cbdc2b9`），還沒有併回 after-pv7 線；要等 `d44-regression-pv7` 結案（第 1 節第 1 點）。四平台 L1：run 39470556006（#503），結果見 HANDOFF。

### OT-2 late-count-increment

- `build/acceptance/object_fps_pvp/late_count.py`（新）：兩個來源各算一次，必須相等。
  - start_phase：每段 (player, epoch, life) 最後計數的加總。
  - action-frames：D48 P 式的增量；計數掉下來代表重設，從 0 重新算。
  - 工具失效（不給數字）的情況：`dropped_observations>0`、frames 的 `evidence_end.lost>0`、兩個來源不相等。
  - 舊的「取最大值」讀法不提供。
- 單元測試 `test_late_count.py`，7 項：重設、兩來源一致、不一致、dropped、lost、單一來源、缺計數器。
- 開發時發現 action-frames 的最後一行是 `evidence_end`（帶 `lost`），已處理，並把它的 `lost` 納入工具失效。

### OT-3 network-reset-detector

- `network_resets.py`（新）：掃整場 Match trace 的非 respawn 重設，以「每個探針階段 1 次 epoch 2 的 starvation」為預期，其他的列為候選事件。
  - 候選的種類：其他原因、別的 epoch、同一玩家第二次、超過階段數的 starvation。
  - 工具失效：trace 缺漏、沒有單一的結尾、`dropped>0`、預期的重設少於階段數。
- 單元測試 `test_network_resets.py`，4 項。
- 限制：唯一已知的 v7 網路 E3 實例（第 08 批 dev-network，`census.txt:9`）在 `run_network.py` 開始寫 Match trace 之前，沒有 trace 可讀。所以新偵測器的檢出力只由合成樣本支撐，沒有真實的陽性樣本。

### OT-4 loss-seed（一半）

- 遺失亂數的種子改成參數：`impaired_network.py`（`flow_generators`、`DEFAULT_LOSS_SEED`）、`action_probe.py`（`network_generator`）、`run_gameplay.py` 與 `run_network.py` 的 `--loss-seed`。
  - 預設值重現原本的常數：`0x47594F＋2i`／`0x475950＋2i`，以及 `0x47594F＋i`。
  - 種子寫進 evidence：`network-rtt-*.json` 的 `loss_seed`、`relay.json` 的 `network.loss_seed`。
- 單元測試 `test_loss_seed.py`：預設值和舊常數的丟包位置相同；換種子後丟包位置不同；種子進 evidence。
- `run_short_stall.py`、`run_client_delay.py` 的 `FROZEN`（D50 條件 3，替換模組屬性前核對的雜湊）已更新為新的 `action_probe.py`、`impaired_network.py` 雜湊。這不是改判定：釘住的強度不變，只是釘到新檔案。
- `ActionRelay` 照舊用三個參數呼叫基底類別，種子在之後設定。原因是兩支延遲 runner 的測試替換了基底類別的建構子（第一次改成四個參數時，它們的測試全部失敗）。
- 實跑（用主 checkout 的執行檔、本分支的 runner）：
  - `run_gameplay.py --case network20`（預設種子 4675919＝0x47594F）通過，隨機丟包 58 個。
  - `run_network.py --network-impairments --loss-seed 12345` 三個階段都通過，每個階段隨機丟包 70 個。三個階段用同一個基底種子，所以丟包數相同；舊常數下也一樣（第 16 批 D45 檢查 2：三個階段都是 77 個）。
- 驗收工具的全部 `test_*.py` 都通過；`mutations.json` 指向這些檔案的 2 個錨點仍在。
- **沒做的另一半：Gateway→Match 延遲注入的正式模式**。它要把 `run_short_stall.py` 的「替換 `IpcPause`」改成凍結工具裡的正式 case，等於改掉 D50 條件 3 的機制本身，是設計變更，留給你決定（第 7 節 OT-0d）。

### OT-5 old-new-analysis（d44 第 1 次 session 的證據）

| 項目 | 舊工具 | 新工具 | 結果 |
|---|---|---|---|
| late 修正（103 個有 action-frames 的目錄） | `judge_c2.late_corrections`：121 | `late_count`：start_phase 121、frames 121 | 每個目錄都相等，工具失效 0 |
| 網路重設（network-r1～r7，開發跑次 network-r1、network-impaired-r1） | E3：0；`record_flags`：0 | 預期重設數都等於階段數（1、1、4），候選 0 | 一致 |
| 遺失種子 | 固定常數 | 預設值 | 丟包位置相同（單元測試） |

## 7. 等你決定（第 5 節之外，實作時新增）

- **OT-0d 注入的正式模式**：(a) 把 `IpcDelay` 搬進 `backpressure_probe.py`，在 `action_probe.run_case` 加一個 `host-ipc-delay` case，`run_short_stall.py` 改呼叫它，不再替換模組屬性（D50 條件 3 就不再需要）；(b) 維持現狀（runner 替換＋雜湊釘住），等短停頓真的要升為計數母體時再做。建議 (b)：D146 DR-4 把短停頓設為只記錄，現在沒有常設回歸要用它。
- **OT-0e 階段之間用不同種子**：要不要讓 impairments 的三個 RTT 階段用不同的種子（例如基底＋階段序號），目前三個階段的丟包位置相同。改了會讓預設行為不同於舊跑次，所以這次沒做。
  - **決定（D153，使用者在分支會話討論後決定，轉給主會話）**：
    - 現況：`run_network.py` 在每個 RTT 階段開一個新的 relay（迴圈裡的 `impaired_gateway(http, udp, rtt, args.loss_seed)`），會話編號從 0 開始，所以三個階段的丟包位置相同（預設種子各 77 個，種子 12345 各 70 個）。
    - 決定 1，工具不改：同一次跑次內三個階段維持同一個種子，也不加「每個階段換種子」的選項。三個階段是受控對照，只有 RTT 不同；換種子會把 RTT 和丟包位置混在一起。目前沒有實際需求，照 AGENTS.md §1 不先做一般化。
    - 決定 2，交給下一個 L2 的宣告：impairments 與遊戲的 network20／40 案例，每次跑次用不同的基底種子（例如第 k 次用 DEFAULT＋1000×k），種子清單事前宣告；runner 已經把種子寫進 evidence。這解決第 16 批 D45 檢查 2 的限制：重複跑次只抽樣主機時序、不抽樣丟包型態，15 次 impairments 其實只有 1 組丟包位置。屬於 OT-0c（新凍結清單生效的那一次 L2）。
    - 讀法：宣告要寫明，重複跑次現在同時抽樣主機時序與丟包型態，但仍然不是丟包型態的窮舉。
    - 注意：`run_network.py` 的覆蓋條件（`dropped_first`≥2、`dropped_burst`≥4）是決定性的強制丟包，不受種子影響；`action_probe.py` 的 network 案例要求 `random_dropped`>0，換種子後理論上可能是 0。宣告前用要宣告的種子清單各跑一次乾跑確認。

## 8. OT-1 與 OT-6（D152）

使用者：「OT-0a 直接改就行，目前沒有也沒有發佈，不需要被這個技術債。其他的照建議執行。」所以：
- OT-0a 選 (a)：相位決策直接寫進 `*-commands.jsonl` 的新 kind，schema 升到 3；凍結讀者一起改，不另開檔案、不留相容層。
- OT-0b：E1-6 納入（OT-6）；`N-trace-overwrite-on-restart` 移到 `re-evaluate 之後`。
- OT-0c：新的凍結清單從下一個 L2 生效。
- OT-0d：注入的正式模式維持現狀。
- OT-0e：三個 RTT 階段維持同一個種子（這一項沒有建議，照「預設行為不變」處理）。之後的 D153 確認這一點，並把「跑次之間換基底種子」交給下一個 L2 的宣告（第 7 節）。

### OT-1 phase-decision-trace（E1-3）

- 產品：
  - `MovementTrace.hpp`：新增 `PhaseDecision` kind、`MovementPhasePath`（first／window／late）與四個欄位。
  - `MovementTraceWriter.hpp`：schema 3。其他 kind 的位元組和版本 2 相同，只改版本號；只有 `phase_decision` 在 `started_ns` 之後多 `phase_path`、`phase_error_seconds`、`phase_shift_seconds`、`phase_deadband_seconds`。
  - `LocalPlayerPrediction::TrackPhase`：決定之後寫一筆（`TracePhaseDecision`，const、只寫 trace）。
- **寫哪些決定**：每個首次決定（含沒有修正的）、每個 late 修正、啟動修正的 window 決定。
  - 第一版每個 window 決定都寫：實跑 clean-60 時，16 秒寫了 1177 筆，其中 1174 筆落在 deadband 內、沒有修正。原因是 window 滿了之後每個新樣本都重新判一次，每秒約 40 次。E1-3 要的是「首次決定、視窗修正、late 修正與幅度」，所以 deadband 內的 window 決定不寫。
- 讀者：`command_evidence.read_trace_events` 接受 schema 1～3。
  - `phase_decision` 只在 schema ≥3 合法；`phase_path` 必須是三種之一；三個秒數必須有限、deadband ≥0；`count` ≥1。
  - 其他 kind 帶 `phase_*` 鍵就停。
  - `quad_evidence` 用同一個讀者。
- 日誌契約：`log-contract.zh-Hant.md` 的 §1.2 表格改成 schema 3；R4 加上這個例外（低頻但要和移動紀錄對齊時間，所以不寫成日誌行）。
  - Replacement Criteria 第 3 條（一組鍵的變更要改 3 個以上分析器）：這次只改了 `command_evidence` 一個讀者（quad 共用），而且本來就是升版並重新凍結，不觸發。
- 測試：
  - `PredictionTests`「PvP phase decisions are written to the movement trace」：首次決定（修正與不修正）、啟動修正的 window 決定、late 修正都寫進 trace；deadband 內的 window 決定不寫（跑滿 `MovementPhaseWindowSamples`＋20 幀）。
  - `DiagnosticFileTests` 的位元組 golden 改成 schema 3，加一行 `phase_decision`。
  - `test_command_evidence` 加 schema 3 與驗證的案例。
  - `fixtures/log_v1/sample-commands.jsonl` 換成新 writer 的真實片段（200 筆，含 2 筆 `phase_decision`）。
- 突變：`ot1-late-decision-untraced`、`ot1-every-window-decision-traced` 都 killed。
  - 第二個第一次存活：測試只跑 20 幀，到不了 window 決定（要 240 個樣本），那段檢查是空的。改成跑滿 window 之後 killed。
  - 既有突變 `v7-09b-reacquire-on-window-correction` 的錨點照原意更新，仍 killed。
- 實跑（新建置）：
  - loss-shot 通過；trace 裡的修正次數 1，等於 start_phase 紀錄的修正次數 1；late 0，等於 `late_count` 的 0。兩個獨立來源一致。
  - clean-60 通過。
- digest 不動：只寫 trace，`test_diagnostic_isolation` 通過。

### OT-6 timer-baseline-after（E1-6）

- `timer_baseline.hpp` 加 `when`。`gameplay_action.hpp`、`quad_main.cpp` 在跑次結束後再量一次 3 秒（`timer_after`，Client 或 bot 仍連線），寫進 client JSON。
- `gameplay_evidence`、`gameplay_soak_evidence`、`quad_evidence` 原樣轉出 `timer_baseline_after`／`timer_after`，只當解讀用，不進判定。
- `action_probe.py` 等 probe 結束的上限從 25 秒改成 30 秒：16 秒的跑次，加 3 秒的跑次後基準與決定的排空。
- 實跑：host-ipc-1000ms 通過，前後的 late P99 是 6.3／7.6 ms；quad clean-60 1 輪通過，7.1／7.8 ms。

### 測試判定的變更（AGENTS.md §14.4）

- `DiagnosticFileTests` 的 golden：Data Contract 的釘住，照使用者核准的契約變更（D152）改成 schema 3。其他 kind 的位元組只有版本號不同；新 kind 有自己的 golden 行。
- `test_gameplay_evidence`、`test_gameplay_soak_evidence` 的「timer 只能原樣轉出」守衛：預期的行加上 `timer_baseline_after` 那一行。
  1. 守的性質：analyzer 只轉出 timer 基準，不拿來判定。
  2. 放寬的面：無。仍然只允許這兩行原樣轉出；任何其他含 timer 的行都會讓測試失敗。
  3. 收緊的面：無。
  4. 不適用。
  5. 根據：E1-6，D152。
- `run_short_stall.py`、`run_client_delay.py` 的 `FROZEN`（D50 條件 3）：釘到新的 `action_probe.py` 雜湊，強度不變。
