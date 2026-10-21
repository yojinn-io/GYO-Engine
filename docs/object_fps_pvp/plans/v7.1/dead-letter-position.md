# dead-letter-position：確認 D57（計畫，第 1 版）

- 狀態：**結案**（D145 選 (a+)；四平台 L1 通過，run 39441359248；第 5 節）。
- 依據：README 的 `dead-letter-position` 列（「確認 D57 是否仍成立：ingress 日誌和丟棄回報維持兩份契約、對齊同一套原則，或合成一份」；只改文件；估 2 h）、INVENTORY P6、`plans/v7/HANDOFF.md` D57（:102）、D73⑤（:118）、「死信隊列」評估 7～9（:492-494）、`network-architecture.zh-Hant.md:297`（死信原則）、`protocol-v7.zh-Hant.md` §4、`log-contract.zh-Hant.md` 第 5 節。
- 檔位：medium（只讀與文件）。選 4 的 (a+) 時多一個小測試（high，約 0.5 h）。
- file:line 以 `fdb94a4` 為準。

## 1. 問題

D57（2026-10-11）把死信定位成**設計原則**：每個出口只有「執行」或「帶原因的死信」兩種結果；序號連續＋到期替代讓 Match 偵測得到缺號；死信有界。log 契約（原第 10 批）與 `GatewayDropReport`（原第 11 批）以這些原則對齊，不另開一份死信 Data Contract。D73⑤ 把「ingress v1 與 `GatewayDropReport` 是否統一成一份死信 Data Contract」移到 v7.1。現在兩邊都已實作（`drop-report`、`log-contract`），要確認 D57 是否仍成立。

## 2. 現況（讀程式與文件得到的事實）

| 面向 | ingress v1（日誌） | `GatewayDropReport`（wire） |
|---|---|---|
| 讀者 | 驗收工具與人（`ingress_evidence.py` 等） | Match（記帳與 `gw_drop` 日誌） |
| 單位 | 每 10 秒視窗的累計計數，玩家行 6 組、全域行 1 組 | 每個串流的累計計數，加至多 4 筆樣本（身分，不帶內容） |
| 版本 | 日誌 `version=1`，規則在 [log-contract](../../log-contract.zh-Hant.md) R3 | wire 的 pv7，規則在 [protocol-v7](../../protocol-v7.zh-Hant.md) §1、§4 |
| 詞彙 | `drop_<reason>_<kind>_<unit>` 鍵 | `DropReason` 值名＝ingress 鍵轉大寫，一值一鍵 |

兩邊已經共用一套詞彙：

- 原因字只在 `gateway/ingress_statistics.go` 的名稱表定義一次；`DropReason` 的值名由它推出（`protocol-v7.zh-Hant.md` §4）。
- `tests/object_fps_pvp/fixtures/drop_reasons.json`（29 個值）綁定每個 `DropReason` 的值、名稱、ingress 鍵與範圍；Go（`drop_report_golden_test.go:43`）、C++（IPC 測試）、Python（`test_drop_reasons.py`，讀 proto 原始碼）都讀它。
- 只寫日誌、不上 wire 的原因（`room_unavailable`、`cleared_on_close_*`、`abandoned_on_close_*`：發生時 link 已失效，結構上無法回報）在 `protocol-v7.zh-Hant.md` §4 以文字列出。

找到的缺口（只有一個）：測試檢查「每個 `DropReason` 對到一個 ingress 鍵」，但沒有檢查反方向——一個 `drop_` 計數器沒有對應的 `DropReason` 時（`drop_report_golden_test.go:80-83` 跳過 reason 0），它是不是已宣告的「只寫日誌」原因。新增一個 Gateway 丟棄出口卻忘了上 wire，現在不會被擋；那種丟棄仍然有原因計數（符合 D57 的第一條），但 Match 永遠不知道，而且文件的清單會悄悄過時。

## 3. 評估

- **D57 的前提仍成立**：兩份東西的讀者、單位、版本規則都不同（日誌給工具與人、跟著日誌契約演進；wire 給 Match、跟著協議版本演進）。合成一份契約會讓日誌的改動牽動協議版本，或反過來；這正是 D57 不採 (c) 的理由，v7.1 的實作沒有改變它。
- **「對齊同一套原則」已經從文字變成可檢查的東西**：同一張名稱表、同一份跨語言 golden。要補的只有上面的反方向缺口。
- 結論：D57 仍成立；建議維持「兩份契約、一套詞彙、一套原則」，並把對應關係寫清楚。

## 4. 等你決定

- **(a) 維持 D57，只改文件**：`log-contract.zh-Hant.md` 第 5 節加一張對應表（ingress `drop_` 鍵 ↔ `DropReason` ↔ 上 wire 或只寫日誌，含理由），並指向 `drop_reasons.json` 與三種語言的測試；`network-architecture.zh-Hant.md:297` 的 D57 段落更新為「v7.1 已以同一張名稱表與 `drop_reasons.json` 對齊」；`protocol-v7.zh-Hant.md` §4 的只寫日誌清單連到該表；INVENTORY P6 結案。約 1 h。
- **(a+) 同 (a)，再補反方向的守衛（建議）**：Go 測試斷言「沒有 `DropReason` 的 `drop_` 計數器」恰好等於宣告的只寫日誌集合；新增出口而沒有分類就失敗。另加 1 個突變（把一個上 wire 的原因改成 reason 0 → 測試失敗）。約 1.5 h，四平台 L1。
- **(b) 合成一份死信 Data Contract**：日誌與 wire 共用一份文件與一套版本。不建議（理由見第 3 節）；若選它要重新規劃，估 6～10 h，並觸及 protocol-v7 的版本規則。

## 5. 實作（D145，(a+)）

- 使用者：「dead-letter-position 選 (a+)：維持 D57，文件加對應表，再補反方向的 Go 守衛與突變。L1 通過後實作。」`log-contract` 的四平台 L1（run 39428630663，`71c9c76`）通過後開始。
- **守衛的第一次執行就抓到計畫沒列的類別**：照第 4 節寫成「沒有 `DropReason` 的 `drop_` 計數器＝只寫日誌集合」時，有 12 個計數器不在集合裡。對照分析器的 `removed_paths`（`tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md:83`）後分成兩類：
  - 已刪除的路徑（11 個）：pv7 刪掉的出口（edge-input、edge-results 的 A3、edge-actions 的 A5 與 D56、D-12 的兩個 snapshot 守衛）。依日誌 R1 鍵留在 v1，恆為 0。
  - 另一個單位（1 個）：`drop_ipc_queue_overflow_input_commands`，同一事件以封包數上 wire（32）。
  兩類都不是新出口、不改 wire，也不改 D57 的結論；第 2 節「只有一個缺口」的判斷不變，只是第 2 節把「沒有 `DropReason`」都當成「只寫日誌」，少看了這兩類。守衛改成三類宣告。
- 程式：`drop_report_golden_test.go` 的 `TestDropReasonsMatchTheGoldenAndTheKeys` 雙向檢查（宣告的計數器確實是沒有 `DropReason` 的丟棄；每個沒有 `DropReason` 的 `drop_` 計數器都有宣告類別）。原本只檢查只寫日誌的 10 個計數器沒有 `DropReason`，改寫後這個檢查保留，範圍擴大到三類。
- 突變 `dlp-unclassified-drop-exit`（批次 `v71-dead-letter-position`，`go` 檢查）：把轉送的 `fwd_stale_sequence_input_packets` 改成丟棄（新出口沒有 `DropReason` 也沒有宣告）→ killed。
- 文件：`log-contract.zh-Hant.md` 第 5.1 節（對應表，五類）；`protocol-v7.zh-Hant.md` §4 的「不上 wire」段落連到該表；`network-architecture.zh-Hant.md` 的 D57 段落加 v7.1 的補充；INVENTORY P6 已解決。
- 驗證（本機）：`go test ./...`（gateway）通過；突變 1／1 killed；`test_run_mutations.py` 通過。四平台 L1 通過（`572cc39`，run 39441359248）。
- 判定變更的說明（AGENTS.md §14.4 的 1～5；這個測試不是 Design Invariants 的守衛，也不是驗收判定，照該節的精神事後報告）：
  1. 守的性質：每個 `drop_` 計數器要嘛上 wire（有 `DropReason`），要嘛有宣告的理由不上 wire。
  2. 放寬的面：無。舊判定失敗的情況（宣告只寫日誌的計數器帶了 `DropReason`）新判定仍失敗：手動讓 `drop_cleared_on_close_commands` 帶上原因，新判定報「declared unreported but is not an unreported drop」。沒有既有突變以這個測試的舊訊息為預期。
  3. 收緊的面：沒有 `DropReason` 又沒有宣告的 `drop_` 計數器（新出口忘了分類）原本通過，現在失敗；宣告的計數器若不是丟棄鍵也失敗。舊判定用 `Fatalf` 停在第一個，新判定用 `Errorf` 列出全部。
  4. 不適用（沒有放寬）。
  5. 根據與證據：本節前面的 12 個計數器與 `removed_paths` 的對照；突變 `dlp-unclassified-drop-exit`。
