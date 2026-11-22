# 網路 Module（`engine/modules/net`）

更新：2026-11-22（新頁，`v8-arch-gyo2-net-module` 第 1 步前半 1a：傳輸層）。由來：[事前宣告](net-module-declaration.zh-Hant.md)（修訂 2，2026-11-22 動工前核准「照建議，全部核准」）D-2、D-3、D-12、第 8 節 G-2、第 12 節。**Design Invariants 與 Replacement Criteria 是草稿，最終文字在合併前核准定稿。**本頁照 1a 的實作寫；1b（登記單位 `NetModule`、Peer 實體、套用點、握手、存活）之後補。

Owner：架構。程式：`engine/modules/net/`（`GYO::Engine::Net`，target `GYO::Net`；1a 沒有登記單位）。行為測試：`tests/common/net/`（ctest `net.header`、`net.connection`、`net.transport`、`net.link`，標籤 `cpu`）；突變清單 [`scripts/net_mutations.json`](scripts/net_mutations.json)（`nt-`）。

## 一個例子

PvP 射擊的 Client 在第 100 步要把「開槍」可靠地送到 Match，同時每個 tick 送一份不可靠的位置。

- Client 的 `Connection` 有兩條通道：0 號 Ordered Reliable（優先度 1）、1 號 Unreliable（優先度 0）。`Send(0, 開槍)`、`Send(1, 位置)` 只是排進佇列。
- 這一幀的送件時，`Collect(now, {tick=100, 世代})` 把該送的片裝進封包（先放優先度高的），交給 `UdpLink` 送出。標頭帶 tick 100、連線識別、packet id、以及「我收到 Match 的最新封包是哪個、它之前 32 個收到哪些」。
- 這個封包掉了。Match 的回覆沒有確認它；過了 `max(1.5×RTT, 下限)`，「開槍」放進下一個封包重送。位置不重送：下一個 tick 會送新的。
- Match 的 `Connection::Receive(bytes, now)` 讀標頭、比連線識別、去重，把訊息放進各通道的收件佇列，`Pop(0)` 拿到「開槍」，恰好一次。每個丟掉的封包或片都記在一個理由下（重複、太舊、分片逾時……）。
- 測試把 `UdpLink` 換成 `MemoryLink`：同一個種子每次掉同樣的包、重複同樣的包、延後同樣的包，上層一行不改。

單機的 2D 橫版不選網路：沒有 socket、沒有 `Connection`（1b 之後：World 沒有角色就不登記任何東西，G-1）。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::OS ◄── GYO::Net
```

- 登記檔 `config/modules.csv` 的 `net` 列（1a）：`library_only=yes`、`default=no`、`depends` 空。1b 加登記單位後改成宣告 D-10 的 `library_only=no`、`default=yes`、`depends=world_time;rollback;authority;entity_ids`。
- link 寫在 `engine/modules/net/CMakeLists.txt`：`GYO::Base`、`GYO::OS`。socket 只經 `core/os` 的 `DatagramSocket`（宣告 D-1；AGENTS §6：一個外部組件一個 adapter，OS 的 socket 的 adapter 是 `core/os`，`UdpLink` 只是轉接）。網路 Module 不 include 平台標頭。
- 標頭：
  - `NetFailure.hpp`：錯誤 `NetFailure`（E2，`…Failure` 照命名規範 N25）。
  - `Link.hpp`：`Endpoint`、`ReceivedDatagram`、`Link`（interface）。
  - `UdpLink.hpp`、`MemoryLink.hpp`：兩個 Link。
  - `PacketHeader.hpp`：標頭、封包種類、`SequenceNewer`。
  - `Connection.hpp`：通道、設定、丟棄理由、計數、`Connection`。
- 內部：`src/ByteCursor.hpp`（小端序的 span 讀寫，不依賴 `core/io` 的 stream；宣告第 7 節第 4 項）。

## 2. 組成

| 部件 | 做什麼 | 不做什麼 |
|---|---|---|
| `Link` | 送一個 datagram 到某個 `Endpoint`；收一個（沒有就回「沒有」）；不阻塞 | 不認得封包、通道、遊戲狀態 |
| `Endpoint` | 不透明的 `u64`，意思屬於做出它的 Link；上層只比較、複製 | — |
| `UdpLink` | 包 `OS::DatagramSocket`；`ToEndpoint` 把 IPv4 四個 octet 與 port 放進低 48 位元；決定 OS 錯誤對網路的意思：送件 `WouldBlock`→`LinkBusy`、`Unreachable`→`Unreachable`、收件 `TooLarge`→`DatagramTooLarge`、其他→`LinkFailed`（os.md DI 7 把這個判斷交給呼叫者） | 不重試、不計數 |
| `MemoryLink` | 同一行程的一對端點；每個方向可設掉包率、重複率、延後（延後幾步）；splitmix64 固定種子，跨平台結果相同；計數注入的掉包、重複、延後；可觀察每個注入的 datagram | 不讀時鐘：時間是 `Advance()` 的步數 |
| `Connection` | 一條連線的純狀態機：`Send` 排訊息、`Receive(bytes, now)` 收、`Collect(now, stamp)` 給要送的 datagram、`Pop` 交出訊息、`Close` 送斷線 | 不碰 socket、不讀時鐘、不認得 World |

**設定**（`ConnectionSettings`；預設值與系譜）：

| 欄位 | 預設 | 系譜 |
|---|---|---|
| `mtu` | 1200 | lightyear `crates/io/link/src/mtu.rs:5` `DEFAULT_MTU` |
| 重送延遲 | `max(1.5 × 平滑 RTT, resendMinDelay)`；`resendMinDelay` 預設 0 | lightyear `channel/builder.rs:620-638` `ReliableSettings`（`rtt_resend_factor` 1.5、`rtt_resend_min_delay` 預設 0） |
| 平滑 RTT | 第一個樣本直接採用，之後 `rtt += (樣本 − rtt) / 12`，整數奈秒 | lightyear `crates/core/sync/src/ping/estimator.rs:4` `RTT_EWMA_ALPHA = 1/12` |
| `fragmentTimeout` | 3000 ms | lightyear `channel/receive.rs:53` `DISCARD_AFTER` |
| `maxMessageBytes` | 16 MiB | lightyear `channel/fragments/receive.rs` `DEFAULT_MAX_FRAGMENTED_MESSAGE_SIZE` |
| `disconnectPackets` | 10 | lightyear `crates/connection/netcode/src/client.rs:57` `num_disconnect_packets` |
| `reliableWindow` | 256 | 本卡定：u16 的序號用一半以內就不會判錯先後；256 遠大於 64 Hz 下一個 RTT 內的可靠訊息數 |
| `inboxCapacity` | 1024 | 本卡定（設定值） |
| `sendQueueCapacity` | 4096 | 本卡定（設定值） |
| `maxPendingFragmented` | 64 | 本卡定（設定值） |

封包層記住的歷史（送出 1024 個、收到 1024 個）是內部常數，遠大於一個標頭確認的 33 個。

## 3. wire 格式

所有欄位小端序。版本 `kProtocolVersion = 1`；格式只隨版本變。

**標頭**（24 bytes；lightyear 的五欄 `type`、`packet_id`、`ack`、`ack bitfield`、`tick` 加魔數、協議版本、連線識別、World 世代，宣告 D-2）：

| 位移 | 大小 | 欄位 | 說明 |
|---|---|---|---|
| 0 | u16 | 魔數 | `0x4E47`（線上的位元組是 `'G' 'N'`） |
| 2 | u8 | 協議版本 | 1 |
| 3 | u8 | 封包種類 | 下表 |
| 4 | u32 | 連線識別 | 握手產生（1b）；不符的 datagram 丟掉並計數 |
| 8 | u32 | World 世代 | 送端 World 被替換時加一（D-5，第 2 步起用） |
| 12 | u16 | packet id | 每送一個封包加一，只增不重用（2^16 回繞）；第一個是 1 |
| 14 | u16 | ack | 收到對方最新的 packet id（什麼都沒收到時是 0：id 0 要回繞一圈才送到） |
| 16 | u32 | ack bitfield | 第 i 位＝ `ack − 1 − i` 也收到了（之前 32 個） |
| 20 | u32 | tick | 送端 World 的 tick（D-9：判定用的時間只有 tick） |

**封包種類**：

| 值 | 種類 | 說明 |
|---|---|---|
| 1 | `Data` | 帶訊息（可以沒有），要求對方確認 |
| 2 | `AckOnly` | 只有標頭；不要求確認，兩端閒置時不會互相回 ack 不停（lightyear `packet_type.rs:27-37`） |
| 3 | `Disconnect` | 送端關閉連線；冗餘送 `disconnectPackets` 個 |
| 16～19 | 保留 | 1b 的握手（請求、挑戰、回應、接受）；1a 讀到是 `UnknownPacketKind` |

其他值是 `UnknownPacketKind`。0 不是種類。

**Data 的 payload**：標頭之後一連串的 entry，直到 datagram 結束。

| 欄位 | 大小 | 說明 |
|---|---|---|
| 通道 | u8 | 通道的編號（兩端照同樣的順序列通道） |
| flags | u8 | bit 0：分片；其他位元必須是 0 |
| id | u16 | Ordered Reliable：通道內的訊息序號；Unreliable 的分片訊息：重組用的編號；Unreliable 不分片：0 |
| 分片序號 | u16 | 只在分片時有 |
| 分片總數 | u16 | 只在分片時有；2 以上 |
| 長度 | u16 | 這一片的位元組數 |
| 內容 | 長度 | — |

- 不分片的訊息最多 `mtu − 24 − 6` bytes（1200 時 1170）；超過就切成每片 `mtu − 24 − 10` bytes（1200 時 1166）。
- 讀不了的 payload（未知的通道、分片序號超過總數、超過上限的長度、剩下不夠）是 `MalformedPayload` 或 `Truncated`；整個 datagram 丟掉、計數、`Receive` 回錯誤。

## 4. 通道、確認、重送

- **兩種通道**（宣告 D-2，第 2 節沒有 F 列要別的）：
  - `Unreliable`：每則最多一次、不保證順序；分片的訊息全到才交出，`fragmentTimeout` 內沒到齊就整則丟。
  - `OrderedReliable`：每則恰好一次、照送的順序；收方去重、缺號時等。
- **優先度**：每條通道一個數值；裝封包時先放優先度高的通道的片，相同的照通道的順序（第 5 步才用到頻寬分配）。
- **確認**（Gaffer 式，照 lightyear）：packet id 只增；每個標頭確認對方最新的 packet id 與它之前 32 個；封包被確認 ⇒ 它帶的可靠訊息的片被確認。
- **重送在訊息層**：可靠的片在重送延遲之後還沒被確認，就放進下一個封包（不是重送舊封包）。
- **被確認之後就不能丟**：一個封包裡只要有一片可靠訊息超出收方的窗，整個封包在被使用之前就拒收（`ReliableWindowFull`），不記成收到、不確認，送方會重送。
- **AckOnly**：收到要求確認的封包、而這一輪沒有資料要送時，送一個 AckOnly。

## 5. 丟棄的計數（宣告 D-12）

每個丟棄的出口一個理由，各自計數（`ConnectionStats::drops`），每條通道另記各理由丟了幾片（`ChannelStats::piecesDropped`）。計數不寫判定資料；1b 起交給 Peer 實體或 World 的 Resource。

| 理由 | 單位 | 什麼時候 |
|---|---|---|
| `DecodeFailed` | datagram | 標頭或 payload 讀不了（`Receive` 也回錯誤） |
| `ConnectionMismatch` | datagram | 連線識別不符 |
| `DuplicatePacket` | datagram | 已收過的 packet id |
| `TooOld` | datagram | 比收到的歷史（1024 個）還舊，分不出是不是重複 |
| `ReliableWindowFull` | datagram | 帶了超出收方窗的可靠片：拒收、不確認 |
| `DuplicateMessage` | 片 | 已收過的可靠片 |
| `FragmentTimeout` | 片 | Unreliable 的分片訊息逾時沒到齊 |
| `BufferFull` | 片 | Unreliable 的收件佇列滿，或分片重組的位置滿 |
| `SendRejected` | 訊息 | `Send` 拒收（送件佇列滿，或大於 `maxMessageBytes`） |

「片」：不分片的訊息是一片，分片的訊息每個分片一片；重送也算送出的片。

判定 G-2 的件數突き合わせ用兩種單位，都要逐項相等：
- datagram（每個方向）：送出＋注入的重複＝收下＋各封包層理由的丟棄＋注入的掉包；
- 片（每個方向、每條通道）：送出的片＋注入的重複裡的片＝交出的片＋各理由丟的片＋注入的掉包裡的片。

## Design Invariants

**草稿**（宣告第 12 節）。最終文字在合併前核准定稿。守衛欄寫 1a 已有的；綁實作方式的守衛（`gyo_expect_links` 等）在 scenario「伺服器＋兩個 Client」通過之後立（AGENTS §3「ガードを立てる時期」；宣告第 8 節）。

<a id="nt-1"></a>
**NT-1 職責。** 網路 Module 的職責是：(1) 連線（握手、存活、斷線理由）；(2) 傳輸（標頭、ack、通道、分片）；(3) 複製（登記、結構與值、確認、帳本、可見性）；(4) 輸入的收送與緩衝、入口；(5) 伺服器訊息的收送；(6) 對時的量測與給時間 Module 的輸入；(7) 丟棄的計數。不存判定資料的副本、不回滾、不認得 Player、不認得特定產品。1a 有 (2)、(7) 與 (1) 的斷線封包。

<a id="nt-2"></a>
**NT-2 一套內部協議。** Link 之上只有一種協議；Link 只有 bytes 與連線狀態。1a：`Link` 只有 `Send`、`Receive`；`UdpLink`、`MemoryLink` 換了上層不改（`net.transport` 用 `MemoryLink`、`net.link` 用 `UdpLink`，`Connection` 同一份）。

<a id="nt-3"></a>
**NT-3 冪等的鍵。** 每一筆帶宣告 D-4 表的鍵，重複收到不改結果。1a：可靠訊息的鍵（連線、通道、訊息序號），重複收到丟掉並計數；封包層以 packet id 去重。守衛：G-2（`net.transport`）；突變 `nt-ordered-no-dedup`、`nt-packet-no-dedup`。

<a id="nt-4"></a>
**NT-4 判定用的時間只有 tick。** 牆鐘只在 IO 層。1a：`Connection` 不讀時鐘，時間由呼叫者給（`NetTime`），只用來算 RTT、重送與分片逾時；標頭的 tick 照呼叫者給的寫。

<a id="nt-5"></a>
**NT-5 確認點。** 只在宣告 D-7 的兩個條件成立後寫 `ConfirmedThrough`。（第 4 步）

<a id="nt-6"></a>
**NT-6 不選就沒有**（G-1）。（1b）

<a id="nt-7"></a>
**NT-7 依賴。** 網路依賴權威、回滾、時間、entity_ids；它們都不依賴網路；外部組件只經 `core/os`。1a：只連 `GYO::Base`、`GYO::OS`。

<a id="nt-8"></a>
**NT-8 丟棄有理由、有計數。** 1a：第 5 節的九個理由；`Connection` 沒有不計數的丟棄出口。守衛：G-2 的件數突き合わせ（`net.transport`）、`net.connection` 的各案；突變 `nt-timeout-uncounted`、`nt-connid-unchecked`。

<a id="nt-9"></a>
**NT-9 編號。** 伺服器配的實體在 Client 上用同一個編號；範圍由產品分開，握手時檢查不重疊（Q1 核准：兩端同號，照 LIM-7）。（第 2 步）

<a id="nt-10"></a>
**NT-10 每收件者的狀態在 Peer 實體。** 帳本、可見性、輸入緩衝；不進判定、可重建。（1b 起）

### Replacement Criteria

**草稿**（宣告第 12 節，寫在積上去之前）。符合任一條時，停下來交使用者「修還是重建」。

- **RC-NT1**：職責清單（NT-1）要多一項，或判不出某樣東西屬網路、權威、回滾、時間哪一個。
- **RC-NT2**：有點名的產品在一個 World 的生命內用完產品分給伺服器的編號範圍。
- **RC-NT3**：要讓複製正確，必須在 Kernel 或編排層加網路專用的欄位、分支或觀察點。
- **RC-NT4**：每加一種 Link，上層（傳輸、複製）就要改。
- **RC-NT5**：同一個回避策（對映、去重、狀態的副本）要放進兩個以上的地方，或 Link、中繼開始需要遊戲的狀態才能正確。
- **RC-NT6**：G-8 的數字超過使用者選的頻寬預算，而且相關性與優先度都已用上。
- **RC-NT7**：每加一種要複製的元件，就要改網路 Module 的程式（不只是登記）。
