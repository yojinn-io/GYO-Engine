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
| Client worker statistics | `NetworkStatistics.cpp` `WorkerStatisticsLine`（2026-10-20 起行尾有 worker 自己的流失計數） | `[ObjectFPS/PvP] worker statistics ` | 隱含 1 | 同上 | `network_statistics.py`※ |
| Match ingress statistics | `IngressStatistics.hpp` | `[ObjectFPS/PvP Match] ingress statistics version=1 ` | 1 | `fixtures/ingress_v1/match.txt` | `ingress_evidence.py`※、08c 的判定工具 |
| Gateway player／gateway ingress statistics | `ingress_statistics.go` | `player ingress statistics version=1 `、`gateway ingress statistics version=1 ` | 1 | `fixtures/ingress_v1/gateway-*.txt` | `ingress_evidence.py`※、edge-rate-limit 的 `judge_l2.py` |
| Gateway statistics | `server.go` | `gateway statistics players=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※（視窗配對） |
| Gateway player send statistics | `send_statistics.go` `sendStatisticsLine` | `player send statistics ` | 隱含 1 | `keys-v1.tsv` | `network_statistics.py`※、`ingress_evidence.py`※、`judge_l2.py` |
| Gateway transport | `server.go`（Close） | `gateway transport ` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py`（只取限速鍵） |
| Gateway runtime transport | `runtime_link.go`（link 關閉） | `runtime transport ` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py` |
| Gateway drop report | `drop_report.go` `dropReportLine` | `drop report player=` | 隱含 1 | `keys-v1.tsv` | `judge_l2.py` |
| Match gw_drop report／violation | `IpcHost.cpp` | `[ObjectFPS/PvP Match] gw_drop report `、`… gw_drop violation ` | 隱含 1 | `IpcHostTests.cpp` | `compare_reports.py`（drop-report 的證據目錄） |
| Gateway player evicted | `server.go` | `player evicted player=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、`judge_l2.py` |
| Gateway start／HTTP | `cmd/main.go` | `Object FPS Gateway start executable_sha256=`、`Object FPS Gateway HTTP=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、`judge_l2.py` |
| Match start／ready | `match_main.cpp` | `[ObjectFPS/PvP Match] start executable_sha256=`、`Object_FPS_PVP Match ready: ` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※、各 runner 等待 ready |
| Match ipc closed／hint／accept failed | `IpcHost.cpp` | `[ObjectFPS/PvP Match] ipc closed reason=`、`… ipc hint: `、`… ipc accept failed` | 隱含 1 | `IpcHostTests.cpp`＋`keys-v1.tsv` | `ingress_evidence.py`※、`run_quad.py`※、`run_action_short.py`※ |
| Match join／evicted | `IpcHost.cpp` | `[ObjectFPS/PvP Match] join player=`、`… evicted player=` | 隱含 1 | `keys-v1.tsv` | `ingress_evidence.py`※ |
| Match ipc statistics | `NetworkStatistics.cpp` `IpcStatisticsLine`（IpcHost 的 lane 計數，match_main 每個統計視窗與結束時寫） | `[ObjectFPS/PvP Match] ipc statistics version=1 ` | 1 | `NetworkStatisticsTests.cpp`（逐字）＋`keys-v1.tsv` | 目前沒有工具（2026-10-20 新增，E1-7：snapshot 從排入到開始寫出的等待；action lane 的送出比「到期」與「開始有待送內容」兩者較晚的時刻晚了多少，閒置的 lane 可以立刻送，不算晚） |
| role started（Match 2 條、Client 2 條） | `NetworkStatistics.cpp` `RoleStartedLine`，由各 role 的 owner 在 Start 後寫 | `[ObjectFPS/PvP Match] role started version=1 `、`[ObjectFPS/PvP] role started version=1 ` | 1 | `NetworkStatisticsTests.cpp`＋`keys-v1.tsv` | `ingress_cross_check.py`（2026-10-20 新增：計時器後端） |
| Match end | `match_main.cpp`（最後一行） | `[ObjectFPS/PvP Match] end version=1 ` | 1 | `keys-v1.tsv` | `ingress_cross_check.py`（被 Windows 的 TerminateProcess 結束時沒有這一行；`authority_tick` 是結束時的 Tick，房間清空時 Match 會重設，所以常是很小的值） |
| Client hit feedback | `PvpApplication.cpp`（`LocalHitFeedback::Observe` 的結果） | `PvP hit feedback version=1 ` | 1 | `keys-v1.tsv`＋`HitFeedbackTests.cpp`（原因的判斷） | 目前沒有工具（2026-10-20 新增：每次受擊與不閃紅的原因 played／dead／new_life） |
| clock sync（三端） | 本體 `ClockSync.hpp` `ClockWindowLine`、`clock_sync.go` `line()` | Match `[ObjectFPS/PvP Match] clock sync window_ms=`；Gateway runtime `clock sync window_ms=`；Gateway 每位 Client `client clock sync player=`；Client `[ObjectFPS/PvP] clock sync player=` | 隱含 1 | `fixtures/log_v1/clock-sync.tsv`（C++ 與 Go 讀同一份） | 目前沒有工具（2026-10-20 升 A 級：三端的對時是之後分析的輸入） |

### 1.2 A 級：JSONL 紀錄檔

| 檔案 | 寫出端 | 檔頭 | 釘住 | 讀者 |
|---|---|---|---|---|
| 移動紀錄（`*-commands.jsonl`） | `MovementTraceWriter.hpp` | 每行 `schema_version: 2` | `DiagnosticFileTests.cpp`（位元組 golden） | `command_evidence.py`※ 等 |
| `match-ingress.jsonl` | `MatchIngressTrace.hpp` | 每行 `schema: object_fps_pvp.match_ingress`、`version: 1` | `fixtures/ingress_v1/match-ingress.jsonl` | `ingress_evidence.py`※ |
| `match-shots.jsonl` | `MatchShotTrace.hpp` | 每行 `version: 2`（沒有 schema 欄） | `CombatTests.cpp` | 各 L2 的 `shadow_*.py` |

三種檔頭不同，照實登記、不改。新的紀錄檔用 `{schema, version}`。

### 1.3 B 級

Client 的 `PvP summary`（2026-10-20 起行尾有依 Match 原因分的 `rejected_<reason>=`）、`PvP client gateway source=`、連線階段與錯誤、`cause records`、`connection failed`、`network worker failed`、`shutdown cleanup failed`；Match 的 death／respawn、`statistics tick=`、`gw_transport`、leave、lifecycle；Gateway 的 player statistics、join／hello／active／expired／left 等生命週期行、`UDP stopped`，以及 `runtime disconnected reason=<liveness_timeout|write_timeout|version_mismatch|protocol|eof|other> error=<引號內的原錯誤>`（原因代碼由 `errors.Is` 分類，`disconnect_reason_test.go` 守住；原錯誤的文字不變）。

## 2. 版本與相容

- **R1**：同一版內只能在行尾追加鍵。改名、刪除、重排、改意義、改前綴都是破壞性變更。
- **R2**：沒有版本鍵的行族（隱含版本 1），破壞性變更一律**改用新的事件名或前綴**（例如 `network statistics.v2 `），讓 v1 讀者的錨點找不到行，凍結的工具因此大聲失敗（`network_statistics.py` 丟 `no statistics lines`）。不在既有行補寫 `version=1`：補寫本身就會改到凍結讀者解析的行。只有本來就帶 `version=` 且讀者會檢查的行族（ingress，以及之後新增的行族）用 `version=N` 升版。破壞性變更要在該批事前宣告新的凍結清單。
- **R3（ingress，D-LOG）**：兩端維持 v1、只在行尾追加。讀者以組別推斷每段追加的鍵（`ingress_evidence.py:108-131`）：目前 Gateway 玩家行 6 組、全域行 1 組、Match 行 1 組。已刪除的路徑照寫 0（墓碑），不刪鍵。
- **R4（JSONL）**：既有 kind 可以加欄位；新增 kind 要升版（`command_evidence.py`、`ingress_evidence.py` 遇到不認得的 kind 會停）。逐筆的高頻紀錄寫到另外的 JSONL；低頻事件與視窗彙總寫成日誌行。最後一行是 `trace_end`；寫出或關閉失敗時程序 exit 1。
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
