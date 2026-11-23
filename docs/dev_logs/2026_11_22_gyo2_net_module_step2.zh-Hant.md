# 2026-11-23 網路 Module 第 2 步：元件的複製（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（1b 4c658a72、主會話的 b62496eb 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md)（修訂 2；使用者 2026-11-22 動工前核准）D-4、D-5、D-13、C-MS1；判定 G-5、G-7、G-8；scenario「伺服器＋兩個 Client」。
- **實作者**：Opus／high。
- **不做的**（主會話的卡片）：Resource 的複製（Q8 (b)，交主會話決定）；K3 的寫法（第 4 步）。

## 做了什麼

- **`Codec.hpp`**：`NetWriter`／`NetReader`（逐欄、小端序；整數、浮點數照位元、enum、bool）、`Codec<T>`（`encode`、`decode`、`version`）、`DefaultCodec<T>`（只有數值與 enum）。沒有位元組照抄。
- **`Replication.hpp`／`Replication.cpp`**：
  - `NetEntry(setup).Replicate<T>`、`.ServerMessage<M>`；`NetRegistry::Hash`（名字、順序、編碼版本）取代 1b 的空登記雜湊。
  - 伺服器 `ReplicateOut`（`ExclusiveWorld`，`Main`／`Last`）：讀 Client 的確認與 resync，和帳本比出結構更新（生成、刪除、加減元件，結構有變的實體的值併進去），送這一幀的伺服器訊息，再送變過的值（分則、整個實體不拆、空 tick 也送一則）。
  - Client `ApplyReplication`（`ExclusiveWorld`，`Main`／`First`）：結構更新照順序整則套用（`SpawnWithId`、`Insert`、`Modify`、`Remove`、`Despawn`），值等結構 tick、新的蓋舊的，處理完才回確認；記 D-7 的兩個條件（`latestConfirmed`、`maxAppliedTick`）；世代與 epoch。
  - `CollectServerMessages<M>`（伺服器，`PostUpdate`）、`DeliverServerMessages<M>`（Client，`PreUpdate`，等結構 tick 套用後寫 `Message<M>`）、`NotifyReplica`（發 `ServerWorldReplaced`）。
  - `OnServer()`、`OnClient()`：讀 `NetWorldRole` 的執行條件。
  - 帳本 `PeerLedger`（Peer 實體上的元件）、`NetReplicaState`、`NetOutbox`、`NetWorldLink`（從 `NetModule.hpp` 搬來）、`ServerWorldReplaced`、`NetLinkFailed`。
- **`NetModule`**：通道固定成 0 號 Ordered Reliable、1 號 Unreliable；Build 一律建登記表（不登記進 Kernel，G-1 不變），有角色才登記第 7 節的東西；權威策略下多標 `ReplicateOut`、`ApplyReplication`、`DeliverServerMessages:*` 為 `OneEndOnly`；準備時伺服器的世代加一、Client 的第二個以上 World 要 resync；`ReceiveNet` 替伺服器的每條連線補 Peer 實體（換 World 後重建）、發 `NetLinkFailed`；`Check` 加 `NT-E5`（K3）。
- **`NetService`／`ServerHost`**：世代（`BeginServerWorld`、`Generation`）、Client World 的計數；每條連線送出的位元組數（`BytesSentTo`，G-8）、`UserIdOf`。
- **測試**（`gyo_net_tests` 52 案；ctest 加 `net.replication`、`net.scenario`）：
  - 範例遊戲 `GameTestSupport.hpp`（自建的 2D 樣本）：位置每 tick 動、血量加減、`Target`（元件裡存 `EntityId`）、門只由伺服器生成（地圖實體，`OnServer()`）並開關、伺服器訊息 `DoorOpened`（同一幀生成的門也在同一幀宣告），規則都是 tick 的函數。
  - `replication:` G-5；空 tick 也送、確認過的不再送；兩端登記不同 `ProtocolMismatch`；K3 `NT-E5`；`OnServer()` 兩端同登記時指紋相同（K2）；`OnServer()` 在 Client 不跑；伺服器換 World → `ServerWorldReplaced`，Client 換 World → 整份重送；`NetLinkFailed`；G-8。
  - `scenario:` 伺服器＋兩個 Client（`HubLink` 把兩條 MemoryLink 接到伺服器；掉包 10%、重複 5%、延後 20%）：兩個玩家移動、門在 tick 30 開、60 關，兩個 Client 最後都和 Match 相同，各收到兩則門的訊息、都在門存在之後。
  - `link:` G-7 的縮小版：UDP loopback 上的範例遊戲，Client 最後和伺服器相同。
- **文件**：[`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md) 新增第 8 節（複製），第 7 節補登記的東西與 `NetLinkFailed`、`NT-E5`，NT-1、NT-3、NT-8、NT-9、NT-10 補第 2 步。

## 和宣告不同的地方

- **每個 tick 送一次，不是每一幀**：一幀跑了好幾個 tick 時只送最後一個；同一個 tick 不送第二次；沒有固定步的 World 不複製。測試用 `ManualDelta` 讓一幀一個 tick。
- **結構更新只在有變化時送**：宣告寫「每個 tick、每個 Peer 一則」。值每個 tick 都送（空的也送）；結構沒變化時不送，值帶的「要的結構 tick」就是上一則。
- **resync 與 epoch**（宣告沒寫）：Client 換 World 後，新 World 要整份。Client 送 resync（epoch 加一），伺服器清空那個 Peer 的帳本；在途的舊 epoch 訊息丟掉並計數。沒有它，換掉的 Client World 收不到已經送過的實體。
- **換 World 之間的訊息**：Client 收到新世代後照樣把新世代的訊息套用到現在的 World；產品沒換 World 時，同號的實體被 `SpawnWithId` 拒絕（計數）。
- **伺服器訊息的順序**：送在同一條 Ordered Reliable、結構之後，Client 另外等它要的結構 tick。兩道保證重疊，所以「伺服器先送訊息」的突變捕不到（見突變）。
- **socket 壞掉**：照卡片，現狀（記在 `NetService`）加 World 的訊息 `NetLinkFailed`；交給 runner 的部分在合併前核准時交使用者。

## 自己判的細節（L17 的範圍，揭露）

- **名字**：`NetEntry`、`Replicate`、`ServerMessage`、`Codec`、`NetWriter`、`NetReader`、`NetRegistry`、`PeerLedger`、`NetReplicaState`、`NetOutbox`、`NetWorldRole`、`ReplicateOut`、`ApplyReplication`、`CollectServerMessages:<名字>`、`DeliverServerMessages:<名字>`、`NotifyReplica`、`ServerWorldReplaced`、`NetLinkFailed`、`ReplicaDrop`、`NT-E5`。
- **值的一則約 1100 bytes**：1200 的 MTU 下不分片的上限是 1170（第 3 節），留一點餘裕。
- **未確認的值訊息只記 256 個 tick**：再舊的忘掉（它們的實體會因戳記還舊而再送）。
- **「變過」的判斷**：Kernel 的 `ExclusiveWorld::ReadColumn` 給每格的變更戳記；帳本記確認過的快照的 `ExclusiveWorld::Stamp()`。
- **Client 寫值用 `ExclusiveWorld::Modify`**：它把格的變更戳記設成這一步的，遊戲的 `Changed<T>` 讀得到（G-5 的 `positionsChanged > 0`、scenario 的兩個 Client 都證明了）。Kernel 不用改。
- **`SpawnWithId` 在 Client 沒被丟**：G-5 的 `SpawnRejected` 是 0；伺服器範圍 [1, 100000]、Client [100001, 200000]。
- **Peer 實體佔 Match 的編號**（LIM-7 允許）；Peer 不複製（不是登記的元件）。
- **`MissingTarget`、`StaleValue`**：晚到的值的對象已刪掉，或比寫過的舊。G-5 有 61、492 筆，都是有損 Link 下預期會有的。

## 驗證

- 本機（macOS x86_64，`core` preset）：
  - `gyo_net_tests` 52 案全過；
  - `ctest -R "net\.|os\.|cmake\.|naming|authority|entity_ids|orchestration|world_time|rollback"` 70 個全過；
  - 全部的 ctest 178 個全過。
- **G-5**（事前宣告第 8 節修訂 2 的文字；種子 `0x5EED0005`，兩個方向掉包 20%、重複 10%、延後 25%（1～3 步）；規則跑到 tick 400）：
  - (1) 觀測成立的 tick 168／391（43.0%，門檻 10%），不相等 0。
  - (2) 伺服器停止變動後 42 幀排空、全部確認，Client 和伺服器相同（76 個實體：位置 57、血量 43、`Target` 29、門 19）。
  - (3) 伺服器訊息 81 則，在實體存在之前交出的 0 則。
  - (4) 結構 送 250＝套用 250；伺服器訊息 送 81＝收 81＝交出 81；值 送 503＝收 408＋掉包裡 95、收 408＝處理 408＋待處理 0＋舊世代或舊 epoch 0；確認 送 408＝收 408；確認點成立的 tick 342；丟棄 `MissingTarget` 61、`StaleValue` 492、其他 0。
  - 兩次執行的數字逐項相同（`net-step2-cases-run1.log`、`-run2.log`）。
- **G-8**（範例遊戲跑到 tick 120、無損的 MemoryLink、手動時鐘；130 幀）：兩次執行每 tick 的位元組逐項相同。合計 43 950 bytes，每 tick 最大 625、平均 338.1；規則停止後每 tick 55 bytes（一則空的值、確認與 keepalive）。數列在證據裡。預算不判（Q7 未定）。
- **G-7**：UDP loopback 上的縮小版 G-5（規則跑到 tick 90），Client 最後和伺服器相同、伺服器訊息都在實體之後。三平台等 PR 的 L1。
- **突變**（`scripts/net_mutations.json`，第 2 步加 10 個，共 37 個）：**全清單本機 37／37 killed**（`mutations-all.log`）。
  - 加的：`nt-value-before-structure`、`nt-ledger-at-send`、`nt-empty-tick-not-sent`、`nt-ledger-not-advanced`、`nt-generation-unchecked`、`nt-removed-not-encoded`、`nt-stale-value-applied`、`nt-resync-ignored`、`nt-onserver-everywhere`、`nt-k3-as-k2`。
  - 第一次跑：`nt-removed-not-encoded` 是 wrong_failure。我以為讀錯會被計成 `Malformed`，實際上數量欄還在、後面的位元組被讀成別的欄位，沒有讀不了的地方，失敗出現在 G-5 (1)。expect 改成 (1) 的訊息（它仍只在結構的編碼漏一欄時出現在這個案例）。
  - 第一次跑：`nt-message-before-structure`（伺服器先送訊息再送結構）survived，**沒有加進清單**：同一幀的訊息與結構走同一條有序的通道、多半在同一個 datagram，Client 又在同一次 `ApplyReplication` 裡套完結構才交出訊息；之後 Client 另外等結構 tick，兩道保證重疊，沒有測試能單獨看到它（§14.5：捕不到的不加）。
  - §14.5：每個都寫了「只有它捕到的」與 `guards`（D-4、D-5、D-7、D-13、判定 G-5）。沒加的：上面那一個；「Client 等結構 tick 的伺服器訊息」（同上，和通道的順序重疊）；`Changed<T>`（Kernel 的 `Modify` 一定打戳記，網路 Module 沒有別的寫入路徑可以突變）。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step2/`，git 不追蹤）：`net-step2-cases-run1.log`、`-run2.log`（G-5、G-7、G-8、scenario 等的全部數字）、`ctest-selected.log`、`ctest-all.log`、`mutations.log`（第 2 步的 11 個的第一次）、`mutations-all.log`（全清單）。

## 之後要接的

- 第 3 步：輸入、對時、重連。
- 第 4 步：K3（寫 `RollbackInbox`、確認點 `ConfirmedThrough`；現在 `NT-E5` 擋著）。
- 第 5 步：可見性、優先度（`NotSent`）。
- Resource 的複製（Q8）：主會話決定後。
- `NetLinkFailed` 之外交給 runner 的路：合併前核准時交使用者。
