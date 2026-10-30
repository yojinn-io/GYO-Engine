# pv8-seq-ceiling 的事前宣告

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：草稿，等你核准（含下面「等你決定」的 2 項）。** 契約：[protocol-v8](../../protocol-v8.zh-Hant.md) §2a「用盡的位置」（已定稿，`pv8-final-approve-round2`）。檔位 high｜Opus 5.5，估時 4～6 h（超過 9 h 停下回報）。

## 範圍

不改 wire（proto、欄位型別、版本值都不動）。把每一處「計數用盡」的條件從 uint64 上限改成產品常數 `WireCounterMax`＝`UINT32_MAX`（放在 `Wire.hpp`），用盡後的行為照原樣。

### 實際的位置（以 `eb056b2c` 為準）

| # | 位置 | 現在的條件 | 行為（不變） | 在 §2a 表中 |
|---|---|---|---|---|
| 1 | `PvpMatch.cpp:19-23` `FutureTick` | `tick > u64max - duration` | 拋 `Authority deadline exhausted` | 有 |
| 2 | `PvpMatch.cpp:151` `SubmitActions` | `tick_ == u64max` | 拋 `Authority tick exhausted` | 有 |
| 3 | `PvpMatch.cpp:270-273` 重生 | life、epoch `== u64max` | 拋 `overflow_error` | 有 |
| 4 | `PvpMatch.cpp:422` `ContiguousPending` | 迴圈上限 `sequence < u64max` | 計數停在上限 | **沒有** |
| 5 | `PvpMatch.cpp:431` `ResetMovementEpoch` | epoch `== u64max` | 拋 `overflow_error` | 有 |
| 6 | `PvpMatch.cpp:458` `Tick` | `tick_ == u64max` | 拋 `Invalid match tick` | **沒有** |
| 7 | `PvpMatch.cpp:477` | `lastResolvedCommand == u64max` | `SequenceExhausted` 重設 | 有 |
| 8 | `LocalPlayerPrediction.cpp:80` `SeedLead` | 迴圈上限 `< u64max` | 少產領先命令 | 「開工時列出」 |
| 9 | `LocalPlayerPrediction.cpp:391` `Advance` | `< u64max` 才產命令 | 不產命令 | 同上 |
| 10 | `LocalPlayerPrediction.cpp:497` | `== u64max` | `frozen` | 同上 |
| 11 | `ClientConnection.cpp:1019` | `allocatedThrough == max<ActionId>` | `Action IDs exhausted` | 有 |
| 12 | `gateway/server.go:358` | `nextPlayer == ^uint64(0)` | HTTP 503 `player_ids_exhausted` | 有 |

- 4、6 是 §2a 的表漏掉的，條件和 2、7 同類。不改的話，tick 或命令序號在 2^32 之後還會前進，下一批縮窄時會撞上編碼的 Assert。
- 1 的新條件寫成 `duration > WireCounterMax || tick > WireCounterMax - duration`，避免 `duration` 大於上限時無號相減繞回。
- 編碼邊界的 `GYO_ASSERT(≤ WireCounterMax)`（§2a 原則）留到 `pv8-seq-narrow`，和型別縮窄一起做。這批只改條件。

## 受入判定（看結果之前宣告）

### K1：權威結果不變

- base＝`eb056b2c`（本批之前），branch＝本批，同一台 Intel Mac，兩棵樹各建 `gyo_object_fps_pvp_authority_digest`，`compare_authority_trees.py` 在 scale 1、scale 3 各比 1 次。
- 判定：**38 個情境 × 2 個 scale，差異 0 個**才通過。決定性的比對，不重跑；有差異就停下回報（AGENTS §14 開頭），不修改後重比。

### K2：每一處都有邊界測試

- 每個位置一個測試：值＝`WireCounterMax − 1` 時照常前進，值＝`WireCounterMax` 時出現上表的行為。
- 突變：每處把條件改回 uint64 上限（或拿掉檢查），對應的測試必須失敗。以 `mutations.json` 登記，只在預期失敗時算 killed。12 處都要 killed 才通過。

### K3：既有測試

- `test` preset 的全部 CTest 與 `go test ./...` 通過。D184 的相關突變（find 落在改動檔案的既有突變）全部 killed。

## 等你決定

### `pv8-seq-ceiling-test-seam`：測試怎麼走到 2^32

測試沒辦法真的跑 42 億個 tick。目前沒有任何用盡邊界的測試（`grep exhausted` 只找到領先命令用盡的測試，與計數無關）。各處能不能從外部把值設到邊界：

| 位置 | 從現有的公開介面能不能到 |
|---|---|
| 8～10（預測） | 能：`SeedLead` 收的權威 `PlayerState` 可以直接給 `lastResolvedCommand＝WireCounterMax − 1` |
| 12（Gateway） | 能：Go 測試在同一個 package，可以直接設 `nextPlayer` |
| 1～7（Match） | 不能：tick 只能由 `Tick` 逐一推進，epoch、life、命令序號也只能一步一步加 |
| 11（Client 動作號） | 不能：`allocatedThrough` 在 `Impl` 裡 |

選項：

- (a) 把判定抽成一個純函式 `WireCounterExhausted(value)`（加上 `FutureTick` 的版本）放在 `Wire.hpp`，各處呼叫它；測試直接測函式的邊界，各處用突變確認「呼叫的是它」。不加測試專用介面，但突變只能證明 1～7、11 呼叫了函式，不能證明行為在 Match 裡真的接上。
- (b) 給 `PvpMatch` 與 `ClientConnection` 加測試用的建構參數（例如起始 tick、起始動作號），只在測試中使用。行為能端到端地測，但等於公開一個產品不用的介面，日後可能被誤用。
- (c) (a) 為主，另外只對 2、6、7 這三處（tick 與命令序號，最可能在長時間對局碰到）用 (b) 的起始 tick 參數做端到端測試。

建議 (a)：這批的本質是換常數，函式的邊界測試加突變就能抓到「條件寫錯」與「某處沒換」兩類錯誤；端到端的保證交給 `pv8-seq-narrow` 的編碼 Assert。

### `pv8-seq-ceiling-table-fix`：§2a 的表加上 4、6

protocol-v8 已定稿。這兩行是範圍清單的補漏，不改 wire 也不改語意。建議 (a) 在 protocol-v8 §2a 的表加上這兩行，並註明是本批補的；(b) 不改 protocol-v8，只寫在本宣告與 dev_log。

## 範圍的限界

- 本機只有 Intel Mac；K1 只在這台比，跨平台靠 golden 子集與 L1。
- 這批不碰 wire，不做 L2／L3。
