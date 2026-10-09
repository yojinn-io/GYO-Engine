# 第 07 批：Match 的 Tick 改用 Waiter，IpcHost 改用 asio

狀態：**進行中**（2026-10-09 開始）。PR 線 P2（PR #73）。依賴第 06 批（`7da6b0d`，本批的 before 是第 06 批的頭）。
建議檔位 high；重設的交接、停止與 join 的順序、寫出的選擇（未開始的 snapshot 可替換）局部 xhigh。本批沒有另開 xhigh 審查 agent（需要使用者同意）。

## 起因

- Match 的 Tick 用相對的 `condition_variable_any::wait_for`（`MatchRuntimeHost.cpp:294-295`），等待從 Advance 之後才開始算，又有 macOS 計時器合併的 leeway。P2-log 量到晚醒以 2～4 ms 為主（含 Advance）。第 06 批 L2 的後兩輪，主機進入 8 ms 狀態時 `legal_match_p95_ms` 約 10→19～24 ms。
- IpcHost 是一條輪詢執行緒：每輪睡 1 ms（`IpcHost.cpp:331`）、accept 每 5 ms 重試（`:341`）、重設的 future 每 10 ms 輪詢（`:344`、`:349`）。P2-log 量到約 787 次／秒的迭代。
- 模擬執行緒由 `match_main` 的 `std::jthread` 建立（`match_main.cpp:54`），IpcHost 也用 `std::jthread`。

## 範圍

### A. Match 的模擬角色

- `MatchRuntimeHost` 自己擁有模擬執行緒（Engine 的 `GYO::Threads` 角色，名稱 `gyo-match-sim`），和 Client 的模擬角色相同：`Start`／`Stop`，解構時停止並 join。`Advance` 維持公開，給決定性的測試。
- 每一步：取樣時刻 → `Advance` → 以「取樣時刻＋`secondsUntilNextTick`」為絕對期限，用角色的 Waiter 等待。重設要求以 `Notify` 喚醒，停止由角色的停止要求喚醒。條件變數移除。
- Tick 的晚醒照 P2-log 的定義記錄（預定＝取樣時刻＋`secondsUntilNextTick`）；現在等待是絕對的，所以晚醒不再包含 Advance 的時間。
- 發布通知：Advance 產生 snapshot、Join／Leave 的結果、驅逐，或完成重設時，在鎖外呼叫一次 IpcHost 設定的通知（只做 post，不阻塞）。
- `snapshot_` 槽在被取走前就被新的覆蓋時計數，寫進 Match 的 `network statistics` 行（行末新增 `snapshot_overwrites=`，原有欄位不動）。

### B. IpcHost 改為一條 asio io 執行緒

- 一條角色執行緒（`gyo-match-ipc`）執行 `io_context`。accept、讀、寫、計時器都是非同步的；1／5／10 ms 的輪詢全部移除。
- 狀態：接受連線 → 等重設完成 → 連線中 → 關閉並等重設完成 → 接受下一個連線。重設的完成由 Match 的通知喚醒，不輪詢 future。
- 讀：`async_read_some`；frame 的長度、解析與版本檢查和現在相同。
- 寫：等 socket 可寫，再以非阻塞的 `write_some` 寫出，所以「在可寫的那一刻才選要寫哪一個 frame」：
  - 優先序不變：controls > actions > snapshot。
  - 未開始寫的 snapshot 會被更新的取代；已經寫出任何位元組的 frame，寫完才選下一個。
  - Transport 的 MovementTrace（合併與部分寫出的年齡）照舊。
- actions 的通道：每位玩家 30 Hz 的期限，用一個 `steady_timer` 等最早的期限。
- 連線結束的路徑明確化：讀錯誤或 EOF、協議違規（長度、解析、版本、未知訊息）、佇列溢位（joins／controls 超過 64）、寫錯誤，各自寫一行 `ipc closed reason=…`，然後重設、等待完成、接受下一個連線。
- P2-log 的 `ipc_iterations` 改為計算 io 的處理次數（pump 與 accept），語意隨之改變。

## 驗收

- **L1**：
  - Host：`Start`／`Stop`；實時下 Tick 約 60 Hz；重設在通知下及時完成；發布通知在 snapshot、結果、驅逐、重設完成時發出；覆蓋計數；Tick 晚醒的統計（含 notified）。
  - IpcHost（新的測試，用 loopback 的 TCP 客戶端）：先收到 Ready；Join→JoinResult；snapshot 送達；controls 優先於 snapshot；讀得慢時 snapshot 合併、只送最新、部分寫出的 frame 會寫完；斷線→重設→下一個連線收到 Ready 且狀態已清空；連線中與等待連線時停止都能及時返回。
  - 既有：全量 CTest（權威 digest 不變）、Go 的測試、突變。
- **開發跑次**：矩陣的 clean 與故障案例、`backpressure_probe.py` 的 host-ipc 層。
- **L2**：L1 完成後寫事前宣告，送使用者核准。before＝第 06 批的頭，after＝本批的頭；看 Match 的 Tick 晚醒、IPC 的迭代與 CPU、gameplay 判定，並依主機狀態分組。完成後做 P2-log、第 06、07 批的橫向對比（交接的「未結事項」）。

## 停止條件

- 權威 digest 改變。
- 需要改 wire。
- macOS 的 Tick 晚醒沒有改善。
- 既有的驗收斷言需要放寬。

## 架構

- 模擬執行緒的擁有者從 `match_main` 移到 `MatchRuntimeHost`（產品內，和 Client 的模擬角色一致）。新的依賴邊：`runtime_host`→`GYO::Threads`（Engine 既有的模組，方向是產品→Engine）。
