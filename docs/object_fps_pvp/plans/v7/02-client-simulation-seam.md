# 第 02 批：ClientSimulation 接縫（行為不變）

狀態：**實作與 L1 完成**（2026-10-08，分支 `claude/pvp-v7-p1a`，自 master `a7cba38`）。PR 線 P1a（02、03a；第 03 批依 D40 移到 P1b），PR 在第 03a 批之後開。紀錄見[交接](HANDOFF.md)的「第 02 批進度」。
起因：v7 README 的前提：矩陣用的無頭 probe 自己重做了命令產生（`gameplay_action.hpp:106-161`；`:128` 只取最新一份 snapshot，`:133` 逐幀 Advance，`:161` `sleep_until`）。timing_main、quad_main、network_main 也是同樣的結構。只改產品的話，矩陣量到的仍是 v6 的行為（[盤點](INVENTORY.md)第 4 節）。

## 目標與範圍

做：

- 新增產品庫 `client_simulation`（依賴 domain），提供 `ClientSimulation`（`include/RetroFPS/Pvp/ClientSimulation.hpp`）。它持有 v6 產品原本 inline 的三樣東西：`LocalPlayerPrediction`、`PredictionElapsedTime`、snapshot 閘（`lastSnapshotTick`）。介面：
  - `Observe(self, tick, rules)`：同一批中最新一份自身狀態；Tick 不比上一次新就忽略；有規則時先套用再 Reconcile。對應 v6 的 `PvpApplication.cpp:419-424`。
  - `Frame(now, ClientInputSample)`：在 `now` 取樣 elapsed、沒有控制時清掉掛著的跳躍、Advance；視窗有變時回傳要發布的完整視窗。對應 v6 的 `:969-976`。
  - `Reset()`、`SelectArena(arena)`（重建預測與 elapsed，**保留** snapshot 閘，和 v6 相同）、`ClearJumpRequest()`、`PendingInput()`、`ShotTiming()`、`Observation()`。
- 邊界的決定（實作時）：
  - Drain 與 `SnapshotTimeline.Push` 留在呼叫端：Drain 的其他結果（裁決、武器回饋、世代重設）屬於呈現與主執行緒；第 04 批的模擬角色改用 ClientConnection 的另一個自身樣本佇列，不使用 Drain。
  - `SendInput` 由呼叫端轉交 `Frame` 回傳的視窗，和 v6 的寫法相同；本批的 `client_simulation` 因此不依賴網路庫。第 04 批由模擬角色自己送出。
- 改呼叫它的地方：`PvpApplication`；無頭 probe 的 `gameplay_action.hpp`（矩陣）、`action_main`、`timing_main`、`quad_main`、`network_main`。GUI probe 經由 `PvpApplication`，自動走同一條路徑。
- 矩陣的 drain 停頓故障，改由 probe 內的局部 snapshot 來源 adapter 注入，不加正式的測試旗標。
- 等價測試（`tests/object_fps_pvp/ClientSimulationTests.cpp`）：
  - 參考模型 `V6ApplicationPath` 是 v6 產品那段 inline 邏輯的逐字凍結版（master `9a6fa8e`）。
  - 兩者以同一條決定性虛擬線路（`PvpMatch` 產生 snapshot、延遲送達、60 Hz 重送）與同一串幀序列各跑一次：60 FPS 加抖動、30 FPS（一幀多份 snapshot）、144 FPS、250 ms 停頓、120 ms 長幀、中途的 session 重設與 arena 重選、失去控制時的跳躍。
  - 逐幀比對發布的視窗（命令、epoch、life、observedTick）、觀測值與 ShotTiming，必須完全相同。比對在同一個程序內進行，不受各平台 libm 差異影響；第 03 批沿用這個參考模型證明「一幀一份時與 v6 相同」。
- 原始碼守衛 CTest `object_fps_pvp.probe_command_path`（只在選擇本產品時啟用）：驗收 probe 的原始碼中，除了 include 行以外不得出現 `LocalPlayerPrediction`、`PredictionElapsedTime`。

不做：

- 不改相位追蹤（第 03 批）、不開執行緒（第 04 批）。
- 不改 wire、Match、Gateway、網路 worker。
- 不改 Engine。

## 驗收點

- L1：
  - 等價測試在各種幀率與停頓下逐位元組相同。
  - 突變（`mutations.json` 的 `v7-02-*`）：不套用移動規則、沒有控制時不清跳躍、換 arena 時重設閘、改用實際時鐘取樣、probe 直接驅動預測，都要被預期的斷言抓到。
  - 原始碼守衛與它自己的 self-test。
  - CTest 全部通過，CI 四平台。
  - 權威兩樹 digest 35／35。
- 開發跑次（不計次，但保留）：兩棵 tree 的矩陣 clean-30、clean-60 各 1 輪，確認凍結分析器接受產生的 trace。

## 建議檔位

high；等價性（特別是 Drain 與 Reconcile 的順序、`PredictionElapsedTime` 的取樣時點）局部 xhigh。

## Architecture Delta（產品內）

1. 需求：任務 1、2 的 30 FPS 對比，需要矩陣走產品的命令產生路徑。
2. 問題：無頭 probe 自己重做命令產生，與產品路徑分岔。
3. 邊界：產品內新增 Client 模擬的邊界（新的產品庫）。
4. 影響：PvpApplication、4 個無頭 probe、產品測試；只有 `object_fps_pvp`。
5. 依賴方向：`client_simulation → domain、network`；app_support 與無頭 probe → `client_simulation`。產品所屬的支援程式依賴產品，方向不變。
6. Ownership：命令產生從 `PvpApplication::Update` 移到 `ClientSimulation`，owner 仍是 `object_fps_pvp`。
7. 更小的變更不可行：另做一套走 GUI 產品路徑的 30 FPS 量測，無法沿用 25 案矩陣與 v6 參考包，也無法交錯對比。

## 停止條件

- golden 不一致。
- probe 不經過 SDL 就無法連到 ClientSimulation。
- 權威 digest 改變。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal） | L1、開發跑次 |
| Windows、Linux、macOS arm64 | CI 的 L1；實機未驗證 |

## 結果（2026-10-08）

- L1：
  - 建置無新警告；CTest 全標籤 64／64（新增 `probe_command_path`；preset 的 `cpu|shader` 為 60／60）。
  - 等價測試 4 個案例、19,973 個斷言通過。
  - 突變 5／5 killed。
  - 權威兩樹比對（base master `a7cba38`，detached worktree `../GYO-Engine-v7base`）：35 個情境 0 不同。
- 開發跑次（不計次）：分支 tree 的矩陣 clean-60、clean-30 各 1 輪都通過，凍結分析器接受產生的 trace；`run_network.py` 1 次通過。base tree 的開發跑次沒有跑：正式的 before 是第 03a／05 批的 v6 建置（`fee92ff`）。
- probe 端的行為差異（改走產品路徑後，照實記錄）：
  - 第一幀的 elapsed 由「距離開始的時間」變成 0（`PredictionElapsedTime` 的第一次取樣），只差開始前寫檔的時間。
  - `action_main`、`timing_main` 原本不套用 Match 的移動規則，現在和產品一樣套用；兩者都不跳躍。
  - `network_main` 在模擬停頓之後原本會重設 elapsed 的基準，現在和產品一樣包含停頓；停頓後重新播種，`LocalPlayerPrediction` 會把 elapsed 夾到 1 Tick。以 `run_network.py` 實跑一次（開發跑次）通過，包含 6 秒應用程式停頓、worker keepalive 與重新播種。
- 證據：`build/target/_build/test/logs/pvp-v7-batch02-20261008/`（`files.sha256` 的 SHA-256 `903bbb43…`）：base 建置腳本與日誌、`authority/`、`dev-matrix/`、`dev-network/`、`ctest.log`、突變報告。
