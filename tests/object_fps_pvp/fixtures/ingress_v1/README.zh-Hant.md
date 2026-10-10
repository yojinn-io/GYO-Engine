# ingress v1 的 golden 樣本

owner：object_fps_pvp。第 08c 批（Gateway 丟棄回報）的 log 行族與細節檔，暫定 v1；第 10 批決定沿用或改版。
Go（Gateway）、C++（Match）與驗收工具（`ingress_evidence.py`，F3）解析同一份樣本。

| 檔案 | 內容 | 產生者 |
|---|---|---|
| `gateway-player.txt` | `player ingress statistics` 行 | `apps/object_fps_pvp/gateway/ingress_statistics.go` 的 `playerIngressStatisticsLine` |
| `gateway-global.txt` | `gateway ingress statistics` 行 | 同檔的 `gatewayIngressStatisticsLine` |
| `match.txt` | `[ObjectFPS/PvP Match] ingress statistics` 行 | `apps/object_fps_pvp/include/RetroFPS/Pvp/IngressStatistics.hpp` 的 `MatchIngressStatisticsLine` |
| `match-ingress.jsonl` | Match 的細節檔樣本 | `apps/object_fps_pvp/include/RetroFPS/Pvp/MatchIngressTrace.hpp` 的 `MatchIngressTraceWriter`（測試用寫出端產生後逐字比對） |

三個 `.txt` 各有三行，依序是同一個程序、同一位玩家連續的三個視窗：全部為 0 的視窗、帶非零值的視窗、`final=1` 的最後一段。
非零值是合成的：同一行內的值互不相同（鍵互換時測試會失敗），並且每一行都滿足下面的 IG、I0、T3、I2、J5a、S1、S2（真實的 log 只保證整個程序的總和成立）。
副檔名用 `.txt`，因為儲存庫的 `.gitignore` 忽略 `*.log`。

## 行的格式

```
player ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> <key>=<int> ...
gateway ingress statistics version=1 window_ms=<int> final=<0|1> <key>=<int> ...
[ObjectFPS/PvP Match] ingress statistics version=1 player=<id> window_ms=<int> final=<0|1> <key>=<int> ...
```

- 開頭固定是 `version`、`player`（Gateway 全域行沒有）、`window_ms`、`final`。
- 計數的鍵依 golden 的順序排列；鍵的完整清單與順序以上表的產生者為準（Go 一份、C++ 一份，各自只有一個名稱對照）。
- 鍵名規則：`[fwd_|drop_]<原因>[_<種類>]_<單位>`。
  - Gateway：`fwd_`＝已交給 Match（由 Match 分類）；`drop_`＝Gateway 執行的丟棄；沒有前綴的是守恆式的輸入（`received_datagrams`）。
  - 種類是資料包標頭的種類：`hello`、`input`、`actions`、`other`；Match 端的動作那一側用 `actions`。
  - 單位只用 `datagrams`、`packets`、`inputs`、`commands`、`batches`、`shots`、`snapshots`（Gateway 全域行拒收 Match 的 snapshot，D55 ②）、`acks`（Match 行：重設時丟掉的動作 ack，D55 ①）、`samples`（Match 行：movement slack 樣本，F2b-2）。
- 值一律是十進位整數（`^\d+$`），0 也照寫。
- **每行是自上一行以來的增量**；`final=1` 是最後一段。分析器加總同一程序、同一玩家的所有行，並要求視窗連續。缺 `final=1` 或缺某個視窗行時判成未判定或停止，絕不當成 0。
- Match 的 player=0 桶（未知玩家）每個視窗都寫一行。
- 新鍵只加在行尾；改變既有鍵的意義或順序要升版本。
- 不得含 `player send statistics`、`[ObjectFPS/PvP Match] network statistics`、`[ObjectFPS/PvP] worker statistics`（`network_statistics.py:17-19`），也不得含 `rate_accepted_packets=… rate_limited_packets=… max_session_window_packets=…` 的連續組合（`action_probe.py:594`、`run_gameplay_soak.py:29`）。
- Gateway 的寫出時機：每 10 s（`statisticsInterval`）一段，和 `gateway statistics`／`player send statistics` 同一個節奏。全域行在 `gateway statistics` 行之後，每段都寫；現有玩家的行在它的 `player send statistics` 行之後，每段都寫（全 0 也寫）；已移除、還有未取走計數的玩家接在後面（依 id 遞增），沒有計數的段不寫（等於 0）。`window_ms` 是自上一段（第一段自 Gateway 建立）以來的毫秒數，各段的連續以全域行判定。
- Gateway 的 `final=1`：由 `sync.Once` 在第一條結束路徑寫一次：Close（ctx 取消或 HTTP 結束；排在 `gateway transport` 行之後）或 runtime 失效（排在 `runtime disconnected` 行之後）。兩者都在 runtime link 關閉、writer goroutine 退出之後才取計數，所以 I0 在 final 之後對每位玩家精確成立。最後一段包含全域行，以及當時的玩家、該段有計數的玩家、之前寫過行的每一位玩家，所以每位寫過行的玩家恰好一行 `final=1`。final 之後不再寫 ingress 行；之後處理的資料包不屬於任何一段（已知缺口第 8 項）。

## 守恆式（golden 的每一行都成立）

- **IG**（Gateway 玩家行）：`received_datagrams`＝從 `fwd_main_input_packets` 到 `drop_admission_unknown_packets` 的所有 `*_packets`。每個屬於 session 的資料包恰好落在一種結果；舊序號的 Input 一律計在 `fwd_stale_sequence_input_packets`，不論走哪一條線或是否被已寫出去重。
- **Gateway 全域**：`received_datagrams`＝四個 `drop_*_datagrams`＋各玩家的 `received_datagrams`。
  - runtime link 在處理某個資料包時失效（`receivePacket` 回傳錯誤，接著 `runtimeFailed`）：這個資料包不計入玩家，改計 `drop_room_unavailable_datagrams`，兩條守恆式照樣成立。
- **I0**（Gateway 玩家行）：`fwd_handed_main_commands`＋`fwd_handed_rejected_lane_commands`＝從 `fwd_link_written_main_commands` 到 `drop_abandoned_on_close_commands` 的九個鍵。交給 link 的命令在 `input()`／`reject()` 成功返回時計一次，線間轉移不重算；寫出數在 `l.conn.Write` 成功之後計。
- **T3**：各玩家 `drop_rate_limited_*_packets`（含 hello）的總和＝全域行的 `drop_rate_limited_packets`（Session 限速器 `rate_limited_packets` 在該視窗的增量）。
- **I2**（Match 行）：
  - `received_commands`＝六個分類（`accepted_new`、`pending_copy`、`late_first`、`late_copy`、`resolved_copy`、`resolved_untracked`）的 `*_commands`＋各拒絕原因的 `*_commands`；
  - `received_inputs`＝`accepted_inputs`＋各拒絕原因的 `*_inputs`；
  - `received_actions_batches`／`_shots`＝`accepted_actions_*`＋各拒絕原因的 `*_actions_*`。
  - `handoff_rejected`、`rotation_discarded`、`leave_discarded`、`reset_discarded` 是收下之後才丟掉的命令（收到時已計為 `accepted_new`），不在 I2 之內；`reset_discarded_actions_shots`、`leave_discarded_actions_shots` 是收下之後、交給模擬之前被重設或 Leave／踢出丟掉的 shot（收到時已計在 `accepted_actions_shots`），也不在之內。`reset_discarded_actions_acks` 是重設時丟掉、還沒送出的動作 ack（每位玩家最多一個），也不在之內。
  - `handoff_rejected`、`staged_over_window` 與動作的 `full` 依程式推導走不到，應恆為 0；L2 的 0 不當證據。
- 不大於游標的命令依 Match 的替代紀錄（`MatchIngressLedger.hpp`）分類：
  - 每個被替代（Tick 解析時 host 還沒收到）的序號開一筆紀錄，計 `substituted_commands`。紀錄在游標超過序號 600 個（`ConnectionQualityWindowTicks`，每 Tick 解析一個序號）之後以 `aged` 關閉；每位玩家最多 1024 筆（保留期內最多 601 筆，所以 `overflow` 應恆為 0）。
  - `late_first`：紀錄還開著、第一次收到；`late_copy`：之後的副本；`resolved_copy`：帳本看著解析、仍在保留期內、沒有紀錄也沒有溢出的序號，也就是 Match 實際執行過的命令的副本；`resolved_untracked`：其他（超過保留期、溢出、帳本開始觀察之前解析的）。
  - `late_only_inputs`：每個命令都是 `late_first` 或 `late_copy` 的收下的 input。
  - slack 樣本路徑（上限 64 個被替代序號）不受帳本影響：保留期內超過 64 個的晚到只進帳本，不成為樣本。
- **J5a**（Match 行，所有紀錄都已關閉時）：`substituted_commands`＝`late_first_commands`＋七個 `unarrived_<關閉原因>_commands`。`unarrived_*` 只計第一份副本到達之前就關閉的紀錄，依關閉原因（`aged`、`epoch`、`life`、`removed`、`reset`、`overflow`、`end`）分開。每個跑次結束都經過 EOF→重設，紀錄以 `reset` 關閉；沒有經過重設就結束時，最後的 `final=1` 行之前以 `end` 關閉。所以加總整個程序時這條精確成立；單一視窗內可能有還開著的紀錄。注入時的判定式「late_first＋依 epoch／life／reset 關閉的數＋aged＝substituted，而且 never_arrived＝0」對應到 `unarrived_removed`＋`unarrived_overflow`＋`unarrived_end`＝0。
- **S1、S2**（Match 行，movement slack 樣本在 host 端的去向；已知缺口第 6 項的 host 端）：
  - 候選樣本：`slack_executed_samples`（Tick 解析時已收到的序號，slack＝解析時刻－第一份到達）與 `slack_late_samples`（被替代、仍在 slack 的 64 筆上限內的序號第一次晚到，slack 為負）。
  - 每次發布只帶上一次發布以來最小的一個候選；輸掉或被取代的計 `slack_merged_samples`。發布之前因換代、Leave、踢出、重設或程序結束而丟掉的計 `slack_discarded_samples`。進入發布的計 `slack_published_samples`，其中值為負的另計 `slack_published_negative_samples`。
  - 發布之後：IPC 取走之前被下一次發布覆寫的計 `slack_overwritten_samples`（`snapshot_` 被覆寫）；IPC 取走的計 `slack_taken_samples`；重設或程序結束時仍在等 IPC 的計 `slack_unclaimed_samples`。IPC 取走之後、寫出任何位元組之前被較新的 snapshot 取代（latest-wins）的計 `slack_coalesced_samples`，是 `slack_taken_samples` 的一部分。
  - S1：`slack_executed`＋`slack_late`＝`slack_merged`＋`slack_discarded`＋`slack_published`；S2：`slack_published`＝`slack_overwritten`＋`slack_taken`＋`slack_unclaimed`（單位都是 `_samples`）。沒有待發布或待取走的樣本時精確成立；每個跑次結束都經過重設，最後的 `final=1` 行之前也會把剩下的計入，所以加總整個程序時精確成立。
  - IPC 取走之後的去向（寫出、連線關閉時丟掉）與 Client 端（佇列溢出、settling 期間忽略）不在這裡，屬於第 10 批的觀測缺口第 3 項。計數是加總，看不出「連續幾次發布都帶負樣本」。
- 屬性鍵不在守恆式之內：`fwd_resolved_at_write_commands`、`fwd_retention_expired_commands`、`fwd_routed_*_inputs`、`late_only_inputs`、`slack_published_negative_samples`、`slack_coalesced_samples`。Gateway 全域行的 `drop_old_tick_snapshots`（tick 不晚於上一個，D1）與 `drop_regressed_snapshots`（某位玩家的 life、epoch 或已解析游標倒退，D2）也不在之內。
- I1（Gateway 寫出數＝Match 的 `received_commands`）跨兩個程序，不在 golden 之內，判定條件見 `docs/object_fps_pvp/plans/v7/08c-drop-report.md`。

## `match-ingress.jsonl`

- 每行一個 JSON 物件，開頭固定 `"schema":"object_fps_pvp.match_ingress","version":1,"kind":…`。解析端把它當物件讀，不依賴欄位順序（寫出端依下面列出的順序）。值是整數、字串或 `null`。
- `kind`：
  - `rejection`：`time_ns`、`player_id`、`reason`、`epoch`、`life`、`first_sequence`、`last_sequence`、`commands`、`cursor`、`current_epoch`、`current_life`。`reason` 是 Match 行的輸入拒絕原因之一（`IngressName(IngressInputRejection)`）。`player_id`、`epoch`、`life` 與序號照 input 原樣；沒有命令的 input，`first_sequence`、`last_sequence` 為 `null`；Match 不認得的玩家，`cursor`、`current_epoch`、`current_life` 為 0。每個（玩家、原因）每個統計視窗（Match 行的視窗）只寫前 16 筆，其餘只計 `suppressed`；Match 不認得的玩家共用 player=0 桶的 16 筆。動作的拒絕只計數，不寫細節。
  - `substitution`：每個被替代的序號一筆，在關閉時寫出：`player_id`、`epoch`、`life`、`sequence`、`substituted_ns`、`substituted_tick`、`first_arrival_ns`、`late_us`、`copies_after_first`、`reference_age_us`、`close`。沒有到達過的序號，`first_arrival_ns`、`late_us`、`reference_age_us` 為 `null`；帶來第一份的 input 指向 host 不認得的 snapshot 時，`reference_age_us` 也是 `null`。`close` 是 `aged`、`epoch`、`life`、`removed`、`reset`、`overflow`、`end` 之一（`IngressName(IngressSubstitutionClose)`）。
  - `trace_end`：最後一行，`records`（之前寫出的行數）、`suppressed`、`dropped`。
- 時間欄位（`*_ns`）是 steady_clock 的 `time_since_epoch`，和 movement trace 同一把尺；`late_us`＝(`first_arrival_ns`－`substituted_ns`)／1000（向零截斷），這個晚到成為 slack 樣本時（還在 64 筆內），樣本值 `movement_slack_us` 等於它取負號（在 ±1 s 的夾限內）。`substituted_ns`、`first_arrival_ns` 用 host 的時鐘讀數（和 slack 樣本同一個讀數）；`rejection` 的 `time_ns` 用 trace 自己的 steady_clock 讀數，拒絕不讀 host 的時鐘。
- 紀錄的順序是 host 保留的順序：拒絕在收到時、替代紀錄在關閉時。
- 檔名不得以 `commands.jsonl` 結尾（`command_evidence.py:231`、`command_evidence_v7.py:100`、`quad_evidence.py:66`、`:258` 用 glob `*commands.jsonl`）。
- 只有 `--movement-trace` 時才寫：預設在同目錄，`<x>-commands.jsonl`→`<x>-ingress.jsonl`，其他檔名加 `.ingress.jsonl`；`--ingress-trace <path>` 可以覆寫。只給 `--ingress-trace`、檔名以 `commands.jsonl` 結尾，或和 movement trace 同一個路徑，都是用法錯誤（exit 2）。
- 有界：host 的緩衝最多 16384 筆，Match 的主執行緒在每個統計視窗寫 Match 行時取走並寫檔；緩衝滿了計 `dropped`。`suppressed`、`dropped` 不讓 Match 失敗。
- 結束順序：IPC 與模擬停止 → 最後一次取統計（還開著的替代紀錄以 `end` 關閉）→ 取走緩衝 → 寫 `final=1` 行 → 寫剩下的紀錄與 `trace_end`。所以 `trace_end` 是最後一行，`end` 關閉的紀錄在它之前。
- I/O 失敗時 Match exit 1：開不了檔在啟動時；寫入失敗在結束、寫完 `trace_end` 之後。沒有 `trace_end` 的檔表示程序沒有正常結束，分析器判成未判定。

## 驗證

- Go：`apps/object_fps_pvp/gateway/ingress_statistics_test.go`（`go test ./gateway/...`）。
- C++：`tests/object_fps_pvp/IngressStatisticsTests.cpp`（CTest `object_fps_pvp.cpu`）；命令列與 exit code 在 CTest `object_fps_pvp.match_ingress_cli`（`match_ingress_cli_check.cmake`）。
- 修改樣本時，Go、C++ 與 Python 的測試必須同時通過；值的意義改變就升版本，而不是改寫 v1。
