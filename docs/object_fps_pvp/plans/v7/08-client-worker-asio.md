# 第 08 批：Client 的網路 worker 改用 asio

狀態：**實作暫停，評估完成，等使用者決定範圍**（2026-10-09）。worker 的改寫在本機的 stash（`batch08-wip`），`object_fps_pvp.worker` 目前失敗（「unexpected stalled action batch」），尚未調查。PR 線 P2（PR #73）。依賴第 07 批。
檔位 high；喚醒與期限的交接、generation 邊界、停止與離開房間的順序局部 xhigh（使用者 2026-10-09「檔位照建議來」，對這部分開 1 次 xhigh 審查）。

## 起因

- Client 的網路 worker 每輪睡 min(2 ms, 到下一次輸入期限)（`ClientConnection.cpp:607-610`）；P2-log 量到每個 Client 約 416 次／秒的喚醒。
- `SendInput`／`SubmitAction` 不會喚醒 worker，要等下一輪輪詢。
- 第 07 批的橫向對比顯示，clean-30 在 8 ms 主機狀態仍偏高，候選之一就是這個睡眠輪詢（交接的「未結事項」）。

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

## 驗收

- **L1**：
  - 全量 CTest，其中 `object_fps_pvp.worker` 的斷言不放寬（停止條件）。
  - 新的測試：`SendInput`／`SubmitAction` 的即時喚醒、各期限的節拍、generation 邊界、停止時的離開。
  - 突變；TSan。
- **開發跑次**：25 案矩陣、`backpressure_probe.py`、`run_network.py`。
- **L2**：L1 完成後寫事前宣告，送使用者核准。之後做第 07 批之後預定的橫向對比（Client 端，clean-30）。

## 評估（D36 確認前）

使用者要求（2026-10-09）：第 07 批 L2 的 clean-30 在同一個 8 ms 主機狀態下，after 分成快（第 1、4、5 輪：Client P95 約 67～70 ms、Match P95 約 9～13 ms）與慢（第 2、3、6 輪：約 100 ms、約 20 ms）兩群，before 也一樣。只用既有的證據分析，不改產品程式，不跑新的量測。

### 1. 量測工具的量化

- probe 每一幀開頭取一次時刻（`gameplay_action.hpp:114` 的 `ns`）。「送出動作」（`:163`）與「取到裁決」（`:128`）都記這個幀開始的時刻，所以分析器的 `legal_client_p95_ms`（`gameplay_evidence.py:184`）是幀長的整數倍。`legal_match_p95_ms`（`:183`）的終點是 Match 的 `snapshot_produced`，不量化，起點仍是幀開始。
- 驗證：第 06、07 批 L2 的 clean-30／clean-60 每個合法動作的「取走−送出」除以幀長，全部接近整數（偏離最大 0.28 幀，來自幀開始的抖動）。clean-30 只出現 1、2、3 幀。P95 約 67～70 ms 就是 2 幀，約 100 ms 就是 3 幀：快慢兩群是 P95 落在第 2 幀還是第 3 幀。

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
- **約 33 ms（Match 之後）的差距分在 c 與 d**：
  - c 的中位數只有約 18 ms 與約 34 ms 兩個值，由 Gateway 結果 ticker 相對 Match Tick 格點的相位決定：relay 下行相位約 17.5 ms 時是 18 ms，約 1 ms 時多等一個 ticker，變成 34 ms。
  - d 是 probe 的幀相位（10 對 29 ms）。
  - 兩者加起來決定裁決落在第 2 還是第 3 幀。
- **clean-60**：新 Match（第 07 批 after）的 Match P95 每一輪都是 8.8～11.9 ms，不論主機狀態。舊 Match 有 7.6 ms 與 21～25 ms 兩群，而且 4 ms 狀態也出現 21.0 ms（第 07 批 L2 before 第 2 輪）。
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

- 這些節拍彼此獨立、同頻，每個跑次的相位固定。所以有時剛好趕上、有時剛好錯過，和 v6 D21「命令跟著畫面幀」是同一類問題。
- Match 的 Tick 是 60 Hz，snapshot 每 Tick 一份（`Movement.hpp:21`），不在這張表裡。

### 4. 對 D36 的影響

- D36 只影響 a 的開頭：現在 worker 每 ≤2 ms 就會看到新動作，a 的中位數本來就約 2 ms。D36 最多縮短約 1～2 ms（D36 表上寫的「平均約 16 ms」不成立：那是以 30 Hz 格點送出為前提的估計）。
- 剩下的：b 裡的節拍 3（Gateway→Match，每隔一次 ticker）、c 的節拍 4、5 的相位、d 的幀相位。

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

## 停止條件

- `worker_main` 的斷言需要放寬。
- 需要改 wire。
- 權威 digest 改變。
