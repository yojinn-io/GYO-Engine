# PvP v8：pv8-contract-round2（第 2 輪寫進 protocol-v8）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 medium（條文）＋對抗式檢查 1 次（Sonnet 5.5）；引擎 Opus 5.5。

**狀態：** 完成（`044584fd`）。[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) 是草稿，等使用者核准定稿。

## 做了什麼

- protocol-v8 加入第 2 輪 A 層：§2a（序號欄位縮窄成 u32 的清單、保留 u64 的欄位、刪除 `match_id`、上限的根據、用盡的位置、預留號碼）；§6 縮窄後的大小（推論，`pv8-seq-narrow` 實測改寫）；§7 改指向 pv9；§8 pv8-line 的批次順序；附錄 B（pv9 的事件串流、對局狀態、動作種類）與附錄 C（只記形狀的項目）。
- 主會話核對兩份 proto 的最大號：第 2 輪合成稿把 `MatchStatus` 放在 `WorldSnapshot` 第 4 號，和 client 的 `match_hold_us` 撞號，改成 client 6／runtime 5。ActionResults、ActionBatch 也依 client／runtime 各自的下一個空號配。
- protocol-v7 §3 的 Hello 間隔「1000 ms」改成程式的 250 ms（`pv8-hello-doc-mismatch` (a)，出處 v7.1 D188、`ClientConnection.cpp:425-429`）。

## 對抗式檢查（Sonnet 5.5，只讀，約 3 分）

- 欄位表、保留清單、預留號碼表都對得上兩份 proto，沒有撞號或 client／runtime 寫反。
- must 2 個，已納入：用盡條件要逐處改成對 `WireCounterMax` 判斷（新增「用盡的位置」表，8 處）；`nextPlayer` 用 `^uint64(0)` 判斷時，縮窄後第 2^32＋1 位玩家會被截斷成 id 1。
- should，已納入：`RuntimeError.player_id`、`GatewayDropReport.player_id` 漏列；`match_id` 的刪除要含 `GET /rooms` 的 JSON 與相關測試、文件；Go 端隨縮窄要改的型別；reserved 號碼啟用時要先移除 reserved 行；附錄 B 的 `LanePending()`／`LaneDue()` 一致性、事件框被取代與 DropReason 34、link 重建與短線重連分開。
- note：截斷的判讀找不到反例（Gateway 不做跨訊息比較，先截斷再驗證），補寫了根據。
