# PvP v7.1 pv7-design：pv7 的契約一次定完，Gateway 回到無狀態邊緣

日期：2026-10-15～2026-10-16。**完成**（草案與方向評估 `eb62fd7`、定稿 `e009201`；只改文件，使用者核准定稿，D96）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 pv7-line 的 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`）。
計畫與紀錄見 [pv7-design](../object_fps_pvp/plans/v7.1/pv7-design.md)、[Gateway 的方向評估](../object_fps_pvp/plans/v7.1/gateway-direction.md)；產出是 [protocol-v7](../object_fps_pvp/protocol-v7.zh-Hant.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D92～D96。

## 做了什麼

- v7.1 採做法 (a)（D90）：pv7 是唯一一次 wire 變更，所有 wire 欄位與語意在這一批一次定完，之後各批照順序在同一條 PR 線實作。定稿後再改 wire 要先徵求同意。
- 設計（D92，ultracode）：3 個設計方向（最小改動 7 分、語意分離 7.5 分、以可診斷為主 6 分），評審以「語意分離」的規則骨架為底合成：診斷沒有否決權、診斷訊框不刷新活性、跨時鐘域只傳照抄的時間戳或單一時鐘域的持續時間、每個丟棄原因一個鍵。
- 中途插入 Gateway 方向的評估（D93，[設計事故 2026-10-15](../object_fps_pvp/plans/v7.1/Incident/2026-10-15-gateway-state-drift.md)：Gateway 從無狀態漂移成持有 Match 決策的副本）。3 位評估者分別評 (a) 無狀態邊緣、(b) 遊戲 UDP 直接到 Match、(c) 維持現狀，評審建議 (a)（7.5 分；(b) 5 分、(c) 5.5 分）。
- 使用者選 (a)（D94）：Gateway 只翻譯、逐則檢查、原樣轉送；去重、帳本與結果重送交給 Match。protocol-v7 因此多出 §2a「IPC 送達語意」，丟棄原因縮為 29 個，pv7-line 多出 edge 系列各批。
- 其餘待決事項照建議（D95）：D-1 不再需要；D-2 hitbox 欄位那批先放 A1 的 E、S 與影子紀錄；D-3 參考年齡在第一個 Tick 量；D-4 snapshot 的兩段停留時間上 wire；D-5 診斷值違規只記錄不關 link；D-6 動作拒收回覆送到 Client；D-7 確認排除清單。
- 定稿（D96）：另外加入 Client 的 GYOP 序號，帶在 PlayerInput／ActionBatch 上，只作診斷。

## 驗證與經過

- 設計的對抗式檢查：必須修正 3、應修正 5；xhigh 檢查：必須修正 2、應修正 7。例如「剩餘時間 ≤0 仍呼叫 Read 會讓 link 永不失效」「Client 以 409 判定版本不符會誤判 `room_full`」「Gateway 被 SIGTERM 時 final 送不出去，L2 等式做不到」，都已改進草案。
- 方向評估的預查：v7 的 L2 語料（445 份 Gateway 日誌）中，有狀態的出口不是 0（拒絕線溢出丟棄 1488、A3 丟棄 42、舊序號 Actions 436），所以「全部為 0 就選 (c)」的條件不成立。
- 更正（edge-input-passthrough 準備時）：Match 每個 clean-60 跑次多處理的重複命令原寫約 1.15 萬，是除錯了跑次數，正確約 3.84k；不影響 D-8 的結論。
- §2a 的 xhigh 對抗式檢查（約 22 分）：沒有任何發現推翻 D94；必須修正 2（結果的 30 Hz 改為 Gateway 每位玩家單槽持有、Match 最小 id 優先；中間 commit 也要轉送 ACK-only 批），都已套用。
- 工時：README 初版估 pv7-line 91 h，pv7-design 重估 111 h，D94 後 170～179 h，§2a 檢查後 177～191 h（約 1.07 倍上界，未達停止規則），加上序號欄位約 178～193 h，使用者接受。
- 留給後續量測的設定（都不是 wire 欄位）：A1 的冷卻保留上限 K 與 Client 的 S 取法，2026-10-19 依 pv7-checkpoint 的影子資料決定（D136，`72cef25`）；時鐘跳變門檻交給 heartbeat-sync 的 L2。
- 定稿之後，實作批又依使用者決定改了幾處條文（D99、D104、D105、D107、D112），wire 都不變。

## 證據

只改文件，沒有本機跑次；方向評估的預查讀的是 `build/target/_build/test/logs/pvp-v7-16-l2-20261015/`、`pvp-v7-16-l2b-20261015/`、`pvp-v7-08c-l2-20261011/`（git 忽略）。
