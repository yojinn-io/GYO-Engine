# PvP v7.1 edge-results-resend、edge-input-passthrough、edge-results-passthrough：Gateway 回到無狀態邊緣之一，Match 在 ACK 前重送結果、輸入與結果原樣轉送

日期：2026-10-16～2026-10-17。**完成**（edge-results-resend `5c0d8f5`；edge-input-passthrough B `42f6e88`、A `6d9ecf5`，L2 通過；edge-results-passthrough `f79bb10`，L2 通過）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`，2026-10-20）。
計畫見 [v7.1 README](../object_fps_pvp/plans/v7.1/README.md) 的三列與 [Gateway 方向](../object_fps_pvp/plans/v7.1/gateway-direction.md)；L2 宣告見 [edge-input-passthrough](../object_fps_pvp/plans/v7.1/edge-input-passthrough-l2.md)、[edge-results-passthrough](../object_fps_pvp/plans/v7.1/edge-results-passthrough-l2.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的規劃紀錄與決策 D106～D113。

## 做了什麼

- edge-results-resend（D106）：Match 的結果通道由「每個裁決只送一次」改成在 Client ACK 之前重送。每位玩家每 1/30 秒最多一框，每框是退役值加至多 8 筆裁決（最小的 4 筆未確認 id 每框都有，其餘 4 個槽位用游標循環）；退役前進、ACK-only 補送退役、批被拒收時再送一次。Gateway 這時仍有帳本、冪等合併副本，Client 收到的資料包不變。
- edge-input-passthrough（D108）：事故根因元件在這批刪除。輸入改成一個資料包對一則 IPC 訊息、原樣轉送；刪除 `lastResolved` 副本、命令表、epoch／life、拒絕線、已寫出去重、截短、D1、D2；link 改成依到達順序的有界 FIFO（每位玩家至多 64 則，滿了丟最舊並回報 32）；解碼成功就提交序號與活性（D-11）。被刪路徑的 ingress v1 鍵照樣輸出 0。Match 另加 host mutex 的鎖等待量測（B commit，只記錄）。
- edge-results-passthrough（D111）：刪除 Gateway 的 session 結果帳本（requests、decisions、退役、`retiredSent`、`retirementAsked`、結果節拍）與 A3。Match 寫來的結果框架放進每位玩家的單槽 `resultSlot`（latest-wins），守 30 Hz；被取代的計入新鍵 `drop_results_over_rate_packets`（回報 34）。controls 改成在 UDP 寫出者寫出前才編 GYOP 序號，修掉「晚編號的封包先寫出時 control 被 Client 當成舊封包丟掉」的順序問題。

## 驗證與經過

- edge-results-resend：CTest 72／72、`go test -race -count=3`、權威 digest 兩樹 0 差異；突變 31 個全部殺死（第一輪 2 個是錯誤訊息不符，修正讀取迴圈的期限與期望值後殺死）。通道計時的局部 xhigh 檢查 must 0、should 3：新裁決可能晚到（無損模型 369 個裁決中 63 個晚到，最多 33 ms），§9「Client 看到的不變」說法太強，依 D107 改成「內容不變；新裁決最多晚 1 個間隔」，並列進下一批 L2 的對照。實際約 1 h（估 7～8 h）。
- edge-input-passthrough：舊的 50 個突變隨程式刪除，第一輪 214 個中 205 個殺死，其餘修正寫法後殺死。`ingress_cross_check.py` 只接受 I3 以 §9 宣告的形狀失敗（§14.4 的判定變更，寫了理由）。xhigh 程式檢查 should 5 全部補測試；L2 宣告第 1 版經 D45 檢查（必須修正 7、應修正 8），第 3 版由使用者核准（D109）。
- L2 第 1 次 session 停在第 58 項（B 樹 clean-60 的 I1 undetermined，關閉時被 reset 的已知競態，宣告缺漏）。依 §14.1 回報，使用者選 (a)（D110）：只改 I1 的規則、整個 session 重跑，第 1 次保留為證據。
- 第 2 次 session 98／98 完成、沒有停下、受干擾 1 個，**通過**。替代率 A−B 都在門檻內；soak 逐出 0、A／B 視窗比 0.97（門檻 1.5）；鎖等待 ≥250 µs 兩樹都是 0；Match 副本 3838 與預測相符，負載沒有上升（CPU 比 1.06）。實際約 7 h（估 20～26 h）。
- 同一段時間做了 heartbeat-sync 的睡眠跑次（只記錄）：Go 的單調時鐘睡眠時停止、C++ 的 `steady_clock` 繼續走；runtime link 撐過睡眠，Client 醒來後約 1 秒逾時斷線（既有行為），D111 記為體驗優化的未結事項。
- edge-results-passthrough：突變 168 個中 166 個第一輪殺死，2 個修正判讀後殺死。實作中發現 §9 的「每隔一則丟一則時 ≤2⌈n/8⌉ 則內送達」做不到（Client 只能連續 ACK），使用者選 (a) 改成 ≤2⌈n/8⌉＋2（D112），契約行為不變。
- L2 第 1 次 session 在第 12 項停下：受干擾的項目累計 3 個（Chrome 的 Helper 佔 CPU），完成的 11 項沒有判定不成立；不重跑、保留。第 2 次 session（D113，在使用者不用電腦的時段，Chrome 結束後）120／120 完成、受干擾 2 個，**通過**：下行延遲 A−B 都在 1 ms 門檻內，射擊時序最大 +2.26 ms（門檻 16.7 ms），回報 34 只出現在停頓的釋放。實際約 6 h（估 12～16 h）。
- Architecture Delta（D-14）：Gateway 新增 `resultSlot` 與診斷計數 `resultsForInactive`，刪除 session 帳本；edge-results-resend 只動 IpcHost 內部狀態。

## 證據

`build/target/_build/test/logs/pvp-v71-edge-results-resend-20261016/`、`pvp-v71-edge-input-l2-prep-20261016/`、`pvp-v71-edge-input-l2-20261017/`（第 1 次，停止）、`pvp-v71-edge-input-l2-20261017b/`（含 `judgement.md`）、`pvp-v71-heartbeat-sleep-20261017/`、`pvp-v71-edge-results-l2-prep-20261017/`、`pvp-v71-edge-results-l2-20261017/`（第 1 次，停止）、`pvp-v71-edge-results-l2-20261017b/`（含 `judgement.md`）（git 忽略）。
