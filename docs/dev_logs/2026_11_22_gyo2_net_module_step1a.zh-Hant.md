# 2026-11-22 網路 Module 第 1 步前半（1a）：傳輸層（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（第 0 步 3076ad79 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md)（修訂 2；使用者 2026-11-22 動工前核准「照建議，全部核准」，追問 Q4 後「維持，開始」）。
- **實作者**：Opus／high（宣告第 11 節第 1 步）。

## 為什麼

- 宣告第 11 節第 1 步的前半：不認得 World 的部分（D-2 傳輸層、D-3 的 Link、D-12 的丟棄計數）先做，判定 G-2。
- 登記單位 `NetModule`、Peer 實體、套用點、握手（G-3）、存活（G-4）、G-1 在 1b。

## 做了什麼

- **新 Module `engine/modules/net/`**（`GYO::Engine::Net`，target `GYO::Net`；link `GYO::Base`、`GYO::OS`）：
  - `NetFailure.hpp`：`NetFailureCode`（從 1 起，十個碼）、`ToString`、`NetFailure`、`NetResult`。
  - `Link.hpp`：`Endpoint`（不透明的 `u64`）、`ReceivedDatagram`、`Link`（`Send`、`Receive`）。
  - `UdpLink`：包 `OS::DatagramSocket`；`ToEndpoint`／`ToAddress`（IPv4 四個 octet＋port 放進低 48 位元）；OS 錯誤的對映。
  - `MemoryLink`：一對端點；每個方向的掉包率、重複率、延後（以 `Advance()` 的步數計）；splitmix64 固定種子；計數注入的掉包、重複、延後；注入時可觀察 datagram。
  - `PacketHeader`：24 bytes 的標頭（魔數 `'G' 'N'`、版本 1、種類、連線識別、World 世代、packet id、ack、ack bitfield、tick），小端序；`SequenceNewer`。
  - `Connection`：純的狀態機；兩種通道與優先度；Gaffer 式 ack；訊息層重送；Ordered Reliable 的序號、去重、照序交出；分片（Unreliable 缺片逾時整則丟）；AckOnly；`Close` 冗餘送 Disconnect；九個丟棄理由的計數（`ConnectionStats`、每通道 `ChannelStats`）；`Inspect` 不靠狀態讀一個 datagram。
  - `src/ByteCursor.hpp`：小端序的 span 讀寫（內部）。
- **登記檔** `config/modules.csv` 加 `net` 列：`library_only=yes`、`default=no`、`depends` 空（1b 改成登記單位）。
- **測試** `tests/common/net/`（`gyo_net_tests`，22 案；ctest `net.header`、`net.connection`、`net.transport`、`net.link`，標籤 `cpu`）：
  - `transport:` G-2 與 `MemoryLink` 的可重現性；
  - `header:` 往返、文件化的位元組、壞的標頭回錯誤、序號回繞、碼的名字；
  - `connection:` ack bitfield、確認後不重送／沒確認在延遲後重送、`max(1.5×RTT, 下限)`、AckOnly 不引發回 ack、連線識別不符、可靠片重複只交出一次、分片缺片不交出與逾時、可靠分片、優先度、標頭的 stamp、Close、`Send` 拒收、讀不了的 datagram；
  - `link:` `Endpoint` 的打包、UDP loopback 往返（帶寄件者）。
- **文件**：新頁 [`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md)（例子、位置與依賴、組成與設定的系譜、wire 格式、通道與確認、丟棄計數、Design Invariants 草稿 NT-1～NT-10、Replacement Criteria 草稿 RC-NT1～RC-NT7；標「最終文字在合併前核准定稿」）。

## 和宣告不同的地方

- **Link 沒有「連線的狀態」**：D-3 寫 Link 是「送、收、連線的狀態」。UDP 與記憶體 Link 都沒有連線，1a 的 `Link` 只有 `Send`、`Receive`。連線的狀態（存活、斷線理由）在 1b 的連線層；有連線的 Link（WebSocket 等）出現時再看要不要加。
- **錯誤型別叫 `NetFailure`**，不是卡片寫的 `NetError`：解碼失敗、socket 壞掉都是 E2（D-12），命名規範 N25 定 E2 的字尾是 `…Failure`。
- **D-12 的理由清單**：
  - 1a 有：太舊（`TooOld`：比收到的歷史還舊的封包）、重複（分成封包層 `DuplicatePacket` 與訊息層 `DuplicateMessage`）、解碼失敗、緩衝滿（分成拒收整個封包的 `ReliableWindowFull` 與 Unreliable 的 `BufferFull`）、分片逾時；另加 `ConnectionMismatch`（D-2）與 `SendRejected`（`Send` 拒收的訊息）。
  - 1a 沒有：窗外、已消耗（輸入，第 3 步）、協議不符、未知的來源（握手，1b）、World 世代不符（第 2 步）。
- **件數的突き合わせ用兩種單位**（G-2 的判定文字不變）：宣告寫「送的＝交出的＋各理由的丟棄計數＋注入的掉包數」。注入的掉包是 datagram，交出的是訊息，一個 datagram 帶多則訊息、可靠的會重送，所以照字面一個式子對不起來。測試在 datagram 與「片」兩種單位各列一式，每個方向、每條通道逐項相等（`net.zh-Hant.md` 第 5 節；測試檔開頭的註解）。為了對得起來：
  - `MemoryLink` 不重複它丟掉的 datagram（每個 datagram 只有一種下場）；
  - 測試以 `Connection::Inspect` 讀注入的掉包與重複各帶了幾片。
- **宣告沒寫、本卡判的**（L17 的範圍，揭露）：
  - 第一個 packet id 是 1：什麼都沒收到時 ack 欄是 0，id 0 要回繞一圈才會送出。
  - 一個封包只要有一片可靠訊息超出收方的窗，整個封包在使用前拒收、不確認：封包被確認就等於訊息被確認，收下之後就不能再丟。
  - Ordered Reliable 的收件佇列滿時不丟、留在窗裡等 `Pop`；Unreliable 的滿了就丟（`BufferFull`）。
  - 封包種類 16～19 保留給握手，1a 讀到是 `UnknownPacketKind`。

## 名字、常數與系譜

- 名字照命名規範：目錄 `engine/modules/net`、namespace `GYO::Engine::Net`（N17 從目錄推）、target `gyo_net`／`GYO::Net`（N14）、錯誤 `NetFailure`（N25）。`Connection`、`Link`、`UdpLink`、`MemoryLink`、`Endpoint`、`PacketHeader`、`PacketKind`、`DropReason`、`Stamp` 是本卡定的。
- 常數（`net.zh-Hant.md` 第 2 節的表）：MTU 1200、重送 `max(1.5×RTT, 下限)`（下限預設 0）、RTT 的 EWMA 1/12、分片逾時 3000 ms、訊息上限 16 MiB、斷線冗餘 10 個：都照 lightyear 736e1695，註解寫了檔名與行號。窗 256、收件佇列 1024、送件佇列 4096、重組 64 是本卡定的設定值。沒有用 master 的常數（P9）。
- RTT 用整數奈秒算（跨平台相同）。

## 驗證

- 本機（macOS x86_64，`core` preset）：
  - `gyo_net_tests` 22 案全過；
  - `ctest -R "net\.|os\.|cmake\.|naming"` 22 個全過；
  - 全部的 ctest 173 個全過（沒有無關的失敗）。
- **G-2 的數字**（種子 `0x5EED0002`；兩個方向都是掉包 20%、重複 10%、延後 25%（1～3 步）；A→B 送 Ordered Reliable 10 000 則與 Unreliable 10 000 則，B→A 送 Ordered Reliable 2 000 則；每 50 則一則 3000 bytes 的分片訊息；1187 輪排空）：
  - Ordered Reliable：A→B 交出 10 000 則、B→A 2 000 則，恰好一次、照順序。
  - Unreliable：交出 7 858 則，沒有重複；分片訊息交出 106 則，內容都完整。
  - A→B 的 datagram：送 2 773＋注入的重複 241 ＝ 收下 2 200＋`DuplicatePacket` 241＋注入的掉包 573（3 014）。
  - A→B 的 Ordered Reliable 片：送 15 033＋重複裡 1 311 ＝ 交出 10 400＋`DuplicatePacket` 1 311＋`DuplicateMessage` 1 510＋掉包裡 3 123（16 344）。
  - A→B 的 Unreliable 片：送 10 400＋重複裡 967 ＝ 交出 8 070＋`DuplicatePacket` 967＋`FragmentTimeout` 165＋掉包裡 2 165（11 367）。
  - B→A 的 datagram：1 178＋88 ＝ 940＋88＋238（1 266）；Ordered Reliable 片：2 965＋231 ＝ 2 080＋231＋267＋618（3 196）。
  - `TooOld`、`ReliableWindowFull`、`BufferFull`、`DecodeFailed`、`ConnectionMismatch`、`SendRejected` 都是 0。
- **突變**（新清單 [`scripts/net_mutations.json`](../architecture/target/v2/scripts/net_mutations.json)，12 個，`nt-`）：本機 **12／12 killed**。
  - `nt-ackbits-31`、`nt-ack-ignores-pieces`、`nt-resend-no-rtt`、`nt-ordered-no-dedup`、`nt-fragment-incomplete`、`nt-connid-unchecked`、`nt-timeout-uncounted`、`nt-ackonly-elicits`、`nt-packet-no-dedup`、`nt-magic`、`nt-version`、`nt-unknown-kind`。
  - `nt-unknown-kind` 第一次是 wrong_failure：測試對解碼成功的結果讀 `error()`，`GYO_ASSERT` 中止，訊息沒印出來。測試改成先看 `has_value()`（`CodeOf`，`connection:` 的四處同改），判定不變；重跑 killed。
- **§14.5 的確認**：12 個都在 `what` 寫了「只有它捕到的」、`guards` 指宣告的 D-2、D-4、D-12 或判定 G-2。`nt-magic`、`nt-version` 是格式常數，一個常數一個（README 基準 2）。加的數不超過「條文數 × 5」。沒加的：`UdpLink` 的錯誤對映（沒有可識別的失敗訊息能在 loopback 上重現）、Ordered Reliable 照序交出的單獨突變（G-2 的「照順序」判定與 `nt-ordered-no-dedup` 已涵蓋相鄰的誤差；等漏抓被觀測到再加）。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step1a/`，git 不追蹤）：`g2-transport.log`（G-2 的全部數字）、`ctest-net-os-cmake-naming.log`、`ctest-all.log`、`mutations.log`（含 `nt-unknown-kind` 的重跑）。

## 1b 要接的

- `net` 列改成登記單位（`NetModule`、`default=yes`、`depends=world_time;rollback;authority;entity_ids`），補 G-1。
- 握手（種類 16～19）、存活與斷線理由（G-3、G-4）在 `Connection` 之上的連線層；`Connection` 的連線識別由握手給。
- 丟棄計數交給 Peer 實體（Match）或 World 的 Resource（Client），交日誌或產品讀。
- `docs/architecture/README.md` 的 Module 清單與〈Design Invariants〉的連結：合併前核准、DI 定稿時一起加。
