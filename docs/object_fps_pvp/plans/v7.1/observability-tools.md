# observability-tools：觀測工具（計畫，第 1 版）

- 狀態：**計畫第 1 版。照 D148（「沒有問題就照建議走」）先做只動驗收工具的部分（OT-2～OT-5）；要你決定的事在第 5 節。**
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
