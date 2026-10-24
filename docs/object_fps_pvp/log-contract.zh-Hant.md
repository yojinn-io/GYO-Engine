# PvP 日誌 Data Contract v1

更新：2026-10-20（v7.1 `log-contract`，D142）。Owner：`object_fps_pvp`。**狀態：生效。**
本文件取代 [protocol-v7](protocol-v7.zh-Hant.md) §1「日誌只加鍵」。計畫與決策見 [v7.1 log-contract](plans/v7.1/log-contract.md)。

日誌與紀錄檔是 Client、Match、Gateway 和驗收工具（凍結的分析器、各 L2 的判定工具）之間的 Data Contract（AGENTS.md §8）。本契約規定哪些行受保護、怎麼改、怎麼驗證。Engine 不認識任何產品的行族（[trace](../architecture/trace.md) Design Invariant 6）；本契約整份在產品內，刪除產品時不必改公共層。

## 1. 兩級行族

**A 級**：有工具解析的行。改它會打斷凍結的分析器，所以受本契約保護。**B 級**：只給人讀的診斷文字，可以自由改；工具要開始解析某個 B 級行，必須先把它升為 A 級並附釘住測試。

錨定：讀者以下表的「錨點」比對（C++ 的行在 LogFile 時間戳之後、Go 的行在 log 時間戳之後）。一行不得含有別族的錨點；共用清單在 `tests/object_fps_pvp/fixtures/log_v1/prefixes.txt`。

### 1.1 A 級：日誌行

| 行族 | 寫出端 | 錨點 | 版本 | 釘住 | 讀者（凍結的標 ※） |
|---|---|---|---|---|---|
| Match network statistics | `NetworkStatistics.cpp` `MatchStatisticsLine`（2026-10-20 起行尾有 `tick_overrun_*`、`tick_stalls`、`advance_*`，E1-5） | `[ObjectFPS/PvP Match] network statistics ` | 隱含 1 | `NetworkStatisticsTests.cpp`（逐字）＋`keys-v1.tsv` | `network_statistics.py`※、`ingress_evidence.py`※（視窗邊界）、`judge_c2.py`、edge 系列與 tt2 的判定工具 |
| Client worker statistics | `NetworkStatistics.cpp` `WorkerStatisticsLine`（2026-10-20 起行尾有 worker 自己的流失計數；2026-10-24 起再追加 Client 捨棄的封包計數 `unparsed_datagrams`、`early_datagrams`、`invalid_snapshot_datagrams`、`ignored_snapshot_datagrams`、`unexpected_kind_datagrams`，silent-branch-asserts C3） | `[ObjectFPS/PvP] worker statistics ` | 隱含 1 | 同上 | `network_statistics.py`※ |
| Match ingress statistics | `IngressStatistics.hpp` | `[ObjectFPS/PvP Match] ingress statistics version=1 ` | 1 | `fixtures/ingress_v1/match.txt` | `ingress_evidence.py`※、08c 的判定工具 |
| Gateway player／gateway ingress statistics | `ingress_statistics.go` | `player ingress statistics version=1 `、`gateway ingress statistics version=1 ` | 1 | `fixtures/ingress_v1/gateway-*.txt` | `ingress_evidence.py`※、edge-rate-limit 的 `judge_l2.py`（證據目錄） |
| Gateway statistics | `server.go` | `gateway statistics players=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※（視窗配對） |
| Gateway player send statistics | `send_statistics.go` `sendStatisticsLine` | `player send statistics ` | 隱含 1 | `keys-v1.tsv` | `network_statistics.py`※、`ingress_evidence.py`※、`judge_l2.py`（各 L2 的證據目錄） |
| Gateway transport | `server.go`（Close） | `gateway transport ` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py`（證據目錄；只取限速鍵） |
| Gateway runtime transport | `runtime_link.go`（link 關閉） | `runtime transport ` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py`（各 L2 的證據目錄） |
| Gateway drop report | `drop_report.go` `dropReportLine` | `drop report player=` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py`（各 L2 的證據目錄） |
| Match gw_drop report／violation | `IpcHost.cpp` | `[ObjectFPS/PvP Match] gw_drop report `、`… gw_drop violation ` | 隱含 1 | `IpcHostTests.cpp` | `compare_reports.py`（drop-report 的證據目錄） |
| Gateway player evicted | `server.go` | `player evicted player=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、`judge_l2.py`（各 L2 的證據目錄） |
| Gateway start／HTTP | `cmd/main.go` | `Object FPS Gateway start executable_sha256=`、`Object FPS Gateway HTTP=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、`judge_l2.py`（各 L2 的證據目錄） |
| Match start／ready | `match_main.cpp` | `[ObjectFPS/PvP Match] start executable_sha256=`、`Object_FPS_PVP Match ready: ` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、各 runner 等待 ready |
| Match ipc closed／hint／accept failed | `IpcHost.cpp` | `[ObjectFPS/PvP Match] ipc closed reason=`、`… ipc hint: `、`… ipc accept failed` | 隱含 1 | `IpcHostTests.cpp`＋`keys-v1.tsv` | `ingress_evidence.py`※、`run_quad.py`※、`run_action_short.py`※ |
| Match join／evicted | `IpcHost.cpp` | `[ObjectFPS/PvP Match] join player=`、`… evicted player=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※ |
| Match ipc statistics | `NetworkStatistics.cpp` `IpcStatisticsLine`（IpcHost 的 lane 計數，match_main 每個統計視窗與結束時寫） | `[ObjectFPS/PvP Match] ipc statistics version=1 ` | 1 | `NetworkStatisticsTests.cpp`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-20 新增，E1-7：snapshot 從排入到開始寫出的等待；action lane 的送出比「到期」與「開始有待送內容」兩者較晚的時刻晚了多少，閒置的 lane 可以立刻送，不算晚） |
| role started（Match 2 條、Client 2 條） | `NetworkStatistics.cpp` `RoleStartedLine`，由各 role 的 owner 在 Start 後寫 | `[ObjectFPS/PvP Match] role started version=1 `、`[ObjectFPS/PvP] role started version=1 ` | 1 | `NetworkStatisticsTests.cpp`＋`keys-v1.tsv` | `ingress_cross_check.py`（2026-10-20 新增：計時器後端） |
| Match end | `match_main.cpp`（最後一行） | `[ObjectFPS/PvP Match] end version=1 ` | 1 | `keys-v1.tsv` | `ingress_cross_check.py`（被 Windows 的 TerminateProcess 結束時沒有這一行；`authority_tick` 是結束時的 Tick，房間清空時 Match 會重設，所以常是很小的值） |
| Client hit feedback | `PvpApplication.cpp`（`LocalHitFeedback::Observe` 的結果） | `PvP hit feedback version=1 ` | 1 | `keys-v1.tsv`＋`HitFeedbackTests.cpp`（原因的判斷） | 目前沒有工具（2026-10-20 新增：每次受擊與不閃紅的原因 played／dead／new_life） |
| Match resync statistics | `IngressStatistics.hpp` `MatchResyncStatisticsLine`（`match_main.cpp` 每個統計視窗、每位玩家一行，沒有 player=0；最後一個視窗以 `final=1` 關閉寫過的每位玩家） | `[ObjectFPS/PvP Match] resync statistics version=1 ` | 1 | `IngressStatisticsTests.cpp`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-23 新增，v7.1 `outage-resync` 的 `drop-counters`：`slack_clamped_samples`＝被 ±1 s 夾限改變的發布樣本，屬於 `slack_published_samples`；2026-10-23 `match-resync` 起行尾追加 `resyncs`＝Match 換的 Resync epoch 數、`late_beyond_no_commands`＝晚到超過 N_o 而不產生樣本的第一份副本數） |
| Client phase summary | `ClientSimulationRole.cpp` `PhaseSummaryLine`（模擬 role：連線換代或換玩家時、role 停止時各一行，只在那一代有本機玩家時寫） | `[ObjectFPS/PvP] phase summary version=1 ` | 1 | `ClientSimulationRoleTests.cpp`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-23 新增，`drop-counters`：`phase_samples_unmatched`＝命令已離開 64 格命令環的 slack 樣本，第 5 節的死信） |
| Client connection interrupted／resumed／summary | `ClientConnection.cpp` `ConnectionInterruptedLine`、`ConnectionResumedLine`、`ConnectionSummaryLine`（網路 worker：進入中斷時、恢復時各一行，連線的傳輸關閉時一行摘要） | `[ObjectFPS/PvP] connection interrupted version=1 `、`[ObjectFPS/PvP] connection resumed version=1 `、`[ObjectFPS/PvP] connection summary version=1 ` | 1 | `ConnectionLivenessTests.cpp`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-24 新增，`client-interrupted`：`gap_ms`＝距上一份有效 snapshot 的整毫秒；`max_gap_ms`＝兩份有效 snapshot 的最長間隔；`transient_socket_errors`＝只計數的暫時性 socket 錯誤） |
| Gateway session rebound | `server.go`（`receivePacket` 的 Hello，每次改綁一行） | `session rebound version=1 ` | 1 | `rebind_test.go`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-24 新增，`identity-rebind`：`old_port`、`new_port`＝改綁前後的來源埠，`silent_ms`＝原 endpoint 最後驗證之後的整毫秒；不記 token 與完整位址） |
| clock sync（三端） | 本體 `ClockSync.hpp` `ClockWindowLine`、`clock_sync.go` `line()` | Match `[ObjectFPS/PvP Match] clock sync window_ms=`；Gateway runtime `clock sync window_ms=`；Gateway 每位 Client `client clock sync player=`；Client `[ObjectFPS/PvP] clock sync player=` | 隱含 1 | `fixtures/log_v1/clock-sync.tsv`（C++ 與 Go 讀同一份） | 目前沒有工具（2026-10-20 升 A 級：三端的對時是之後分析的輸入） |

### 1.2 A 級：JSONL 紀錄檔

| 檔案 | 寫出端 | 檔頭 | 釘住 | 讀者 |
|---|---|---|---|---|
| 移動紀錄（`*-commands.jsonl`） | `MovementTraceWriter.hpp` | 每行 `schema_version: 3`（v7.1 D152：版本 2 加上 Client 相位追蹤的 `phase_decision`；版本 2 的其他 kind 位元組不變，只改版本號） | `DiagnosticFileTests.cpp`（位元組 golden） | `command_evidence.py`※ 等（讀版本 1～3） |
| `match-ingress.jsonl` | `MatchIngressTrace.hpp` | 每行 `schema: object_fps_pvp.match_ingress`、`version: 1` | `fixtures/ingress_v1/match-ingress.jsonl` | `ingress_evidence.py`※ |
| `match-shots.jsonl` | `MatchShotTrace.hpp` | 每行 `version: 2`（沒有 schema 欄） | `CombatTests.cpp` | 各 L2 的 `shadow_*.py` |

三種檔頭不同，照實登記、不改。新的紀錄檔用 `{schema, version}`。

### 1.3 B 級

Client 的 `PvP summary`（2026-10-20 起行尾有依 Match 原因分的 `rejected_<reason>=`）、`PvP client gateway source=`、連線階段與錯誤、`cause records`、`connection failed`、`network worker failed`、`shutdown cleanup failed`；Match 的 death／respawn、`statistics tick=`、`gw_transport`、leave、lifecycle；Gateway 的 player statistics、join／hello／active／expired／left 等生命週期行、`UDP stopped`，以及 `runtime disconnected reason=<liveness_timeout|write_timeout|version_mismatch|protocol|eof|other> error=<引號內的原錯誤>`（原因代碼由 `errors.Is` 分類，`disconnect_reason_test.go` 守住；原錯誤的文字不變）。

### 1.4 規劃中的 A 級行族（v7.1 `outage-resync`，D172）

下列行族在各批實作時加進 1.1 的表，附寫出端的釘住測試（`keys-v1.tsv` 與逐字測試），錨點加進 `prefixes.txt`。L2 要讀它們，所以一開始就是 A 級（Design Invariant 1）。都是新的事件名，帶 `version=1`（R2）。鍵照加入的批次排列：後面的批次只在行尾追加（R1、Design Invariant 2）。

| 行族 | 寫出端（預定） | 錨點 | 鍵（依序；括號是加入的批次） |
|---|---|---|---|
| Client 相位摘要 | 已在 `drop-counters` 實作，見 1.1 | `[ObjectFPS/PvP] phase summary version=1 ` | `player`、`phase_samples_unmatched`（`drop-counters`） |
| Client 連線中斷 | 已在 `client-interrupted` 實作，見 1.1 | `[ObjectFPS/PvP] connection interrupted version=1 ` | `player`、`gap_ms`（偵測時距上一份 snapshot）、`epoch`（`client-interrupted`） |
| Client 連線恢復 | 已在 `client-interrupted` 實作，見 1.1 | `[ObjectFPS/PvP] connection resumed version=1 ` | `player`、`gap_ms`、`epoch_before`、`epoch_after`（`client-interrupted`） |
| Client 連線摘要 | 已在 `client-interrupted` 實作，見 1.1 | `[ObjectFPS/PvP] connection summary version=1 ` | `player`、`interruptions`、`max_gap_ms`、`transient_socket_errors`（`client-interrupted`） |
| Match 重新對時統計 | 已實作，見 1.1（`drop-counters` 前三鍵、`match-resync` 後兩鍵） | `[ObjectFPS/PvP Match] resync statistics version=1 ` | `player`、`final`、`slack_clamped_samples`（`drop-counters`）；`resyncs`、`late_beyond_no_commands`（`match-resync`） |
| Gateway session 重新綁定 | 已在 `identity-rebind` 實作，見 1.1 | `session rebound version=1 ` | `player`、`old_port`、`new_port`、`silent_ms`（`identity-rebind`；不記 token 與完整位址） |

- **各端只記自己觀察到的事**（第 5 節）：`phase_samples_unmatched` 是模擬端的 64 格命令環觀察到的，所以由模擬端寫，不經網路 worker。
- **不追加到 ingress 行**：ingress 已有 8 組（R3），再加會觸發 Replacement Criteria 1。
- **clock sync 行（三端，A 級）**：
  - 每一行行尾追加 `reason=`（R1，D172 的 D-8 (b)），值是：
    - `window`：10 s 到期；
    - `final`：連線結束，和 `final=1` 同時；
    - `resume`：恢復時提前結束，`final=0`。
  - 已實作（`client-interrupted`，三端都加了 `reason=`）。Client 與 Gateway 的每位 Client 視窗可能出現 `resume`（Gateway 的在 `identity-rebind` 加上：`echo.go` `noteVerified`）；Match 與 Gateway runtime 的 clock sync 行量的是 Gateway↔Match，值只會是 `window` 或 `final`。
  - 漂移的回歸不因 `resume` 重新開始，`drift_ppm` 的意義不變。
  - 釘住：`fixtures/log_v1/clock-sync.tsv` 加一欄。
- **移動紀錄**（schema 3）：`reset` 事件的 `reset_reason` 新增值 `resync`（`match-resync` 起，已實作）。這是既有 kind 的新列舉值，不加欄位、不升版；`command_evidence.py` 遇到未知的值會照字串保留（`command_evidence.py:325-329`）。凍結的讀者在該批先查。
- **移動紀錄的相位決策**（R4 的例外，`client-interrupted` 起，已實作）：恢復時「只重設相位擷取」寫一筆 `phase_decision`，`phase_path=resume_reset`、`count=0`、`phase_shift_seconds` 記被取消的剩餘修正量。之後的下一次決定照 `first` 記；`MovementTrace.hpp` 對 First 的定義加上「或只重設擷取之後」。和 `reset_reason=resync` 一樣是既有 kind 的新列舉值，不升版，凍結的讀者在該批先查。先查的結果：`command_evidence.py` 原本只接受 first／window／late 與 `count ≥ 1`，會把它當成損壞；repo 裡的讀者只多接受 `resume_reset` 且 `count=0`（`test_command_evidence.py`）。已凍結的副本讀不到含它的紀錄，`resync-l2-l3` 的宣告要重新凍結。
- **死信**：Client 的 64 格命令環對不上的相位樣本，計 `phase_samples_unmatched`（第 5 節的死信原則；`outage-characterize` 確認它現在無聲丟棄）。

## 2. 版本與相容

- **R1**：同一版內只能在行尾追加鍵。改名、刪除、重排、改意義、改前綴都是破壞性變更。
- **R2**：沒有版本鍵的行族（隱含版本 1），破壞性變更一律**改用新的事件名或前綴**（例如 `network statistics.v2 `），讓 v1 讀者的錨點找不到行，凍結的工具因此大聲失敗（`network_statistics.py` 丟 `no statistics lines`）。不在既有行補寫 `version=1`：補寫本身就會改到凍結讀者解析的行。只有本來就帶 `version=` 且讀者會檢查的行族（ingress，以及之後新增的行族）用 `version=N` 升版。破壞性變更要在該批事前宣告新的凍結清單。
- **R3（ingress，D-LOG）**：兩端維持 v1、只在行尾追加。讀者以組別推斷每段追加的鍵（`ingress_evidence.py:108-131`）：目前 Gateway 玩家行 6 組、全域行 1 組、Match 行 1 組。已刪除的路徑照寫 0（墓碑），不刪鍵。
- **R4（JSONL）**：既有 kind 可以加欄位；新增 kind 要升版（`command_evidence.py`、`ingress_evidence.py` 遇到不認得的 kind 會停）。逐筆的高頻紀錄寫到另外的 JSONL；低頻事件與視窗彙總寫成日誌行。最後一行是 `trace_end`；寫出或關閉失敗時程序 exit 1。
  - 例外（v7.1 D152）：Client 的相位決策（`phase_decision`：首次決定、啟動修正的 window 決定、late 修正；`phase_path`、`phase_error_seconds`、`phase_shift_seconds`、`phase_deadband_seconds`，`count` 是用到的樣本數）寫在移動紀錄裡，不寫成日誌行。它要和同一檔案的 generated／sent 事件對齊時間，屬於重播相位追蹤的輸入（E1-3）。只有這個 kind 帶 `phase_*` 鍵；讀者在其他 kind 看到它們就停。
- **R5**：日誌不改變控制流程（權威 digest 守住）；診斷值不進判定（`test_diagnostic_isolation.py`）。

## 3. 值的文法

- 被 `network_statistics.py` 解析的行（Match network、Client worker、Gateway send），值只能是整數、小數、`na`、`-`、`p50/p90/p99/max` 形式，或以逗號分隔的整數；其他字串會讓凍結的 `value()` 丟 `ValueError`（`test_log_contract.py` 的反向測試）。
- ingress 行的值只能是十進位整數。
- 字串值只能放在自己的行族或 B 級行；自由文字只能是最後一個欄位。
- clock sync 的 drift 不是有限值時寫 `na`（兩端相同，`clock-sync.tsv`）。

## 4. 驗證

| 守衛 | 內容 |
|---|---|
| `fixtures/log_v1/keys-v1.tsv` ＋ `test_log_contract.py`（CTest `object_fps_pvp.log_contract`） | 沒有自己 golden 的 A 級行族：從寫出端的原始碼讀出鍵序列，v1 鍵必須是它的前綴。network／worker statistics 由 C++ 的逐字測試釘住 |
| 同上 | repo 裡的 `network_statistics.py`、`ingress_evidence.py`、`command_evidence.py` 讀真實樣本（`fixtures/log_v1/sample-*`，取自 `tt2-trace` L2 的 A 樹 clean-60，本機路徑已替換）：沒有 ContractStop、沒有 ValueError、未知鍵 0；反向：改了事件名的行讀不到、追加字串值會丟 ValueError、對調 ingress 鍵會 ContractStop |
| `clock-sync.tsv` | 輸入欄位 → 預期本體的 8 列；`ClockSyncTests.cpp` 與 `clock_sync_test.go` 逐字比對 |
| `prefixes.txt` | 不得出現在別族行的語句；`IngressStatisticsTests.cpp`、`ingress_statistics_test.go`、`ingress_lines_test.go` 共用 |
| `ingress_cross_check.py`（CTest `object_fps_pvp.ingress_cross`） | 真實的 Gateway＋Match 跑次之後，`ingress_evidence` 與 `network_statistics` 都讀它的日誌 |

限制：凍結清單與各 L2 的判定工具在 git 忽略的證據目錄，CI 只能證明 repo 裡的分析器讀得到；凍結副本靠各批 L2 的核對與重播。

## 5. State Authority 與死信原則

- 每個元件只記錄自己執行或直接觀察到的事，不重新推導別的元件的判斷：Client 只數收到的裁決（拒絕原因是 Match 給的值）。
- Client 對時行的 player id 是 worker 自己 session 的身分（加入時設定、關閉時清除，`ClientClockSession`），不是別人權威狀態的複本。
- 死信三原則（D57，[network-architecture](network-architecture.zh-Hant.md) §7）：每則離開「待處理」的訊息只有執行或帶原因的死信兩種結果。日誌上對應為：每個死信原因都有計數鍵（ingress 的 `drop_*`、`gw_drop` 的原因鍵），能拿到內容時另寫細節紀錄（`match-ingress.jsonl`）。

### 5.1 `drop_` 鍵與 `GatewayDropReport` 的對應（D145）

ingress 日誌與 `GatewayDropReport` 維持兩份契約（讀者、單位、版本規則都不同：日誌給工具與人、照本契約演進；wire 給 Match、照 [protocol-v7](protocol-v7.zh-Hant.md) 演進），共用一套詞彙：原因字只在 `gateway/ingress_statistics.go` 的名稱表定義一次，`DropReason` 的值名＝ingress 鍵轉大寫。Gateway 的每個 `drop_` 鍵恰好屬於下表一類。

| 類別 | 鍵 | 理由 | 守衛 |
|---|---|---|---|
| 上 wire（29 個） | `tests/object_fps_pvp/fixtures/drop_reasons.json` 列出的鍵，一鍵一值 | 可回報的丟棄（protocol-v7 §4 的 (b)） | `drop_reasons.json` 綁定值、名稱、鍵與範圍；Go（`drop_report_golden_test.go`）、C++（IPC 測試）、Python（`test_drop_reasons.py`）都讀它 |
| 只寫日誌：link 已失效 | 玩家行 `drop_cleared_on_close_commands`、`drop_abandoned_on_close_commands`、`drop_{cleared,abandoned}_on_close_actions_{packets,shots,acks}`；全域行 `drop_room_unavailable_datagrams` | 發生時 link 已失效，結構上無法回報（protocol-v7 §4 的 (c)） | Go 測試（下述）。`_actions_shots`、`_actions_acks` 四個鍵自 edge-actions 起恆為 0，也在 `removed_paths` 裡 |
| 只寫日誌：可導出的總和 | 全域行 `drop_rate_limited_packets` | 等於玩家行 `drop_rate_limited_*_packets` 的和，各項已上 wire | Go 測試 |
| 已刪除的路徑 | 玩家行 `drop_resolved_trim_commands`、`drop_rejected_lane_overflow_commands`、`drop_discarded_on_leave_commands`（edge-input）、`drop_invalid_actions_{packets,shots}`（edge-results，A3）、`drop_link_rejected_actions_{packets,shots}`、`drop_discarded_on_leave_actions_{shots,acks}`（edge-actions）；全域行 `drop_old_tick_snapshots`、`drop_regressed_snapshots`（D-12） | pv7 刪除了這些出口；依 R1 鍵留在 v1，恆為 0 | Go 測試；L2 由分析器的 `removed_paths` 判定（`tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md`） |
| 另一個單位 | 玩家行 `drop_ipc_queue_overflow_input_commands` | 溢出的輸入窗口帶的命令數，是 I0 的屬性；同一事件以封包數上 wire（`DROP_IPC_QUEUE_OVERFLOW_INPUT_PACKETS`） | Go 測試 |

Gateway 看不到的丟棄（kernel 接收緩衝溢出）沒有鍵。

`drop_report_golden_test.go` 的 `TestDropReasonsMatchTheGoldenAndTheKeys` 雙向檢查：宣告為不上 wire 的計數器確實是沒有 `DropReason` 的丟棄；每個沒有 `DropReason` 的 `drop_` 計數器都在宣告裡。新增 Gateway 的丟棄出口時，要嘛加 `DropReason`（連同 proto、名稱表與 `drop_reasons.json`），要嘛在該測試與本表宣告類別；兩者都沒有就失敗（突變 `dlp-unclassified-drop-exit`）。

## 6. 定案的暫定事項（D142）

- **D58(1)**：runtime link 失效時那個資料包計在全域的 `room_unavailable`（`server.go:474-482`）。
- **D58(3)**：F2b-1 改寫 F2a 測試斷言，照 D60 接受（使用者 2026-10-20 明確確認）。
- **D58(4)**：動作 ack 的單位是 `acks`（`IngressStatistics.hpp:128`、`:256`、`:267`）。
- **D58(5)**：slack 樣本的單位是 `samples`（`IngressStatistics.hpp:129`）；只給 `--ingress-trace` 而沒有 `--movement-trace` 是用法錯誤，exit 2（`match_main.cpp:57-58`）。
- **D58(8)**：I3 的「拒絕線命令數」用寫出數。passthrough 之後 I3 不適用；讀 v7final 等舊樹的日誌時照這個定義解讀。
- **D37**：Client 的日誌預設寫在執行檔旁的 `logs/client-<本地時間>.log`（`main.cpp`）；`--log` 可以指定。
- **coalesced_snapshots**：`gateway transport coalesced_snapshots` 是送出端所有取代的總和；全域 ingress 的 `drop_coalesced_snapshots` 只計發布到達時的取代；兩者的差是送出迴圈中途的取代（`backpressure_test.go` 守住）。鍵名不改。

## Design Invariants

1. 被工具解析的行一定是 A 級，並有寫出端的釘住測試（逐字 golden，或 `keys-v1.tsv` 的鍵序列前綴）。
2. A 級行族在同一版內只在行尾追加；v1 鍵序列是目前鍵序列的前綴。
3. 破壞性變更讓舊的讀者大聲失敗（R2），不得讓它靜默讀錯。
4. 值的文法照第 3 節；自由文字只在最後一欄。
5. 每個元件只記錄自己執行或直接觀察到的事。
6. 日誌不改變控制流程。
7. Engine 不認識產品的行族。
8. 讀者錨定在訊息開頭比對；行內不含別族的錨點（`prefixes.txt`）。

### Replacement Criteria

出現任一項時，停止追加，改提 v2（新的事件名或版本，並重新凍結讀者）：

1. ingress 的組別超過 8 組。
2. 同一版內第 2 次出現「只在行尾追加不夠用」的需求。
3. 一組鍵的變更需要改 3 個以上的分析器。
