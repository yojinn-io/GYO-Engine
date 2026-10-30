# PvP v8：pv8-obs-bytes（實際位元組與超車比例的觀測）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 medium，引擎 Opus 5.5。

**狀態：** 實作完成（`a7455eb9`），不改 wire。實際的數字要在下一次 L2／LAN 場次收集。Gateway 新增的診斷狀態欄位是 Architecture Delta，等使用者同意。

## 做了什麼

- Gateway `player send statistics` 行尾追加 `results_bytes`、`snapshot_bytes`：該視窗內完成寫出的資料包大小（含標頭）的 `p50/p90/p99/max`，沒有寫出時為 `-`。新型別 `sizeStats`，在 `snapshotWritten`、`actionWritten` 記錄 `len(packet.packet)`。
- Client `worker statistics` 行尾追加 `stale_snapshot_datagrams`、`stale_results_datagrams`：`stale_datagrams` 之中 Snapshot 與 ActionResults 的個數（`ClientConnection.cpp` 的 stale 判斷處依 `packet->type` 分開計數）。用途：量 ActionResults 被較新的 snapshot 超車而被丟的比例（pv8-round2 附錄 B-1 的前置條件）。
- log contract：兩個行族的表加上追加的鍵；照 R1 只在行尾追加，值的文法照 §3。`keys-v1.tsv` 是 v1 鍵序列的前綴，不必改。
- `state_whitelist_test.go`：`sizeStats` 與 `sendStatistics.resultBytes`／`snapshotBytes`／`sizeStats.sizes` 分類為既有的 `classDiagnostic`（和 `intervalStats` 同類）。守衛的判定規則不變，但 Gateway 多了狀態欄位，照 network-architecture 的 Design Invariants 屬 Architecture Delta。

## 檢驗

- Go：`go test ./...` 通過；`TestSendStatisticsLineKeepsEveryKey` 改成含新鍵的逐字比對，記錄測試加上大小的斷言與 take 後歸零。
- C++：`NetworkStatisticsTests.cpp` 的逐字比對改成含新鍵，並加一個新鍵的測試。
- `test` preset 全部 93 個測試通過。
- D184 的相關突變（find 與改動行重疊的 3 個）：3／3 killed。
