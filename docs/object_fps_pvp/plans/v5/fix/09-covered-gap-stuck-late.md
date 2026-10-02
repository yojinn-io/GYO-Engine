# 09 涵蓋空檔後 Client 卡在「剛好遲到」（連續 Held 直到 Starvation 重設）

狀態：**已解決（2026-10-02實作與CPU驗證完成，PR #11送審中；實機report-only GUI冒煙1輪PASS（不計次）；第03批結案驗收未執行）**。2026-10-02於[08](./08-a1-clock-drift.md)的模擬評估中發現。Owner：`object_fps_pvp`。
相關：A1（`ff11ee3`）與守門（`9cd7f26`）之前就存在的恢復路徑；A1的緊湊餘裕使它更常發生。
索引見[README](./README.md)；stall reseed見[02](./02-a1-cancelled-by-stall-reseed.md)，掉幀見[03](./03-a1-missed-frame-starvation.md)。

## 問題成因

1. **涵蓋空檔丟時間。**`LocalPlayerPrediction::Advance`在未確認命令為空且幀長超過3 Tick
   （`MovementMaximumRegularFrameSeconds`＝50ms）時，只推進1 Tick，其餘時間丟棄，避免長暫停後lead膨脹。
   卡頓期間Host持續執行並ACK，卡頓結束那一幀Client的未確認命令剛好被確認完，就會觸發。
2. **丟掉的時間讓Client落後。**Host在卡頓期間已用Held替代了之後的序號；Client恢復後產生的命令
   對應的序號，Host已經（或即將）替代。
3. **stall reseed偵測不到。**重新播種的條件是`authority.lastResolvedCommand > oldTip`。RTT>0時
   Client看到的ACK總是晚RTT/2：Host替代序號k之後，ACK=k要再過約RTT/2才到；這期間Client已產生k+1，
   於是每幀都是「ACK＝tip－1」，條件永遠不成立。
4. **穩定在遲到狀態。**每個命令都比它的Tick晚幾ms到，Host每Tick都替代（15 Tick後改為Neutral），
   佇列一直為0；30 Tick後Starvation保險絲（[03](./03-a1-missed-frame-starvation.md)「共同機制」）重設epoch。
   這段約0.5秒，玩家的輸入全部被丟棄。

模擬追蹤（A1、RTT 20、每400ms卡45ms）：卡頓後一幀61.7ms只產生1個命令（時間被丟棄），
之後每Tick `src 2`（Held）、`queued 0`；`FRAME tip 3627 ack 3626`、Host在命令送達前約7ms已替代3627，
持續到下一次卡頓才因未確認命令非空而正常追趕。

A1把lead之外的餘裕壓到約4ms，丟掉的時間幾乎必定超過餘裕；未對齊時餘裕是0–16.7ms的抽籤，
一部分空檔被吸收。

## 影響

CPU模擬（[08](./08-a1-clock-drift.md)的模擬器），15分鐘、18組相位、0ppm。

| 情境 | RTT | A1 重設／Held% | 未對齊 重設／Held% |
|---|---|---|---|
| 每60秒一次53ms卡頓（共252次卡頓） | 20 | 238／1.199 | 102／0.305 |
| 同上 | 40 | 89／0.232 | 8／0.050 |
| 每400ms卡45ms，持續60秒 | 20 | 421／2.785（風暴那一分鐘Held約35%） | 39／0.282 |
| 同上，worker改為立即送出 | 20 | 347／2.365 | 39／0.219 |

- RTT 20時，A1下約94%的53ms卡頓都以一次Starvation重設收場（重設本身會丟棄未確認命令並修正位置）。
- RTT 0不發生：ACK幾乎即時，stall reseed能偵測到。
- 改worker排程（[03](./03-a1-missed-frame-starvation.md)「根因更新」）不能解決本問題。
- 不受影響：正確性（Actual／Held／HP／彈藥／生命週期）；只是輸入被替代與重設。

## 如何復現

模擬器原始碼與用法見[08](./08-a1-clock-drift.md)「如何復現」；本問題需要含追蹤輸出的版本
（`build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/drift/Sim.cpp`，以同目錄`build.py`建置`sim_prod`）。

```bash
E=build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/drift
SIM_DBG_FROM=60300 SIM_DBG_TO=61400 $E/sim_prod A1 storm 0 20 1 70 | less
$E/sim_prod A1 hitch53 0 20 6 900 | head -1     # 重設數與Held
```

讀法：`FRAME el`為幀長（ms）、`tip／ack`為Client最新命令與看到的ACK；`RES ... src`為Host裁決來源
（1 Actual、2 Held、3 Neutral）、`queued`為裁決後佇列；`RESET reason 3`為Starvation。

## 解決方案

未實作。08的提案中，持續相位追蹤收到Host回報的「替代後才到達」負值樣本：連續2個就立即把相位往前修正
（不等完整窗口），Client在數幀內補回lead，不必等Starvation重設。模擬（全部ppm×RTT總和）：
53ms卡頓重設A1 992→提案76、卡頓風暴1322→18（Held平均1.35%→0.43%）；同一原型不加遲到修正時，
風暴RTT 20的Held為9.1%、重設335。細節見08「模擬結果」。
未採用、未評估：放寬Starvation保險絲（固定政策，牽動所有恢復路徑）、讓stall reseed條件考慮RTT。

## 驗證

- 現象：上表；風暴情境RTT 0／40與未對齊對照見08的模擬輸出。
- 修正後須在同一模擬器重跑，並補產品單元測試（涵蓋空檔後的遲到恢復）。
