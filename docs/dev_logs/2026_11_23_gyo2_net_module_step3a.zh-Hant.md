# 2026-11-23 網路 Module 第 3 步前半（3a）：輸入、重連、元件的輸入紀錄（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（第 2 步 8afde689 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md)（修訂 2；使用者 2026-11-22 動工前核准）D-11、C-IN1～3、D-8 的重連、第 5 節的回滾與權威兩列；判定 G-6、G-9。
- **實作者**：Opus／high。
- **不在 3a**：對時（D-9，要改時間 Module 的文字，等使用者定形狀）；world_time 沒有碰。
- **主會話追加**：Client 寫複製元件時值相同就不寫（CCR-8）。

## 做了什麼

- **回滾 Module：元件的輸入紀錄**（宣告第 5 節，使用者已核准）：
  - `ComponentInputHistory<T>`（`History.hpp`）：每個 tick 記整欄（每個實體一份），重跑每一步前逐格寫回，`Keep`／`PutBack` 也逐格。形狀照 lightyear 的 `InputBuffer`（每個實體按 tick 的輸入）。
  - `InputRecord` 多一欄 `kind`（Resource 照舊為預設）；`RollbackEntry::Input<T>(length, DataKind)`；`RollbackRegistry` 的雜湊含 `kind`。R-1 的掛點不改；只在選了回滾策略的 World 建（R-4，照原本的路徑）。
- **權威 Module：元件輸入的代為登記**：`kComponentOps` 加 `RecordComponentInput<T>`，`authority.Component<T>(name, {.input = true})` 在選了回滾策略的 World 代為登記元件的輸入紀錄；AU-E6 的「有紀錄的輸入不查」照紀錄的 `kind` 找元件或 Resource。AU-E7 不變（本來就查元件與 Resource）。
- **網路的輸入**（D-11）：
  - `NetEntry(s).Input<T>(name, InputOptions<T>)`：編碼、冗餘份數（預設 5）、窗（預設 64）、`held`（沿用上一格時清掉單次觸發的欄位）。協議雜湊含輸入。
  - Peer 實體上的 `PeerInput<T>`（按 tick 的緩衝、計數）與 `PeerControls`（控制的實體，產品寫）。
  - 入口 `ApplyInput:<名字>`（`FixedUpdate`／`First`）：兩端同名、同存取、同掛載；有權威策略時經 `authority.Ingress`，K1 直接 `AddSystem`。沒有那一格就沿用上一格並計數「缺」。
  - Client：`LocalInput<T>(world)->Set(tick, value, view)`；`SendInputs` 每 tick 一則（`end_tick`＋最後 N 格，Unreliable）；每格帶 `ViewTime{tick, overstep}`。
  - Match：`ReceiveInputs` 替每個 Peer 備好緩衝，收窗 `(已消耗, tick + L]`，已消耗、窗外、重複各自計數，每個（Peer, tick）只收一次。
- **重連**（D-8）：握手的請求帶 salt（每次握手 Client 抽一個）；同一個 endpoint 帶別的 salt 是新的握手，Match 先結束舊連線再配新的；斷掉的 session 的 Disconnect 先送完再開新的握手；已斷的 session 收到的 datagram 也計數（原本不計數：D-12 的漏洞）。
- **CCR-8**：`Replicate<T>` 的寫入路徑先用編碼函式把現值編碼、和收到的位元組比，相同就不 `Modify`（`ApplyOutcome::Unchanged`，`NetReplicaState::unchangedNotWritten` 計數）。條文以 Kernel 卡登記的為準（`docs/cross-component-rules.md` 還沒有這條：**待 Kernel 卡登記**）。
- **測試**：
  - `tests/common/net/InputTests.cpp`（`net.input`）：G-6；入口兩端指紋相同、經 `Ingress`。
  - `tests/common/net/ReconnectTests.cpp`（`net.reconnect`）：G-9。
  - `SessionTests.cpp`：同一個 endpoint 的新握手是新連線、重送的回應不是。
  - `ReplicationTests.cpp`：CCR-8（確認大多到不了，伺服器一再重送同一個值，`Changed<T>` 不再為真；玩的時候照常為真）。
  - `tests/common/rollback/StrategyTests.cpp`：元件輸入的 R-2 等價（重跑時每個實體的輸入照紀錄寫回）、入口在回滾之前跑時保留這一幀的輸入。
  - `tests/common/authority/`：元件的判定輸入代為登記紀錄（只在有回滾策略的 World）；AU-E6 不查有紀錄的元件輸入、查沒有紀錄的。
- **文件**：[`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md) 新增第 9 節、補第 3、7、8 節與 NT-1、NT-3、NT-10；[`rollback.zh-Hant.md`](../architecture/target/v2/rollback.zh-Hant.md) 的 `Input<T>` 列與 R-6；[`authority.zh-Hant.md`](../architecture/target/v2/authority.zh-Hant.md) 第 4 節限界那一條與 AU-E6 列。三份的 Design Invariants 改動都標「實作，合併前核准定稿」。

## 和宣告不同的地方

- **握手多一個 salt 欄**（宣告沒寫）：沒有它，同一個 endpoint 的重連（舊連線還在 Match）和「接受掉了、重送的回應」分不出來，Match 會把舊連線還給新的 Client。請求的格式變了（`kNetProtocolVersion` 沒升：還沒有發行的版本）。
- **R-6 寫成「七種登記」**：元件的輸入紀錄算第七種，形式上是 `InputRecord` 多一欄 `kind`。
- **入口的緩衝從連上的第一幀起就有**：第一次跑 G-6 時件數對不上（入口跑的步 ≠ 消耗＋缺）。原因：Match 的 Peer 在收到第一則輸入訊息之前沒有 `PeerInput<T>`，入口查不到它就不跑、不計數。這是實作的漏洞（D-12：每個步都要算進消耗或缺），改成 `ReceiveInputs` 每幀替每個 Peer 備好緩衝。判定沒有改。
- **tick 對齊不在 3a**：Client 的輸入以 Match 的 tick 為鍵；測試直接讀 Match 的 tick 加上提前量。Client 自己的入口用 Client 的 tick 讀緩衝，和 Match 的鍵對不上；對時（3b）之後才一致。寫明是暫時的。

## 自己判的細節（L17 的範圍，揭露）

- **名字**：`InputOptions`、`PeerInput`、`PeerControls`、`ViewTime`、`InputCounts`、`ApplyInput:<名字>`、`ReceiveInputs`、`SendInputs`、`LocalInput`、`ComponentInputHistory`、`RecordComponentInput`、`ApplyOutcome::Unchanged`。
- **單次觸發的形狀**：作者把觸發放在輸入元件的欄位裡；網路每個（Peer, tick）只收一次，所以冗餘的幾格裡任一格帶到就算那一格、只觸發一次；那一格始終沒到時沿用上一格，`InputOptions::held` 清掉觸發。系譜：Unity 的 `InputEvent`、lightyear 的 `ActionState` 的 just pressed。
- **`ViewTime` 的小數一個 byte**：照 lightyear 的 `Overstep` 上 wire 的寫法（`crates/core/core/src/time.rs:25-33、90-104`：內部 U0F16、上 wire U0F8）。
- **冗餘 5**：lightyear `crates/inputs/inputs/src/config.rs:16-19、50` 的 `packet_redundancy`。**窗 64**：回滾 `InputHistoryLength::Long`，lightyear 收 ±64。
- **緩衝的修剪**：Client 留最新一格往前 2 × 窗；Match 收件時丟掉已消耗的。

## 驗證

- 本機（macOS x86_64，`core` preset）：
  - `gyo_net_tests` 57 案全過；
  - `ctest -R "net\.|os\.|cmake\.|naming|authority|entity_ids|orchestration|world_time|rollback|scenario"` 78 個全過；
  - 全部的 ctest 180 個全過；
  - 網路測試兩次執行，印出的數字逐項相同（RTT 的那行除外）。
- **G-6**（事前宣告第 8 節；種子 `0x5EED0006`，兩個方向掉包 20%、重複 10%、延後 25%（1～3 步）；400 個 tick，提前量 3）：
  - 比的 tick 400：等於 Client 送的 361，沿用上一格 39，都不是的 0。
  - 單次觸發：送 40、Match 看到 37（3 個在沒到的格裡）、在沒送的 tick 觸發的 0。
  - 件數：收到的格 1 617＝收下 361＋重複 186＋已消耗 1 069＋窗外 1；收下 361＝消耗 361＋還在前面的 0；入口跑的步 415＝消耗 361＋缺 54（缺 54＝比的 tick 裡沿用的 39＋沒給輸入的 tick 15）；輸入訊息 送 410＝讀 328＋隨掉包遺失 82（Link 排空後）。
  - 第一次跑時「步＝消耗＋缺」與「訊息 送＝讀＋遺失」對不上：前者是上面的實作漏洞；後者是測試沒有排空 Link（最後一則還在路上），加了「伺服器單獨跑到路上沒有東西」。都只改一次。
- **G-9**（種子 `0x5EED0009`，掉包 10%、重複 5%、延後 50%（1～6 步））：Client `Close` 後產品馬上換 World：
  - Match 只剩新連線的 Peer，連線識別和舊的不同；舊連線結束的理由 `ClosedByRemote`。
  - 新 World 上 G-5 的方法：觀測 47 次、不相等 0。
  - 斷線期間 Match 刪掉的實體 10 個，Client 上 0 個。
  - 舊連線的 datagram：新連線連上之前收到的 3 個計 `UnknownSource`；連上之後連線識別不符的 0 個（這次舊的都在新連線連上前到了）。判定照宣告看「被丟、計數」，兩者合計 3。
- **G-5**、G-2、G-8 的數字和第 2 步相同。
- **CCR-8**：確認的掉包 80%。只是重送的期間 `Changed<Position>` 增加 0 次；沒寫的相同值 766 次（觀測期間 180 次）；玩的時候 `Changed<Position>` 6 497 次。
- **回滾**：元件輸入的 R-2 等價（每個實體的 Health 與 World 摘要和直跑的相同）、入口在回滾之前跑時保留這一幀的輸入。
- **突變**（各清單照 §14.5，都寫了 `guards`）：
  - net 加 9 個：`nt-input-not-held`、`nt-held-fires-again`、`nt-input-window-unchecked`、`nt-input-late-stored`、`nt-input-overwritten`、`nt-input-missing-uncounted`、`nt-input-entry-not-ingress`、`nt-ccr8-write-same`、`nt-reconnect-reuses-connection`。
  - rollback 加 3 個：`rb-component-input-not-written-back`、`rb-component-input-not-put-back`、`rb-component-input-as-resource`。
  - authority 加 2 個：`au-component-input-no-history`、`au-component-input-kept-as-resource`。
  - 第一次跑：`nt-input-overwritten` 是 wrong_failure。覆蓋時被算成「收下」，失敗的是「收下＝消耗＋還在前面的」，不是我原本指的式子。expect 改成它。`nt-reconnect-reuses-connection` survived：G-9 裡 `Close` 的 Disconnect 總是先到 Match，salt 的比較從沒用上。改成 session 層的一案（舊連線還在時同一個 endpoint 的新握手），killed。
  - 沒加的：冗餘份數（G-6 不判「缺」的多寡，改小也照樣通過）；已斷的 session 收到 datagram 時的計數（舊 session 在測試看到之前就被換掉了，沒有測試看得到）。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step3a/`，git 不追蹤）：`net-all-run1.log`、`net-all-run2.log`（G-6、G-9、CCR-8 等的全部數字）、`ctest-selected.log`、`ctest-all.log`、`mutations-new.log`（新的突變與重跑）、`mutations-all.log`（net 全清單、rollback 與 authority 的輸入相關突變）。

## 突變的結果

- net 全清單 **46／46 killed**。
- rollback 的輸入相關 10 個、authority 的輸入相關 6 個：第一次各有 1 個 stale（find 對不上），其餘 killed。
  - `rb-input-ring-off-by-one`：find 那一行 `ComponentInputHistory` 也有，變成不唯一。find 往前加兩行 `InputHistory` 自己的程式，守的性質不變。
  - `au-z6-input-history-ignored`：find 是 3a 改掉的 `inputsKept` 那一行。find 跟著新的寫法改，replace 照舊讓迴圈不跑。
  - 兩個改寫後重跑都 killed（README「改名或訊息變了時改寫 find 與 expect」的範圍，沒有緩的面）。合計 rollback 10／10、authority 6／6。

## 之後要接的

- 3b：對時（時間 Module 的入口與 WT 文字，使用者定形狀之後）；之後 Client 自己的入口與送件都以對時後的 tick 為鍵。
- CCR-8 的條文：Kernel 卡登記後，這份 dev_log 的代號照它。
