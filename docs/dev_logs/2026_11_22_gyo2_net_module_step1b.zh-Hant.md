# 2026-11-22 網路 Module 第 1 步後半（1b）：握手、存活、登記單位、Peer 實體（v8-arch-gyo2-net-module）

- **Owner**：架構（GYO2 主會話「[GYO2][不良少年]新架構重構」開出的任務卡 `gyo2-net-module`）。
- **分支**：`claude/gyo2-net-module-da73cd`（1a 701dbe4f、主會話的 97c888bc 之後）。
- **事前宣告**：[`net-module-declaration.zh-Hant.md`](../architecture/target/v2/net-module-declaration.zh-Hant.md)（修訂 2；使用者 2026-11-22 動工前核准）D-3、D-4 的範圍檢查、D-6、D-8 的存活、D-10、D-12；判定 G-1、G-3、G-4、G-7。
- **實作者**：Opus／high。

## 做了什麼

- **連線層**（純的狀態機，時間由呼叫者給）：
  - `Handshake.hpp`：`DisconnectReason`（十個理由）、`IdRange`、`HandshakeFacts`、`ConnectRequest`、`ConnectAccepted`、`Rejection`、`SessionSettings`、`CompareFacts`；請求、接受、拒絕的編解碼；`Detail::ChallengeOf`（SHA-256 取前 64 位元）。
  - `ClientSession`：請求、回應每 `keepaliveInterval` 重送；接受、拒絕、`HandshakeTimeout`；連線→中斷→斷線（`Timeout`）；收到 Disconnect 是 `ClosedByRemote`；閒置時送 AckOnly 當 keepalive；`Close`。
  - `ServerHost`：無狀態的挑戰；回應的挑戰對了才比對、問產品（`Admission`）、配連線識別（splitmix64、非 0、不重複）；每個連線一條 `Connection`；逾時；keepalive；事件 `Connected`／`Disconnected`。
- **傳輸層的補充**：`PacketKind` 加 16～20（握手）與 `IsHandshake`；`Connection::KeepAlive`；`Connection` 讀到握手的種類是 `DecodeFailed`；`DropReason` 加連線層的 `UnknownSource`、`Unexpected`（共 11 個）。
- **`NetService`**（Product 一份的傳輸物件，O-15）：Link（給的或自己開的 UDP）＋連線層；`Pump`、`Flush`、`Close`；Link 的錯誤記下並計數；一個 Product 只綁一個有角色的 World。
- **`NetModule`**（登記單位）：
  - `Configure`：有 `NetProductOptions` 才建 `NetService`；UDP 開不了回報 `NT-E4`。
  - `Build`：有 `NetWorldOptions` 才登記 `Peer`、`NetWorldLink`、`PeerConnected`、`PeerDisconnected`、`ReceiveNet`（`Main`／`First`）、`SendNet`（`Main`／`Last`）；有權威策略時兩個 System 標 `authority.OneEndOnly`，K1 不碰權威。準備函式放 `NetWorldLink`、算指紋、把事實（步長、範圍、指紋）交給 `NetService`。
  - `Check`：`NT-E1`（有角色、沒有 Product 的設定）、`NT-E2`（角色不同）、`NT-E3`（第二個有角色的 World）。
- **登記檔** `net` 列改成登記單位：`library_only=no`、`default=yes`、`depends=world_time;rollback;authority;entity_ids`、`GYO::Engine::Net::NetModule`、`engine/net/NetModule.hpp`。CMake 另連 `GYO::Time`、`GYO::Kernel`、`GYO::Orchestration`。
- **測試** `tests/common/net/`（`gyo_net_tests` 41 案；ctest 加 `net.handshake`、`net.liveness`、`net.module`）：
  - `handshake:`（G-3）：相符的連上、接受帶的欄位；五項各改一項各回自己的理由；K1 不比指紋、Client 有指紋而伺服器沒有是 `FingerprintMismatch`；別的伺服器的挑戰 `ChallengeFailed`；產品拒絕 `Refused`；`HandshakeTimeout`；小於挑戰的請求不回。
  - `liveness:`（G-4）：雙向有資料時不送 keepalive、有資料的那一幀不附 keepalive；閒置時第一個 keepalive 在 110 ms（10 ms 一幀，超過 100 ms 的第一幀）、之後每 110 ms；1 s 整還是連線、多 1 ns 中斷；10 s 整還是中斷、多 1 ns `Timeout`；資料刷新存活、中斷回到連線；伺服器的逾時；`Close` 送 10 個 Disconnect、伺服器 `ClosedByRemote`。
  - `module:`：G-1（不選角色、不給設定時 System、Resource、元件、訊息逐項和不選網路 Module 相同、Product 一份的共用處沒有 `NetService`）；有角色時兩個 System 掛在 `Main`／`First`、`Last`；有角色沒設定 `NT-E1`；MemoryLink 的整合（連上 → 兩端都有 Peer、`PeerConnected` → Client `Close` → 伺服器 `PeerDisconnected(ClosedByRemote)`、Peer 被刪）；Client 不回應時伺服器 `Timeout`；權威策略：同樣的判定部分連上、不同的 `FingerprintMismatch`、K1 Client 連上有策略的伺服器；`NetWorldOptions::idRange` 重疊 `IdRangeOverlap`。
  - `link:`（G-7）：兩個 Product 在 UDP loopback 握手成功；步長不同的 Client 被拒（`StepLengthMismatch`）。
- **文件**：[`net.zh-Hant.md`](../architecture/target/v2/net.zh-Hant.md) 第 1、2 節改寫，第 3 節補握手的種類與 payload，新增第 6 節（握手與存活）、第 7 節（接線），NT-1、NT-6、NT-7、NT-8、NT-10 補 1b 的內容與守衛。

## 和宣告不同的地方

- **握手的種類是五個**：宣告第 4 節與 1a 只保留 16～19 四個（請求、挑戰、回應、接受）。拒絕要帶理由給 Client，另加 20 `Rejected`。
- **請求帶「網路協議版本」u32**，和標頭的 `kProtocolVersion`（分幀）分開：標頭的版本不同時整個 datagram 讀不了，回不了 `ProtocolMismatch`；所以要比對的版本放在請求的內容裡（`kNetProtocolVersion = 1`）。
- **keepalive 用 AckOnly**，不另設種類：AckOnly 本來就不引發回 ack（主會話的卡片寫「你判」）。
- **Socket 壞掉不回給 runner**：D-12 寫「socket 壞掉：E2 回給 runner」。收發在 System 裡，System 回 `Err` 會讓 World 壞掉（E1）。1b 把 Link 的錯誤記在 `NetService`（`LinkFailures`、`LastLinkFailure`），World 照跑，產品或 runner 從 Product 一份的共用處讀。怎麼交給 runner 是之後的事，合併前核准時給使用者看。
- **Client 的握手失敗**：沒有 Peer 實體，`PeerDisconnected` 的 `peer` 是 0。
- **World 世代**：標頭的世代 1b 一律寫 0（第 2 步做 `ReplaceWorld` 時才加一）。
- **伺服器沒有「中斷」狀態**：卡片只要 Client 的三個狀態；伺服器只有逾時。

## 自己判的細節（L17 的範圍，揭露）

- **名字**：`NetService`、`NetWorldLink`、`NetProductOptions`、`NetWorldOptions`、`NetRole`、`ClientSession`、`ClientState`、`ServerHost`、`HostEvent`、`DisconnectReason`、`IdRange`、`HandshakeFacts`、`ConnectRequest`、`ConnectAccepted`、`Rejection`、`SessionSettings`、`ReceiveNet`、`SendNet`（N18 動詞片語）、`Peer`、`PeerConnected`、`PeerDisconnected`（N22 過去式）、`NT-E1`～`NT-E4`。
- **常數**：keepalive 100 ms、Timeout 10 s、中斷 1 s 照卡片；握手逾時 15 s 照 lightyear netcode `lib.rs:115`（`CONNECTION_TIMEOUT_SEC`）。門檻的比較一律「超過」（`>`），照 lightyear netcode `server.rs:893`、`:856`。
- **協議雜湊**：`Fnv1a64("gyo.net.registrations/1;")`（空的登記）。第 2 步有複製的登記時改成由名字、順序、編碼版本算。
- **secret 與連線識別**：`NetProductOptions::seed`，沒給用 `std::random_device`。
- **`Peer`、`NetWorldLink` 的雜湊函式什麼都不雜湊**：連線識別是隨機的，不是兩次執行要一致的 World 狀態。World 摘要因此看不到它們。
- **Peer 實體用 World 的配發器**（`Spawn`）：Match 的 Peer 佔 Match 的範圍。LIM-7「本機的實體不和權威共用編號空間、怎麼分開由產品決定」：要分開時由產品給範圍；第 2 步做同號時再看。
- **重連**：`NetService` 不自己重連；新的 World 準備時（`ReplaceWorld`）若舊的 Client 連線已斷，才開新的握手。
- **`OneEndOnly`**：照卡片標了。1b 的兩個 System 不寫判定資料，標不標都不改一致性指紋（`CollectedOf` 只收寫判定資料的 System），只出現在 `DatasetReport`。第 2 步收件寫判定資料時才真正需要。

## 驗證

- 本機（macOS x86_64，`core` preset）：
  - `gyo_net_tests` 41 案全過；
  - `ctest -R "net\.|os\.|cmake\.|naming|authority|entity_ids|orchestration|world_time"` 65 個全過；
  - 全部的 ctest 176 個全過（沒有無關的失敗）。`entity_ids.default`（產品的登記單位表由真的 `config/modules.csv` 產生）也過。
- G-2 的數字和 1a 相同（同一個種子，傳輸層的行為沒變；理由表多兩個 0）。
- 判定的事前宣告：G-3、G-4 的判定照宣告第 8 節的原文；G-4 的「前一刻與後一刻」用 1 ns。
- **突變**（`scripts/net_mutations.json`，1b 加 15 個、改寫 1 個的 expect，共 27 個）：**全清單本機 27／27 killed**。
  - 加的：`nt-compare-version`、`-hash`、`-step`、`-range`、`-fingerprint`、`nt-k1-compared`、`nt-challenge-unchecked`、`nt-admission-ignored`、`nt-request-min-size`、`nt-liveness-not-refreshed`、`nt-keepalive-with-data`、`nt-interrupt-at-threshold`、`nt-timeout-at-threshold`、`nt-registers-without-role`、`nt-peer-not-despawned`。
  - `nt-unknown-kind` 的 expect 改寫：16～20 現在是表裡的種類，測試的清單從 {0, 4, 16, 19, 255} 改成 {0, 4, 15, 21, 255}，訊息去掉「保留的握手種類」。守的性質（表外的種類回 `UnknownPacketKind`）不變。
  - 第一次跑：`nt-challenge-unchecked`、`nt-liveness-not-refreshed` 是 wrong_failure，`nt-keepalive-with-data` survived。
    - 前兩個：測試在預期的檢查之前先 `REQUIRE` 或在別的檢查失敗。改成先取值、再用帶訊息的檢查；`nt-liveness-not-refreshed` 的效果在「下一次 Update 還是連線」才看得到，訊息移到那裡。
    - 第三個：每 16 ms 都有資料時，距上次送出永遠不超過 100 ms，突變和正確的行為一樣。加一段「資料相隔 150 ms、有資料的那一幀不附 keepalive」。
    - 改一次之後三個都 killed；判定的意思沒變（G-4 的「雙向有資料時不送 keepalive」多了一種情境）。
  - §14.5：15 個都寫了「只有它捕到的」與 `guards`（D-3、D-4、D-6、D-8、D-10、判定 G-1、G-3、G-4）。G-3 的五項各一個（各自獨立會錯的條件，README 基準 3）。沒加的：`OneEndOnly` 的標記（1b 沒有可觀測的差別）、`NT-E2`／`NT-E3`／`NT-E4`（設定錯誤的回報，沒有點名的功能依賴它）。數量 15，未超過「改的條文 5 條 × 5」。
- **證據**（主 checkout `build/evidence/gyo2-net-module/step1b/`，git 不追蹤）：`net-1b-cases.log`（G-1、G-3、G-4、G-7 與整合案例）、`g2-transport.log`、`ctest-selected.log`、`ctest-all.log`、`mutations.log`（1b 的 16 個與三個的重跑）、`mutations-all.log`（全清單 27／27）。

## 之後要接的

- 第 2 步：複製的登記與協議雜湊；World 世代（`ReplaceWorld` 時加一、Peer 隨新 World 重建：1b 換 World 時，已連上的連線在新 World 裡沒有 Peer 實體）；Client 收值寫進 World。
- Link 的錯誤怎麼交給 runner（上面「和宣告不同的地方」）。
- `docs/architecture/README.md` 的 Module 清單與〈Design Invariants〉：DI 定稿時一起加。
- Windows／Linux：PR 的 L1 跑 G-7（UDP loopback）之後補結果。
