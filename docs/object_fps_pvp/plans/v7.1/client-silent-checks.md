# client-silent-checks：Client 的再檢查與「應為 0」的分支（HEAD `831ccd9`；2026-10-24 使用者決定，D183）

日期：2026-10-24。D181 新增的批次，處理 [silent-branch-asserts.md](silent-branch-asserts.md) 第 8 節的 K1～K4。PR #95，分支 `claude/pvp-v71-system-observe-test`。

分類和 silent-branch-asserts 相同（[error-handling.md](../../../architecture/error-handling.md)）：走不到的是 Programmer Error，改成 assert；走得到的照現狀保留，或加計數。Go 端的 assert 照 D180 用 (g2)「讓 runtime link 失敗」。

方法：主會話逐點讀 HEAD 的程式，追「為什麼走不到」的守衛。行號以 HEAD 為準。

## 1. K1（U1）：時間線 `Push` 的回傳值被丟掉

位置：`PvpApplication.cpp:446-447`，`static_cast<void>(timeline.Push(...))`。

`SnapshotTimeline::Push`（`SnapshotTimeline.hpp:50-63`）在以下情況回傳 false：tick 為 0 或不比上一份新；到達時刻倒退；玩家欄位無效、id 重複；某位玩家的 epoch 或 life 比上一份小。

為什麼走不到：

- 進入 `receivedSnapshots` 的 snapshot 都通過了 `ClientConnection.cpp` 的驗證：tick 不為 0、id 不為 0 且不重複、位置有限、yaw／pitch 有效（`ValidMovementCommand`）、epoch 與 life 不為 0。
- 只有 tick 比已套用的新、epoch 與 life 不倒退的才會套用並排入佇列（`:629-638`）。佇列滿時丟最舊的，順序不變。
- 到達時刻是同一個網路執行緒逐包讀的 `Clock::now()`（`:518`），不會倒退。
- 時間線和 `state.snapshot` 一起重來：換代時兩邊都清（App 在換代時 `timeline.Reset()`），App 不在 Playing 或沒有自己時也 `Reset()` 並且不推。同一代內清掉 `state.snapshot` 的只有 `RefreshLobby`，那一代是離開請求，不會再有 snapshot。

**建議**：assert（`const bool pushed = timeline.Push(...); GYO_ASSERT(pushed);`）。

## 2. K2（U2）：`Reconcile` 遇到無效或倒退的權威狀態時無聲 return

位置：`LocalPlayerPrediction.cpp:232-242`。

為什麼產品走不到：

- 欄位的檢查（id、epoch、life、垂直速度、生死狀態、待處理命令數、位置、yaw／pitch）都是 `ClientConnection.cpp` 驗證過的子集。
- 倒退的檢查：
  - `ClientSimulation::Observe` 先濾掉 tick 不比上一次新的（`ClientSimulation.cpp:31`），`lastSnapshotTick_` 和預測一起 `Reset`；
  - Client 驗證每一份套用的 snapshot 對上一份都不倒退（同 epoch 內 `lastResolvedCommand` 也不倒退），而套用的 snapshot 一定含自己（不含就 throw「Local player left」），所以逐份遞推成立。
- 換代或換玩家時 `ClientSimulationLoop::Run` 會 `simulation_.Reset()`。

但 `Reconcile` 是公開方法，有 8 個測試檔直接呼叫（`PredictionTests.cpp` 等）。直接改成 assert，餵無效狀態的測試就會中止。

選項：

- (a) 直接在 `Reconcile` 裡 assert。故意餵無效狀態的測試要改成期望 assert（§14.4 的判定變更）。
- (b) `Reconcile` 改成回傳 `[[nodiscard]] bool`（是否採用），產品的呼叫端 `ClientSimulation::Observe` 對它 assert。類別的契約維持「無效或較舊的狀態被忽略」，測試可以檢查回傳值；產品路徑的保證放在產品邊界。和 K1 的 `Push` 同一種寫法。
- (c) 保留現狀，只記錄。

**建議 (b)**：不必重寫條件（不重複），測試不受影響，產品路徑仍有 assert。

## 3. K3（U3）：`SendInput` 被無聲丟掉的窗口

位置：`ClientConnection.cpp:943-972`（兩個多載，產品只用帶 generation 的那個，從模擬執行緒呼叫，`ClientSimulationRole.cpp:197`）。共有 7 個 return：

| 代號 | 條件 | 走不走得到 | 理由 |
|---|---|---|---|
| r1 | generation 不是現在的 | 走得到（設計） | 模擬執行緒的窗口來自上一代；介面說明寫明這種情況丟掉 |
| r2 | 不在 Playing 或 Connecting | 走不到 | 模擬端有自己的 snapshot 才會產生窗口，那一代已經進過 Playing；同一代內 phase 不會倒回（失敗與離開都會換代） |
| r3 | 玩家不是這一代的玩家 | 走不到 | 玩家在 Welcome 時設定，模擬端讀的是同一代的玩家 |
| r4 | `ValidInput` 不成立 | 走不到 | 窗口由預測產生，命令都有效 |
| r5 | epoch 或 life 和最新 snapshot 不同 | 走得到（設計） | 模擬執行緒用較舊的 snapshot 產生窗口，網路執行緒已套用新的 epoch 或 life（重生）；註解寫明 |
| r6 | 去掉已確認的命令後是空的 | 走得到，不是丟棄 | 全部已確認，沒有東西要送 |
| r7 | 窗口改了或少了未確認的命令 | 走不到 | 預測守住 network-architecture §7「一個命令身分只有一種內容」：`Resume` 在 tip 之下不重新產生序號（`LocalPlayerPrediction.cpp:111-114`）；重新播種只發生在全部確認後或新 epoch，新 epoch 會清掉已送出的窗口 |

選項：

- r2、r3、r4、r7：(a) assert；(b) 保留。
- r1、r5：(a) 在 worker statistics 行尾追加一個計數鍵（例如 `stale_input_windows`，日誌契約 R1 允許只在行尾追加）；(b) 保留不計數。
- r6：保留（不是丟棄）。

**建議**：r2、r3、r4、r7 用 assert，r7 等於替 §7 的 Design Invariant 多加一道守衛；r1、r5 計數，和 D180 的 C3「處理封包不留尾巴」一致。風險：worker probe 或測試若故意送誤用的窗口，會在實作時中止，那時再逐個交你看。

## 4. K4：已計數、註解寫「應為 0」的 4 個分支

| 代號 | 位置 | 為什麼走不到 | 建議 |
|---|---|---|---|
| S1 | `MatchRuntimeHost.cpp:206`（`StagedOverWindow`） | 准入把每個命令限制在 (cursor, cursor + MaxFutureCommands]，暫存只留 cursor 之後的 | assert |
| S2 | `MatchRuntimeHost.cpp:421-438`（`HandoffRejected`） | `CanSubmitInput` 只會因為玩家不在、輸入無效、身分不同、超出窗口或衝突而失敗。玩家離開會刪暫存（`:649`）；身分只在 `match_.Tick` 內改變，同一次 Tick 的輪替會刪掉舊身分的暫存（`:457-484`，和 A1 同一個守衛）；超出窗口與衝突在准入時已排除，cursor 只會前進 | assert；分支裡寫 trace 的那段會變成死碼，一起刪 |
| S3 | `server.go:1172-1177`（`ErrInput`） | `link.input` 的檢查和 `DecodeInput` 相同 | (g2) `invariantFailed` |
| S4 | `action_delivery.go:142-146`（`ErrActions`） | `link.actions` 的檢查（玩家不為 0、筆數上限）和 `DecodeActions` 相同 | (g2) `invariantFailed` |

影響：

- S1、S2 在 Match 端改成 assert，壞掉時 Match 程序中止。A1～A3 已經是同一個政策。
- 它們的日誌鍵（`staged_over_window`、`handoff_rejected`、`drop_malformed_*`）照日誌契約 R1 不能刪，保留並一直為 0（`drop_malformed_*` 仍會被真正解析不了的封包計數）。

## 5. 決定（D183）

全部照建議：K1 assert；K2 選 (b)；K3 的 r2、r3、r4、r7 assert，r1、r5 計數，r6 保留；K4 的 S1、S2 assert，S3、S4 用 `invariantFailed`。

## 6. 估時

追守衛約 1.5 h（本文件）。實作、突變、L1 約 3～4 h。合計在 D181 給的 4～6 h 內。
