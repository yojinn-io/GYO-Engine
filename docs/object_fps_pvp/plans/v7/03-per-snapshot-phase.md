# 第 03 批：每份 snapshot 進相位追蹤（任務 2 的一半）

狀態：**實作完成，移到 P1b**（2026-10-08，使用者決定 D40）。分支 `claude/pvp-v7-p1b`（`03b2ea7`，自 P1a 的 `7f13f03`）；和第 04 批一起合併。依賴第 02 批。
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

## 實作（2026-10-08）

- `LocalPlayerPrediction::ObservePhaseSample(authority)`：只跑 TrackPhase（不 ACK、不重新播種、不修正位置），只採用同一位玩家、同一 epoch、同一 life 的樣本。另加 `Reseeds(authority)` 查詢。
- `ClientSimulation::ObserveSample(snapshot, self)` 暫存這一批 Drain 到的 snapshot；`Observe` 把「上次之後、最新之前」的樣本依 Tick 順序交給 `ObservePhaseSample`，再 Reconcile 最新一份。最新一份會重新播種時，丟掉較舊的樣本（它們描述的是被丟掉的相位，觸發的修正也不會被 slew）。
- 觀測值新增 `phaseSamples`、`phaseSampleSequence`（唯讀的診斷）。`Movement.hpp` 中「30 FPS 約八秒」的註解改正。
- 產品與 4 個讀 Drain 批次的 probe 在 `Observe` 之前呼叫 `ObserveSample`；守衛同時檢查存在與順序。

## 驗收的結果

- 等價：穩定 60 FPS 與 144 FPS（每幀最多一份）時，和凍結的 v6 參考模型逐位元組相同，涵蓋 session 重設與 arena 重選，並斷言發生過相位決定。
- 多份時：每個樣本最多用一次、序號遞增；三份打亂順序送入時樣本 +3（只看最新 +1）；舊 snapshot 絕不 Reconcile；同一批兩次 late 會立即修正（v6 看不到）。
- 突變 `v7-03-*` 4／4 killed（審查前的版本）；權威兩樹比對 35／35。
- xhigh 審查（使用者同意）：沒有 blocker。主要發現與處理：
  - [major] 既有的收斂與 Held 測試只走 v6 路徑 → `MovementRecoveryTests` 改為 v6／產品兩條路徑各跑一次（TIMEOUT 60 秒）。
  - [minor] 同一批裡舊樣本觸發的修正被接著的重新播種丟掉、計數卻照算 → 最新一份重新播種時丟掉舊樣本。
  - [minor] 測試涵蓋與守衛的順序檢查 → 已補。
  - [minor，既有] settling 期間收進來的舊樣本留在下一個視窗（P90 實際約 P93、同一段延遲可能修正兩次）→ 屬相位追蹤的定義，本批不改，列為後續項目（交接的未結事項）。
- **已知的失敗**：產品路徑的回復測試中，fps 30、RTT 20、108 ms 停頓、網路受損、worker 4000、authority 6000 這一個組合，在恢復期限後有 4 次 Held（v6 路徑 0 次；產品路徑 486 個組合中只有這 1 個）。拿掉「重新播種時丟掉舊樣本」也一樣失敗，所以原因是本批的核心。
  - 機制（xhigh 審查的分析）：樣本加倍讓相位更早收斂（240 樣本的視窗約 4 秒填滿，v6 在 30 FPS 約 8.5 秒）；在命令跟著畫面幀產生的架構下，命令的發佈時間被量化到幀上，收斂後的餘裕偏緊，幀一晚到就有一步錯過自己的 Tick。v6 也會進入同樣的穩態，只是比較晚。
  - 開發跑次的 clean-30（8 ms 狀態）也失敗，型態相同：Actual 97.6%、Held 21／20、每個 Client 修正 2 次。
- 決定（D40）：第 03 批移到 P1b，和第 04 批（命令在固定步邊界產生，量化消失）一起合併；不放寬測試。第 04 批之後若這個測試仍然失敗，就是真正的問題。
- 證據：`build/target/_build/test/logs/pvp-v7-batch03-20261008/`（`authority/`、`dev-matrix/`、`dev-network/`、`cpu-tests-after-review.log`、`source-after-review.diff`、突變報告）。
