# 第 02 批：ClientSimulation 接縫（行為不變）

狀態：**未開始**。PR 線 P1a（02、03、03a）。
起因：v7 README 的前提：矩陣用的無頭 probe 自己重做了命令產生（`gameplay_action.hpp:106-161`；`:128` 只取最新一份 snapshot，`:133` 逐幀 Advance，`:161` `sleep_until`）。timing_main、quad_main、network_main 也是同樣的結構。只改產品的話，矩陣量到的仍是 v6 的行為（[盤點](INVENTORY.md)第 4 節）。

## 目標與範圍

做：

- 新增產品庫 `client_simulation`（依賴 domain、network），提供 `ClientSimulation`。
- 它包住目前 `PvpApplication::Update` 的命令產生路徑，順序不變：
  1. `ClientConnection::Drain`
  2. `SnapshotTimeline.Push`
  3. Reconcile（只用最新一份）
  4. `PredictionElapsedTime` 取樣
  5. `LocalPlayerPrediction.Advance`
  6. `SendInput`
  7. 發布 Observation
- 對外介面是 `Frame(elapsed, InputSample)`：本批仍由呼叫端的幀驅動。
- 改呼叫它的地方：`PvpApplication`；無頭 probe 的 `gameplay_action.hpp`（矩陣）、`action_main`、`timing_main`、`quad_main`、`network_main`。
- 矩陣的 drain 停頓故障，改由 probe 內的局部 snapshot 來源 adapter 注入，不加正式的測試旗標。
- 先在 base commit 寫 characterization test，錄下 golden 命令流：
  - 輸入：合成的 snapshot 序列加 elapsed 序列。
  - 比對：命令內容、epoch、observedTick、取樣時點。
  - refactor 之後必須逐位元組相同。
- 原始碼守衛 CTest（只在選擇本產品時啟用）：probe 不得直接呼叫 `LocalPlayerPrediction::Advance`。

不做：

- 不改相位追蹤（第 03 批）、不開執行緒（第 04 批）。
- 不改 wire、Match、Gateway、網路 worker。
- 不改 Engine。

## 驗收點

- L1：
  - golden 在 refactor 前後相同。
  - 突變：在 ClientSimulation 內把 Reconcile 移到 Advance 之後，或改變取樣時點，golden 都要失敗。
  - 插入直接呼叫 Advance 的程式時，原始碼守衛會觸發。
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
