# 第 08 批：Client 的網路 worker 改用 asio

狀態：**完成：L2 第 2 次通過**（2026-10-09；第 1 次在第 1 輪依停止條件停下，判定 2 修改後從頭重跑）（2026-10-09；範圍依 D43 與使用者的補充）。PR 線 P2（PR #73）。依賴第 07 批。
檔位 high；喚醒與期限的交接、generation 邊界、停止與離開房間的順序局部 xhigh（使用者 2026-10-09「檔位照建議來」，對這部分開 1 次 xhigh 審查）。

## 起因

- Client 的網路 worker 每輪睡 min(2 ms, 到下一次輸入期限)（`ClientConnection.cpp:607-610`）；P2-log 量到每個 Client 約 416 次／秒的喚醒。
- `SendInput`／`SubmitAction` 不會喚醒 worker，要等下一輪輪詢。
- 第 07 批的橫向對比顯示，clean-30 在 8 ms 主機狀態仍偏高，候選之一就是這個睡眠輪詢（交接的「未結事項」）。
  - 更正（2026-10-09，核對）：第三點引用的第 07 批橫向對比結論已更正：新 Match 的 clean-30 快慢兩群來自 probe 幀相位與結果的時槽，不是主機狀態；而且舊 worker 的送出側在 8 ms 狀態沒有變慢（a 的 P95 3.0～3.6 ms）。本批的動機仍成立的是 worker 輪詢的喚醒次數與 CPU。另外 `ClientConnection.cpp:607-610` 是 `660310e` 的行號，本批開始時（`cfc5542`）是 `:612-615`。

## 範圍

- worker 改為一條 asio io 執行緒（`GYO::Threads`，`gyo-client-net`）：
  - UDP 用 `async_receive_from`。
  - 每種期限各一個 `steady_timer`：Hello、輸入的重送與 token 補充、動作、收包逾時、大廳的輪詢。
- `SendInput`、`SubmitAction`，以及 `Drain` 讓 ACK 前進時，以 post 喚醒 worker。
- 速率語意不變：
  - 輸入：60 Hz 的 token bucket（`InputSendBurst`），沒變的視窗在期限時重送。
  - Hello：連線中每 1 秒，連線前每 250 ms。
  - 收包逾時 5 秒。
  - 動作：30 Hz 的最小間隔；依 D36（2026-10-09 使用者確認：「D36 照建議，開始實作」），符合資格時在 `SubmitAction` 當下送出。
- httplib 留在 worker：只在大廳切換與關閉時阻塞 UDP（現況）。
- **ACK 維持在主執行緒的 `Drain`**：
  - 範圍原本寫「L1 顯示語意不變時，改為網路角色收到裁決時前進」。
  - 改了語意會變：Match 會在 Client 的遊戲取走裁決之前就退休它們，而 Client 收到 `retired_through` 時會刪除對應的紀錄（`ClientConnection.cpp:353`），主執行緒尚未取走的裁決就會遺失。
  - 維持的代價只是主執行緒停頓時 ACK 晚一點送出，32 筆的動作視窗為上限，不影響正確性。
- P2-log 的 worker 統計：`wakes` 改為計算 io 處理函式的執行次數。
- **D43 加入**：
  - Gateway→Match 的 action batch 套用第 06 批的 I/2 容許（評估的選項 (1)）。
  - Client 在 worker 收到裁決時記下時刻，`Drain` 帶出；分析器 v7 新增不量化的「產生→worker 收到」指標，凍結的分析器不改（選項 (3)）。
  - 結果的事件驅動轉送另開第 08b 批（選項 (2)）。
- **使用者的補充（2026-10-09）**：
  - link 迴圈會被輸入、動作、結果叫醒（`runtime_link.go:246`），留著 I/2 等於允許待決的請求約 60 Hz 重送。兩個選項：(a) 嚴格的最小間隔 I 加到期計時器；(b) 保留 I/2 並記錄上限。**選 (a)**：和 IpcHost 的 action lane、第 08b 批的規格一致。
  - 實作：資格回到 `now.Before(w.nextSend)`；link 迴圈的固定 ticker 改為一個計時器，每次處理後設在「有待送內容的玩家中最早的 `nextSend`」（`nextActionDeadline`），沒有待送時以 I 為週期。

## 實作與 L1（2026-10-09）

- worker：一條 asio io 執行緒（`gyo-client-net`）。每次喚醒跑和舊版相同的處理（`Handle`、`Network`、`PollLobby`），所以資格判斷的程式不變；喚醒來源是 socket 可讀、其他執行緒的 post、以及一個等最早期限的計時器（`Arm`）。
- 計時器的精度：
  - 只用 asio 的計時器時，沒確認的輸入視窗重送是 57 次／秒：macOS 的計時器合併讓每次晚約 0.9 ms。
  - 改成「期限前 2 ms 由 asio 計時器叫醒，最後一段用角色的 Waiter（kqueue NOTE_CRITICAL）等」之後是 58 次／秒，和舊的輪詢版本相同（舊版暫時重建後量 10 次，都是 58）。io 執行緒在最後一段會阻塞最多 2 ms，和舊輪詢的最後一次短睡相同。
    - 核對（2026-10-09）：待量測。57／58 次每秒的量測日誌沒有保留；要確認就以 `1e81da9`（舊）、只用 asio 計時器的變體、`730984e` 各跑 `object_fps_pvp.worker` 10 次，保存 `unacknowledged_resends_1s` 的日誌（交使用者決定）。
- `worker_main` 的修改（使用者核准「worker_main 照建議改」）：
  - 記下 `firstActions` 之前先等 50 ms，讓 D36 在分配途中送出的那一批先抵達。「停頓期間每批 8 發、沒有 ACK」的斷言不變。
  - 新增（只加不放寬）：ACK 前進後 40 ms 內送出、單發 `SubmitShot` 在 40 ms 內送出（D36）、重複到達的結果不改變收到的時刻。前兩項之前先讓 worker 閒置 50 ms：第一版沒有閒置，殘留的期限計時器會帶出送出，突變存活，改了之後 killed。
- xhigh 審查（1 位）：沒有 blocker、major。
  - minor 2 件已修：handler 在 `Service` 之外拋出例外時，記錄並公開為連線失敗，io 繼續執行（原本 worker 會悄悄停擺）；`PostService` 改為 `noexcept`，post 失敗時清掉旗標。
  - nit：持鎖時 post（審查確認不會死結，維持）；計時器的精度（已如上處理）；除錯輸出殘留（每次量完都已還原，commit 前再確認沒有）。
  - 測試缺口：`Drain`／`SubmitAction` 的喚醒、`receivedAt` 已補斷言與突變；「新視窗只等 1 個 token」的期限規則沒有能單獨抓到的測試，留作已知的缺口。
  - 審查確認 `worker_main` 的失敗是測試的時間競態，不是產品的錯誤。
- 驗證：
  - 全量 CTest 70／70（新增 `object_fps_pvp.action_latency_v7`；權威 digest 不變）；`go vet`、`go test`、`go test -race`。
  - `object_fps_pvp.worker` 連跑 30 次全部通過；新舊版本各 40 次也都通過。
    - 核對（2026-10-09）：待量測。這兩次連跑的輸出沒有存檔，無法從證據確認；要確認就以 `ctest -R object_fps_pvp.worker --repeat until-fail:30` 在新舊建置各跑一次並存檔（交使用者決定）。
  - 突變 5／5 killed：`v7-08-link-no-deadline-timer`、`v7-08-link-half-tolerance`、`v7-08-drain-no-wake`、`v7-08-submit-no-wake`、`v7-08-receipt-overwritten`。
  - TSan：worker 驗收 3 次，沒有報告。
- 開發跑次（clean-30，1 次，只記錄）：Gateway→Match 的 action batch 間隔 P50 從約 68～70 ms 變成 33.4 ms；worker 收到裁決只比 relay 下行晚約 0.2 ms；worker 每個 Client 約 210～218 次／秒（P2-log 約 416）。
  - 更正（2026-10-09，L2 停下之後）：這次跑的是改成嚴格間隔之前、帶 I/2 容許的建置（最短間隔 16.8 ms，小於 I），所以 33.4 ms 是 I/2 下頻繁重送的結果，不是嚴格間隔的結果。L2 的判定 2 照這個數字訂，是錯的（見「L2 第 1 次執行」）。
- 開發跑次（矩陣與兩個 probe）：
  - 25 案矩陣全部通過。
  - 偶發失敗 2 次，原因未確定：
    - `run_network.py` 1 次「an application stall reset the movement epoch」。
    - `backpressure_probe.py` 的 host-ipc-250ms 1 次「Unexplained epoch reset for player 1 (reason backlog)」。重設前，sim 角色的 generation 間隔縮成 13.3 ms（相位追蹤加速）。
  - 為了判斷是不是新 worker 造成的，用 `1e81da9`（本批之前的頭）重建舊的 network／timing probe，配第 07 批 L2 的 Gateway，和新版交錯跑（`ab/baseline.sh`）：

    | | `run_network.py`（失敗／次數） | `backpressure_probe.py`（失敗案例／案例數） |
    |---|---|---|
    | 新 worker | 1／12 | 1／19（失敗的那次在第 1 案停下） |
    | 舊 worker | 0／8 | 0／18 |

  - 判讀：次數太少，分不出新舊（1／12 對 0／8，Fisher 精確檢定 p＝1）。不能說是新 worker 造成的，也不能排除；失敗的跑次保留，沒有重跑來取代。
    - 更正（2026-10-09，核對）：`ab/baseline.sh` 的「舊」是舊 probe 配第 07 批 L2 的 Gateway，「新」大多是新 probe 配第 08 批的 Gateway（含 D43 的 link 修改），只有 `new-oldgw-1/2` 是新 probe 配舊 Gateway；所以比較的是「worker＋link 修改」整組，不是只有 worker。交錯也只有前 3 對 `run_network.py`，old-net-4～8 與 old-bp-1～3 是連續跑的。分不出新舊（Fisher p＝1）的結論不變。
- 證據：`build/target/_build/test/logs/pvp-v7-batch08-20261009/`（git 忽略）：`mutations.json`、`ctest-full.log`、`tsan-worker-*.txt`、`dev-clean-30/`、`dev-matrix/`、`dev-network/`、`dev-backpressure/`、`ab/`（舊 probe 與交錯跑次）；`dev-*.argument-error.txt` 是第一次因 zsh 沒有拆開參數、runner 立刻結束的紀錄。

## 驗收

- **L1**：
  - 全量 CTest，其中 `object_fps_pvp.worker` 的斷言不放寬（停止條件）。
  - 新的測試：`SendInput`／`SubmitAction` 的即時喚醒、各期限的節拍、generation 邊界、停止時的離開。
  - 突變；TSan。
- **開發跑次**：25 案矩陣、`backpressure_probe.py`、`run_network.py`。
- **L2**：L1 完成後寫事前宣告，送使用者核准。之後做第 07 批之後預定的橫向對比（Client 端，clean-30）。

## 評估（D36 確認前）

使用者要求（2026-10-09）：第 07 批 L2 的 clean-30 在同一個 8 ms 主機狀態下，after 分成快（第 1、4、5 輪：Client P95 約 67～70 ms、Match P95 約 9～13 ms）與慢（第 2、3、6 輪：約 100 ms、約 20 ms）兩群，before 也一樣。只用既有的證據分析，不改產品程式，不跑新的量測。

- 更正（2026-10-09，核對）：第 1 輪的 clean-30（兩棵 tree）是 4 ms 狀態（TimerBaseline 20.2／20.5 ms），after 的第 4 輪 22.0 ms 剛好在門檻上；8 ms 狀態下明確的快群是 after 第 5 輪（23.7 ms）。after 的快慢兩群不是主機狀態造成的結論不變。「before 也一樣」只對 `legal_client_p95_ms` 成立（before 第 3 輪在 8 ms 狀態也是快群）；before 的 `legal_match_p95_ms` 只有 4 ms 的第 1 輪是快的（11.5 ms），8 ms 的五輪是 18.2～22.5 ms，是舊 Match 的晚醒（見第 07 批的更正）。

### 1. 量測工具的量化

- probe 每一幀開頭取一次時刻（`gameplay_action.hpp:114` 的 `ns`）。「送出動作」（`:163`）與「取到裁決」（`:128`）都記這個幀開始的時刻，所以分析器的 `legal_client_p95_ms`（`gameplay_evidence.py:184`）是幀長的整數倍。`legal_match_p95_ms`（`:183`）的終點是 Match 的 `snapshot_produced`，不量化，起點仍是幀開始。
- 驗證：第 06、07 批 L2 的 clean-30／clean-60 每個合法動作的「取走−送出」除以幀長，全部接近整數（偏離最大 0.28 幀，來自幀開始的抖動）。clean-30 只出現 1、2、3 幀。P95 約 67～70 ms 就是 2 幀，約 100 ms 就是 3 幀：快慢兩群是 P95 落在第 2 幀還是第 3 幀。
  - 更正（2026-10-09，核對）：「每個合法動作都接近整數幀」只對 clean-30 成立（偏離最大 0.285／0.278 幀，P95 0.11／0.15 幀）。clean-60 的偏離中位數 0.06～0.08 幀、P95 0.25～0.33 幀、最大 0.44～0.46 幀，第 07 批有 14％ 的動作偏離超過 0.25 幀：60 FPS 的實際幀長不固定，`legal_client_p95_ms` 仍是兩個幀開始之間的時間，但不再接近名目幀長的整數倍。「clean-30 只出現 1、2、3 幀」只對第 07 批 L2 成立；第 06 批 L2 的 clean-30 有 4 筆 4 幀（before 第 2、5、6 輪），第 5 輪 before 的 129.9 ms 就是 4 幀。clean-30 的快慢兩群是 P95 落在第 2 或第 3 幀的結論不變。

### 2. 逐段拆解

時間戳都在 C++ `steady_clock` 的時鐘上（`test_steady_clock.py` 讓 relay 用同一個時鐘）。

| 段 | 起點 → 終點 | 紀錄 |
|---|---|---|
| a | probe 送出（幀開始）→ relay 上行第一次看到這個動作 ID | 有（`actions.jsonl`、`relay.json` kind 6） |
| — | Client 送出 | **缺**（Client 的 trace 只記移動） |
| — | Gateway 收到、送往 Match；Match 收到動作 | **缺** |
| b | relay 上行 → Match 裁決 Tick 的 `snapshot_produced` | 有（`match-commands.jsonl`） |
| — | Match 送出結果；Gateway 收到結果 | **缺** |
| c | Match 裁決 Tick → relay 下行第一次看到這個裁決 | 有（`relay.json` kind 7） |
| — | Client worker 收到結果 | **缺** |
| d | relay 下行 → probe 取走（幀開始） | 有 |

第 07 批 L2 的 clean-30，合法動作逐筆合併（中位數／P95，ms）：

| tree／群 | 輪 | 動作數 | a | b | c | d | Match（送出→Tick） | Client（送出→取走） |
|---|---|---|---|---|---|---|---|---|
| after／快 | 1、4、5 | 102 | 2.1／3.2 | 3.9／9.7 | 18.1／51.2 | 9.9／28.3 | 5.1／11.6 | 35.8／67.3 |
| after／慢 | 2、3、6 | 102 | 2.6／3.4 | 2.7／16.7 | 34.3／50.8 | 28.7／30.4 | 4.3／19.9 | 66.6／99.6 |
| before／快 | 1、3 | 68 | 2.0／3.2 | 7.3／15.3 | 46.2／49.0 | 9.5／12.3 | 10.4／17.4 | 66.0／70.8 |
| before／慢 | 2、4、5、6 | 136 | 2.1／3.2 | 14.0／19.9 | 30.8／51.3 | 31.9／35.7 | 16.4／22.0 | 66.7／100.9 |

- **約 10 ms（Match 之前）的差距在 b**（relay 上行 → 裁決 Tick）：after 的 P95 9.7 對 16.7 ms，before 15.3 對 19.9 ms。a 在兩群相同（約 2～3 ms）。b 裡面沒有時間戳，但 Gateway→Match 的 action batch 間隔 P50 在所有跑次都約 68～70 ms（P2-log 的 `link_action_interval_ms`），也就是兩個 ticker：見下面的節拍清單第 3 項。
  - 更正（2026-10-09，第 08 批 L2 停下之後；上文保留原文）：「68～70 ms＝兩個 ticker」沒有根據。`link_action_interval_ms` 同時計入請求、重送與只有 ACK 前進的批次（`action_delivery.go` 的 `actionBatch`：`acknowledged > retired` 也會送）。動作之間約隔 265 ms，所以 P50 量到的是同一個動作前後兩批的間隔，不是 link 的節拍。改成嚴格間隔加計時器之後，P50 仍是 68.4 ms，最短間隔 33.4 ms（≥I）。所以 b 的差距不能用這個指標歸因到節拍 3；節拍 3 的「每隔一次 ticker」是讀程式得到的，只影響重送與 ACK 的批次。
  - 更正（2026-10-09，核對）：after 的差距只在 P95（9.7 對 16.7 ms），中位數反而慢群較小（3.9 對 2.7 ms）：慢群裡有幾筆動作的 relay 上行剛好落在 Tick 之後，要多等約 1 Tick（b≈16.7 ms），34 筆的 P95 由這幾筆決定。before 的差距（15.3 對 19.9 ms）主要是主機狀態：舊 Match 在 8 ms 狀態下 Tick 晚到，b 的中位數 9～15 ms，而 4 ms 的第 1 輪是 5.1 ms；before 的快群混了 4 ms 的第 1 輪與 8 ms 的第 3 輪。
  - 補充更正（2026-10-09，核對）：(1)「所有跑次都約 68～70 ms」本身也不成立。本評估分析的第 07 批 L2 clean-30，玩家 1 每個視窗的 P50：after 68.8～70.8 ms，before 36.3～69.3 ms；玩家 2 每個視窗只有 4～6 批（33.9～102.7 ms）。第 06 批 L2 before（P2-log 的 Gateway）玩家 1 是 43.6～102.6 ms，而 link 程式相同的第 06 批 Gateway 是 65.2～70.2 ms：這個指標會隨結果通道與 ACK 的時機改變。(2) 更正中「節拍 3 的『每隔一次 ticker』只影響重送與 ACK 的批次」也沒有根據：link 迴圈除了 ticker，每收到輸入、動作、結果都會被叫醒（`runtime_link.go:163`、`:246`，`action_delivery.go:158`），nextSend 一過，下一次喚醒就送出，不必等下一個 ticker；帶 I/2 的建置最短間隔 16.8 ms 也顯示喚醒很密。「動作之間約隔 265 ms」對玩家 1 成立（中位數 266 ms；玩家 2 約 815 ms）。
- **約 33 ms（Match 之後）的差距分在 c 與 d**：
  - c 的中位數只有約 18 ms 與約 34 ms 兩個值，由 Gateway 結果 ticker 相對 Match Tick 格點的相位決定：relay 下行相位約 17.5 ms 時是 18 ms，約 1 ms 時多等一個 ticker，變成 34 ms。
    - 更正（2026-10-09，核對）：(1)「c 的中位數只有約 18 與約 34 ms 兩個值」只對 after（第 07 批的 Match）成立；before 的 c 中位數是 28～48 ms，before 快慢兩群的差在 d（9.5 對 31.9 ms），c 反而快群較長（46.2 對 30.8 ms）。(2)「relay 下行相位」是 `segments.py:74` 以（下行−裁決 Tick）對 2 Tick 取餘，等於 c 對 2 Tick 取餘，所以「相位約 17.5 ms 時 c＝18 ms」是恆等式，不能說明原因。(3) 第 07 批的 Match 之後，Gateway 的結果幾乎都在 snapshot 轉送後 1 ms 內送出（clean-30 after 各跑次中位數 99.9％，before 1.8％；`pvp-v7-08b-plan-20261009/adversarial/lock.stdout.txt`），送出時刻跟著 Tick 走，不是自由相位的 ticker；鎖定的原因未查。(4) 每個動作的 c 落在 Tick 整數倍附近：clean-30 的同一跑次內多半是相隔 2 Tick 的兩個值（第 08 批 L2 多數跑次是 1 Tick 與 3 Tick，3 Tick 約 51 ms 佔 21～56％），也有跑次混入另一個奇偶（第 07 批 after 第 6 輪 0／1／2／3 Tick 為 4／11／17／2 筆）；clean-60 分布在 0～3 Tick，中位數 2 Tick（34 ms）。所以 c 的中位數是 18 還是 34，取決於該跑次裁決 Tick 的奇偶與結果的送出時槽，而且同一跑次內也不是固定的一個值。
  - d 是 probe 的幀相位（10 對 29 ms）。
  - 兩者加起來決定裁決落在第 2 還是第 3 幀。
- **clean-60**：新 Match（第 07 批 after）的 Match P95 每一輪都是 8.8～11.9 ms，不論主機狀態。舊 Match 有 7.6 ms 與 21～25 ms 兩群，而且 4 ms 狀態也出現 21.0 ms（第 07 批 L2 before 第 2 輪）。
  - 更正（2026-10-09，核對）：「4 ms 狀態也出現 21.0 ms（第 07 批 L2 before 第 2 輪）」是主機狀態的分類問題：那一次 TimerBaseline 是 20.4 ms，但 Match 自己的晚醒有 19.7％ 落在 4～8 ms（其他 4 ms 跑次 ≤3％，8 ms 跑次 ≥41％），主機在這一輪內轉入 8 ms 狀態。舊 Match 的 clean-60 兩群（7.6 ms 與 21～25 ms）和 Match 自己的計時狀態一致。新 Match 不論狀態 8.8～11.9 ms 的結論不變。
- 證據：`build/target/_build/test/logs/pvp-v7-08-eval-20261009/`（git 忽略）：`segments.py`（`6a3fa237…`）、`groups.py`（`dd64a4c3…`）、`segments.json`（`5b13a879…`）、`groups.json`（`56531256…`）。

### 3. 路徑上 30 Hz／30 FPS 的節拍

| # | 節拍 | 位置 | 性質 | 第 06 批 |
|---|---|---|---|---|
| 1 | probe 的幀（clean-30 為 30 FPS）：送出動作與取走裁決都在幀上 | `gameplay_action.hpp:167` | 固定格點（落後時重新錨定到當下） | 不適用 |
| 2 | Client 的動作送出：最小間隔 | `ClientConnection.cpp:364`、`:386` | 送出後重新錨定（`now+I`）；worker 每 ≤2 ms 輪詢檢查，所以沒有「每隔一次」 | 未改 |
| 3 | Gateway→Match 的 action batch | `runtime_link.go:238`（ticker）、`:246`（新動作的 wake）、`action_delivery.go:175`（`now.Before(w.nextSend)`）、`:329`（寫出後重新錨定） | 固定 ticker＋寫出後重新錨定：**有第 06 批修掉的同一個「每隔一次 ticker」缺陷**。待決的動作會持續重送，所以新動作通常也要等到 `nextSend` 之後的 ticker（最多約 2I）。間隔 P50 約 68～70 ms 佐證 | **未修** |
| 4 | Match→Gateway 的結果（IpcHost 的 action lane） | `IpcHost.cpp:454-455`、`:424` | 選出時重新錨定（`now+I`），第 07 批起由計時器在期限時喚醒，沒有「每隔一次」，但相位固定 | 第 07 批改為事件驅動 |
| 5 | Gateway→Client 的結果 | `server.go:585`（ticker）、`action_delivery.go:212`（I/2 容許）、`:343`（寫出後重新錨定） | 固定 ticker＋寫出後重新錨定 | 第 06 批修了「每隔一次」，相位仍固定 |
| 6 | Client 取走裁決：遊戲的幀（產品）／probe 的幀 | `ClientConnection.cpp` `Drain`；`gameplay_action.hpp:122-128` | 幀節拍 | 不適用 |

- 更正（2026-10-09，核對，第 3 列）：「有第 06 批修掉的同一個『每隔一次 ticker』缺陷」「新動作通常也要等到 nextSend 之後的 ticker（最多約 2I）」「間隔 P50 約 68～70 ms 佐證」都不成立。link 迴圈每收到輸入、動作、結果都會被叫醒（`runtime_link.go:163`、`:246`；新動作本身也會 `notify`，`action_delivery.go:158`），不是只有 ticker；第 06～08 批 L2 clean 的新動作到達 relay 時，link 上可能還有待送內容的只有 0～2 筆（第 06、07 批各 192 筆中；第 08 批 L2 前一個上行 kind 6 的中位數約 200～230 ms，<34 ms 的 0 筆），裁決 Tick 的 k 在 before 幾乎全為 0（clean-30 {0:403, 1:5}）。間隔 P50 混入 ACK 批次與兩位玩家，不是 link 節拍（見本節第 2 點的補充更正）。§4 的「剩下的：b 裡的節拍 3」同樣不成立。
- 更正（2026-10-09，核對，第 4、5 列）：第 5 列寫「固定 ticker＋寫出後重新錨定，相位固定」，但第 07 批的 Match 之後，Gateway 的結果幾乎都在 snapshot 轉送後 1 ms 內送出（`pvp-v7-08b-plan-20261009/adversarial/lock.stdout.txt`），實際送出跟著 Tick 走。第 4 列的 lane 在每次選出時以選出時刻＋I 重新錨定，包括只帶 retired 的重複包與沒有內容的選出（`IpcHost.cpp:454-455`），而且任何一次 Pump 都可能選出（`:410-411`），不只是計時器到期；同一跑次內的 c 是相隔 2 Tick 的兩個值的混合（見第 2 節 c 的更正），可見結果趕不趕得上第一個時槽在跑次內會變，「相位固定」不成立。原因未查。

- 這些節拍彼此獨立、同頻，每個跑次的相位固定。所以有時剛好趕上、有時剛好錯過，和 v6 D21「命令跟著畫面幀」是同一類問題。
  - 更正（2026-10-09，核對）：「這些節拍彼此獨立、每個跑次的相位固定」不成立：第 07 批的 Match 之後，Gateway 結果與 snapshot 轉送鎖在一起（節拍 5 跟著 Tick）；同一跑次內 c 是相隔 2 Tick 的兩個值的混合（第 08 批 L2 clean-30 較大值佔 21～56％），節拍 4、5 相對 Tick 的關係在跑次內會變；probe 的幀格點在跑次中途也會重新錨定（`gameplay_action.hpp:170`，第 08b 批記錄 9 個跑次中途 ≥5 ms）。對舊 Match 而言，`legal_match_p95_ms` 的快慢主要是主機的計時狀態，不是相位。
- Match 的 Tick 是 60 Hz，snapshot 每 Tick 一份（`Movement.hpp:21`），不在這張表裡。

### 4. 對 D36 的影響

- D36 只影響 a 的開頭：現在 worker 每 ≤2 ms 就會看到新動作，a 的中位數本來就約 2 ms。D36 最多縮短約 1～2 ms（D36 表上寫的「平均約縮短 16 ms」與實測不符；那個估計的前提沒有紀錄）。
- 剩下的：b 裡的節拍 3（Gateway→Match，每隔一次 ticker）、c 的節拍 4、5 的相位、d 的幀相位。
  - 更正（2026-10-09，核對）：「b 裡的節拍 3（每隔一次 ticker）」不成立，見上面第 3 節表後的更正（第 3 列）。

修法選項：

| 選項 | 內容 | 批次 | 停止條件 | Architecture Delta | 建議 |
|---|---|---|---|---|---|
| (1) | Gateway→Match 的 action batch 套用第 06 批的 I/2 容許 | 08（併入；Go 小改） | 不改 wire、不碰權威 digest；relay 的「每秒 ≤31」只量 Client↔Gateway，不受影響；`worker_main` 不受影響 | 否 | 建議 |
| (2) | 結果改為事件驅動轉送：Match 產生裁決時立即送（lane 符合資格時），Gateway 收到新裁決時立即轉送（保留 30 Hz 最小間隔與 I/2 容許），不再等固定 ticker 的相位 | 新批次 08b（第 09 批之前） | 不改 wire、不碰權威 digest；relay 的「結果每秒 ≤31」要重新確認（最小間隔保留，應維持 ≤31）；`worker_main` 不受影響 | 否（產品內） | 建議：第 09 批要用這些通道推導 FireGate 常數，先消除相位比較好 |
| (3) | 量測：Client 在 worker 收到裁決時記時間戳，Drain 帶出；新的分析器 v7 指標「產生→worker 收到」不量化（凍結的分析器不改） | 08 或第 10 批（紀錄） | 不改 wire；分析器只新增 | 否 | 建議放進 08（和 worker 的改寫同處） |
| (4) | 產品的幀節拍（遊戲在幀上取走裁決） | 不處理 | — | — | 這是遊戲本身的體驗，不是傳輸的延遲；保持 |

### 5. 之後的量測設計（clean-30 的相位雙峰）

- 每個跑次記錄相位：probe 幀相對 Match Tick 格點、relay 下行相對 Tick 格點（本評估的 `segments.py` 已能從既有紀錄算出）。
- 判定不用單一跑次的 `legal_client_p95_ms`，改用不量化的段落：「送出→relay 下行」（既有紀錄即可），加上選項 (3) 之後的「送出→worker 收到」。
- clean-30 至少 12 次，報告各跑次的分布，並依相位分組。
- 凍結的分析器不改；新指標放在分析器 v7 或新的分析腳本。
- 要新的跑次才能回答的：b 段內部（Gateway 收到、送往 Match、Match 收到）與 c 段內部（Match 送出、Gateway 收到）沒有時間戳，要在 Gateway 與 Match 加 trace 才能拆開；這次不跑。

## L2 的事前宣告（2026-10-09 使用者核准）

- **目的**：
  - 確認 b 段（relay 上行 → 裁決 Tick）的約 10 ms 差距，是否因 link 的修正而縮小。
  - 確認 Client 的 worker 改為事件驅動後，gameplay 沒有變差，喚醒與 CPU 減少。
  - 留下不量化的「產生→worker 收到」（after）作為第 08b、09 批的基準。
- **產物**：before 是第 07 批的頭建置的 Gateway 與 action probe（probe 含 Client 端的程式），after 是本批的頭建置的版本。Match 共用（兩者之間 Match 的原始碼沒有差異）。雜湊寫進 `artifacts.sha256`，跑次前後各核對一次。
- **主機**：沿用之前 L2 的做法（每輪前後 5 秒 sleeper、記錄背景的高 CPU 程序、機器閒置）；每個跑次依 probe 的 TimerBaseline 記錄主機狀態。
  - 更正（2026-10-09，核對）：TimerBaseline 是 probe 連線前量的 3 秒（`timer_baseline.hpp:3-5`），抓不到跑次中的轉換，見[第 07 批](07-match-tick-ipc.md) L2 宣告「主機」的更正。本批 L2 的主機都在 4 ms 狀態，沒有受影響。
- **案例與順序**：clean-30 12 輪；clean-60、upstream-250ms、gateway-250ms 各 6 輪。每輪 before 與 after 各 1 次，奇數輪先跑 before。共 60 次，約 30 分鐘。
- **判定**（全部成立才算通過）：
  1. after 的所有跑次，gameplay 判定都通過。
  2. after 的 Gateway `link_action_min_interval_ms`，在有樣本的每個視窗都 ≥ I−1 ms（32.3 ms）。（2026-10-09 修改；原文：「after 的 Gateway `link_action_interval_ms` P50，在有樣本的每個視窗都在 33.3±2 ms。」理由見「L2 第 1 次執行」。）
- **記錄並比較**（使用者的補充）：
  - 每個合法動作的相位：probe 幀相對 Match Tick 格點；relay 下行相對 Tick 格點。
  - b 段依相位分組的中位數與 P95，before 對 after；同時列出 probe 幀的相位，因為 b 段的差距也可能來自 Tick 的相位。
  - a、c、d 段與「產生→worker 收到」（after）。
  - worker 的喚醒與 CPU、`legal_match_p95_ms`／`legal_client_p95_ms`、relay 的「任一秒結果數」。
- **停止條件**：跑次錯誤時停下，不自行重跑；after 出現 gameplay 失敗、判定 2 不成立、產物雜湊不符時停下回報。
- **證據**：`build/target/_build/test/logs/pvp-v7-batch08-l2-<日期>/`。

### L2 第 1 次執行（2026-10-09，第 1 輪停下）

- 證據：`build/target/_build/test/logs/pvp-v7-batch08-l2-20261009/`（`run.py`、`judge.py`、`progress.jsonl`、`judgement.json`；宣告的雜湊在 `declaration.sha256`，產物在 `artifacts.sha256`）。
- 第 1 輪 clean-30：before、after 的 gameplay 都通過（主機都是 4 ms 狀態）。after 的 `link_action_interval_ms` P50 是 68.4／66.6 ms，判定 2 不成立，依停止條件停下，沒有重跑。
- 原因是判定 2 的前提錯了，不是產品退步：
  - 判定 2 的 33.3 ms 來自開發跑次，但那次跑的是帶 I/2 的建置（見「實作與 L1」的更正）。
  - 這個指標同時計入請求、重送與 ACK 的批次；動作之間約隔 265 ms，P50 量到的是同一個動作前後兩批的間隔。before（第 07 批的 Gateway）同樣是 69.4 ms。
  - 嚴格間隔本身成立：after 的最短間隔 33.4／35.4 ms（≥I）；帶 I/2 的建置是 16.8 ms。
- 這一輪只記錄的數字（各 1 次，不下結論）：

  | | b 中位數 | probe 幀相位中位數 | `legal_client_p95_ms` | worker 喚醒／秒 | worker CPU／秒 | 產生→worker 收到（P50／P95） | relay 任一秒結果數 |
  |---|---|---|---|---|---|---|---|
  | before | 7.8 ms | 7.0 ms | 68.2 | 425／427 | 0.019 | — | 31 |
  | after | 10.2 ms | 6.2 ms | 69.4 | 222／211 | 0.008～0.012 | 29.1／64.3 ms | 31 |

- 判定 2 的修改（使用者 2026-10-09 決定「判定 2 照建議改，從頭重跑 L2」）：改成「after 每個有樣本的視窗，`link_action_min_interval_ms` ≥ I−1 ms（32.3 ms）」。這是 link 嚴格間隔的直接證據；帶 I/2 的建置會不成立。其餘的宣告不變，從頭重跑 60 次，證據放新的目錄；第 1 次的目錄保留。

### L2 第 2 次執行（2026-10-09，通過）

- 證據：`build/target/_build/test/logs/pvp-v7-batch08-l2-20261009-2/`（產物與第 1 次相同，雜湊一致；`judgement.json`）。
  - `judge.py` 在跑次之後修了一處：統計時略過沒有值的項目（upstream／gateway 的案例沒有 legal 延遲，原本會當掉）。只影響記錄，不影響判定；修改後的雜湊附在 `session.sha256`。
- **判定：通過**。60 次全部跑完：
  - 判定 1：after 的 gameplay 全部通過（before 也全部通過）。
  - 判定 2：after 每個視窗的 link 最短間隔都 ≥ 32.3 ms（最小 33.4 ms）。
- 主機狀態：before 30 次都是 4 ms 狀態；after 28 次 4 ms、2 次 8 ms（第 3 輪的 upstream、gateway）。
- 各段的中位數（ms；a＝產生→relay 上行、b＝relay 上行→裁決 Tick、c＝Tick→relay 下行、d＝relay 下行→probe 取得）：

  | 案例 | | a | b（P50／P95） | c | d | 產生→worker 收到（P50） | worker 喚醒／秒 | worker CPU／秒 |
  |---|---|---|---|---|---|---|---|---|
  | clean-30（12 輪） | before | 2.1 | 6.9／11.2 | 17.7 | 7.5 | — | 431 | 0.02 |
  | | after | 0.35 | 10.5／13.7 | 17.7 | 5.3 | 30.5 | 217 | 0.01 |
  | clean-60（6 輪） | before | 2.2 | 8.9／11.1 | 34.2 | 4.9 | — | 433 | 0.02 |
  | | after | 0.33 | 12.1／15.2 | 34.2 | 3.6 | 46.1 | 214 | 0.01 |
  | upstream-250ms | before | 2.2 | 8.6／13.1 | 34.2 | 5.1 | — | 433 | 0.02 |
  | | after | 0.33 | 12.7／14.9 | 34.2 | 3.3 | 46.5 | 217 | 0.01 |
  | gateway-250ms | before | 2.0 | 8.5／11.3 | 34.2 | 5.3 | — | 433 | 0.02 |
  | | after | 0.33 | 12.3／14.3 | 34.2 | 3.5 | 46.4 | 217 | 0.01 |

- b 依 probe 幀相位（相對 Match Tick 格點，每 1/4 Tick 一組）分組的中位數（ms；n＝動作數）：

  | 案例 | 相位組 | before | after |
  |---|---|---|---|
  | clean-30 | 0～4.2 ms | 11.4（14） | 12.9（97） |
  | | 4.2～8.3 ms | 7.6（246） | 10.2（296） |
  | | 8.3～12.5 ms | 5.9（114） | 6.9（15） |
  | | 12.5～16.7 ms | 1.3（34） | — |
  | clean-60 | 0～4.2 ms | 10.9（29） | 13.2（103） |
  | | 4.2～8.3 ms | 8.6（175） | 11.1（101） |

  - 更正（2026-10-09，核對）：這張表的「probe 幀相位」是 `segments.py:73` 的（送出−裁決 Tick 的 snapshot_produced）對 1 Tick 取餘（`judge.py:74` 以 1/4 Tick 分組）。新 Match 的 Tick 格點是等間隔的，所以它確實是幀相對 Tick 格點的位置；但第 08 批 L2 的 k 幾乎全為 0，相位＋a＋b 恆等於 1 Tick（本表的動作在各組 100％ 成立，相位 12.5～16.7 ms 那組 71％）。所以同一相位組內 b 的差距就是 a 的差距加上組內相位分布的差：例如 clean-30 的 0～4.2 ms 組，a＋b 兩邊都是 13.18 ms，b 多 1.56 ms、a 少 1.63 ms。「同一個相位組內也多 1.0～2.6 ms」是 D36 縮短的 a 移到 b，不是 b 裡另有延遲。

- 觀察（只記錄，原因未查）：
  - **宣告的目的 1（b 段的差距是否縮小）：沒有縮小，b 反而變長**。各案例的中位數多約 3.5 ms；同一個相位組內也多 1.0～2.6 ms。
  - a 縮短約 1.8 ms（D36：送出不再等 worker 輪詢），所以 a＋b 合計多約 1.7～3.7 ms。`legal_match_p95_ms` 的中位數 clean-30 11.8→13.9、clean-60 13.1→14.8。
    - 更正（2026-10-09，核對）：a＋b（送出→裁決 Tick）各案例中位數的差是 +1.6～+2.4 ms（clean-30 8.57→10.94、clean-60 10.90→12.51、upstream-250ms 10.98→13.13、gateway-250ms 10.51→12.64），不是 1.7～3.7 ms。其餘數字正確。
  - 兩邊 probe 幀相位的分布不同（after 集中在前兩組），所以分組內的比較也不完全對等。
  - b 裡面沒有時間戳（Gateway 收到、送往 Match、Match 收到），這次的證據不能分辨是 link 的嚴格間隔（新的請求碰到 ACK 批次剛送出，要等到 `nextSend`）還是其他原因。這只是候選之一，未驗證。要分辨需要 Gateway 與 Match 的 trace（第 10 批）或另外的量測。
  - 處理（使用者 2026-10-09 決定）：併進第 08b 批的 ultracode 規劃一起調查。
  - 更正（2026-10-09，核對）：b 變長的原因已由第 08b 批回答：after 的 k 全為 0，b 等於 relay 上行到下一個 Tick 的剩餘時間；候選的 link 嚴格間隔對 b 被否定（新動作到達時前一個上行 kind 6 的中位數約 200～230 ms，<34 ms 的 0 筆）。b 變長來自 a 縮短（D36）與 probe 起點相對 Tick 提早，見[第 08b 批](08b-event-driven-results.md)「b 段變長的調查」。宣告目的 1 的前提也不成立：第 07 批 after 的 b 差距只在 P95（少數跨過 Tick 邊界的動作），before 的差距主要是舊 Match 在 8 ms 狀態的晚醒，都不是 link 節拍。
  - c 不變，d 縮短約 1.3～2.2 ms，客戶端合計（產生→probe 取得）的中位數持平（clean-30 34.9→35.8、clean-60 48.8→49.6）。
  - worker 的喚醒約減半（431→217／秒），CPU 約減半。relay 任一秒的結果數 after 都是 31；before 的 clean-60 有一次 33。
- Client 端的主機狀態橫向對比（第 07 批之後預定）：clean-30 的 24 次全部落在 4 ms 狀態，沒有 8 ms 狀態的樣本，這次無法對比。clean-30 的 `legal_client_p95_ms` 在兩邊都是 67～71 ms，沒有出現第 07 批時的快慢兩群。

## 停止條件

- `worker_main` 的斷言需要放寬。
- 需要改 wire。
- 權威 digest 改變。
