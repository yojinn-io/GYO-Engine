# PvP v7.1 dead-letter-position＋incident-close：死信維持兩份契約，兩份事故紀錄結案

日期：2026-10-20～21。**完成**（dead-letter-position：計畫 `34c7756`、守衛 `572cc39`、結案 `8ecfe17`；incident-close `786115a`；四層總表 `71f550e`；只改文件與一個 Go 守衛）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#82](https://github.com/yojinn-io/GYO-Engine/pull/82)（after-pv7 線）合併（`5ee499a`，2026-10-21）。
計畫與紀錄見 [dead-letter-position](../object_fps_pvp/plans/v7.1/dead-letter-position.md)、[2026-10-10 事故](../object_fps_pvp/plans/v7/Incident/2026-10-10-gateway-silent-drop.md)、[2026-10-15 設計事故](../object_fps_pvp/plans/v7.1/Incident/2026-10-15-gateway-state-drift.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D145、D148。

## 做了什麼

- dead-letter-position（P6）：確認 v7 的 D57 是否仍成立。D57 把死信定位成設計原則（每個出口只有執行或帶原因的死信兩種結果），不另開一份死信 Data Contract。`drop-report` 與 `log-contract` 都實作之後重看：ingress 日誌給工具與人、跟著日誌契約演進，`GatewayDropReport` 給 Match、跟著協議版本演進，讀者、單位、版本規則都不同，D57 的前提仍成立；兩邊已共用同一張原因名稱表與一份跨語言 golden（`drop_reasons.json`，29 個值）。
- 找到的缺口只有一個：測試只檢查「每個 `DropReason` 對到一個 ingress 鍵」，沒有檢查反方向。新增丟棄出口卻忘了上 wire，現在擋不住。
- 使用者選 (a+)（D145）：維持 D57（兩份契約、一套詞彙、一套原則），[日誌契約](../object_fps_pvp/log-contract.zh-Hant.md)第 5.1 節加 `drop_` 鍵與 `DropReason` 的對應表，再補反方向的 Go 守衛與突變，排在 `log-contract` 的四平台 L1 通過之後。
- incident-close（D148：使用者睡前交代，沒有問題就照建議走）：兩份事故紀錄改為已結案。2026-10-10「Gateway 悄悄丟命令」：修正是 v7 第 08c 批、`drop-report` 與 edge 系列，重新量測是 `d44-regression-pv7`（未再現）；M0 與 FireGate 隨 A1 取消。2026-10-15「Gateway 狀態漂移」：行動 1～4 全部完成；`log-contract` 的 D143（加 Gateway 套件變數要使用者同意）就是約束在運作的證據。
- 順帶處理 INVENTORY `N-netarch-ambiguity`：`network-architecture.zh-Hant.md` 的舊段落仍描述 pv6 的 Gateway 合併與去重，加補充指向 protocol-v7 §2a 與 Design Invariants，原文保留。
- 四層總表（使用者要求，由圖書管理員撰寫，併進 PR #82）：2026-10-15 設計事故紀錄第 11 節「後日談」開頭補上「補丁有多少」。第 1 層：Gateway 自己長出的狀態，4 次加入、8 種機制；第 2 層：那些機制留下的 33 個計數鍵與 11 個過渡原因；第 3 層：建在被截斷資料上的 3 組推導（FireGate 與 M0、late 修正的常數、相位估計器的常數）；第 4 層：待評估清單的 9 項。前兩層在 pv7 直接刪除，後兩層除了等 WAN 數據的一項，都已結案或取消。2026-10-10 事故紀錄第 7 節加連結。

## 驗證與經過

- 反方向的 Go 守衛第一次執行就失敗：12 個沒有 `DropReason` 的 `drop_` 計數器不是「只寫日誌」。對照分析器的 `removed_paths` 後分成兩類：pv7 刪除的路徑 11 個（依日誌規則 R1 保留鍵、恆為 0），以及同一事件以封包數上 wire 的另一個單位 1 個。計畫第 2 節把「沒有 `DropReason`」都當成「只寫日誌」，少看了這兩類；結論不變，守衛改成三類宣告，對應表列五類。
- 突變 `dlp-unclassified-drop-exit`（把一個轉送改成沒有分類的丟棄）killed；本機 `go test ./...` 通過；四平台 L1 通過（`572cc39`，run 39441359248），**`dead-letter-position` 結案**。
- 判定變更照 AGENTS.md §14.4 事後補記 1～5：只收緊、沒有放寬面；舊判定的失敗例（宣告為只寫日誌的計數器帶了 `DropReason`）手動確認新判定仍失敗。
- incident-close 只改文件，**結案**。結案條件裡的事故行動 4（AGENTS.md 加入各元件的設計約束）由圖書管理員另外完成（PR #79、#80）。

## 證據

沒有獨立的證據目錄：dead-letter-position 的證據是四平台 L1 與 repo 裡的守衛、突變；incident-close 只改文件。事故的重新量測見 `build/target/_build/test/logs/pvp-v71-d44-l2-20261021c/`（git 忽略，`d44-regression-pv7`）。
