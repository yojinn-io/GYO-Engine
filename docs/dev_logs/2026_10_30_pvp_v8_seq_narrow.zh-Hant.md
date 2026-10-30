# PvP v8：pv8-seq-narrow（序號欄位縮窄成 uint32、刪 match_id、預留號碼）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 high（大小釘局部 xhigh）｜Opus 5.5；§14.4 的審查者 Sonnet 5.5。

**狀態：** 完成。改了 wire，但版本值仍是 7；改成 8 在 `v8-product-crouch` 的 wire 接入時做。

相關文件：[事前宣告](../object_fps_pvp/plans/v8/pv8-seq-narrow-declaration.md)（`pv8-seq-narrow-declaration-approve`）、[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) §2a、§6、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。

## 做了什麼

- **proto**：84 個欄位 uint64 → uint32（client 38、runtime 46，用兩份 proto 機械核對）；`Welcome.match_id` 刪除為 `reserved 2; reserved "match_id";`；B／C 層的號碼照 §2a 的表預留。`GatewayDropReport.merged_streams` 保留 u64，補進 §2a 的保留清單（`pv8-seq-narrow-merged-streams` (a)）。Go 綁定以 protoc 36.2.0、protoc-gen-go v1.36.11 重產。
- **Go（Gateway）**：player id、動作號、tick 改成 uint32（`DecodeInput`／`DecodeActions` 的參數、`map[uint32]`、`reservation.playerID`、`dropEvent` 的序號欄位，寫進 `DropSample` 時擴張回 u64）。session id（GYOP 標頭）與診斷計數維持 uint64。`nextPlayer` 維持 uint64 計數，上一批的 `>= math.MaxUint32` 檢查之後才轉成 uint32。HTTP 的 join 回覆與 `GET /rooms` 拿掉 `match_id`；`ReadyFitsWelcome` 以 `PlayerId: math.MaxUint32` 計算。
- **C++**：`Wire.hpp` 新增 `wire::Counter(value)`：`GYO_ASSERT(value <= WireCounterMax)` 後轉成 uint32。IpcHost 30 處、ClientConnection 10 處寫縮窄欄位的 setter 全部經過它。u64 寫進 u32 的 setter 在變數上沒有編譯警告（只有常數會），所以靠掃描守衛確認沒有漏。C++ 內部型別維持 u64（`pv8-seq-cpp-types` (b)）。
- **大小（N3）**：以 `proto.Size` 實測並改寫 protocol-v8 §6；釘成完全相等。

  | 訊息（含 24 B 標頭） | 實測 | 釘住的位置 |
  |---|---|---|
  | 4 人 Snapshot，契約最大 | 724 B（40＋171N） | `capacity_test.go`（N＝1～4）、`worker_main.cpp` |
  | 4 人 Snapshot，型別最大 | 798 B（42＋189N） | `capacity_test.go` |
  | 4 人 Snapshot，合法的戰鬥值 | 674 B | `network_main.cpp` |
  | PlayerInput 12 個命令 | 402 B | `adapter_test.go` |
  | ActionBatch／ActionResults 8 筆 | 470／398 B | `actions_test.go` |

- **新守衛（N4）**：
  - `narrowing_test.go`：protoreflect 逐欄核對（縮窄集合是 uint32、保留集合維持原本的 kind、表外不得有 64 位元欄位）、預留號碼與 `reserved "match_id"`、client／runtime 同號欄位的 kind 一致；以及 2^32＋1 的 varint 解碼成 1、`DecodeInput` 的結果和送 1 相同。
  - `WireTests.cpp`：`wire::Counter` 在上限內轉型、超過上限觸發 Assert。
  - `test_wire_counter_sites.py`：縮窄欄位的 setter 都緊接 `wire::Counter(`，兩個檔各 30、10 處。
  - `server_test.go`：join 回覆與房間列表沒有 `match_id`。

## 檢驗與經過

- N1（事前宣告）：base `7b40c59a`、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「38 scenarios, 0 differing」。
- N2：既有的位元組穩定測試（`PvP datagram encoding of every message type is byte-for-byte stable` 等）不改判定即通過。
- N5：登記的突變 12／12 killed（新 9 個＋判定測試被改到的既有 3 個；find 落在改動行上的既有突變 0 個）。protoreflect 守衛另外手動改 proto 重產 3 次：`tick` 改 `fixed32`、`view_tick_q16` 改 `fixed64`、拿掉 `reserved 22 to 24`，3／3 由守衛本身抓到。
- N6：`test` preset 的 CTest 100／100，`go test ./...` 通過。
- §14.4（`pv8-seq-narrow-test-changes` (a)）：T1～T5 照宣告的表改；審查者（Sonnet 5.5）核對 5 點都成立：表以外只有型別跟著改（變數、map 鍵、參數、轉型），值、條件、斷言都沒動；40 個 setter 沒有漏。審查者指出 T2 的表漏列 `network_main.cpp`（N3 有列），以及「四處數值一致」的說法不精確（network 用合法值，pv7 時也是 1073 與 1019），已在宣告中補註。使用者核准（`pv8-seq-narrow-test-changes-approve`）。
- 和宣告不同的地方：
  - N4 的編碼 Assert：宣告寫「每個編碼邊界送 `WireCounterMax + 1`」。編碼函式在匿名 namespace，而且 Match 在上一批起就停在上限，從合法狀態走不到；照上一批 test-seam (a) 的做法，改成測 `wire::Counter` 的 Assert，再用掃描守衛確認 40 個 setter 都經過它。
  - proto 手動突變：第一版「`tick` 改回 u64」是因 Go 編譯失敗才判成 killed，不是守衛抓到的，不算檢出力。改成換 wire 型別但不換 Go 型別，並把守衛收緊成保留欄位必須維持原本的 kind。
- 經過：
  - Gateway 測試的型別跟著改花了不少輪：編譯器一次只報 10 個錯誤。用會話的工作檔裡的小工具（`gofix.py`）自動處理「`uint64(...)` 換 `uint32(...)`」與「往上找 `NAME := uint64(`」兩類，其餘逐一手改。
  - 1 個新突變（拿掉 snapshot 的 `respawn_tick` 複製）被別的測試（`TestV5SnapshotLifecycleReloadAndJumpValidation`）抓到，是 `expect` 登記錯，修正後 killed。
  - `hit_record_test.go`、`state_whitelist_test.go` 的 gofmt 問題在 HEAD 就有，沒有動。

## 證據

- 兩樹比對、突變報告、proto 手動突變的腳本與 log 在會話的工作檔（`narrow-n1-s1`、`narrow-n1-s3`、`narrow-mutants.json`、`narrow-mutants-2.json`、`proto_mutants.sh`、`proto-mutant-*.log`），不放進 repo；結果行照抄在上面。
