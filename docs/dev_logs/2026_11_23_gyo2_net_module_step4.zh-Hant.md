# 2026-11-23 網路 Module 第 4 步：模擬又回滾的 Client（K3）（v8-arch-gyo2-net-module PR-2）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-pr2`（從 gyo2 ce64e177 開，主會話先放了三個 docs commit）。
- **事前宣告與核准**：
  - [`net-module-pr2-gates.zh-Hant.md`](../architecture/target/v2/net-module-pr2-gates.zh-Hant.md) 閘口 1 與第 3 節（使用者 2026-11-23「照建議，全部核准」）。
  - [`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md) D-5（K3 的寫法）、D-7（確認點）、第 8 節 G-10、第 20 節 U4、Q8 (b)。
- **實作者**：Opus／high。

件數の突き合わせ：一致（G-10：入口的進入 8＝伺服器生成並送出的 8、退出 1＝伺服器刪掉的 1；值訊息 收到 301＝處理 301＋待處理 0；讀不了、超過深度都是 0）

## 1. 回滾 Module（閘口 1 的 A～G）

- **`RollbackInbox`**（`Inbox.hpp`）：
  - `Entered(entity, tick)`、`Exited(entity, tick)`：每個實體按 tick 記「進入／退出」。`AliveAt(entity, tick)` 回答呼叫者說的「C 時存不存在」（沒說過就沒有答案），`AliveWith(alive, tick)` 把它套到記下的存活集合上。
  - `ResourceValue<T>(tick, value)`：另一組欄（`ResourceValues<T>()`、`ResourceValuesOf(key)`），值記在 `kResourceEntity`（0）底下，規則和元件的欄相同。
  - `Prune` 也修剪 Resource 的值與進入、退出，每個實體留 C 當時或之前最後一筆。
  - `Entered`、`Exited` 不再回 NotImplemented（改成 `void`，和 `Value` 一樣）；`PartialValue`、`NotSent`、`FromEvent` 照舊。
- **官方策略**（`StateRollbackStrategy.cpp`）：
  - 判斷：C 的存活集合＝記下的，加上呼叫者明說的（進入 ≤ C、沒在 C 以前退出的加進去，C 以前退出的拿掉）。和記下的不同就回滾（例：只有退出、沒有值不同）。值的比對用這個集合。
  - 還原：`Alive` 用這個集合（不在的用同號生成，退出的刪掉），值照入口放回；歷史在 C 的存活集合也換成它（`RollbackHistory::SetAliveAt`）。
  - 每一欄的權威值：元件用 `ValuesOf`，Resource 用 `ResourceValuesOf`（`ColumnHistoryBase::IsResource`）。
- **`ResourceHistory`**（`History.hpp`）：`Differs`、`AddTo`、`Adopt` 照元件的規則接權威值（之前一律不比、不放）。
- **文件**：[`rollback.zh-Hant.md`](../architecture/target/v2/rollback.zh-Hant.md) 第 2 節、R-8、限界照 A～G 改，標「實作，合併前核准定稿」。E（「權威刪掉整個實體沒有入口」）拿掉；B 的緩的面寫進限界。

## 2. 網路（K3）

- `NT-E5` 拿掉。Client 角色、選了回滾策略的 World 是 K3：準備 World 時照 `RollbackRegistry` 記下哪些複製的元件、Resource 會回滾，以及策略的 `DefaultInputLength()`（官方策略＝深度）。
- 結構更新：
  - 伺服器生成 → `Entered`（不在 `First` 生成）；
  - 刪除 → `Exited`（不在 `First` 刪除）；
  - 加減會回滾的元件 → `Value`／`Removed`；
  - 不會回滾的元件 → 直接寫，實體還不在時先留著，出現後寫進去。
- 值：會回滾的 → `Value`（比 C 舊的照寫，MC-1；比 C 舊超過深度的丟掉、計 `BeyondDepth`）；不會回滾的 → 直接寫（同 K1）。
- Resource：會回滾而且值就是 Resource 本身的 → `ResourceValue`；其他照 `apply`。
- 確認點：`latestConfirmed`（D-7 的兩個條件）變了就寫 `ConfirmedThrough`。
- `ReplicaDrop` 加 `BeyondDepth`（第 7 個理由）。
- 文件：[`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md) 新的第 13 節（K3）、NT-1；第 7 節的 `NT-E5` 說明拿掉。

## 3. 測試

- **回滾**（`rollback strategy: R-8 A:`／`R-8 F:`）：
  - 明說進入的實體在 C 放回（同號生成、照入口的值），之後的回滾留著；
  - C 之後才進入的在 C 不在；
  - 只有退出也回滾、在 C 刪掉、之後不回來；
  - 修剪留 C 當時或之前最後一筆；
  - Resource 的權威值：不同就回滾並放回、一致不回滾、C 之前的值在 C 算、修剪留 C 的那筆。
  - `rollback checks: the entries not made…`：拿掉 `Entered`、`Exited`（它們實作了；原本那兩個判定改由上面的案例守）。
- **網路**（`net.k3`，`RollbackClientTests.cpp`）：確定性的 2D 樣本。
  - `Body`、`Speed` 是會回滾的判定元件，`Push` 是判定輸入，`Wave` 是會回滾的判定 Resource（只有伺服器的步寫），`Tint` 是不判定、不回滾的複製元件。
  - 伺服器在第 30～90 步每 10 步生成一個箱子、第 100 步刪掉第一個（`OnServer`，兩端同登記）。
  - 玩家實體兩端都由準備函式放（固定編號，兩邊配發器範圍之外）。
  - Link 單程固定延遲 3 步。
  - **G-10**（判定照宣告，不放寬）：300 幀，最後 80 步 Client 送預設輸入。結束時取 Client 的第 P 步與它的 `Digest`；伺服器之後一幀一步跑到 P，比它記下的第 P 步的 `Digest`。另檢查箱子不被回滾刪掉、箱子的 `Tint` 和伺服器相同、`Wave` 相同、件數。
  - **伺服器一幀三步**：伺服器每三個 Client 幀跑一幀、一幀三步（tick 的速率相同，伺服器的幀率低）。確認點跳著前進，G-10 照樣成立。
  - **權威刪掉整個實體**：伺服器刪掉的箱子在 Client 上也沒了。
  - `replication: a client with a rollback strategy (K3) is built`：取代原本「K3 被 NT-E5 拒絕」的案例（範例遊戲在步裡寫 `DoorOpened`、在步外讀，回滾策略報 RB-E8，所以這一案用一個只複製 `Position` 的登記）。

## 4. 數字

- 本機（macOS x86_64，Intel i9-9980HK，`core` preset）：全部 ctest 197 個通過。
- **G-10**（判定成立）：Client 的第 P 步＝318，`Digest` 等於 Match 的第 318 步。
  - 入口：值 3 742、拿掉 0、進入 8、退出 1、確認點 310。
  - 值訊息 收到 301＝處理 301＋待處理 0；結構套用 9；丟棄全部 0。
  - 箱子 伺服器 6＝Client 6（第 100 步刪掉一個）；`Tint` 全部相同；`Wave` 相同。
- **伺服器一幀三步**（判定成立）：P＝349，`Digest` 相等；確認點跳著前進的幀 101 個。
  - 入口：值 1 362、進入 8、退出 1、確認點 336。
  - 值訊息 收到 103＝處理 103；箱子 6＝6。
- **權威刪掉整個實體**：伺服器刪掉的箱子在 Client 上也沒了。
- 其他判定照舊：G-5 觀測 164、不一致 0；G-8 44 078 bytes；G-9 觀測 42、不一致 0、連線識別不符 5。

## 5. 突變

- **新加 13 個**（都寫了 `guards`）：
  - 回滾 6：`rb-entered-not-restored`、`rb-lifecycle-not-a-difference`、`rb-entered-at-any-tick`、`rb-lifecycle-prune-drops-c`、`rb-resource-authority-ignored`、`rb-resource-not-put-back`。
  - 網路 7：`nt-k3-entered-missing`、`nt-k3-exit-missing`、`nt-k3-not-confirmed`、`nt-k3-as-k1`、`nt-k3-waiting-not-written`、`nt-k3-values-direct`、`nt-k3-resource-direct`。
- **退役 1 個**：`nt-k3-as-k2`（守「K3 被 NT-E5 拒絕」，NT-E5 拿掉了；§14.5 的 7，由 `nt-k3-as-k1` 接手）。
- **find 跟著改**（守的性質不變）：`rb-alive-not-restored`、`rb-authority-not-laid-over`、`rb-history-not-adopted`（`*alive` 改成明說之後的 `alive`）、`rb-differs-comparison-ignored`（`ResourceHistory` 也有 `Same`，find 多帶前一行讓它唯一）。
- **結果**：回滾全清單 88／88；網路全清單第一次 78／80。
  - `nt-k3-as-k1`、`nt-k3-resource-direct` 是 wrong_failure：
    - 前者沒有確認點、不會回滾，箱子沒有被刪，失敗的是 `Digest`；
    - 後者直接寫 `Wave` 也會被之後的記錄收進歷史，一幀一步時收斂，一幀三步時 `Digest` 不同。
    - expect 改成實際捕到它的那一條，重跑 killed。
  - `nt-k3-values-direct` 的 expect 原本寫得太寬（`G-10`），改成 `Digest` 那一條（§14.2：要認得出是它引起的），重跑 killed。
  - 合計：網路 80／80。
- **沒加的**：晚到的值超過深度（`BeyondDepth`）、Resource 的入口修剪：樣本裡沒有亂序與晚到（Link 只有固定延遲），細節，等觀測到再加（§14.5）。

## 6. 自己判的（揭露）

- **`Entered`、`Exited` 改成 `void`**：和 `Value`、`Removed` 一樣；它們實作了，沒有要回報的失敗。
- **明說的存活和記下的不同，本身就是回滾的理由**：沒有這條，只有退出（沒有值不同）時不會回滾，退出的實體留著（閘口 1 的 3：退出的在 C 被刪）。
- **還原時把歷史在 C 的存活集合換成明說之後的**：之後在同一個 C 或更晚比對時，記下的就是權威的。
- **K3 不在 `First` 生成實體**：網路生成的實體等確認點的回滾放回，所以在 Client 上出現得比 K1 晚約一個來回，之後由預測補到現在。另一個做法是在 `First` 先生成，但回滾到 C（< 生成的 tick）時會照閘口 1 的 A 被刪掉，不會回滾的元件也跟著沒了。
- **不會回滾的元件等實體出現再寫**（`waitingDirect`）。
- **深度用 `DefaultInputLength()`**：回滾的契約沒有「深度」的掛點，官方策略的 `DefaultInputLength()` 就是深度（R-1 的掛點，註解寫明）；作者自己的策略回 0 時，網路不丟晚到的值。
- **G-10 的樣本自己寫**（`RollbackClientTests.cpp`），沒有用範例遊戲：範例遊戲在步裡寫訊息、在步外讀（RB-E8），而且邏輯只在伺服器跑，不是預測的樣本。

## 7. 範圍的限界

- 本機 macOS x86_64、`core` preset。
- G-10 只有一個 Client（宣告：遠端玩家的輸入本線不送）。
- 伺服器換 World 時，入口的確認點不會往回走；K3 的 Client 要跟著換 World。

## 8. 證據

主 checkout `build/evidence/gyo2-net-module/step4/`（git 不追蹤）：`ctest-all.log`、`net-messages.txt`、`mutations-rollback.log`、`mutations-net.log`。

## 之後要接的

- 第 5 步：可見性（`NotSent`）與頻寬（閘口 2）。
- 合併前核准：rollback.zh-Hant.md 的 A～G、net.zh-Hant.md 第 13 節的條文定稿。
