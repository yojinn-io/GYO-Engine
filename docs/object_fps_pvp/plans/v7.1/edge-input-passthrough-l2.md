# edge-input-passthrough 的 L2 事前宣告（第 3 版，2026-10-16 使用者核准，D109）

- 狀態：**定稿**（2026-10-16 使用者核准，D109；第 10 節照建議：J4a 1.5 倍、跑 soak-long、睡眠那段在本 session 結束後接著做、次數照第 5 節）。以下保留草稿的文字（AGENTS.md §14.1：核准後不放寬）。
  - 第 1 版經 D45 的門檻來源對抗式檢查（1 位 xhigh）：送核准前必須修正 7 項、應修正 8 項。本版的修正對應表在第 11 節。
  - 第 3 版：兩棵樹已建置（B 的全量 CTest 72／72），`run_l2.py`／`judge_l2.py` 已寫好並乾跑（31 種情況全部符合預期；B 的開發跑次 6 個；A 沒有跑過任何項目）。乾跑時發現的 11 處不精確已寫進正文（第 12 節）。
  - 工具目錄：`build/target/_build/test/logs/pvp-v71-edge-input-l2-prep-20261016/`（`commands.txt` 有確切指令，`dryrun.md` 有乾跑矩陣，`prep-files.sha256` `1ec7eda5…`）。
- 依據：D94、D95（D-10、D-11、D-12）、D96、D108；protocol-v7 §2a、§4、§9；README `edge-input-passthrough` 列；gateway-direction.md 的停止條件（Match 負載明顯上升就停下重評 D-8）。
- 先例：v7 08c 的 L2 事前宣告（`plans/v7/08c-drop-report.md` 第 188 行起）、v7 第 16 批（`run_16.py`、`judge_16.py`）。
- 數字沒有標「實測」的，是程式推導或推論。v7 語料（`build/target/_build/test/logs/pvp-v7-16-l2-20261015/`、`pvp-v7-08c-l2-20261011/`）是不同建置，只當作量級與跑次間差距的參考，不當門檻。

## 1. 目的

這批是行為改變：Gateway 把每個 Client 資料包的輸入窗口原樣轉給 Match（一包一則 IPC），不再去重、合併、截短，沒有拒絕線，也沒有 snapshot 的 tick／cursor 守衛（D1、D2）；link 依到達順序寫出，每位玩家的輸入佇列上限 64 則，滿了丟最舊並回報。

L2 **判定**：

1. 遊戲行為沒有變差：既有判定、D44 事件、替代率、逐出與連線品質。
2. Match 的負載沒有明顯上升（gateway-direction 的停止條件，第一次給數值）。
3. 新路徑的帳目成立：佇列溢出在無故障情境是 0；ingress 帳目除了宣告的例外都成立；丟棄回報兩端相等。

**只記錄、不判定**：Match 的副本分類數、參考年齡樣本數、故障情境的佇列溢出數、IPC 流量、short-stall 與 client-delay 的 `run_case` 判定（設計上只記錄，D60）。

一個重要的事實（D45 檢查 S2，實測）：clean 時 B 本來就是「一包一則 IPC」（`received_inputs` 1921 ＝ `fwd_main_input_packets` 1921；v7 第 16 批的 clean、quad 的 `coalesced_input_windows` 都是 0）。A 在 clean 只多了每則 IPC 帶的已寫出命令（約 +2 個／則），D-8 擔心的「每則 SubmitInput 複製 snapshot」主要在故障情境增加（B 原本合併的資料包：host-ipc-1000ms 約 102、gateway-1000ms 34～71）。

## 2. 兩棵樹

| 樹 | 內容 |
|---|---|
| before（B） | 本批的第 1 個 commit（`42f6e88`）：只加 Match 的 host mutex 鎖等待直方圖與其測試（診斷，不改行為），其餘同 `edge-results-resend`（`5c0d8f5`） |
| after（A） | 本批的第 2 個 commit（`6d9ecf5`）：Gateway 原樣轉送與其餘全部 |

- 本批拆成兩個 commit，讓鎖等待在兩棵樹都量得到（v7 沒有這個指標）。
- B→A 的程式差分：Gateway（`gateway/*.go`）；`runtime_v7.proto` 的 reserved（重產的 enum 也會讓 Match 的 `DropReason_IsValid` 把 22～24、43、44 判成 `reason_range`；Match 的其他程式不變）；worker probe（`worker_main.cpp`，L2 不用）；測試與文件。兩棵樹的 Match 雜湊都記下。
- probe（action、timing、network、quad）兩棵樹沒有原始碼差分：**只用 B 的建置**，兩棵樹共用，排除建置差異。
- 兩棵都是 detached、乾淨的 worktree，各自乾淨建 Match、Gateway；記 `artifacts.sha256` 與 Gateway 的 `vcs.revision`、`vcs.modified=false`。runner 與分析器一律從主 checkout 執行。主 checkout 從宣告提交後到 session 結束只能有文件變更。

## 3. 工具與雜湊

| 工具 | 雜湊 | 用途 |
|---|---|---|
| `frozen-pv7.sha256`（54 檔，`pvp-v71-version-errors-20261016/`） | `b4a8963001883160d5bb7642b7da89afd343c686d29752874461762e17a291c2` | 凍結的 runner 與分析器 |
| `frozen-pv7-rooted.sha256`（同上 54 檔的同一摘要，路徑加 `build/acceptance/object_fps_pvp/`；driver 逐行和上一列核對） | `2d789560…` | `run_client_delay.py --frozen-list` 讀的是相對 repo 的路徑 |
| `judge_16.py`（`pvp-v7-16-prep-20261014/`） | `29258be1ba4e63e7…` | D44 事件 E1、E2、E3 的偵測器（import，不改） |
| `compare_reports.py`（`pvp-v71-drop-report-20261016/`） | `36e65f3a9f302b85…` | J7 |
| `sleeper.py`（`pvp-v7-16-prep-20261014/`） | `98105dbf71a5818d…` | 閒置門檻與每跑次的 sleeper |
| `run_l2.py`（新寫） | `c9e5cc01483c175515f5cd1a2569ba0501d621e409524a386f91ac58bf787653` | driver |
| `judge_l2.py`（新寫；J4a 的倍數寫死為 1.5，你選別的值時雜湊會變） | `58ade1c1b9baefabefdcf80ee2c086e151d1206a95f95d9589d2d2dd5b4af72b` | 判定 |
| `artifacts.sha256`（兩棵樹的 Match、Gateway，B 的 4 個 probe 與 arena） | `c708b1add9fb16e722612e1192a2948a4e63d12cd51d14e5c65efac628c4ad9f` | 執行檔 |

driver 在 session 開始與每個跑次之前核對以上雜湊、兩棵樹的 HEAD 與乾淨狀態、執行檔雜湊、主 checkout 的非文件變更（未追蹤的非文件檔也算變更）。另外，每個 Match 與 Gateway 的啟動行記的執行檔雜湊要等於該樹的 artifact。兩棵樹都用 B 部署的 arena（`0026013c…`）。

## 4. 主機

照第 16 批：`caffeinate -dims`；PR 的 Auto-fix 暫停；插電、不移動機器；閒置門檻（3 次 5 秒 sleeper 都 < 10 ms，每 30 秒重試、最多 30 分）；每個跑次前後各 1 次 5 秒 sleeper 與 CPU 前 5 名。

## 5. 情境 × 次數 × 順序

B、A 交錯，每個情境內輪流由 B 或 A 先跑；每個項目只跑 1 輪（`--rounds 1`），由 driver 控制次數。

| 情境 | runner 與參數 | 每棵樹 |
|---|---|---|
| clean-60 | `run_gameplay.py --case clean-60 --probe <action>` | 10 |
| quad clean-60 | `run_quad.py --case clean-60 --rounds 1 --probe <quad>` | 5 |
| network-impairments | `run_network.py --network-impairments --probe <network>` | 5 |
| backpressure | `backpressure_probe.py --layers gateway host-ipc --durations-ms 1000 --probe <timing>` | 10（每次 2 個 case） |
| Gateway→Match 延遲 | `run_short_stall.py --delay-ms 50 --window-ms 100 --fault-at T --rounds 1`（T＝2.0 與 11.5 各 4 次） | 8 |
| Client→Gateway 延遲 x1 | `run_client_delay.py --profile x1 --delay-ms 200 --window-ms 2000 --fault-at 7.5 --frozen-list …` | 5 |
| Client→Gateway 延遲 ramp | `run_client_delay.py --profile ramp --peak-ms 250 --rise-ms 1500 --fall-ms 500 --fault-at 7.5 --frozen-list …` | 5 |
| soak-long | `run_gameplay_soak.py --soak --fps 60`（1808 秒） | 1 |

- `run_short_stall.py` 的第 k 輪用 `fault_at[(k-1) % n]`（`:27-28`、`:290`），交錯時每次只跑 1 輪就永遠是第 1 個值；所以 driver 每次只傳一個 `--fault-at`。
- soak 兩棵樹的先後記下；heartbeat-sync 的睡眠測試（若做）在兩次 soak 都結束、session 外才做。
- 共 98 個項目（每棵樹 49 個），10 輪；每個情境與每次重複輪流由 B 或 A 先跑；soak-long 排在最後，B 先 A 後。各情境排在哪幾輪見 `run_l2.py --list-only`（寫進 `commands.txt`）。
- 估計（B 的開發跑次的時間）：約 127 分鐘（每棵樹短項目 29.4 分，兩次 soak 67.6 分），加上閒置門檻。

## 6. 判定

### 6.1 逐跑次判定（不成立就立刻停在那個跑次）

兩棵樹都判：B 不成立同樣停下，代表工具或環境有問題，先回報。

| 代號 | 判定 | 適用 | 門檻與來源 |
|---|---|---|---|
| J1 | 既有判定 | clean-60、quad、network、backpressure、soak 的凍結分析器判定都通過；short-stall、client-delay 的 `run_case` 只記錄 | v7 第 16 批：25／25、120／120 通過（實測，不同建置） |
| J1e | D44 事件 | 所有跑次 E1、E2、E3 都是 0（`judge_16.py` 的偵測器；E1 的故障窗規則照它套用到 short-stall 與 client-delay 的注入；quad 以 round 目錄判 E1、E2，沒有故障窗） | v7 第 16 批 260 個 case-run 都是 0（實測；這個基準不含 quad、short-stall、client-delay，已查的 16 個 quad round 的非重生重設都是 0） |
| J5 | 佇列溢出 | clean-60、quad、network、client-delay、soak：A 的 `drop_ipc_queue_overflow_input_packets` 是 0（所有玩家、所有視窗，含 `final=1`；judge 直接讀 `gateway.log`，凍結分析器不輸出這個新鍵）；backpressure、short-stall 只記錄 | §2a 的上限 64 則；`take()` 每次寫出前清空佇列，要溢出 writer 必須卡住約 1.07 秒以上（推論）。這是不變式檢查 |
| J6 | ingress 帳目 | 見 6.3 的逐項預期表 | protocol-v7 §9 的鍵對應與宣告的例外 |
| J7 | 丟棄回報 | 合格跑次（backpressure 的 gateway-1000ms、host-ipc-1000ms 中至少有 1 則回報的跑次）：Gateway 寫出與 Match 讀到的回報依 (player, report_sequence) 逐則相等、違規 0；兩棵樹各至少 1 個合格跑次；缺少 final 只記錄（兩種 case 都是） | protocol-v7 §4；drop-report 的短跑次分別 8／8、3／3 有回報（實測） |
| J3e | 逐出 | 所有跑次 0 | v7 所有語料都是 0（實測） |

### 6.2 session 結束才判（需要完整的 B 與 A）

session 完整＝完成的項目數等於排程數、名稱逐一相符；不完整就不判，回報。

| 代號 | 判定 | 門檻 | 來源與限制 |
|---|---|---|---|
| J2 | 替代率（`match-commands.jsonl` 中 source 不是 actual 的已解析命令） | clean-60、quad、network、backpressure（逐 case）、short-stall（逐 T）、x1：A 的中位數 − B 的中位數 ≤ max(B 的範圍寬度, 2000 ÷ 該情境 B 的已解析命令數中位數 ‰)；ramp、soak 只記錄 | 替代成對出現，最小變化約 2／1920＝1.04‰（實測）；ramp 的跑次間差距 2.8～4.9‰（08c L2，實測），5 次分不出來；soak 每棵樹 1 次，沒有範圍可言 |
| J3 | 連線品質 | soak：A 的逐出 0，`connection_quality_failures_max` ≤ B；不跑 soak 時 J3 記為「未判定」，不算通過 | 品質視窗 600 tick、加入後第 1 個視窗不判，逐出至少要 40 秒（`MatchRuntimeHost.cpp:670-671`）；clean-60 的 Match 只活約 19 秒，短跑次結構上量不到 |
| J4a | Match CPU | soak（約 180 個視窗）：A 的 `cpu_s`／`window_s` 平均 ≤ B × **1.5**（倍數請你決定，第 10 節）；短情境只記錄 | 第一次給數值；同一 session 的 idle-clean-60 10 次的範圍 0.0146～0.0173（+18%，實測） |
| J4b | Tick 晚醒 | 每個情境（第 5 節的 8 列：backpressure 的兩個 case 合併、short-stall 的兩個 T 合併），把兩棵樹各自所有視窗的 `tick_late_bins` 合併，A 的第 99 百分位所在格 ≤ B 的所在格＋1 格 | 先例 08c J7（差 ≤1 tick）；格界 250、500、1000、2000、4000、8000、16000、32000、64000 µs，最後一格以上為一格 |
| J4c | host mutex 鎖等待 | 每個情境（同 J4b 的 8 列）、input 與 step 各自，合併直方圖後的尾端比例（≥250 µs 的等待數 ÷ 總等待數）：A ≤ 2×B＋1‰ | 沒有既有基準；第 0 格涵蓋 0～250 µs，p99 在兩棵樹都會落在第 0 格，比 p99 沒有檢出力（D45 M5）。B 的開發跑次全部在第 0 格（最大 24 µs，實測），所以實際門檻約為「A 的 ≥250 µs 等待 ≤ 1‰」 |

J4 的「無值」：某情境沒有任何視窗值，或沒有 soak 時，J4 記為「未判定」；不算失敗，但也不算通過。

J4 的覆蓋範圍（限制）：Match 的 `network statistics` 行只在滿 10 秒時寫（`match_main.cpp:91`），最後一個視窗不寫。短跑次只量到 Match 啟動後 0～10 秒的一個視窗（其中約 3 秒在玩家加入前）；short-stall 的 11.5 秒、client-delay 的釋放落在沒寫出的視窗，這些故障 J4 看不到。每個跑次至少要有 1 個視窗，否則 J4 對那個情境記為「無值」並回報。

**工具有效性（量不到就停；逐跑次檢查、立刻停）**：每個視窗的 `host_lock_input_waits` ＝ 隨後的 Match ingress 行的 `received_inputs`（±2）；`host_lock_step_waits` 在 600 × window_s ÷ 10 的 ±10% 內（逐視窗）。不符就停，不算判定。B 的開發跑次：input 完全相等，step 600、603（實測），network-impairments 的重連（8 個視窗）也成立。

### 6.3 J6 的逐項預期（凍結的 `ingress_evidence.py`）

| 檢查 | B | A |
|---|---|---|
| IG、gateway_global、T3、I2、S1、S2、trace、link_shots | passed | passed |
| J5a | 所有跑次 passed（不加 `--injection`，分析器逐桶相等；`never_arrived` 只記錄） | 同 B |
| I0 | passed | passed；溢出 > 0 的跑次不判（溢出以封包計） |
| I1 | host-ipc-1000ms、short-stall（中間有 relay）、network（2 條 IPC 連線）允許 undetermined（passed 也可）；其餘必須 passed；failed 一律不成立 | 同 B |
| I3 | passed | failed，而且 gates 都是 0、Gateway 端允許數 0、Match 副本 > 0（宣告的形狀）；其他形狀算失敗 |
| 被刪路徑的鍵 | — | 都是 0：`fwd_rejected_lane_input_packets`、`fwd_written_duplicate_input_packets`、`fwd_handed_rejected_lane_commands`、`fwd_link_written_rejected_lane_commands`、`fwd_merged_duplicate_commands`、`fwd_written_duplicate_commands`、`drop_resolved_trim_commands`、`drop_rejected_lane_overflow_commands`、`drop_discarded_on_leave_commands`、`fwd_resolved_at_write_commands`、`fwd_retention_expired_commands`、全部 `fwd_routed_*_inputs`、Gateway 的 `drop_old_tick_snapshots`、`drop_regressed_snapshots`、`runtime transport` 行的 `coalesced_input_windows` |

整體的 `status` 在 A 會是 failed（因為 I3）；judge 依上表逐項判，不看整體 status。

## 7. 預測（不是停止條件）

| 代號 | 項目 | B（預期） | A（預測） | 推翻條件 |
|---|---|---|---|---|
| P0 | Match 的 `received_inputs` | clean ≈ Gateway 的 main input 封包數 | clean ≈ B；backpressure ＞ B（host-ipc-1000ms 約 +100） | clean 差 > 2%，或 backpressure 不增加 |
| P1 | clean-60 每跑次 Match 收到的命令數 | 約 1.9k | 約 5.8k（≈ B 的 Gateway handed） | A ÷ B 不在 2～4 倍 |
| P2 | Match 的副本分類（pending／late／resolved copy） | 約 0 | 約 3.8k（≈ B 的 written duplicate） | A < B 的 written duplicate 的一半 |
| P3 | `match-ingress.jsonl` 中 `reference_age_us` 非 null 的紀錄數 | — | clean ≈ B；故障情境 ≥ B | clean 差 > 10% |
| P4 | clean-60、quad 的 Match CPU | — | A ÷ B ≤ 1.15 | 超過 1.15 |

## 8. 停止條件與重試規則

- **重試規則**：不重跑、不補跑；所有證據保留；判定不成立就停下回報，再決定（§14.1）。
- 開始前：兩棵樹不乾淨、執行檔雜湊不符、工具雜湊不符、閒置門檻 30 分內不過 → 不開始。
- 進行中：6.1 任一不成立（任一棵樹）→ 停在那個跑次；跑次異常結束或注入沒發生（工具無效）→ 停，不算判定。
- 受干擾的跑次：跑次前或後的 sleeper 最大值 ≥ 10 ms → 記為受干擾，不重跑；累計 3 次就停（第 16 批 U7 的做法）。
- session 結束：6.2 任一不成立 → 回報；J4 不成立時同時依停止條件提出 D-8 的重評。
- 誤停率（推論）：J2 以 B 的範圍寬度為門檻，A 與 B 同分布時每個情境誤停的機率依次數而定（10 次約 1～2%、5 次約 5～8%）；整個 session 約 10 個情境，預期誤停約 0.3～0.5 次。誤停時照規則停下回報，不放寬。

## 9. 差異帶（給 `pv7-checkpoint`）

用本 session 實測的 A−B（各指標的中位數差與範圍），不用第 6 節的通過門檻。

## 10. 等你決定

1. **J4a 的倍數**（建議 1.5 倍）：負載判定第一次給數值，依據只有「高於跑次間的差距」。可選 1.5、2，或只記錄由你看數字判斷。
2. **soak-long 要不要跑**（建議跑，兩棵樹各 1 次，約 68 分）：J3 與 J4a 只在 soak 判；不跑就記為未判定。
3. **heartbeat-sync 的睡眠那段**（D102 延後的 L2）：要你讓機器睡眠；可以在本 session 結束後接著做，或另外排。
4. **次數**（第 5 節）：照第 16 批的規模縮小，共 98 個項目、約 2 h 10 分；要更多跑次就加時間。

## 11. D45 門檻來源檢查的修正對應（第 1 版 → 第 2 版）

| 檢查項 | 修正 |
|---|---|
| M1 分析器範圍、judge 不存在 | 6.3 逐項預期表、被刪鍵的完整清單；J5 由 judge 直接讀 `gateway.log`（含 `final=1`）；judge 先寫好乾跑、雜湊進第 3 節 |
| M2 J1 的適用範圍與 D44 偵測器 | J1 列出適用的 runner；J1e 用 `judge_16.py`（雜湊）；short-stall、client-delay 的 `run_case` 只記錄 |
| M3 J3 短跑次量不到 | J3 只在 soak 判；不跑記為未判定；短跑次只判逐出 0（J3e） |
| M4 J2 的 +1‰ | 改成 max(B 的範圍寬度, 2 個命令)；ramp、soak 只記錄；寫出誤停率 |
| M5 J4c 分格與彙總 | 改判合併直方圖的尾端比例；加工具有效性檢查 |
| M6 判定時點、session 完整、受干擾跑次 | 分成 6.1 逐跑次與 6.2 session 結束；完整性與受干擾規則寫進第 8 節；B 不成立同樣停 |
| M7 short-stall 的 fault-at | 每次只傳一個 `--fault-at`，兩值各 4 次；quad、client-delay 也每次 1 輪；`--frozen-list`、`--probe` 寫明 |
| S1 J4 的視窗覆蓋 | 寫明覆蓋範圍與無值規則 |
| S2 J4a 沒對準 D-8 | 加 P0、P4；J4a 改在 soak 判 |
| S3 P3 量不到 | 改用 `reference_age_us` 非 null 的紀錄數 |
| S4 「Match 不變」不精確 | 第 2 節改寫；probe 只用 B 的建置 |
| S5 J7 的工具與合格跑次 | 第 3 節雜湊；合格跑次的定義 |
| S6 差異帶 | 改用實測的 A−B |
| S7 J4b 的彙總 | 合併直方圖 |
| S8 工具清單 | 第 3 節 |
| 記錄項 | §9 的「上萬」已更正；行號以 commit 時為準；soak 順序與睡眠測試寫進第 5 節 |
| 測試缺口 | 加 host 測試（每次 SubmitInput、每個 step 各記 1 次；被 step 擋住的 SubmitInput 記到等待）與 3 個突變 |

## 12. 乾跑時發現、已寫進正文的不精確（第 2 版 → 第 3 版）

| # | 原寫法 | 現在的寫法 |
|---|---|---|
| 1 | `--frozen-list` 直接傳 `frozen-pv7.sha256` | 傳路徑加了前綴的同一份清單（第 3 節），driver 逐行核對；直接傳會拒絕全部 56 項（已實測） |
| 2 | J5a「照分析器的不適用狀態」 | 分析器沒有不適用狀態；所有跑次 passed，`never_arrived` 只記錄 |
| 3 | I1 的 undetermined | 三種情境允許 undetermined 或 passed，其餘 passed，failed 一律不成立 |
| 4 | J2「2 個命令換算的 ‰」沒有分母 | 2000 ÷ 該情境 B 的已解析命令數中位數 |
| 5 | J4「每個情境」 | 第 5 節的 8 列（backpressure 兩個 case、short-stall 兩個 T 各自合併） |
| 6 | J4c 的實際門檻 | B 全部在第 0 格，實際門檻約為 ≥250 µs 的等待 ≤ 1‰（寫進表中） |
| 7 | 工具有效性放在 6.2 | 逐跑次檢查、立刻停；比對隨後的 ingress 行；step 依視窗長度換算 |
| 8 | J7 只有 host-ipc 可以缺 final | 兩種 case 缺 final 都只記錄 |
| 9 | J1e 的偵測範圍 | quad 以 round 目錄判 E1、E2；基準不含 quad、short-stall、client-delay |
| 10 | 排程細節、soak 順序、arena、未追蹤檔、J4 無值 | 寫進第 3、5 節與 6.2 |
| 11 | J4a 的倍數 | `judge_l2.py` 寫死 1.5；你選別的值時改這一個常數、重算雜湊、再送一次確認 |
