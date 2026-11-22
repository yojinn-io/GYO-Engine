# 2026-11-23 網路 Module：PR-1 合併前設計審查的修正（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（Q8 (b) 99b48b10 之後）。
- **起因**：PR-1 合併前的設計審查（Fable／high，唯讀）：擋合併 2、應改 4、小事 8、應交使用者 5。原文在主 checkout `build/evidence/gyo2-net-module/design-review-pr1-fable.md`。主會話指定修 B1、B2、S1～S4、小事 1～7 與 U1（G-9 的判定讀法），U2～U5 交使用者。
- **做法**：AGENTS §14.3。每個缺陷先寫一個會失敗的測試（Red），再修到 Green，測試留下當回歸。
- **實作者**：Opus／high。

件數の突き合わせ：一致（G-5、G-6、G-9 的件數式照舊成立；新的計數讓伺服器端讀 Client 訊息的出口也進了件數）

## 1. Red 的紀錄

修之前跑 `gyo_net_tests`：77 案中 12 案失敗（新的 11 案＋改過的 G-9），`gyo_state_machine_tests` 38 案中 1 案失敗（小事 5）。輸出在證據的 `red-net.log`、`red-state-machine.log`。

- **S1 的測試第一次沒有 Red**：兩個伺服器 World 用了同樣的實體 id，Client 還留著第一個 World 的實體，值寫得進去；而且範例遊戲在 tick 1 生成門，tick 1 的值要的是結構 tick 1。改成每個 World 用自己的 id、伺服器只登記 `MoveAll`（複製的登記和 `RegisterGame` 相同，協議雜湊一樣）之後 Red。
- **B2 的測試**：第一次用的規則（`GameRules{1000,false}`）每 30 tick 有 `ToggleDoor` 也寫 `DoorOpened`，修好後多出 2 則。規則改成到 tick 1 停（沒有別的 System 寫這個訊息）。改過的測試在修之前的程式上的 Red，用突變 `nt-outbox-cleared-without-tick`（把修正拿掉）證明。
- **G-9 的測試改了之後**：修之前除了連線識別那一項，主要的判定（確認後相等、刪掉的實體不在）也失敗。原因是 B1：重連的新 World 被第一則訊息的 epoch 蓋掉。B1 修好後全部通過。

## 2. 逐條的處理

| 條 | 處理 | 測試（Red → Green） |
|---|---|---|
| **B1** resync／epoch | epoch＝`NetService` 數到的第幾個 Client World 減一，準備 World 時放進 `NetReplicaState`；resync 不再加一。`Current()` 只在伺服器換了 World（已知世代之後的新世代）時採用訊息的 epoch；新 Client World 的第一則訊息也照常比 epoch（以前在世代分支裡直接通過） | `replication: a replaced client World resyncs…`：換第 2、第 3 個 Client World，各跑 30 幀後再 10 幀，`confirmationsReceived` 有增加、`StaleEpoch` > 0、狀態相等 |
| **B2** 伺服器訊息遺失 | `ReplicateOut` 只在有 Peer 要送這個 tick 時才取出 `NetOutbox`；沒跑固定步的那一幀不取，留到下一個 tick | `replication: a server message written in a frame without a fixed step…`：伺服器一幀半步，每幀寫一則，共 40 則；`serverMessagesSent`＝40、Client 收到 40 |
| **S1** 結構 tick 的 0 | `lastStructureTick`、`appliedStructureTick`、要的結構 tick 改成 `optional`；wire 上是「tick＋1，0＝沒有」，欄位大小不變 | `replication: a value part needing the structure of tick 0 waits for it`：伺服器換 World、第一幀（tick 0）的結構更新掉了，tick 1 的值先到；`MissingTarget` 不增加 |
| **S2 (a)** 舊握手結束新連線 | salt 改成遞增：IO 層單調時鐘的奈秒，沒動時取上一個加一。伺服器收到比現有連線小的 salt 時丟掉、計 `Unexpected`，不結束現有連線 | `handshake: a late response of an older handshake…` |
| **S2 (b)** Client 採用舊的接受 | `ConnectAccepted` 帶回 salt，Client 只採用帶自己 salt 的；網路協議版本 1 → 2 | `handshake: a connecting client does not take the accept of an older handshake` |
| **S3** 帳本先於送出 | 結構更新的帳本改動先記下，連線收下才套用；被拒時帳本不動、下一個 tick 再做一次，計 `PeerLedger::reliableRejected`。伺服器訊息被拒時也計（那一則不重送） | `replication: a reliable send the connection refuses…`：伺服器的可靠佇列上限 1、有損的 Link；排空後 Client 和伺服器相等，且確實有被拒的 |
| **S4** 沒有計數的出口 | 伺服器端在 `PeerLedger::drops`（`ReplicaDrop` 的同一組理由）計：確認、resync 讀不了或種類不對（`Malformed`）、epoch 不對（`StaleEpoch`）；輸入訊息讀不了（`Malformed`，`fill` 改成回報讀不讀得了）。Client 的結構更新遇到不在的實體計 `MissingTarget` | `replication: what the server drops from a client is counted on its Peer`、`replication: a structure update for an entity the client does not have is counted` |
| **小事 1** 輸入窗沒有下界 | 還沒消耗過時，被伺服器的 tick 超過的格修剪掉，計 `InputCounts::expired`。件數：收下＝消耗＋過期＋還在前面 | `input: without a controlled entity…`：格數不超過窗、件數式成立 |
| **小事 2** 世代欄 | Client 的輸入填它知道的最新世代（還沒收到複製時用接受帶的世代）；伺服器的確認與輸入都比世代，不同的丟掉、計 `StaleGeneration` | 同 S4 的第一個測試 |
| **小事 3** Link 的錯誤 | 收件只在 `DatagramTooLarge` 時繼續，其他錯誤停掉這一幀的收件 | `module: a lasting Link failure stops the frame's receiving…` |
| **小事 4** `reliableWindow` | 斷言要求 2 的次方 | `connection: the reliable window is a power of two` |
| **小事 5** `AdoptByRequest` 的 A→A | 值等於現值時一律不寫（本機另有待處理的要求也一樣） | `behaviour: adopting the current value writes nothing even with another request pending` |
| **小事 6** 文件與 NT-7 的守衛 | net 第 8 節「只收元件」改掉；`tests/common/net/CMakeLists.txt` 加 `gyo_expect_links(gyo_net …)`（連結集剛好是五個核心庫與登記的四個 Module），照 entity_ids 的寫法 | 突變 `nt-links-more`（多連狀態機就配置失敗） |
| **小事 7** tick 的回繞 | net 第 8 節的限界記一行（64 Hz 約 2.1 年） | — |
| **U1** G-9 的判定 | 判定的文字沒動。測試改成讓宣告的那條路真的被觀測到：斷線前讓伺服器的 datagram 延後 40 步、送 5 幀，再恢復原本的減損、斷線重連，舊連線的 datagram 在新連線建立之後才到；檢查「連線識別不符」那一項 > 0（以前看合計） | `reconnect: G-9 …` |

## 3. 判定的變更（AGENTS §14.4）

- **G-9**（受入判定）：舊的檢查是「連線識別不符＋連上之前的 `UnknownSource` > 0」，新的是「連線識別不符 > 0」。
  1. 守的性質：舊連線還在飛的 datagram 因連線識別不符被丟、計數（宣告第 8 節 G-9 的文字）。
  2. 緩和面：沒有。
  3. 收緊面：只有 `UnknownSource`、沒有連線識別不符時，以前通過，現在失敗。
  4. 不適用（沒有緩和面）。
  5. 根據：審查 U1、宣告的文字；這次的觀測值見第 4 節。
  - 情境另加了「斷線前 5 幀的延後」，是為了讓宣告的那條路出現；判定的其他項目（G-5 的方法、刪掉的實體）沒有變。
- **G-3** 的「版本不同」案例：常數從寫死的 `2` 改成 `kNetProtocolVersion + 1`（版本升到 2 之後，2 不再是「不同」）。守的性質相同，不是緩和。
- **S3、B2 的測試**：都是新的。

## 4. 數字

- **G-9**（改過的測試）：觀測 46、不一致 0；斷線期間刪掉的 10 個不在 Client；舊連線的 datagram 因連線識別不符被丟 5、連上之前的 `UnknownSource` 0。
- **G-5**：觀測 164 個 tick（391 個中）、不一致 0；值訊息 送 565＝收到 451＋隨封包遺失 114；確認 送 451＝收 451；結構 送 250＝套用 250。
- **G-6**：數字和 Q8 時相同（比 404、等於送的 383、沿用 21、錯 0；收下 383＝消耗 383＋前面 0）。
- **G-8**：判定成立；合計 44 078 bytes，和 Q8 時相同（要的結構 tick 的寫法變了，大小沒變）。
- **S3**：可靠送件被拒 66 次，排空後相等。
- **小事 1**：收下 394＝消耗 0＋過期 391＋還在前面 3。

## 5. 突變

- **新加 17 個**（都寫了 `guards`；各自只捕自己那一個修正，`tests/common/mutation/README.md` 的 1、3、9）：
  - 網路 16：`nt-epoch-taken-from-message`、`nt-epoch-not-per-world`、`nt-first-message-epoch-unchecked`、`nt-outbox-cleared-without-tick`、`nt-structure-none-as-applied`、`nt-ledger-before-send`、`nt-older-salt-ends-connection`、`nt-accept-salt-unchecked`、`nt-peer-drop-uncounted`、`nt-structure-missing-uncounted`、`nt-confirm-generation-unchecked`、`nt-input-generation-unchecked`、`nt-input-not-expired`、`nt-link-failure-retried`、`nt-window-not-power-of-two`、`nt-links-more`（NT-7 的連結守衛，配置失敗）。
  - 狀態機 1：`sm-adopt-current-with-pending`。
  - 一批加 17 個，超過「改的 Design Invariants 條文數 × 5」的話，合併前的核准時說明：每個都對應審查的一條缺陷，各自捕一個別的突變捕不到的錯。
- **find／expect 跟著改**：
  - `nt-value-before-structure` 的 find：`requiredStructure > appliedStructureTick` 改成 `!StructureApplied(…)`（S1），replace 照舊，守的性質不變。
  - `nt-reconnect-reuses-connection` 第一次是 wrong_failure：S2 之後拿回舊連線的接受帶的是舊的 salt，新的 session 不採用，失敗停在前面的 `REQUIRE(second.State() == Connected)`。這個 REQUIRE 加上訊息「a reconnect from the same endpoint is a new connection: the newer handshake connects」，expect 照舊就認得。重跑 killed。
  - `nt-links-more` 第一次是 wrong_failure：CMake 的訊息換行，expect 的 `.*` 跨不過去。照 entity_ids 的寫法逐字寫出連結集，重跑 killed。
- **跑的範圍**：網路全清單 71 個（這次改了網路的大部分原始檔）＋狀態機的 `sm-adopt-*` 2 個。
- **結果**：網路 71／71（第一次 69／71，上面兩個重跑 killed）；狀態機 2／2。

## 6. 和指示不同、自己判的（揭露）

- **S1 用 optional＋「tick＋1」，不用結構序號**：宣告 D-5 寫的是「要的結構 tick」；序號會改變 wire 上這個欄位的意思，optional 不會。
- **S2 用遞增的 salt，不用挑戰的時間桶**：時間桶要設期限，期限太短會讓慢的握手失敗。遞增的 salt 只比大小、不需要期限。用單調時鐘的奈秒，同一台機器上重開的 Client 也往上走。
  - 限界：機器重開之後單調時鐘從頭算，但那時伺服器上的舊連線早已逾時（10 秒），不受影響。
  - 系譜：netcode.io 的 connect token 帶建立時間、伺服器不收過期的（用時間分新舊）。
- **S3 選「帳本不動、下一個 tick 再做」，不關連線**：佇列滿是暫時的。太大（超過 `maxMessageBytes`）的結構更新會每個 tick 被拒、一直計數，這時看得到 `reliableRejected` 一直增加。伺服器訊息被拒不重送（每個 Peer 要另外一條佇列），只計數。
- **S4 的伺服器端計數放在 `PeerLedger::drops`**，用 `ReplicaDrop` 同一組理由。
- **小事 1 只做修剪，不加下界**：試過「沒消耗過時，現在的 tick 以前的格算已消耗」，但有了修剪之後兩者在件數上分不出（收了立刻過期），多的分支測不到，拿掉了。
- **網路協議版本 1 → 2**：接受的格式變了（S2）。
- **B2 的判斷用 Peer 的 `lastSentTick`**：有任何 Peer 這個 tick 還沒送，才取出 outbox。

## 7. 驗證

- 本機（macOS x86_64，Intel i9-9980HK，`core` preset）：全部 ctest 185 個全過；`gyo_net_tests` 77 案、`gyo_state_machine_tests` 38 案全過。
- **證據**（主 checkout `build/evidence/gyo2-net-module/review-fixes/`，git 不追蹤）：`red-net.log`、`red-state-machine.log`、`ctest-all.log`、`net-messages.txt`、`mutations-net.log`、`mutations-state-machine.log`。

## 之後要接的

- U2～U5 由主會話交使用者。
- 審查的小事 8：合併前看 L1 的結論。
