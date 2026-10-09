# 第 07 批：Match 的 Tick 改用 Waiter，IpcHost 改用 asio

狀態：**實作與 L1 完成，L2 的事前宣告待使用者核准**（2026-10-09；A `ffe9a99`、B `10003b9`）。PR 線 P2（PR #73）。依賴第 06 批（`7da6b0d`，本批的 before 是第 06 批的頭）。
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
- P2-log 的 `ipc_iterations` 改為計算 io 處理函式的執行次數（accept、讀、寫、計時器、pump），語意隨之改變。

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

## 結果（L1 與開發跑次，2026-10-09）

- L1：
  - 全量 CTest 69／69（新增 `object_fps_pvp.ipc`；權威 digest 不變）；Go 的測試通過。
  - 新的測試：Match 角色 5 件（刻步、重設即時喚醒、發布通知、覆蓋計數、重複 Start）；IpcHost 4 件（Ready 與 Join 結果先於 snapshot、連線之間的重設〔下一個連線之前 Match 就已清空〕、協議違規後接受下一個連線、停止及時）。既有的 host 測試改用 `Start`／`Stop`。
  - 突變 3／3 killed（`v7-07-ipc-no-publish-wake`、`v7-07-ipc-close-without-reset`、`v7-07-ipc-snapshot-before-controls`）。「改回相對等待」的突變要對晚醒時間下斷言才抓得到，在忙碌的 CI 上會誤判，所以不放；絕對期限的效果由開發跑次與 L2 的晚醒分布證明。
  - TSan：IPC 與 host 角色的測試沒有報告。
- 開發跑次（只記錄）：
  - clean-60（主機 4 ms 狀態）：Match 的 Tick 晚醒全部 <250 µs（最大 101～171 µs；P2-log 以 2～4 ms 為主、最大 4.2 ms）。Match 程序 CPU：P2-log 約 0.042 秒／秒 → A 之後約 0.022 → B 之後約 0.015。IPC 的處理次數約 513 次／秒（P2-log 的輪詢約 787 次／秒）。
  - `backpressure_probe.py` 6 案（host-ipc、gateway、downstream 各 250／1000 ms）全部通過。第一次誤把 action probe 當成 `--probe`（它需要 timing probe），那次保留在 `dev-backpressure-wrong-probe/`。
  - `run_gameplay.py` 完整的 25 案矩陣：25／25 通過。
- 證據：`build/target/_build/test/logs/pvp-v7-batch07-20261009/`（git 忽略）：`mutations.json`、`ctest-full.log`、`tsan-ipc.txt`、`tsan-host.txt`、`dev-a-clean-60/`、`dev-b-clean-60/`、`dev-backpressure/`、`dev-matrix/`。

## L2 的事前宣告（草案，待使用者核准）

- **目的**：
  - 確認停止條件「macOS 的 Tick 晚醒沒有改善」不成立。
  - 確認 IPC 改為事件驅動後，喚醒與 CPU 減少，gameplay 沒有變差。
  - 留下依主機狀態分組的資料，給第 07 批之後的橫向對比與第 09 批。
- **產物**：
  - before 是第 06 批的頭建置的 Match，after 是本批的頭建置的 Match。
  - Gateway 與 probe 共用本批的頭建置的版本：Gateway 的 Go 在兩者之間沒有差異；probe 是 Client 端，不含 Match。
  - 雜湊寫進 `artifacts.sha256`，跑次前後各核對一次。
- **主機**：沿用第 06 批 L2 的做法：開始時與每輪前後量 5 秒 sleeper，記錄背景的高 CPU 程序；機器閒置（關掉 Chrome 等）。
  - 每個跑次依 probe 的 TimerBaseline 分為兩種主機狀態：間隔 P99 ≥22 ms 為 8 ms 狀態，其餘為 4 ms 狀態。
  - 舊的分類用 Match 的 snapshot 晚醒，本批改變的正是 Match，所以不再適用（README 已預定第 07 批之後改用 TimerBaseline）。
- **案例與順序**：
  - clean-60、clean-30、host-ipc-250ms、host-ipc-1000ms、gateway-250ms，共 5 案。
  - 6 輪，每輪每案 before 與 after 各 1 次，奇數輪先跑 before。共 60 次，約 30 分鐘。
- **判定**（全部成立才算通過）：
  1. after 的所有跑次，gameplay 判定都通過。before 的失敗只記錄。
  2. after 所有視窗合計的 Tick 晚醒，P99 的上界 ≤1 ms。
  3. before 所有視窗合計的 Tick 晚醒，P99 的上界 ≥2 ms（確認 before 重現了舊的晚醒）。
- **只記錄**：
  - 各主機狀態下兩棵 tree 的 Tick 晚醒分布。
  - IPC 處理次數與 Match 程序 CPU（每秒，中位數）、`snapshot_overwrites`。
  - `legal_match_p95_ms`、`legal_client_p95_ms`；Gateway 的結果與 snapshot 間隔。
- **停止條件**：
  - 跑次錯誤時停下，不自行重跑。
  - after 出現 gameplay 失敗，或判定 2 不成立時，停下回報（後者就是本批的停止條件）。
  - 產物雜湊不符時停下。
- **證據**：`build/target/_build/test/logs/pvp-v7-batch07-l2-<日期>/`。

## 架構

- 模擬執行緒的擁有者從 `match_main` 移到 `MatchRuntimeHost`（產品內，和 Client 的模擬角色一致）。新的依賴邊：`runtime_host`→`GYO::Threads`（Engine 既有的模組，方向是產品→Engine）。
