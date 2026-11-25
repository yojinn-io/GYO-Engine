# 網路 Module（`engine/modules/net`）

更新：2026-11-22（新頁，`v8-arch-gyo2-net-module` 第 1 步前半 1a：傳輸層）；2026-11-23（第 3 步後半 3b：對時、W-8；新增第 10 節，第 9 節拿掉 3a 的暫時做法，NT-1、NT-4 補 3b）；2026-11-23（第 3 步前半 3a：輸入、重連、CCR-8；新增第 9 節，第 3、7、8 節補，NT-1、NT-3、NT-10 補 3a）；2026-11-23（第 2 步：元件的複製、伺服器訊息、`OnServer()`／`OnClient()`、World 世代；新增第 8 節，第 7 節補登記的東西，NT-1、NT-3、NT-8、NT-9、NT-10 補第 2 步）；2026-11-22（第 1 步後半 1b：握手、存活、登記單位、Peer 實體、套用點；第 1、2 節改寫，第 3 節補握手的種類與 payload，新增第 6、7 節，NT-1、NT-6、NT-7、NT-8、NT-10 補 1b）。由來：[事前宣告](net-module-declaration.zh-Hant.md)（修訂 2，2026-11-22 動工前核准「照建議，全部核准」）D-2、D-3、D-12、第 8 節 G-2、第 12 節。**Design Invariants 與 Replacement Criteria 是草稿，最終文字在合併前核准定稿。**本頁照 1a、1b、第 2 步、3a、3b 的實作寫。；2026-11-23（PR-1 合併前設計審查的修正：B1、B2、S1～S4、小事 1～7，dev_log review_fixes）

Owner：架構。程式：`engine/modules/net/`（`GYO::Engine::Net`，target `GYO::Net`，登記單位 `NetModule`）。行為測試：`tests/common/net/`（ctest `net.header`、`net.connection`、`net.transport`、`net.link`、`net.handshake`、`net.liveness`、`net.module`、`net.replication`、`net.scenario`、`net.input`、`net.reconnect`、`net.clock`，標籤 `cpu`）；突變清單 [`scripts/net_mutations.json`](scripts/net_mutations.json)（`nt-`）。

## 一個例子

PvP 射擊的 Client 在第 100 步要把「開槍」可靠地送到 Match，同時每個 tick 送一份不可靠的位置。

- Client 的 `Connection` 有兩條通道：0 號 Ordered Reliable（優先度 1）、1 號 Unreliable（優先度 0）。`Send(0, 開槍)`、`Send(1, 位置)` 只是排進佇列。
- 這一幀的送件時，`Collect(now, {tick=100, 世代})` 把該送的片裝進封包（先放優先度高的），交給 `UdpLink` 送出。標頭帶 tick 100、連線識別、packet id、以及「我收到 Match 的最新封包是哪個、它之前 32 個收到哪些」。
- 這個封包掉了。Match 的回覆沒有確認它；過了 `max(1.5×RTT, 下限)`，「開槍」放進下一個封包重送。位置不重送：下一個 tick 會送新的。
- Match 的 `Connection::Receive(bytes, now)` 讀標頭、比連線識別、去重，把訊息放進各通道的收件佇列，`Pop(0)` 拿到「開槍」，恰好一次。每個丟掉的封包或片都記在一個理由下（重複、太舊、分片逾時……）。
- 測試把 `UdpLink` 換成 `MemoryLink`：同一個種子每次掉同樣的包、重複同樣的包、延後同樣的包，上層一行不改。

連線本身：Client 送「請求」，Match 回「挑戰」（一個它算得出來的雜湊，自己不留任何東西），Client 帶著請求與挑戰回「回應」，Match 比協議版本、協議雜湊、步長、配發器範圍、（有權威策略時）一致性指紋，問產品要不要收，然後回「接受」或「拒絕」（寫明哪一項）。接受之後 Match 的 World 多一個 Peer 實體，並發 `PeerConnected`；Client 關掉或 10 秒沒消息，Peer 實體刪掉並發 `PeerDisconnected`（理由 `ClosedByRemote`、`Timeout`……）。

單機的 2D 橫版不選網路：Product 沒有 `NetProductOptions` 就不建 socket、不建傳輸物件；World 沒有 `NetWorldOptions` 就不登記任何東西（G-1）。

## 1. 位置與依賴

```text
GYO::Base、GYO::OS、GYO::Time、GYO::Kernel、GYO::Orchestration
GYO::WorldTime、GYO::Rollback、GYO::Authority、GYO::EntityIds   ◄── GYO::Net
```

- 登記檔 `config/modules.csv` 的 `net` 列（1b 起，宣告 D-10）：`library_only=no`、`default=yes`、`depends=world_time;rollback;authority;entity_ids`、登記單位 `GYO::Engine::Net::NetModule`（`engine/net/NetModule.hpp`）。1a 時是 `library_only=yes`、`default=no`。
- link 寫在 `engine/modules/net/CMakeLists.txt`：`GYO::Base`、`GYO::OS`、`GYO::Time`（IO 層的單調時鐘，D-9）、`GYO::Kernel`、`GYO::Orchestration`；其他 Module 從登記檔來（`gyo_link_registered_dependencies`）。反方向沒有。socket 只經 `core/os` 的 `DatagramSocket`（宣告 D-1；AGENTS §6：一個外部組件一個 adapter，OS 的 socket 的 adapter 是 `core/os`，`UdpLink` 只是轉接）。網路 Module 不 include 平台標頭。
- 標頭：
  - `NetFailure.hpp`：錯誤 `NetFailure`（E2，`…Failure` 照命名規範 N25）。
  - `Link.hpp`：`Endpoint`、`ReceivedDatagram`、`Link`（interface）。
  - `UdpLink.hpp`、`MemoryLink.hpp`：兩個 Link。
  - `PacketHeader.hpp`：標頭、封包種類、`SequenceNewer`。
  - `Connection.hpp`：通道、設定、丟棄理由、計數、`Connection`。
  - `Handshake.hpp`：斷線理由、`IdRange`、比對的事實 `HandshakeFacts`、請求與接受的內容、`SessionSettings`、`CompareFacts`。
  - `ClientSession.hpp`、`ServerHost.hpp`：連線層（握手、存活、keepalive），仍是純的狀態機。
  - `NetService.hpp`：Product 一份的傳輸物件與 `NetProductOptions`。
  - `NetModule.hpp`：登記單位、`NetWorldOptions`、`Peer`、`PeerConnected`、`PeerDisconnected`、`NetWorldLink`。
- 內部：`src/ByteCursor.hpp`（小端序的 span 讀寫，不依賴 `core/io` 的 stream；宣告第 7 節第 4 項）。

## 2. 組成

| 部件 | 做什麼 | 不做什麼 |
|---|---|---|
| `Link` | 送一個 datagram 到某個 `Endpoint`；收一個（沒有就回「沒有」）；不阻塞 | 不認得封包、通道、遊戲狀態 |
| `Endpoint` | 不透明的 `u64`，意思屬於做出它的 Link；上層只比較、複製 | — |
| `UdpLink` | 包 `OS::DatagramSocket`；`ToEndpoint` 把 IPv4 四個 octet 與 port 放進低 48 位元；決定 OS 錯誤對網路的意思：送件 `WouldBlock`→`LinkBusy`、`Unreachable`→`Unreachable`、收件 `TooLarge`→`DatagramTooLarge`、其他→`LinkFailed`（os.md DI 7 把這個判斷交給呼叫者） | 不重試、不計數 |
| `MemoryLink` | 同一行程的一對端點；每個方向可設掉包率、重複率、延後（延後幾步）；splitmix64 固定種子，跨平台結果相同；計數注入的掉包、重複、延後；可觀察每個注入的 datagram | 不讀時鐘：時間是 `Advance()` 的步數 |
| `Connection` | 一條連線的純狀態機：`Send` 排訊息、`Receive(bytes, now)` 收、`Collect(now, stamp)` 給要送的 datagram、`Pop` 交出訊息、`KeepAlive` 給一個 AckOnly、`Close` 送斷線 | 不碰 socket、不讀時鐘、不認得 World |
| `ClientSession` | Client 端的連線層：請求、回應的重送，接受或拒絕，存活（連線→中斷→斷線），閒置時的 keepalive；接受後持有一條 `Connection` | 同上 |
| `ServerHost` | 伺服器端的連線層：無狀態的挑戰、比對、問產品、配連線識別；每個連線一條 `Connection`、存活、keepalive；事件 `Connected`／`Disconnected` | 同上 |
| `NetService` | Product 一份：Link 與連線層；`Pump` 收、`Flush` 送、`Close` 給產品關 | 不認得 World 的型別（World 那一半在 `NetModule` 的 System） |

**設定**（`ConnectionSettings`；預設值與系譜）：

| 欄位 | 預設 | 系譜 |
|---|---|---|
| `mtu` | 1200 | lightyear `crates/io/link/src/mtu.rs:5` `DEFAULT_MTU` |
| 重送延遲 | `max(1.5 × 平滑 RTT, resendMinDelay)`；`resendMinDelay` 預設 0 | lightyear `channel/builder.rs:620-638` `ReliableSettings`（`rtt_resend_factor` 1.5、`rtt_resend_min_delay` 預設 0） |
| 平滑 RTT | 第一個樣本直接採用，之後 `rtt += (樣本 − rtt) / 12`，整數奈秒 | lightyear `crates/core/sync/src/ping/estimator.rs:4` `RTT_EWMA_ALPHA = 1/12` |
| `fragmentTimeout` | 3000 ms | lightyear `channel/receive.rs:53` `DISCARD_AFTER` |
| `maxMessageBytes` | 16 MiB | lightyear `channel/fragments/receive.rs` `DEFAULT_MAX_FRAGMENTED_MESSAGE_SIZE` |
| `disconnectPackets` | 10 | lightyear `crates/connection/netcode/src/client.rs:57` `num_disconnect_packets` |
| `reliableWindow` | 256 | 本卡定：u16 的序號用一半以內就不會判錯先後；256 遠大於 64 Hz 下一個 RTT 內的可靠訊息數。要是 2 的次方（斷言，審查小事 4）：環以 `id % 大小` 索引，65 536 整除才不會在回繞處讓窗內兩個 id 落在同一格 |
| `inboxCapacity` | 1024 | 本卡定（設定值） |
| `sendQueueCapacity` | 4096 | 本卡定（設定值） |
| `maxPendingFragmented` | 64 | 本卡定（設定值） |

封包層記住的歷史（送出 1024 個、收到 1024 個）是內部常數，遠大於一個標頭確認的 33 個。

**連線層的設定**（`SessionSettings`；都是設定值）：

| 欄位 | 預設 | 系譜 |
|---|---|---|
| `keepaliveInterval` | 100 ms：距上次送出超過它才送 keepalive；握手的請求、回應也照這個間隔重送 | lightyear netcode `crates/connection/netcode/src/lib.rs:116` `PACKET_SEND_RATE_SEC = 1/10`；送 keepalive 的判斷照 `server.rs:893`（`last_send + rate < now`） |
| `interruptAfter` | 1 s：Client 超過它沒收到就進「中斷」 | 主會話定（10 個 keepalive 間隔） |
| `timeoutAfter` | 10 s：超過它沒收到就斷線（`Timeout`），兩端都是 | lightyear netcode `server.rs:34` `CLIENT_TIMEOUT_SECS`；判斷照 `server.rs:856`（`last_receive + timeout < now`） |
| `handshakeTimeout` | 15 s：握手超過它沒完成就 `HandshakeTimeout` | lightyear netcode `lib.rs:115` `CONNECTION_TIMEOUT_SEC` |

門檻一律是「超過」：剛好等於門檻還不算。

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
| 16 | `ConnectRequest` | Client 的請求（下表）；連線識別 0 |
| 17 | `Challenge` | 伺服器的挑戰：u64 |
| 18 | `ConnectResponse` | 請求的內容原樣＋挑戰 u64 |
| 19 | `Accepted` | 接受（下表）；標頭的連線識別＝新的連線識別 |
| 20 | `Rejected` | 拒絕：理由 u8（`DisconnectReason`）＋u16 長度前綴的說明文字 |

其他值是 `UnknownPacketKind`。0 不是種類。握手的封包 packet id 與 ack 欄都是 0，不經 `Connection`（`Connection` 讀到握手的種類是 `DecodeFailed`）。

**請求的內容**（`ConnectRequest`，種類 16 與 18）：

| 欄位 | 大小 | 說明 |
|---|---|---|
| 網路協議版本 | u32 | `kNetProtocolVersion`（3；2 起接受帶 salt、請求帶 epoch，審查 S2、窄複查 R2；3 起每則值訊息帶延後清單，PR-2 第 5 步，PR-2 審查 M2：PR-1 的 2 已進 gyo2，所以升版）：標頭之上的訊息的意思；標頭的 `kProtocolVersion` 是分幀，不同就根本讀不了 |
| 協議雜湊 | u64 | 網路的登記的雜湊；1b 還沒有複製的登記，是空登記的雜湊（`NetProtocolHash`） |
| 步長 | u64 | 奈秒（時間 Module 的 `FixedTime` 的步長） |
| 配發器範圍 | u64＋u64 | [first, last] |
| 使用者 id | u64 | 不透明；產品對到自己的 Player |
| salt | u64 | 3a：每次握手一個；同一次握手重送的回應帶同一個，重連（新的握手）帶更大的（D-8）。審查 S2 起是遞增的：IO 層單調時鐘的奈秒，時鐘沒動時取上一個加一。限界：單調時鐘只在同一個行程內可比，重開的 Client 抽到的 salt 可能比上一個行程的小，這時新握手被當成舊的丟掉，要等舊連線逾時（10 秒，小於握手逾時 15 秒）。接受（Accepted）帶回同一個 salt |
| epoch | u32 | 窄複查 R2：Client World 的 epoch，伺服器的帳本從它開始 |
| 有沒有指紋 | u8 | 0 或 1（World 有權威策略時 1） |
| 指紋 | u16＋bytes | `Fingerprint::Text()`；沒有時長度 0 |
| token | u16＋bytes | 只帶、不驗證（Q7） |

請求的 datagram 不得小於挑戰（32 bytes），不夠就補零：伺服器回的永遠不比收到的大（防放大）；伺服器不回小於挑戰的請求（`Unexpected`）。

**接受的內容**（種類 19）：連線識別 u32（隨機、非 0）、Match 的當前 tick u32、步長 u64（奈秒）、World 世代 u32、Match 的配發器範圍 u64＋u64。

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
| `UnknownSource` | datagram | 連線層：沒有連線的來源送來的資料封包（含斷線之後才到的） |
| `Unexpected` | datagram | 連線層：不合狀態的握手封包（送到伺服器的挑戰、連上後才到的 `Accepted`、小於挑戰的請求） |

「片」：不分片的訊息是一片，分片的訊息每個分片一片；重送也算送出的片。

判定 G-2 的件數突き合わせ用兩種單位，都要逐項相等：
- datagram（每個方向）：送出＋注入的重複＝收下＋各封包層理由的丟棄＋注入的掉包；
- 片（每個方向、每條通道）：送出的片＋注入的重複裡的片＝交出的片＋各理由丟的片＋注入的掉包裡的片。

## 6. 握手與存活（宣告 D-6、D-8）

- **握手**：Client「請求」→ Match「挑戰」→ Client「回應」→ Match「接受」或「拒絕」。
  - **挑戰無狀態**：挑戰＝伺服器的隨機 secret、來源 endpoint、請求的位元組做 SHA-256 取前 64 位元（`Detail::ChallengeOf`）。Match 收到回應時重算，對了才比對、才配連線；在那之前不為握手留任何東西。系譜：netcode.io 的 challenge token（lightyear 的 netcode crate）。
  - **比對的順序**（`CompareFacts`，第一個不同的回拒絕）：網路協議版本、協議雜湊（都是 `ProtocolMismatch`，說明寫哪一項）→ 步長（`StepLengthMismatch`）→ 配發器範圍重疊（`IdRangeOverlap`，說明寫兩個範圍）→ 指紋（`FingerprintMismatch`）。
  - **K1 不比指紋**：Client 沒送指紋（它的 World 沒有權威策略）時不比；送了時，Match 也要有、而且相同。
  - **產品決定收不收**：`NetProductOptions::admission` 在「接受」之前呼叫，回 `Err` 就是 `Refused`，說明是錯誤的訊息。
  - 挑戰回錯：`ChallengeFailed`。同一個 Client 重送回應（它的「接受」掉了）時，Match 回同一個連線識別。
- **斷線理由**（`DisconnectReason`）：`Closed`、`Timeout`、`ClosedByRemote`、`ProtocolMismatch`、`StepLengthMismatch`、`IdRangeOverlap`、`FingerprintMismatch`、`ChallengeFailed`、`HandshakeTimeout`、`Refused`。關閉時冗餘送 10 個 Disconnect（第 2 節）。
- **存活**：
  - 這條連線的任何 datagram（連線識別相符，重複的也算）都刷新「最後收到」。別的連線的不算。
  - 距上次送出超過 `keepaliveInterval`、這一幀又沒有別的要送時，送一個 AckOnly 當 keepalive（不另設種類：AckOnly 本來就不引發回 ack）。有資料要送的那一幀不送 keepalive。
  - Client：超過 `interruptAfter` 沒收到 →「中斷」（連線保留，收到任何東西就回到「連線」）；超過 `timeoutAfter` →「斷線（`Timeout`）」。伺服器：超過 `timeoutAfter` 就斷線。
  - 斷線之後不自己重連：重連是新的 Client World（產品的 `ReplaceWorld`），新 World 的第一幀照它的 `NetConnectTarget` 開新的握手；沒有就不連（D-8）。〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)；**最終文字在合併前核准**〕

## 7. 接線（宣告 D-3、D-10；編排層 O-12、O-15）

- **Product 一份**：`ProductDescription::options` 有 `NetProductOptions` 時，`NetModule::Configure` 建 `NetService` 放進 Product 一份的共用處：
  - Link：給了 `link`（例：`MemoryLink` 的一端）就用它；沒給就照 `bind` 開 UDP（`UdpLink`）。〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)；**最終文字在合併前核准**〕Server 在這裡開（要 listen；開不了回報 `NT-E4`）；Client 在第一次拿到 `NetConnectTarget` 時才開（單人模式不開 port），開不了計一次 link failure（`NetLinkFailed`）、不連。
  - **連誰**：〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)；**最終文字在合併前核准**〕不在 `NetProductOptions`：client World 的 Resource `NetConnectTarget{server, userId, token}`（`server` 是 UDP 的位址或產品給的 Link 上的 Endpoint），由產品的準備函式或替換的 `ReplaceRequest::extra` 放入。net 在 World 的第一幀（`ReceiveNet` 最前面）讀一次：沒有（或位址種類和傳輸不合：計一次 link failure）就關掉還活著的連線、不連；和還活著的連線同一個目標就沿用（新 World 照 R2 送 resync）；不同的目標就關掉舊的（Disconnect 當場送出）、開新的；斷掉的連線丟掉、開新的。系譜：Unity 的單人 `CreateLocalWorld`／多人 `CreateClientWorld`（`ClientServerBootstrap.cs:98`、`:339`）；lightyear、Godot 在執行中給位址。
  - 角色、`SessionSettings`、`admission`；`clock`（沒給就讀 `core/time` 的單調時鐘）；`seed`（伺服器的挑戰 secret 與連線識別的種子，沒給就用 `std::random_device`）。〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)〕使用者 id、token 不在這裡，在 client World 的 `NetConnectTarget`。
  - 沒有 `NetProductOptions`：什麼都不建、不開 socket（G-1）。
- **World 的角色**：`WorldDescription::options` 的 `NetWorldOptions{role, idRange}`。沒有就**什麼都不登記**（G-1）。有角色時登記：
  - ECS 元件 `Peer`（連線識別、使用者 id、丟棄計數）、Resource `NetWorldLink`、`NetConnectTarget`（產品放入，`v8-arch-gyo2-net-connect-target`）、訊息 `PeerConnected`、`PeerDisconnected`；
  - 收件 System `ReceiveNet` 在 `Main`／`First`、送件 System `SendNet` 在 `Main`／`Last`（套用點）；
  - 有權威策略的 World：兩個 System 經 `authority.OneEndOnly` 標記；沒有權威策略的（K1）不碰權威 Module（否則 AU-E2）。
  - `Check`：有角色但 Product 沒有 `NetProductOptions` 是 `NT-E1`；角色和 Product 的不同是 `NT-E2`；一個 Product 第二個有角色的 World 是 `NT-E3`。
- **System 怎麼拿到傳輸物件**：準備函式在 World 放 `NetWorldLink`（指向 `NetService`，O-15）。準備時也把這個 World 的事實交給 `NetService`（〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)；**最終文字在合併前核准**〕Server 的 World；Client 的 World 第一次交出是在第一幀的 `SendNet`，在讀過 `NetConnectTarget` 之後）：步長讀 `FixedTime`，範圍讀 `RangeAllotter`（`NetWorldOptions::idRange` 給了就用它，`SendNet` 也就不讀 `RangeAllotter`），指紋用 `Authority::ConsistencyFingerprintOf`（有權威策略時）。
- **每一幀**：`ReceiveNet` 讀時鐘、`Pump`（收每個等著的 datagram、算存活），把連線層的事件寫進 World：連上 → spawn 一個 `Peer` 實體、發 `PeerConnected{peer, userId}`；斷了 → despawn、發 `PeerDisconnected{peer, userId, reason, detail}`（Client 的握手失敗沒有 Peer 實體，`peer` 是 0）。`SendNet` 更新事實、`Flush`（送）、把每條連線的丟棄計數寫回 `Peer`。tick 讀時間 Module 的 `Tick`，寫進標頭。
- **時間**：時鐘只在 IO 層（存活、RTT），不寫進判定資料（D-9）。
- **不是判定資料**：`Peer`、`NetWorldLink` 不進權威登記、不進 digest；它們的雜湊函式什麼都不雜湊（連線識別是隨機的，不是兩次執行要一致的 World 狀態）。
- **Link 的錯誤**（E2）：`NetService` 記下並計數（`LinkFailures`、`LastLinkFailure`），World 照跑；收件時只有 `DatagramTooLarge`（那一個丟掉）繼續收，其他錯誤停掉這一幀的收件（審查小事 3）；`ReceiveNet` 每幀看到新的失敗就發 World 的訊息 `NetLinkFailed{failures, code, message}`（形狀照 `TimeDiscarded`），產品看得到。交給 runner 的部分在合併前核准時交使用者。
- **第 2 步加的登記**（有角色時）：
  - 兩端：Resource `NetWorldRole`（執行條件讀它）、ECS 元件 `PeerLedger`（Peer 實體上的帳本）、訊息 `NetLinkFailed`。
  - 伺服器：Resource `NetOutbox`、System `ReplicateOut`（`Main`／`Last`，在 `SendNet` 之前，`ExclusiveWorld`）、每種伺服器訊息一個 `CollectServerMessages:<名字>`（`Main`／`PostUpdate`）。
  - 3a：兩端 ECS 元件 `PeerControls`、每種輸入的 `PeerInput:<名字>` 與入口 `ApplyInput:<名字>`；伺服器 `ReceiveInputs`（`ExclusiveWorld`），Client `SendInputs`（`ExclusiveWorld`），有權威策略時都標 `OneEndOnly`。
  - Client：Resource `NetReplicaState`、訊息 `ServerWorldReplaced`、System `ApplyReplication`（`Main`／`First`，在 `ReceiveNet` 之後，`ExclusiveWorld`）、`NotifyReplica`（`Main`／`PreUpdate`）、每種伺服器訊息一個 `DeliverServerMessages:<名字>`（`Main`／`PreUpdate`）。
  - 有權威策略的 World：`ReplicateOut`、`ApplyReplication`、`DeliverServerMessages:*` 也標 `authority.OneEndOnly`（它們用 `ExclusiveWorld` 或寫遊戲的訊息，只在一端）。
  - 傳輸的通道由網路 Module 定：0 號 Ordered Reliable（結構更新、確認、伺服器訊息、resync）、1 號 Unreliable（值）。
  - Client 角色的 World 選了回滾策略是 K3（第 13 節）。第 4 步之前的 `NT-E5`（「第 4 步才有」）已拿掉。K3 上會回滾、上 wire 的值不是 Resource 本身的複製 Resource 是 `NT-E6`（第 13 節；PR-2 設計審查 B2）。

## 8. 複製（宣告 D-4、D-5、D-13、C-MS1）

**登記**（兩端照同樣的順序寫；作者自己登記元件與訊息型別）：

```cpp
Net::NetEntry net(setup);
net.Replicate<Position>("Position", PositionCodec());      // 元件；編碼函式逐欄寫
net.ServerMessage<DoorOpened>("DoorOpened", DoorOpenedCodec());
```

- 元件、伺服器訊息、Resource（第 12 節，Q8 (b)）。
- 編碼（`Codec<T>`：`encode`、`decode`、`version`）：數值與 enum 有預設；其他型別由作者給，**沒有位元組照抄**（padding 不上 wire）。`NetWriter`／`NetReader` 逐欄寫小端序。
- wire id＝登記的順序；協議雜湊＝名字、順序、編碼版本的雜湊（`NetRegistry::Hash`），取代 1b 的空登記雜湊，握手照舊比（兩端登記不同 → `ProtocolMismatch`）。

**送**（伺服器的 `ReplicateOut`，每個 tick、每個 Peer；同一個 tick 不送第二次）：

- **結構更新**：Peer 的帳本記著這個 Client 有哪些實體、每個實體有哪些複製元件（遮罩）。和 World 現在的比：多出來的 → 生成，少了的 → 刪除，遮罩不同 → 加減元件。一則、Ordered Reliable、整則一起套用。結構有變的實體，它的全部複製元件的值併進這一則。沒有變化就不送。連線收下了才改帳本：可靠的送件被拒（佇列滿、太大）時帳本不動、下一個 tick 再做一次，計 `PeerLedger::reliableRejected`（審查 S3）。
- **伺服器訊息**：收集到的（`CollectServerMessages`，用自己的游標讀 World 的 `Message<M>`），在結構更新之後、同一條 Ordered Reliable 送；帶它要的結構 tick；可以只送給一個 Peer（`recipient`）。取不取出 `NetOutbox` 看整個 World 一份的「上次送出訊息的 tick」（`NetOutbox::sentTick`）：沒有跑固定步的那一幀不取，訊息留到下一個 tick（審查 B2）；那一幀新連上的 Peer 照樣送自己的結構與值，但不帶訊息，訊息在下一個 tick 和其他 Peer 一起到（窄複查 R1）。被連線拒絕的計 `reliableRejected`（那一則不重送）。
- **要的結構 tick 的寫法**：wire 上是「結構 tick＋1」，0＝還沒有結構更新；tick 0 的結構和其他 tick 一樣（審查 S1）。
- **值**：帳本裡每個實體記著 Client 上次確認的快照的 World 戳記；元件的變更戳記（Kernel 的 `ReadColumn`）比它新的，送最新的值。Unreliable，分成數則，每則帶 tick、「這個 tick 共幾則」、要的結構 tick；一個實體的資料不拆開（一則約 1100 bytes，放得進一個不分片的封包）。沒有變化也送一則空的（C-RP4）。
- **一個 tick 編碼一次**（3c）：快照（實體依 id 排序、每個元件一格的變更戳記）每 tick 取一次；一個值第一次有 Peer 要送時編碼，同一個 tick 的其他 Peer 用同樣的位元組（`ReplicateOut` 執行中 World 不變，wire 上的位元組和各自編碼相同）。費用與量法見 dev_log 2026-11-23 step3c（判定 G-12）。
- **確認**（應用層）：Client 處理完一則值訊息才回確認（Ordered Reliable）；伺服器收到才把那則帶的實體的戳記前推。封包層的 ack 不前推帳本。帳本只決定送什麼，不是遊戲狀態的副本。

**收**（Client 的 `ApplyReplication`，K1、K2）：

- 結構更新照順序整則套用：生成用 `SpawnWithId`（同號，D-4），加元件用 `Insert`，改值用 `Modify`（會打上變更戳記，遊戲的 `Changed<T>` 讀得到），刪除用 `Despawn`。
- 值等它要的結構 tick 套用後才寫；比已經寫過的（同一個實體、同一個元件）舊的不寫（新的蓋舊的）。
- **值相同就不寫**（3a，CCR-8）：寫之前用那個型別的編碼函式把現值編碼，和收到的位元組比，相同就不 `Modify`、不打變更戳記（`NetReplicaState::unchangedNotWritten` 計數）。值訊息送的是「自上次確認以來的最新值」，確認還沒到時會重送同一個值；不比就寫的話，遊戲的 `Changed<T>` 分不出伺服器真的換了值。比位元組，所以 `T` 不必有 `==`。
- 伺服器訊息等它要的結構 tick 套用後，由 `DeliverServerMessages` 寫進 World 的 `Message<M>`；不算進確認點。
- **D-7 的兩個條件**（K1 也記，不寫回滾的入口）：第 C 個 tick 的值訊息全部處理過（每則都要等它要的結構套用）。`NetReplicaState::latestConfirmed` 是最新的這種 C，`maxAppliedTick` 是寫進 World 的最新的值或結構的 tick。
- 丟棄有理由、有計數（`ReplicaDrop`）：舊世代、舊 epoch、讀不了、`SpawnWithId` 被拒、找不到對象（實體或元件已不在；結構更新也算，審查 S4）、比寫過的舊。
- 伺服器收到 Client 的東西也一樣（審查 S4、小事 2）：確認、resync、輸入訊息讀不了或種類不對是 `Malformed`，確認與輸入的世代不是現在的是 `StaleGeneration`，確認的 epoch 不對是 `StaleEpoch`，計在那個 Peer 的 `PeerLedger::drops`。

**World 世代與 resync**：

- 伺服器每次建 World 時世代加一，寫進標頭與複製的每則訊息。換 World 後，`ReceiveNet` 替已連上的連線重建 Peer 實體，帳本從空開始（整份重送）。
- Client 收到新世代：丟掉舊的進度，發 `ServerWorldReplaced{generation}`；換不換自己的 World 由產品決定。舊世代的訊息丟掉並計數。
- Client 的 World 被換掉（第二個以上的 Client World）：新 World 送 resync，伺服器把那個 Peer 的帳本清空、整份重送；舊 epoch 的訊息在 Client 丟掉並計數。epoch＝`NetService` 數到的第幾個 Client World 減一，每個 Client World 遞增，Client 永不從訊息拿（審查 B1、窄複查 R2）。epoch 跟連線走：握手的請求帶當時的 epoch，伺服器的連線記著它（resync 時更新），伺服器換 World 時新帳本從它開始。新連線為現在的 World 而開時不送 resync（重連不會送兩次整份）；連線是為更早的 World 開的才送。

**只在一端跑的邏輯**（D-13）：`OnServer()`、`OnClient()` 是讀 `NetWorldRole` 的執行條件。兩端登記同一個 System 加條件，一致性指紋相同，另一端不跑。只能用在有角色的 World。

**範圍的限界**：

- 每個 tick 送一次：一幀跑了好幾個 tick 時只送最後一個；沒有固定步、tick 不動的 World 不複製。
  - tick 不動的期間（沒有固定步、伺服器長時間暫停），作者發的伺服器訊息留在 `NetOutbox` 裡、只進不出，恢復時一次送出，可能超過可靠佇列而被拒（有計數）。上限等有點名的情境再定（PR-1 窄複查之後的確認，小事 1）。
- 「複製的實體」＝至少有一個複製元件的實體。全部複製元件都拿掉的實體，在 Client 上被刪掉。Peer 實體不複製。
- 最多 64 種複製元件（遮罩是 64 位元）。
- 換 World 之間 Client 仍套用新世代的訊息到舊 World；產品沒換 World 時，同號的實體會被 `SpawnWithId` 拒絕（計數）。
- tick 在 wire 上截成 u32，各處直接比大小、不處理回繞：64 Hz 約 2.1 年回繞一次，一個 World 連續跑這麼久不在範圍內（審查小事 7）。
- 超過 `maxMessageBytes`（16 MiB）的結構更新每個 tick 被拒、只計 `reliableRejected`，不拆分、不斷線（審查 S3）。點名的規模（約 400 個實體）是幾十 KB；超過時見 RC-NT8。
- 父子關係（Kernel 的 `ChildOf`）不在本線：`ChildOf` 指向不存在的實體時不成立，複製時父實體比子實體晚到要由網路自己等（lightyear 的 `PendingChildOf`）。記成之後網路複製那批的待辦（KR 卡交來的 K8，2026-11-23）。

## 9. 輸入與重連（宣告 D-11、C-IN1～3、D-8；3a）

**登記**（兩端一樣）：

```cpp
Net::InputOptions<Move> options;
options.codec = MoveCodec();
options.held = [](Move& m) { m.fire = 0; };   // 沿用上一格時清掉單次觸發的欄位
Net::NetEntry(setup).Input<Move>("Move", options);
```

- `T` 是玩家實體上的元件，兩端相同；作者自己登記它（判定的輸入用 `authority.Component<T>(name, {.input = true})`，回滾策略的 World 由權威代為登記元件的輸入紀錄）。
- 協議雜湊含輸入（名字、編碼版本、冗餘份數）。
- Peer 實體上：`PeerInput<T>`（按 tick 的緩衝、計數）與 `PeerControls{entity}`（控制的實體，由產品寫；網路不認得 Player）。

**入口 System** `ApplyInput:<名字>`（`FixedUpdate`／`First`）：兩端同名、同存取、同掛載，所以一致性指紋相同。第 n 步把每個 Peer 的第 n 格寫進它控制的實體上的 `T`；沒有那一格就沿用上一格（`InputOptions::held` 清掉單次觸發的欄位），計數「缺」（`missing`）。有權威策略的 World 經 `authority.Ingress`（重跑時不跑，回滾寫回紀錄），K1 直接 `AddSystem`。

**Client**：

- 產品把本機的輸入放進自己 Peer 的緩衝：`Net::LocalInput<T>(world)->Set(tick, value, view)`；`tick` 是 Match 的 tick。
- `SendInputs`（`Main`／`Last`，在 `SendNet` 之前）每 tick 送一則：`end_tick`（緩衝最新的一格）＋最後 N 格，Unreliable。N 預設 5（lightyear `crates/inputs/inputs/src/config.rs:16-19、50` 的 `packet_redundancy`）。
- 每格帶**畫面的時間** `ViewTime{tick, overstep}`：玩家按下時畫面看到的 Match tick 與往下一個 tick 的小數，小數一個 byte（lightyear 的 `Overstep`：內部 U0F16，上 wire 是 U0F8，`crates/core/core/src/time.rs:25-33、90-104`；它的註解說一個 byte 的量化誤差約 0.2%）。

**Match**：`ReceiveInputs`（`Main`／`First`，在 `ReceiveNet` 之後）替每個 Peer 備好緩衝（從連上的第一幀起，入口跑的每一步都算進消耗或缺），把輸入訊息填進去：

- 收的窗＝`(已消耗的 tick, 現在的 tick + L]`，L 預設 64（回滾的 `InputHistoryLength::Long`；lightyear 收 ±64）。已消耗的（`late`）、窗外的（`outsideWindow`）丟掉並各自計數。還沒消耗過（產品還沒給 `PeerControls`）時，被現在的 tick 超過、沒人消耗的格修剪掉，計 `expired`（審查小事 1）。
- 讀不了的輸入訊息、別的世代的輸入（Client 填它知道的最新世代，還沒收到複製時用接受帶的世代）丟掉並計在 Peer 的 `PeerLedger::drops`（審查 S4、小事 2）。
- 每個（Peer, tick）只收一次（C-IN1）：重複的計 `duplicates`、不覆蓋。
- **單次觸發**（C-IN3）：作者把觸發放在輸入元件的欄位裡。冗餘的幾格裡任一格帶到，就算那一格（每格只收一次，所以只觸發一次）；那一格始終沒到時沿用上一格，`held` 清掉觸發，不會再觸發一次。系譜：Unity 的 `InputEvent`（掉包也不漏、不重複的單次事件）、lightyear 的 `ActionState` 的 just pressed。
- 件數：收到的格＝收下＋重複＋已消耗＋窗外；收下＝消耗＋過期＋還在前面的（有 `PeerControls` 時過期是 0）；入口跑的步＝消耗＋缺。

**重連**（D-8）：

- Client 斷線後網路發 `PeerDisconnected`，不替產品換 World。產品換一個新的 Client World（`ReplaceWorld`），新 World 的第一幀照它的 `NetConnectTarget` 用同一個使用者 id 重新握手：新連線、新 Peer、Match 重送整份（新 World 也送 resync）。〔`v8-arch-gyo2-net-connect-target`，使用者 2026-11-25「選(1)，socket延後開」，[事前宣告](net-connect-target-declaration.zh-Hant.md)；**最終文字在合併前核准**〕
- 斷掉的 session 還沒送完的 Disconnect 先送，再開新的握手。
- 握手的請求帶 salt：同一個 endpoint 帶更大的 salt 的回應是新的握手，Match 先結束舊的連線（`ClosedByRemote`）再配新的連線識別；同一個 salt 是重送的回應，回同一個連線識別；更小的 salt 是舊握手晚到的回應，丟掉並計 `Unexpected`，現有的連線不動（審查 S2 (a)）。Client 只採用帶自己 salt 的接受，別的計 `Unexpected`（S2 (b)）。系譜：netcode.io 的 connect token 帶建立時間、伺服器不接受過期的（時間決定新舊）；這裡只比大小、不設期限。
- 舊連線還在飛的 datagram：新連線連上後是連線識別不符（`ConnectionMismatch`），連上之前是 `UnknownSource`，都計數；已斷的 session 收到的也計 `UnknownSource`。
- **tick 對齊**：3b 的對時（第 10 節）。Client 的入口與送件以對時後的自己的 `Tick` 為鍵（3a 測試裡「讀 Match 的 tick 加提前量」的暫時做法已拿掉）。

## 10. 對時（宣告 D-9、第 18 節 (A)；3b）

Client 的 World（K1 也有）掛 `SyncClock`（`ExclusiveWorld<Writes<Tick, FixedTime, TimeSyncScale>>`，`Main`／`Last`，在 `SendInputs` 之前）；它登記進時間 Module 的 `TickWriters`（`AddTickWriter`），有權威策略時標 `OneEndOnly`。狀態在 Resource `ClockSync`（量到的誤差、倍率、跳的次數）。

- **量測**（牆鐘只在 IO 層）：
  - Match 的 tick 在每個封包的標頭；RTT 由封包確認的樣本推（1a 的 EWMA 1/12），jitter＝RTT 偏差的一半（偏差的 EWMA 1/6、第一個樣本的一半；lightyear `crates/core/sync/src/ping/estimator.rs`）。
  - **Match 現在第幾步**：照 lightyear 的 `RemoteTimeline`（`crates/core/sync/src/timeline/remote.rs:109-221`）。收到較新的封包時算 `raw＝它的 Match tick＋RTT/2÷步長－它到的時間÷步長`，平滑的位移 `offset` 第一次取 raw，之後以 EMA 追（權重隨 jitter 從 0.10（≤1 ms）降到 0.02（≥10 ms））；`remote＝現在÷步長＋offset`。
  - **目標**＝`remote＋（RTT/2＋4×jitter）÷步長＋1＋1＋1`：lightyear 的輸入時間線（`crates/core/sync/src/timeline/input.rs:628-629`）：網路延遲、jitter 邊際（4×jitter＋1 tick）、輸入自己的一個 tick、誤差邊際 1 tick（`SyncConfig` 的預設，`sync.rs:210-220`）。
- **調整**（誤差＝目標－（`Tick`＋餘數÷步長））：
  - 剛連上（握手的 Match tick）或 |誤差| > 10 tick：`JumpFixedSteps(round(誤差))`。
  - |誤差| > 1 tick 連續 3 次同方向：倍率 1.05（落後）或 1/1.05（超前）。
  - |誤差| ≤ 1 tick（死區＝lightyear 的 `error_margin`，也就是目標裡加的那一個 tick）：倍率 1。
  - 倍率寫進 `TimeSyncScale`，`AdvanceTime` 乘上產品的倍率。lightyear 的增益隨誤差加大（`1＋0.05×誤差÷10×2`）；這裡照主會話的卡片用固定的 1.05。
- 跳之後，回滾照既有的規則（確認點早於第一筆記錄就放棄）；K3 見第 13 節。

## 11. ECS 元件與 W-8（3b 修正）

網路登記成 ECS 元件的型別（`Peer`、`PeerLedger`、`PeerControls`、`PeerInput<T>`）要 nothrow 可搬移（Kernel spec W-8）。MSVC 的 `std::map`、`std::deque`、`std::unordered_map` 的搬移建構不是 noexcept（libc++、libstdc++ 是），所以元件裡的容器一律用 `std::vector` 或排序的 `std::vector` 的 `FlatMap`（`FlatMap.hpp`）。每個元件的定義旁有 `static_assert(kStorable<…>)`；macOS 與 Linux 驗不出 MSVC 的這一點，L1 的 windows-x64 是實際的守衛。Resource（`NetReplicaState`、`NetWorldLink`）沒有這個斷言，照舊。

## 12. Resource 的複製（宣告 Q8 (b)；第 2 步的補完）

```cpp
net.ReplicateResource<Score>("Score", {ScoreCodec()});                  // 值＝Resource 本身，預設的 apply
N::NetEntry::ResourceOptions<StateMachine<Weather>, Weather> weather;    // 值是 Weather，不是封閉的 StateMachine
weather.read  = [](const StateMachine<Weather>& m) { return m.Current(); };
weather.apply = &AdoptByRequestSilent<Weather>;                          // 或 AdoptByRequest<Weather>
net.ReplicateResource<StateMachine<Weather>, Weather>("Weather", weather);
```

- **登記**：`ReplicateResource<R, V = R>(名字, {codec, read, apply})`。`R` 是作者在兩端登記的 Resource；`V` 是上 wire 的值。`V` 就是 `R` 時 `read`、`apply` 可以不給。`V` 不是 `R` 時（例：狀態機的值型別封閉、Client 造不出 `StateMachine<S>`）兩個都要給（斷言）。wire id＝登記順序；協議雜湊加 `r:名字:版本`，雜湊的版本字串升到 `/3`（值訊息多了一段，見下）。
- **送**（伺服器的 `ReplicateOut`）：每 tick 讀每個登記的 Resource 的變更戳記（Kernel 的 `ExclusiveWorld::ResourceStamp<R>()`）與值（`read`；`nullopt` 或 Resource 不在就不送），所有 Peer 共用。Peer 的帳本 `PeerLedger::resources` 記每個 Resource 上次確認的 World 戳記（和實體的 `confirmedStamp` 同一個尺度）；戳記比它新、或還沒確認過，就送。
- **wire**：每則值訊息在 tick、結構 tick、第幾則、共幾則之後，先放 `u8` Resource 個數與每個（`u16` wire id、`u32` 長度、值），再放實體。Resource 只放在第 0 則（算進那一則的大小），其他則的個數是 0。
- **確認**：和實體同一條。Client 處理完那一則才回確認；伺服器收到時把那一則帶的 Resource 的確認戳記前推（`PendingPart::resources`）。
- **收**（Client 的 `ApplyReplication`，K1、K2）：和實體的值一樣，等那一則要的結構 tick 套用後才寫；比寫過的 tick 舊就丟（`StaleValue`）。寫用登記的 `apply`：
  - 預設（`V`＝`R`）：用 codec 把現值編碼、和收到的位元組比，相同就不寫（CCR-8，`resourceValuesUnchanged` 計數），不同就 `Modify`，Resource 不在就 `SetResource`。
  - 作者給的 `apply`：`Kernel::SystemResult(Kernel::ExclusiveWorld<>&, const V&)`，回 Err 時 World 照 Kernel 的錯誤停。作者的 `apply` 也先比、相同就不寫（CCR-8）。狀態機的兩個套用函式（`AdoptByRequest`、`AdoptByRequestSilent`，狀態機頁第 2 節）照這個形狀，經要求換值，生效在狀態的套用 System（預設 `PreUpdate` 之後），所以收到與生效在同一幀，比它早跑的 System 看到舊值。
  - K3：登記成可回滾的 Resource（值就是 Resource 本身時）寫回滾的入口 `ResourceValue`，其他照 `apply`（第 13 節）。
- **狀態機與網路不互相依賴**：網路不連狀態機，狀態機只用 Kernel 的型別（狀態機 DI 8 的連結集只有 Kernel 與編排層）；產品在自己的 `build` 把兩者接起來。
- **時間的 Resource 不複製**：它們每幀被寫，`Changed()` 恆為真，戳記每 tick 都新；每端各自走自己的時間（D-9）。作者不登記它們；不加守衛。
- **計數**：`PeerLedger::resourceValuesSent`；`NetReplicaState::resourceValuesApplied`、`resourceValuesUnchanged`、`resourcesWritten`（每個 wire id 寫過的最新 tick）。

## 13. 模擬又回滾的 Client（K3；宣告 D-5、D-7，第 4 步）

〔實作，合併前核准定稿〕Client 角色、選了回滾策略的 World 是 K3。網路在準備 World 時，照回滾 Module 的 `RollbackRegistry` 記下哪些複製的元件、Resource 會回滾（`NetWorldLink::rollbackComponents`、`rollbackResources`），以及策略的 `DefaultInputLength()`（官方策略＝深度）。

- **會回滾的元件**：值寫 `RollbackInbox::Value`、拿掉寫 `Removed`，tick 是那一則的 tick；**不在 `First` 直接加減**（回滾 R-2 前提 5）。
- **伺服器生成、刪除的實體**：寫 `Entered`／`Exited`（回滾 R-8 的明說，網路 PR-2 閘口 1 的 A）。網路不在 `First` 生成或刪除實體：實體在確認點的回滾裡用同一個編號生成、照入口的值放回；之後的回滾不會刪掉它（記下的存活集合沒有它也一樣）。退出的實體在確認點的回滾裡刪掉。
  - 〔實作，PR-2 設計審查 B1〕再看到的實體（入口還記得它）：生成是整份，所以它沒帶的會回滾的元件寫 `Removed`（隱藏期間被拿掉的；入口裡較舊的值不會把它放回）。
- **不會回滾的複製元件**：直接寫（同 K1）。實體還不在 World 時（等確認點的回滾放回）先留著（`NetReplicaState::waitingDirect`，每個 wire 留最新的），實體出現後的那一幀寫進去。〔實作，PR-2 設計審查 S1、B1〕
  - 生成帶來的值一律先留著，等回滾在 ≥ 生成 tick 的確認點決定過、實體在 World 裡才寫：之前那份實體（退出還沒在確認點生效）可能被回到退出與進入之間的回滾刪掉。留著的期間，這個實體後來的值也留著。
  - 生成是整份：寫的時候，不在生成清單裡的不回滾元件從實體上拿掉（之前那份留下的）。
  - 拿掉不回滾的元件時，留著的那一個 wire 一併丟掉，之後不會寫回去。
- **複製的 Resource**：會回滾、而且上 wire 的值就是 Resource 本身的，寫 `ResourceValue`（閘口 1 的 F）；不回滾的照第 12 節的 `apply`。會回滾、上 wire 的值不是 Resource 本身（`ReplicateResource<R, V>`，V ≠ R）的，`Check` 回 `NT-E6`、不建 World（PR-2 設計審查 B2；使用者 2026-11-23 選 U2 (a)：一直拒絕）。`apply` 在 `First` 直接寫會回滾的資料，下一次回滾就把它蓋掉（R-2 前提 5）。
- **Client 自己的 Peer 實體**〔實作，`v8-product-pvp-port` 2026-11-24〕：`ReceiveNet` 在步外生成它；確認點早於連上的那一步時，第一次回滾會把它刪掉（PvP 移植時發現：伺服器連上時已有的實體讓那次回滾發生）。K3 的 World 多一個 System `KeepPeerInBeing`（回滾的決定 stage、在 `RunRollback` 之前，`ExclusiveWorld`，標 `OneEndOnly`），替它加回滾 Module 的 `KeepThroughRollback`（回滾頁第 2 節）。守衛：`net.k3`「the client's own Peer entity outlives the rollbacks to points before it connected」；突變 `nt-k3-peer-not-kept`。
- **確認點**（D-7）：第 C 個 tick 的值訊息全部處理過、而且它們要求的結構更新已套用（`NetReplicaState::latestConfirmed`）時，寫 `ConfirmedThrough(C)`。C 只落在伺服器送了值的 tick；伺服器一幀跑好幾步時只送最後一步（第 20 節 U4），C 跳著前進（閘口 1 的 B）。伺服器訊息不算。
- **晚到的值**：比 C 舊的照樣寫進入口（MC-1：回滾在 C 用「C 當時或之前最後的值」）；比 C 還舊超過深度的丟掉，計 `ReplicaDrop::BeyondDepth`。K1、K2 的「比寫過的舊就不寫」不用在 K3 的回滾元件上。
- **計數**：`NetReplicaState::inboxValues`、`inboxRemovals`、`inboxEntries`、`inboxExits`、`inboxConfirmed`；`entriesWhileAlive`（生成到的時候之前那份還在 World 裡）。回滾端的 `RollbackHistory::rollbacksForEntries`：明說進入的實體讓存活集合不同而回滾的次數（使用者 2026-11-23 選 U3：計數、不量 CPU）。
- **範圍的限界**：
  - 伺服器換了 World（新的世代、tick 從頭算）時，入口的確認點不會往回走；K3 的 Client 要跟著換 World（`ServerWorldReplaced`，第 8 節）。
  - 實體出現在確認點之後（約一個來回），不在伺服器生成的那一 tick 就出現；之後由預測補到現在。
  - 遠端玩家的輸入本線不送（宣告 Q7），Client 上它們的輸入元件是預設值。
  - 〔PR-2 設計審查 M7〕伺服器的生成、刪除與會回滾的值只經回滾放進 World。回滾模式是 `Check` 或 `Disabled` 時永遠不放回，Client 的 World 只剩自己的預測。
  - 〔PR-2 設計審查 M4〕`BeyondDepth` 用策略的 `DefaultInputLength()`（輸入紀錄的長度）代替回滾深度：官方策略兩者相同，作者的策略不一定。回滾目前沒有查深度的 API，這一步不加；入口本來就會被較新的條目蓋過、修剪也會整理。
  - 〔U2 (a) 的待辦〕K3 上 V ≠ R 的會回滾 Resource（例如狀態機）一直拒絕。有點名的功能需要時，由狀態機 Module 提供「伺服器的值變成 R」的轉換給網路用（會碰狀態機 DI 8：網路與狀態機互不依賴），那時另開。
  - 〔PR-2 設計審查 S1〕生成帶來的不回滾元件，在回滾於 ≥ 生成 tick 的確認點決定過之後才寫（上一幀寫的確認點，回滾在同一幀的決定階段用它）。放棄的回滾（超過深度）不放回實體時，它們一直留著。
  - 〔PR-2 設計審查 S6〕入口忘掉一個退出的實體之後，比退出更早的值才晚到時，入口會再為它留一筆（MC-1 照收）；數量受晚到的值限制。
- **守衛**：G-10（`net.k3`：確定性的 2D 樣本，最後 80 步輸入是預設值；Client 第 P 步的 `Digest` 等於 Match 的；網路生成的實體不被回滾刪掉；件數）、伺服器一幀三步、權威刪掉整個實體；PR-2 設計審查：`net.k3` 的「hidden and seen again」（B1、S1 a、權威拿掉會回滾的元件、K3＋可見性、不回滾元件的值等實體與加上）、「with losses」（B1 的入口還記得的情況；種子 1、18、24）、「S1 b made to happen」兩案（確定性的 Link，確認點落在 [退出, 進入)；窄複查 N1）、`net.replication` 的 `NT-E6`；突變 `nt-k3-*`。

## 14. 相關性與優先度（宣告 C-VIS1、C-VIS2、F4、F7、F19，第 5 步）

〔實作，合併前核准定稿〕

- **每個 Peer 一份可見性**：Peer 實體上的 `PeerVisibility`（伺服器）。規則由作者給：作者的 System 每 tick 照自己的規則填它，形狀照 replicon 的 `ClientVisibility`（伺服器每個 client 一份、由產品填）。
  - `everything`（預設 true＝看得到全部）、`entities`（看得到的實體，遞增）、`Show`／`Hide`、`Sees`。
  - 沒有 `PeerVisibility` 的 Peer 看得到全部（第 4 步以前的行為）。
- **元件範圍（私人資料，C-VIS2）**：`Replicate<T>(名字, codec, {.privateComponent = true})` 的元件只送給 `PeerVisibility::ShowPrivate<T>(entity)` 的 Peer（replicon 的可見性範圍到元件）。其他 Peer 看得到實體也收不到這個元件。
- **失去可見＝對那個 Peer 刪除**（只用 `WhileVisible`，replicon 的 `ScopeLifetime::WhileVisible`）：結構更新送刪除；再看到時是生成，用同一個編號整份送。Client 端：K1、K2 生成、刪除；K3 寫 `Entered`／`Exited`（第 13 節）。
  - K3 不用 `NotSent` 表示看不到：看不到的實體在 Client 上不該存在（宣告選的 `WhileVisible`），`NotSent` 只表示「這份沒帶」、實體還在。
- **優先度**：`NetEntry::ValueBytesPerTick(bytes)`（預設 0＝不限）給每個 Peer 每 tick 值的位元組上限。放不下時取 lightyear 的累積優先度的概念，做法不同（PR-2 設計審查 M3 更正出處）：lightyear 的累積在可靠通道的待送訊息上（`crates/transport/transport/src/channel/send_reliable.rs` 第 37、153 行：每次 `+= base_priority × multiplier`，到 ack 才移除；`packet/priority_manager.rs` 是排序與 token bucket）；這裡在實體上累積、送出就歸零，下一 tick 用最新的值重新編碼：
  - 每個有值要送的實體，分數＝累積的優先度＋權重（實體上的 `NetPriority::weight`，沒有時 1），由高到低放進上限；第一個一定放。
  - 放不下的累積起來（`PeerLedger::waiting`），送出後歸零；實體對這個 Peer 刪除、整份隨結構送出、或 Client 要求重送全部（Resync）時也歸零（PR-2 設計審查 M1）。
  - 值訊息照舊分則（一則約 1100 bytes）。
  - 退掉的：replicon 的 `priority × (tick − 上次確認的 tick) ≥ 1`。它限的是送的頻率，不是「一個 tick 放不下時分到後面」。
- **延後的實體告訴 Client**：值訊息的第 0 則在 Resource 之後多一段（`u16` 個數＋實體編號），其他則的個數是 0（每則多 2 bytes）。K3 對它們寫 `RollbackInbox::NotSent(entity, tick)`：在這個 C 不比對、不推成「沒變」（回滾 R-8 的第三態）；K1、K2 只是晚點收到。
- **計數**：`PeerLedger::valuesDeferred`、`waitedTicks`、`longestWait`；`NetReplicaState::inboxNotSent`。
- **範圍的限界**：
  - 〔PR-2 設計審查 M6〕有了可見性之後，伺服器訊息（事件）可能指向這個 Peer 看不到、Client 上不存在的實體；G-5 (3)「事件交出時實體已存在」只在看得到全部時成立。事件的收件者與內容由作者決定（`ServerMessage` 的 recipient）。
  - 〔PR-2 設計審查 M8〕延後清單（每筆 8 bytes）只放在第 0 則，不算進預算。大量延後時第 0 則變大、可能分片，整則遺失的機會變高，確認點較常停。有點名的情境延後超過約 100 個實體時再看（例如改成「這個 tick 全部沒帶」）。
  - 頻寬判定的情境是合成的（見 dev_log）。
- **頻寬判定**（閘口 2 的 2.3，事前宣告；使用者選 (b) ≤ 512 kbps）：`net.bandwidth`（ctest 的 `cpu` 標籤，算位元組、不計時）。數字見 dev_log 2026-11-23 step5。
- **守衛**：G-11（`net.visibility`，PR-2 設計審查加「整份重送」的不變元件；`net.visibility` 的私人元件不再顯示一案；`net.k3` 的 NotSent 一案）、頻寬判定；突變 `nt-vis-*`、`nt-private-*`、`nt-budget-ignored`、`nt-budget-first-not-sent`、`nt-priority-not-accumulated`、`nt-priority-weight-ignored`、`nt-deferred-not-notsent`。G-11 另有：私人元件以實體為單位（E2 帶不給人看的 `Secret`）、沒有 `PeerVisibility` 的 Peer（第三個 Client）收不到私人元件；`net.k3` 另有：預算比一個實體小時每 tick 仍送一個、權重大的等得少。

## Design Invariants

**草稿**（宣告第 12 節）。最終文字在合併前核准定稿。守衛欄寫 1a 已有的；綁實作方式的守衛（`gyo_expect_links` 等）在 scenario「伺服器＋兩個 Client」通過之後立（AGENTS §3「ガードを立てる時期」；宣告第 8 節）。

<a id="nt-1"></a>
**NT-1 職責。** 網路 Module 的職責是：(1) 連線（握手、存活、斷線理由）；(2) 傳輸（標頭、ack、通道、分片）；(3) 複製（登記、結構與值、確認、帳本、可見性）；(4) 輸入的收送與緩衝、入口；(5) 伺服器訊息的收送；(6) 對時的量測與給時間 Module 的輸入；(7) 丟棄的計數。不存判定資料的副本、不回滾、不認得 Player、不認得特定產品。1a 有 (2)、(7)；1b 有 (1)（握手、存活、斷線理由，第 6 節）；第 2 步有 (3) 的登記、結構與值、確認、帳本（可見性在第 5 步）與 (5)（第 8 節）；3a 有 (4) 的輸入的收送、緩衝、入口與 (1) 的重連（第 9 節）；3b 有 (6) 對時的量測與給時間 Module 的輸入（第 10 節）；Q8 (b) 在 (3) 加 Resource 的複製（第 12 節；實作，合併前核准定稿）；第 4 步在 (3) 加 K3 的寫法（第 13 節；實作，合併前核准定稿）；第 5 步在 (3) 加可見性、元件範圍、優先度（第 14 節；實作，合併前核准定稿）。守衛（第 5 步）：`net.visibility`、`net.bandwidth`、`net.k3` 的 NotSent 一案。守衛（第 4 步）：`net.k3` 三案；突變 `nt-k3-*`。守衛（Q8）：`net.resource`、`net.statesync`；突變 `nt-resource-*`。守衛（3b）：`net.clock` 兩案；突變 `nt-clock-*`。守衛：G-6（`net.input`）、G-9（`net.reconnect`）；突變 `nt-input-*`、`nt-held-fires-again`、`nt-reconnect-reuses-connection`。第 2 步的守衛：守衛：G-5（`net.replication`）、scenario（`net.scenario`）；突變 `nt-value-before-structure`、`nt-ledger-at-send`、`nt-empty-tick-not-sent`、`nt-ledger-not-advanced`、`nt-generation-unchecked`、`nt-removed-not-encoded`、`nt-resync-ignored`（`nt-k3-as-k2` 隨 NT-E5 拿掉在第 4 步退役，由 `nt-k3-as-k1` 接手）。1b 的守衛：G-3（`net.handshake`）、G-4（`net.liveness`）；突變 `nt-compare-*`、`nt-k1-compared`、`nt-challenge-unchecked`、`nt-admission-ignored`、`nt-request-min-size`、`nt-liveness-not-refreshed`、`nt-keepalive-with-data`、`nt-interrupt-at-threshold`、`nt-timeout-at-threshold`。

<a id="nt-2"></a>
**NT-2 一套內部協議。** Link 之上只有一種協議；Link 只有 bytes 與連線狀態。1a：`Link` 只有 `Send`、`Receive`；`UdpLink`、`MemoryLink` 換了上層不改（`net.transport` 用 `MemoryLink`、`net.link` 用 `UdpLink`，`Connection` 同一份）。

<a id="nt-3"></a>
**NT-3 冪等的鍵。** 每一筆帶宣告 D-4 表的鍵，重複收到不改結果。1a：可靠訊息的鍵（連線、通道、訊息序號），重複收到丟掉並計數；封包層以 packet id 去重。第 2 步：值以（World 世代、實體、元件、tick）為鍵，新的蓋舊的、比寫過的舊就不寫並計數；結構以（World 世代、結構 tick）在一條有序的通道上恰好一次。3a：輸入以（Peer, tick）為鍵，只收一次、不覆蓋；值相同就不寫（CCR-8）。Q8：Resource 的值以（World 世代、wire id、tick）為鍵，比寫過的舊就不寫並計數；預設的 apply 值相同就不寫（實作，合併前核准定稿）。守衛：G-2（`net.transport`）、G-5、G-6；突變 `nt-ordered-no-dedup`、`nt-packet-no-dedup`、`nt-stale-value-applied`、`nt-input-overwritten`、`nt-ccr8-write-same`。

<a id="nt-4"></a>
**NT-4 判定用的時間只有 tick。** 牆鐘只在 IO 層。3b：對時讀牆鐘只為量測，改 World 的只有時間 Module 的 `JumpFixedSteps` 與 `TimeSyncScale`（判定用的仍只有 tick）。1a：`Connection` 不讀時鐘，時間由呼叫者給（`NetTime`），只用來算 RTT、重送與分片逾時；標頭的 tick 照呼叫者給的寫。

<a id="nt-5"></a>
**NT-5 確認點。** 只在宣告 D-7 的兩個條件成立後寫 `ConfirmedThrough`。（第 4 步）

<a id="nt-6"></a>
**NT-6 不選就沒有**（G-1）。Product 沒有 `NetProductOptions` 就不建 `NetService`、不開 socket；World 沒有 `NetWorldOptions` 就不登記任何東西（System、Resource、元件、訊息都和不選網路 Module 時逐項相同）。守衛：`net.module`「without a role or settings the net Module adds nothing (G-1)」；突變 `nt-registers-without-role`。

<a id="nt-7"></a>
**NT-7 依賴。** 網路依賴權威、回滾、時間、entity_ids；它們都不依賴網路；外部組件只經 `core/os`。1b：登記檔 `depends=world_time;rollback;authority;entity_ids`，另連 `GYO::Base`、`GYO::OS`、`GYO::Time`、`GYO::Kernel`、`GYO::Orchestration`。守衛（`net.scenario` 通過後立，AGENTS §3）：`tests/common/net/CMakeLists.txt` 的 `gyo_expect_links(gyo_net …)`（連結集剛好是這五個與登記的四個 Module）；突變 `nt-links-more`。

<a id="nt-8"></a>
**NT-8 丟棄有理由、有計數。** 第 5 節的十一個理由（1a 九個，1b 加連線層的 `UnknownSource`、`Unexpected`）；`Connection`、`ClientSession`、`ServerHost`、`NetService` 沒有不計數的丟棄出口；第 2 步的複製另有六個理由（`ReplicaDrop`），Link 的失敗發 `NetLinkFailed`。守衛：G-2 的件數突き合わせ（`net.transport`）、`net.connection` 的各案；突變 `nt-timeout-uncounted`、`nt-connid-unchecked`。

<a id="nt-9"></a>
**NT-9 編號。** 伺服器配的實體在 Client 上用同一個編號；範圍由產品分開，握手時檢查不重疊（Q1 核准：兩端同號，照 LIM-7）。第 2 步：Client 用 `SpawnWithId` 生成伺服器配的編號；元件裡的 `EntityId` 原樣上 wire、不用對照表。守衛：G-5（實體編號集合與互指的元件逐項相同、`SpawnRejected` 為 0）。

<a id="nt-10"></a>
**NT-10 每收件者的狀態在 Peer 實體。** 帳本、可見性、輸入緩衝；不進判定、可重建。第 5 步加 `PeerVisibility`（作者填的可見性）與帳本的優先度累積。1b：每條連線一個 `Peer` 實體（伺服器每個 Client 一個、Client 一個），連上時 spawn、斷了 despawn，帶連線識別、使用者 id、丟棄計數；第 2 步加帳本 `PeerLedger`（Client 有的實體與元件遮罩、確認過的戳記、未確認的值訊息；Q8 加每個複製的 Resource 確認過的戳記）；3a 加輸入緩衝 `PeerInput<T>` 與控制的實體 `PeerControls`；可見性在第 5 步加。守衛：`net.module`「a client connects, both Worlds get a Peer, and closing ends the server's with ClosedByRemote」；突變 `nt-peer-not-despawned`。

### Replacement Criteria

**草稿**（宣告第 12 節，寫在積上去之前）。符合任一條時，停下來交使用者「修還是重建」。

- **RC-NT1**：職責清單（NT-1）要多一項，或判不出某樣東西屬網路、權威、回滾、時間哪一個。
- **RC-NT2**：有點名的產品在一個 World 的生命內用完產品分給伺服器的編號範圍。
- **RC-NT3**：要讓複製正確，必須在 Kernel 或編排層加網路專用的欄位、分支或觀察點。
- **RC-NT4**：每加一種 Link，上層（傳輸、複製）就要改。
- **RC-NT5**：同一個回避策（對映、去重、狀態的副本）要放進兩個以上的地方，或 Link、中繼開始需要遊戲的狀態才能正確。
- **RC-NT6**：G-8 的數字超過使用者選的頻寬預算，而且相關性與優先度都已用上。
- **RC-NT7**：每加一種要複製的元件，就要改網路 Module 的程式（不只是登記）。
- **RC-NT8**：點名的產品的結構更新超過 `maxMessageBytes`（現在每 tick 被拒、只計數，見第 8 節的限界）。
- **RC-NT9**（觀測項；使用者 2026-11-23 選 U3）：K3 上每個進入可見範圍的實體都讓確認點的回滾整段重跑（`RollbackHistory::rollbacksForEntries` 計數）。點名的產品裡這個成本超過計畫的預算時，檢討改成 lightyear 的做法（伺服器複製來的實體直接生成、帶確認過的歷史，不經整段重跑）。計數的守衛：`net.k3`「hidden and seen again」的 U3 檢查；突變 `rb-entry-rollbacks-not-counted`。
