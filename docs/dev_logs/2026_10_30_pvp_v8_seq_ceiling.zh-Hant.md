# PvP v8：pv8-seq-ceiling（計數用盡的上限改為 WireCounterMax）

日期：2026-10-30。分支 `claude/pvp-v8-pv8-line`。檔位 high｜Opus 5.5；§14.4 的審查者 Sonnet 5.5。

**狀態：** 完成。不改 wire。下一批是 `pv8-seq-narrow`（型別縮窄、刪 `match_id`、預留號碼、編碼邊界的 Assert）。

相關文件：[事前宣告](../object_fps_pvp/plans/v8/pv8-seq-ceiling-declaration.md)（`pv8-seq-ceiling-declaration-approve`）、[protocol-v8](../object_fps_pvp/protocol-v8.zh-Hant.md) §2a「用盡的位置」、[v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md)。

## 做了什麼

- 新增 `include/RetroFPS/Pvp/WireCounter.hpp`：`WireCounterMax`（＝uint32 上限）、`WireCounterExhausted(value)`（`value >= WireCounterMax`）、`WireCounterAdd(value, amount)`（超過上限回 nullopt，`amount` 大於上限時不會無號繞回）。
  - 和契約的差異：protocol-v8 §2a 寫放在 `Wire.hpp`。`Wire.hpp` 依賴 `GYO::Net`，Match 的 domain 程式庫沒有連結 Net；放進去會多一條依賴。所以另開只用標準庫的標頭，`Wire.hpp` 引入它。
- 12 處的條件改為呼叫上面的函式，用盡後的行為不變：`PvpMatch.cpp` 8 處（`FutureTick`、`SubmitActions`、重生的 life 與 epoch、`ContiguousPending`、`ResetMovementEpoch`、`Tick`、`SequenceExhausted`）、`LocalPlayerPrediction.cpp` 3 處、`ClientConnection.cpp` 1 處；Gateway 的 player id 改為 `nextPlayer >= math.MaxUint32`。
- protocol-v8 §2a 的表補上 `ContiguousPending` 與 `Tick` 兩處（`pv8-seq-ceiling-table-fix` (a)）。
- 測試：
  - `WireTests.cpp`：判定函式的邊界（`pv8-seq-ceiling-test-seam` (a)）。
  - `PredictionTests.cpp`：預測在 `WireCounterMax − 1` 與 `− 1 − InitialCommandLead` 的端到端情境。
  - `player_id_ceiling_test.go`：最後一個 id 是 `MaxUint32`，下一個回 503。
  - `test_wire_counter_sites.py`（CTest `object_fps_pvp.wire_counter_sites`）：每個檔案呼叫判定函式的次數，以及沒有任何地方用回 uint64 上限。Match 與 Client 動作號在測試中到不了 2^32，用它確認各處接上；新增一處時要改這裡的計數。
- 既有測試 `PvP sequence and epoch boundaries never wrap into an old command namespace` 的判定變更（下面「經過」）。

## 檢驗與經過

- K1（事前宣告）：base `eb056b2c`、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「38 scenarios, 0 differing」。
- K3 第 1 次不成立（停止，AGENTS §14）：上面的既有測試以 `u64max − 1` 為權威序號，預期 1 個領先命令。宣告寫「目前沒有任何用盡邊界的測試」是錯的：偵察時只搜了 `exhausted`。使用者選 (a)（`pv8-seq-ceiling-wrap-test-a`）：
  - 預測段的邊界改為 `WireCounterMax`，原有斷言全部保留；Match 段的輸入仍用 u64 上限，判定不變。
  - 新增「超過上限」情境：`u64max − 1` 時 0 個領先命令、凍結，`Advance` 後仍 0 個。
  - §14.4 的 1～5：守的性質是「計數到盡頭時不繞回舊的命令空間」；放寬面是不再要求 u32 上限到 u64 上限之間照常前進；收緊面是 u32 上限處就停、超過上限即用盡。
  - 證據：同一組 3 個語意突變（SeedLead 不設上限、Advance 不設上限、frozen 忽略盡頭），舊判定在 `eb056b2c`、新判定在本批都失敗，未改壞時都通過。審查者（Sonnet 5.5）核對 5 點都成立，沒有找到其他放寬。使用者核准（`pv8-seq-ceiling-wrap-test-approve`）。
- K2：登記的 16 個突變 16／16 killed。第 1 次跑有 7 個判成 `wrong_failure`，原因在登記（Test 自身的錯）：
  - `expect` 是正規表示式，測試名中的 `(protocol-v8 section 2a)` 被當成群組；已跳脫。
  - Go 的突變改回 `^uint64(0)` 後 `math` 沒被用到而編譯失敗；替換改用語意相同的 `math.MaxUint64`。
  - 修正後這 7 個重跑全部 killed。
- D184 的相關突變：沒有既有突變的 find 落在改動的行上，0 個。
- K3：`test` preset 的 CTest 100／100，`go test ./...` 通過。

## 證據

- 兩樹比對、突變報告、語意突變的腳本與結果在會話的工作檔（`ceiling-k1-s1`、`ceiling-k1-s3`、`ceiling-mutants.json`、`ceiling-mutants-2.json`、`wrap_mutants.py`、`wrap-mutants.txt`），不放進 repo；結果行照抄在上面。
