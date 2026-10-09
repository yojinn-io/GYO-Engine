# 第 08 批：Client 的網路 worker 改用 asio

狀態：**開始，D36 待使用者確認**（2026-10-09）。PR 線 P2（PR #73）。依賴第 07 批。
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
  - 動作：30 Hz 的最小間隔；依 D36（待確認），符合資格時在 `SubmitAction` 當下送出。
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

## 停止條件

- `worker_main` 的斷言需要放寬。
- 需要改 wire。
- 權威 digest 改變。
