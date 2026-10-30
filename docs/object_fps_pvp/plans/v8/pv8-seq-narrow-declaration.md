# pv8-seq-narrow 的事前宣告

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：已核准（`pv8-seq-narrow-declaration-approve`，2026-10-30）：test-changes (a)、merged-streams (a)。完成：N1 0 差分、N5 12／12＋proto 手動 3／3、N6 100／100；T1～T5 已核准（`pv8-seq-narrow-test-changes-approve`），見 [dev_log](../../../dev_logs/2026_10_30_pvp_v8_seq_narrow.zh-Hant.md)。** 契約：[protocol-v8](../../protocol-v8.zh-Hant.md) §2a（縮窄、刪除、預留）與 §6（大小）（已定稿，`pv8-final-approve-round2`）。檔位 high（大小釘局部 xhigh）｜Opus 5.5，估時 6～10 h（超過 15 h 停下回報）。

## 範圍

會改 wire，但**版本值仍是 7**（改成 8 在 `v8-product-crouch` 的 wire 接入時做，protocol-v8 §8）。

1. **縮窄**：兩份 proto 中 84 個欄位 `uint64` → `uint32`（client 38 個、runtime 46 個）。逐欄清單由兩份 proto 機械列出，與 protocol-v8 §2a 的表一致；表沒有列到的 uint64 欄位只有 `GatewayDropReport.merged_streams`，照 §2a 的規則**保留** u64（只增不減的診斷計數，和 `samples_omitted` 同類），並補進 §2a 的「保留」清單。
2. **保留 u64**：`view_tick_q16`、`ClockSample` 的 3 個 `*_us`、`arena_digest`（fixed64）、`refused_batches`、`GatewayDropReport` 的 `report_sequence`／`samples_omitted`／`merged_streams`、`DropCount.total`、`DropSample` 的 7 個欄位。
3. **刪除** `Welcome.match_id`：`reserved 2; reserved "match_id";`；HTTP 的 `GET /rooms` 與 join 回覆的 JSON `match_id` 一起刪。
4. **預留**：protocol-v8 §2a「預留的號碼」表的全部號碼。
5. **Go**：adapter、server、drop_report、runtime_link 的型別跟著改（`playerID uint64` 參數、`map[uint64]` 等），由編譯器逼出。
6. **C++ 編碼邊界**：IpcHost 的編碼（Snapshot、ActionResults、JoinResult 等）與 ClientConnection 的請求編碼（PlayerInput、ActionBatch），每個縮窄欄位寫入前 `GYO_ASSERT(value <= WireCounterMax)`（Programmer Error，§2a 原則），再 `static_cast<std::uint32_t>`。解碼是 u32 → u64 的擴張，不需要檢查。C++ 內部型別維持 u64（`pv8-seq-cpp-types` (b)）。
7. **大小**：用 `proto.Size` 實測，把 protocol-v8 §6「縮窄之後（推論）」改成實測值並釘成測試。
8. 不做：版本值、蹲下與衝刺的欄位（`v8-product-crouch` 的 wire 接入）、B／C 層的任何行為。

## 受入判定（看結果之前宣告）

### N1：權威結果不變

- base＝`7b40c59a`（本批之前），branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 在 scale 1、3 各 1 次。
- 判定：**38 個情境 × 2 個 scale，差異 0 個**。決定性的比對，不重跑；有差異就停下回報。

### N2：實際的值在 wire 上的位元組不變

- varint 編碼下，小於 2^32 的值無論欄位是 uint64 還是 uint32，位元組都相同。
- 判定：既有的位元組穩定測試（C++ `PvP datagram encoding of every message type is byte-for-byte stable` 等）**不改判定**就通過。若有任何一個要改判定才能通過，停下回報（表示 fixture 用了超過 2^32 的值，或縮窄改到了不該改的欄位）。

### N3：大小實測並釘住

- 以 `proto.Size` 量這幾項（定義照 protocol-v8 §6 的「契約最大」與「型別最大」，uint32 取 `MaxUint32`）：
  - 4 人 Snapshot 的契約最大與型別最大；
  - 12 個命令的 PlayerInput；
  - 8 筆的 ActionBatch 與 ActionResults；
  - 最大的 Welcome。
- 判定：每一項加 24 B 標頭都 ≤1200 B；數值寫進 protocol-v8 §6，並在 `capacity_test.go`、`worker_main.cpp`、`network_main.cpp`、`actions_test.go`（Welcome 邊界）釘成**完全相等**。同一個構造在各處的數值一致（`worker_main.cpp` 與 `capacity_test.go` 都是契約最大；`network_main.cpp` 用合法的戰鬥值，數字本來就不同：pv7 是 1073 與 1019；2026-10-30 審查者指出後補註）。
- 推論值（約 740／822／450 B）只當參考，不是判定。

### N4：守衛

- **protoreflect 覆蓋（Go）**：
  - 走兩份 proto 的描述子，縮窄集合的每個欄位是 uint32、保留集合的每個欄位仍是 uint64，集合以本宣告的表為準；
  - 預留號碼與 `reserved "match_id"` 都存在；
  - client 與 runtime 的同名訊息，同號欄位的型別一致。
  - 檢出力：手動改 proto 兩次重產（把一個縮窄欄位改回 uint64、拿掉一個 reserved），測試都要失敗；結果記在 dev_log。
- **往返**：每個縮窄欄位以 `MaxUint32` 經 adapter 往返不失真（既有的往返測試改用 u32 上限，見 §14.4 的 T1）。
- **編碼 Assert（C++）**：每個編碼邊界送 `WireCounterMax + 1`，`ScopedAssertionHandler` 抓到 Assert；送 `WireCounterMax` 正常編碼。
- **截斷（對抗式檢查 #1 的建議）**：Gateway 解碼一個帶 2^32＋1 原始 varint 的 PlayerInput 時，行為和送入截斷後的值（1）相同，不 crash。
- **match_id**：Go、C++、Python 都沒有讀取路徑；HTTP 回覆沒有 `match_id` 鍵。

### N5：突變

- 新守衛以 `mutations.json` 登記：拿掉某個編碼 Assert、拿掉某個 adapter 的逐欄複製、Welcome 大小計算改回 u64 上限等，每個都要 killed。只在預期失敗時算 killed。
- D184 的相關突變（find 落在改動的行上）全部 killed；find 因改型別而失效的，更新 find（語意相同）並記在 dev_log。

### N6：既有測試

- `test` preset 的全部 CTest 與 `go test ./...` 通過。四平台 L1 在 PR 時跑，凍結的探針（`action_probe.py`、`impaired_network.py`、`backpressure_probe.py`）在 L1 中實跑；這些探針只看 wire type 與版本位元組，縮窄不改變它們（varint 仍是 wire type 0，版本值仍是 7）。

## 會改判定的既有測試（AGENTS §14.4，先列出）

縮窄是已定稿的契約變更，下面這些測試的輸入或釘住的數字必須跟著改。核准本宣告時一併核准「做法」；實作後再照上一批的方式做突變與另一位審查者的核對，把每一項的 1～5 與證據交給你核准後才 commit。

| # | 測試 | 守的性質 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|---|
| T1 | Go 的往返與上限測試：`adapter_test.go`（PlayerInput）、`hitbox_test.go`、`actions_test.go`、`drop_report_test.go` 的 `PlayerId`、`capacity_test.go` 的欄位值 | 欄位取型別最大值時經 adapter 往返不失真、不被拒 | 縮窄欄位的 `math.MaxUint64` 改成 `math.MaxUint32`；保留 u64 的欄位不改 | 不再檢查 u32 上限以上的值（型別已裝不下，編譯器會擋） | 無 |
| T2 | 大小釘：`capacity_test.go`（45＋257×N）、`worker_main.cpp:629`（1073）、`actions_test.go` 的 Welcome＝1200 邊界 | 最壞情況 ≤1200 B，且任何欄位增大都會被抓到 | 改成 N3 的實測值，仍是完全相等 | 無（完全相等的釘） | 數字變小，餘裕的變化會被抓到 |
| T3 | `worker_main.cpp:442` 以 u64 上限當 observed authority tick | D183：統計窗口中 worker 已沒有的權威 | 改成 `WireCounterMax` | 不再送 u32 上限以上的 tick（編碼 Assert 會擋，屬 Programmer Error） | 無 |
| T4 | `network_main.cpp:126-148`、`worker_main.cpp:588-624` 的最大值訊息 | 最大值訊息的編碼與大小 | 縮窄欄位改用 u32 上限 | 同 T1 | 無 |
| T5 | `server_test.go` 的 `credentials.MatchID`、`actions_test.go` 的 `MatchId` | join 回覆與 Welcome 的欄位 | 刪除；改為斷言回覆沒有 `match_id` | 無 | 多斷言「不存在」 |

開工後若發現本表以外的測試要改判定，停下回報（不重蹈上一批的覆轍）。

## 等你決定

### `pv8-seq-narrow-test-changes`：上表 T1～T5 的做法

- (a) 照上表；實作後做突變與另一位審查者的核對，把 1～5 與證據交給你核准再 commit。
- (b) 照上表，但 T2 的大小釘先放寬成「≤1200 B」，等 `v8-product-crouch` 的 wire 接入後才釘完全相等（兩批會連續改同三處的數字）。

建議 (a)：完全相等的釘是抓「無意中加了欄位」的唯一手段；兩批各重釘一次，成本只是改數字。

### `pv8-seq-narrow-merged-streams`：`GatewayDropReport.merged_streams` 保留 u64

protocol-v8 §2a 的表沒有列它；照 §2a 的規則（漏列的預設保留），保留 u64，並在 §2a 的「保留」清單補上。

- (a) 保留 u64 並補列（建議：它是只增不減的診斷計數，和 `samples_omitted` 同類）；
- (b) 縮窄成 u32。

## 範圍的限界

- 本機只有 Intel Mac；N1 只在這台比，跨平台靠 golden 子集與 L1。
- 版本值仍是 7：pv7 與「pv8 縮窄後」的程式在 wire 上無法以版本區分。兩者不會混用（三個角色在同一個 PR 合併，PR 合併前版本值會改成 8），但在這個 PR 的中間 commit 上，舊 Client 連新 Gateway 時不會被版本拒收。
- 這批不做 L2／L3。
