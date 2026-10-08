# 第 03 批：每份 snapshot 進相位追蹤（任務 2 的一半）

狀態：**未開始**。PR 線 P1a。依賴第 02 批。
起因：相位追蹤每幀只用最新一份 snapshot（`PvpApplication.cpp:416-424`、`LocalPlayerPrediction.cpp:106-126`、`:155-158`）。30 FPS 時一幀約 2 份，約一半的樣本到不了；LAN 上 60 FPS 也有 20.2% 的自身 snapshot 落在一幀多份（[盤點](INVENTORY.md)第 4 節）。

## 目標與範圍

做：

- `ClientSimulation` 把同一批中每一份含自己的 snapshot 的 `movementSlack` 樣本，依 Tick 順序交給 TrackPhase。重複的 Tick 與較舊的 Tick 都忽略。
- Reconcile 維持只用最新一份，因為較新的 snapshot 會覆蓋較舊的。若批次開始時發現有理由改成逐份，先提案。
- 仍在呼叫端的執行緒上執行；probe 經接縫自動跟上。

不做：不改相位常數、FireGate 常數，也不改相位追蹤的定義。

## 驗收點

- L1：
  - 合成時間線一幀 2～3 份時，每份樣本恰好使用一次，而且依序（計數斷言）。
  - 一幀 1 份時，輸出與 v6 逐位元組相同（60 FPS 的等價）。
  - 相位追蹤的收斂測試；`FireGateTests` 不變。
  - 突變：只取最新一份時，計數斷言要觸發。
  - CTest，CI 四平台；權威 digest 35／35。
- L2：第 03a 批（只記錄）與第 05 批。

## 建議檔位

high；樣本順序與死區、slew 的交互局部 xhigh。

## Architecture Delta

無（產品內、`ClientSimulation` 內部的行為變更）。

## 停止條件

- 一幀 1 份的合成輸出與 v6 不同。
- 相位追蹤不收斂。
- 必須改相位常數或 FireGate 常數，測試才能通過。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal） | L1 |
| Windows、Linux、macOS arm64 | CI 的 L1；實機未驗證 |
