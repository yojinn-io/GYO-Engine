# PvP v7.1 drop-report：Gateway 把丟了什麼、為什麼丟，回報給 Match

日期：2026-10-16。**完成**（`598b609`；L1 通過；事前宣告的逐則相等在 2026-10-17 的 edge-input-passthrough L2 成立）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 pv7-line 的 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`）。
契約見 [protocol-v7](../object_fps_pvp/protocol-v7.zh-Hant.md) §4；計畫見 [v7.1 計畫](../object_fps_pvp/plans/v7.1/README.md)的「pv7-line 各批」；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的規劃紀錄與決策 D103～D105。

## 做了什麼

- 目的（盤點的 P4，D49 第二階段）：2026-10-10 的事故是 Gateway 悄悄丟掉命令、Match 不知道。這批讓 Gateway 依原因累計丟棄數，透過 runtime link 回報給 Match；排在 edge 各批之前（D90），edge 批的前後對照才有丟棄回報可用。
- 先照當時的出口實作：包括之後會隨 edge 批刪掉的過渡原因（A3／A5、截短、拒絕線溢出、snapshot 守衛 D1／D2 等），edge 批刪程式時一起改成 reserved。名稱表在 Go 的 `dropReasons` 一處綁定，Go、C++、Python 共用一份 golden（40 個值）。
- 補了原本沒有計數的下行出口：寫出失敗、控制佇列滿、送出迴圈中途被新 snapshot 取代，以及 Gateway 讀取端的新蓋舊。
- 回報的節奏：每秒最多一輪，寫出前一刻才組回報（序號 `report_sequence`、樣本、停留時間都在那時決定）；只有變化才送；玩家移除後送一則 final；每寫一則記一行 `drop report`。
- 優雅關閉時先寫所有非空串流的 final，再關 link。
- Match 每則回報記一行，序號倒退、累計倒退等違規只記錄並計數，不關 link（D-5：診斷沒有否決權）。

## 驗證與經過

- 本機 L1：全量 CTest 72／72、`go test -race -count=3`、digest 兩樹比較 0 差異。突變本批 17 個加受影響的既有突變共 194 個全部 killed；第一輪有 6 個沒殺死，都是突變或測試的寫法問題，修正後重跑。
- 短跑次（只記錄）：clean-60、loss-shot、backpressure 6 情境、network、soak-short、quad 全部通過。有回報的跑次兩端依 (player, report_sequence) 逐則相等、沒有違規；乾淨跑次沒有丟棄也沒有回報。Match 先停的情境沒有 final，符合契約。
- 列舉與上限的局部 xhigh 檢查（約 38 分）：must 1、should 6。must-1：優雅關閉時 writer 正在寫，所有 final 都不送；改成有上限地等 writer 退出（3.5 秒）再寫 final。should-1：「每秒最多 1 則」原本只做到「每次觸發最多 1 輪」，已修。
- 動到定稿契約與架構的兩項交使用者決定：契約 §2 的計時措辭改成配合實作（D104）；新增的 Gateway 欄位核准為診斷狀態，並分成三類寫進契約（D105，Architecture Delta）。
- 事前宣告的 L2 等式（Match 收到的與 Gateway 寫出的回報依 report_sequence 逐則相等）放進 edge-input-passthrough 的 L2 當判定 J7，2026-10-17 成立。
- 實際約 2.7 h（估 13～16 h）；四平台 L1 run 38873139138（手動觸發）。
- 遺留：比對腳本 `compare_reports.py` 只存在 git 忽略的證據目錄，之後的 L2 都以雜湊載入它；2026-10-21 補進盤點（N-compare-reports-untracked），排在 `contract-readers` 收進工具目錄。

## 證據

`build/target/_build/test/logs/pvp-v71-drop-report-20261016/`（含 `compare_reports.py`）、J7 的判定在 `pvp-v71-edge-input-l2-20261017b/judgement.md`（git 忽略）。
