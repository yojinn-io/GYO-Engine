# P2-log：網路路徑的 10 秒統計（P2 的 before）

狀態：**完成**（2026-10-09；`983e091`，PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)）。PR #73 的 CI（macos-arm64）發現 host 的測試對時間太敏感，已修正（`364884f`）。PR 線 P2（P2-log、06、07、08、09）。依賴 P1b（`660310e`）。建議檔位 medium。
本批只加記錄、不改行為。它的 commit 是第 06～08 批的 before：之後每一批的修正，都用同一組統計行比較前後。

## 起因

- 第 06～08 批要改的三個地方，現在都沒有可以前後比較的紀錄（[盤點](INVENTORY.md)第 8 節）：
  - Gateway→Client 的結果通道約 18 Hz，只在矩陣的 relay 後量到，Gateway 端沒有紀錄；Gateway→Match 的 action batch 從未量過。
  - Match 的 Tick 用相對的 `condition_variable_any::wait_for`（`MatchRuntimeHost.cpp:294-295`），沒有晚醒紀錄；IpcHost 每輪睡 1 ms（`IpcHost.cpp:327`；accept 每 5 ms 重試，`:336`），沒有迭代次數與 CPU 的紀錄。
  - Client 網路 worker 每輪睡 min(2 ms, 到下一次輸入期限)（`ClientConnection.cpp:607-610`），沒有喚醒次數與 CPU 的紀錄。
- 任務 3 的壓力目前只是推測（README 任務 3）。這些統計也用來確認第 07、08 批是否真的減少了喚醒與 CPU。

## 範圍

做（三個程序各自每 10 秒寫一行，key=value 格式，寫進各自既有的日誌）：

- **Gateway**（`player send statistics`，每位玩家一行，接在既有的 `player statistics` 之後）：
  - 結果通道：送到 Client 的 ActionResults 筆數，以及相鄰兩次寫出完成時刻的間隔。
  - snapshot：送到 Client 的 snapshot 筆數與間隔。
  - runtime link：送到 Match 的 action batch 筆數與間隔（寫出完成時刻）。
  - 間隔的 P50、P90、P99、最大值（ms），以視窗內的樣本精確計算。跨視窗的間隔算進新的視窗；玩家離開時清掉。
- **Match**（`[ObjectFPS/PvP Match] network statistics`，由 `match_main` 的主執行緒寫；本批不在 Tick 與 IPC 執行緒加 I/O）：
  - IPC 執行緒既有的 `[ObjectFPS/PvP Match] statistics tick=…` 一行不動；新行多一個 `network`，解析器才不會混在一起。
  - 程序 CPU 秒數（user＋system）：視窗增量與累計。
  - IPC 迴圈的迭代：連線迴圈與 accept 迴圈每輪加 1，寫出視窗內的次數與每秒次數。
  - Tick 的預定與實際喚醒：
    - 預定＝Advance 前取樣的時刻加 `secondsUntilNextTick`（Tick 的格點）；實際＝等待返回後的時刻。現在的等待是相對的，從 Advance 之後才開始算，所以晚醒包含 Advance 本身的時間。
    - 晚醒用 Engine 的 `LateWakeStats` 記錄；第 07 批改用 Waiter 之後是同一種表示。
    - 被重設要求喚醒（notified）與停止時的喚醒不算晚醒，另計次數。
    - 比預定早醒的次數另計，晚醒記為 0。
- **Client worker**（`[ObjectFPS/PvP] worker statistics player=…`，由 worker 自己寫；一個 probe 程序有多個 Client，所以帶玩家 ID，session 外為 0）：
  - 喚醒次數：Run 迴圈每輪加 1。
  - worker 執行緒的 CPU 秒數：視窗增量。
  - 主執行緒是畫面迴圈，不在那裡加 I/O。worker 本來就會寫日誌（連線失敗時），一次 10 秒一行。
- **CPU 時間的輔助**（產品內，`match_domain`）：程序 CPU（POSIX `getrusage`、Windows `GetProcessTimes`）與呼叫端執行緒的 CPU（POSIX `CLOCK_THREAD_CPUTIME_ID`、Windows `GetThreadTimes`）。
- **解析器**（`build/acceptance/object_fps_pvp/network_statistics.py`）：讀三種日誌，輸出每個跑次的摘要 JSON。第 06～08 批的宣告用它。

不做：

- 不改任何節拍、佇列或送出語意。既有的日誌行（`gateway statistics`、`player statistics`、`gateway transport`、`runtime transport`）一個字都不改：驗收工具解析其中幾行（`action_probe.py:594`）。
- 日誌格式不是正式的 Data Contract：版本與驗證規則在第 10 批訂。本批的統計行到那時再決定是否納入。
- Gateway 的 CPU、Client 的程序 CPU、收包間隔：不在 P2-log 的定義內；收包時間戳的 snapshot 年齡在第 10 批。

## 驗收

- **L1**：
  - Go：間隔統計（分位數、跨視窗、三條通道各自計數、玩家離開後清掉）用注入的時間；統計行的格式。
  - C++：Tick 喚醒的記錄（預定與實際、notified 與早醒的處理）用注入的時間點；Match 與 Client 統計行的格式；CPU 時間單調不減，忙迴圈後增加。
  - 解析器：合成的日誌樣本。
  - 行為不變：權威 digest（CTest `authority_digest`，golden 不變）；全量 CTest；`go vet`、`go test`、`go test -race`。
- **開發跑次**（只記錄，不是正式比較）：clean-60 跑一次，確認三個程序每 10 秒都有一行、數值落在合理範圍。數字寫進本文件，作為 P2 的 before 的初看。

## 停止條件

- 權威 digest 改變。
- 既有的測試、分析器或驗收工具需要放寬或改寫。
- 要改既有的日誌行，或要改 wire。

## 架構

- 全部在產品內（`apps/object_fps_pvp`、它的 Gateway）。Engine 不改；`match_domain` 明確連結 `GYO::Time`（`NetworkStatistics` 直接使用既有的 `LateWakeStats`；原本經 `GYO::Engine` 間接連結），依賴方向是產品→Engine。
- 新檔：`NetworkStatistics.hpp/.cpp`（CPU 時間、Tick 喚醒的記錄、Match 與 worker 的統計行）、Gateway 的 `send_statistics.go`、解析器與它的測試（CTest `object_fps_pvp.network_statistics`）。

## 結果（2026-10-09）

- L1：全量 CTest 68／68（含 `authority_digest`，golden 不變；新的 `network_statistics`）；`go vet`、`go test`、`go test -race` 通過。新的 C++ 測試 4 件（Tick 喚醒的記錄、兩種統計行、CPU 時間、host 的視窗重設）、Go 測試 4 件、解析器測試 2 件。
- 開發跑次的第 1 次發現兩點，修正後跑第 2 次（兩次都保留）：
  - Match 的新行與 IPC 執行緒既有的 `statistics tick=` 同一個前綴 → 新行改名為 `network statistics`。
  - 同一個 probe 程序的兩個 Client 的 worker 行分不出來 → 加上 `player=`。
- 自我檢查：`match_main` 直接寫 `std::clog`，而 IPC 執行緒用 `osyncstream` 寫同一個 streambuf，兩邊同時寫會競爭 → 改用 `osyncstream`。修正後重跑受影響的 CTest 5 件（`cpu`、`network_statistics`、`service_startup`、`authority_digest`、`worker`）與第 3 次 clean-60，都通過。
- 開發跑次 clean-60（第 2 次，通過；只跑約 20 秒，每個程序 1 個完整視窗；只記錄，不是正式比較）：
  - Gateway：結果通道的間隔 P50 66.6／66.7 ms，約 15 Hz，是 `ActionSendRate`（30 Hz）的一半。寫出後重新錨定讓每隔一次 ticker 才送（第 06 批要修的缺陷；盤點在 relay 後量到的「約 18 Hz」是同一個原因）。snapshot 的間隔 P50 16.6 ms（60 Hz）。runtime link 的 action batch 只在有請求或 ACK 時送，間隔 P50 約 70 ms。
  - Match：IPC 迴圈約 787 次／秒（1 ms 睡眠）；程序 CPU 約 0.042 秒／秒；Tick 601 次，晚醒以 2～4 ms 為主（bins 24,13,26,51,482,5,…，P50 ≤4 ms，最大 4.2 ms，含 Advance 的時間）。
  - Client worker：每個 Client 約 416 次／秒、CPU 約 0.022 秒／秒。
- 證據：`build/target/_build/test/logs/pvp-v7-p2log-20261009/`（git 忽略）：`ctest-full.log`、`ctest-full-2.log`、`go-test.log`、`dev-clean-60/`（第 1 次）、`dev2-clean-60/` 與 `dev2-network-statistics.json`（第 2 次）、`dev3-clean-60/`（第 3 次）。

