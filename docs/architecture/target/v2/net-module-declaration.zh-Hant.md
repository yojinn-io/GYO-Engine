# 事前宣告：網路 Module（`v8-arch-gyo2-net-module`）

狀態：修訂 0（草稿；敵對式檢查前）。**待使用者動工前核准。**

性質：批次表第 6 列「網路 Module」的事前宣告（AGENTS §2 基盤 Architecture、§3 Architecture Delta、§4 系譜）。網路是機制、在搬產品之前做（`v8-arch-gyo2-net-is-mechanism-first`）；開卡：`v8-arch-gyo2-next-batches-start`（2026-11-22「網路 Module 可以開卡做了。」）。

讀的版本：`gyo2` eacab4d7（含 #270 交換點、#272 生成世界）；主會話的 HANDOFF 在 `origin/claude/gyo2-time-design` 817f78c5。偵察的原文（五份，各帶檔名行號）存在主 checkout `build/evidence/gyo2-net-module/recon-*.md`（`gyo-docs`、`lightyear`、`bevy_replicon`、`unity-netcode`、`master-structure`）。參考實作的版本：lightyear 736e1695（0.31，複製層已換成 bevy_replicon 0.45）、bevy_replicon d80c849（0.45）、Unity Netcode for Entities 7.2.0（`com.unity.transport` 本機沒有，transport 的預設值查不到）。

---

## 0. 一個例子（先用遊戲講）

**PvP 射擊**：Match 是伺服器，四個 Client 連進來。

- **連線**：Client 經 UDP 送「我要連」。Match 回挑戰，兩邊比協議雜湊、一致性指紋、步長，對不上就拒絕並寫明理由。連上後 Match 給這條連線一個 Peer，產品拿握手帶來的使用者 id，對到自己的 Player（Peer 與 Player 分開，11-11）。
- **每一步**：
  - Client 把「第 100 步：前進、開槍、我畫面上看到的是第 94.5 步」連同前幾步的輸入一起送出（冗餘，掉一包也不缺）。
  - Match 在第 100 步用它；沒到就沿用上一步的輸入，並記一筆「缺了」。
  - Match 的 World 跑完後，網路把這一步改過的值送給每個 Client：生成、刪除、加減元件走可靠有序；值走不可靠、只送最新的。
- **Client 收到**：
  - 不預測的 Client 直接寫進 World。
  - 預測的 Client 把權威值寫進 `RollbackInbox`，說「第 100 步以前都收齊了」，回滾 Module 比對、重跑。
  - 「門開了」「誰擊殺了誰」這類事件走可靠的事件串流，不會比它指到的實體先到。
- **斷線**：資料流本身就是心跳。一陣子收不到就進「中斷」，再久就斷線。重連就是重新握手、重新對時，再收一份完整的世界。

**2D 橫版單機**：不選網路的連線角色，什麼都不開、不建 socket、不掛 System。作者一行網路的程式都不寫。

**格鬥遊戲的 P2P**：權威是佔位（AU-E1）。網路的 Link 與通道不分伺服器、Client，留著 P2P 交換輸入的位置，本線不實作。

---

## 1. 前提表

每條：誰定的、哪些決定掛在它上面。這些前提一變，掛在上面的決定要重看。

| # | 前提 | 誰定的（出處） | 掛在上面的決定 |
|---|---|---|---|
| P1 | 網路是機制、不是某個遊戲的附屬品，搬產品之前做 | 使用者 2026-11-19 `-net-is-mechanism-first` | 本線的存在；Feature 從「會用它的產品」推（第 2 節）；不等 PvP 移植 |
| P2 | 目標是通用引擎：PvP 的強化不影響別的遊戲，單機不因此變複雜 | 使用者 2026-11-14 `v8-arch-engine-scope-general` | D-10「不選角色就什麼都沒有」；第 8 節的單機判定 |
| P3 | 網路是 Module（自己維護、介面是我們的），放 Engine 內；外部組件只在一個 adapter；平台差異放 `core/os` 的 `src/sys/<平台>` | 使用者 2026-11-16 `-module-plugin-owner-definition`；AGENTS §6；`-os-in-core-platform-dirs` | D-1 socket 放 `core/os`；D-2 自建傳輸層 |
| P4 | 權威是作者選的策略式 Module；伺服器權威下判定資料以伺服器為準：規定在權威、搬運在網路、重算在回滾（DI-A 7） | 使用者 2026-11-16 `-authority-is-game-design`、`-authority-strategy-mechanism`；authority.zh-Hant.md DI-A 7 | D-5 複製寫進 `RollbackInbox`；D-6 握手比一致性指紋 |
| P5 | 回滾不認得網路（R-3）；權威值的入口是 `RollbackInbox`（R-8 的形狀、MC-1～MC-5） | 使用者 2026-11-17／11-20 核准的回滾宣告；rollback.zh-Hant.md | D-5、D-7（確認點的規則） |
| P6 | Gateway 原意：外部可接多種 Client、內部一套抽象協議、Gateway 無狀態；內部協議保留身分與冪等 | 使用者 2026-10-15 說明（記憶 `gateway-original-intent`；repo 只有 HANDOFF 與批次表引用） | D-3 Link 與一套協議；D-4 冪等的鍵；第 9 節 Q2 |
| P7 | 斷線偵測與短線重連只在 UDP 上，資料流當心跳，重連＝重新對時；不加 TCP 心跳 | 使用者 2026-10-23（記憶 `udp-only-liveness`） | D-8 |
| P8 | 權威／回滾不參考 master 的算法、常數、數據；09-25 dev_log 不引用；11-01 後的數據只當比較基準；master 的 protobuf 只用來估狀態大小 | 使用者 2026-11-20 `-authority-rollback-no-master-reference`、`-budget-baseline-research` | 本頁所有常數的系譜只寫參考實作或推導；第 6 節的大小估算 |
| P9 | 時間只來自 tick 與被記錄的 boundary 輸入（歸伺服器權威策略）；Client 對時走倍率、步長不動 | 使用者 K.3-11（11-11）、11-17 改寫 `-clock-out-of-kernel`；時間設計 3.3 | D-9 對時；IO 層的牆鐘不進判定 |
| P10 | 外部服務只經交換點進出 World；套用點是 Module 的外部服務在幀裡進出 World 的預設；收件在回滾的決定（`PreUpdate`）之前（`First`）；傳輸物件放 Product 一份的共用處，同步是它自己的事 | 使用者 2026-11-22 核准的交換點（O-12、O-13、O-15） | D-3 的接線；第 7 節給 threads／io 的需求 |
| P11 | Peer 與 Player 分開（每收件者的帳本、可見性屬 Peer） | 使用者 2026-11-11（`plans/kernel/networking.zh-Hant.md` §3b、pv10-requirements 追加） | D-4、D-11 |
| P12 | Engine 不為「斷線重入用同一個編號」加機制；重入的資料可能在 Engine 外的服務 | 使用者 2026-11-15 `-nested-same-id-respawn-not-engine` | D-8 重連不保 session；Player 的對應歸產品 |
| P13 | 預設的配發器 `RangeAllotter`：單調、不重用、不回繞；網路的編號對映不歸它（EI-1）；網路複製的外來編號要先動配發器的範圍 | 使用者 2026-11-22 核准生成世界（`world-gen-merge-approval` :85） | D-4 |
| P14 | 協議設計時點名的功能逐一推 wire 需求；推不出的列未決，不寫成假設 | 使用者 2026-10-31（記憶 `protocol-design-covers-named-features`） | 第 2 節的表；第 9 節 Q4 |
| P15 | 不為防誤用加守衛；守衛只守寫明的契約；底層對誤用靠每批的設計審查 | 使用者 2026-11-21／22（記憶 `no-guards-against-misuse`、`design-review-not-guards`） | 第 8 節判定只守契約 |
| P16 | Module 預設全帶、產品可以關 | 主會話照 L17 判（批次表「不擋路的小事」） | D-10 |

---

## 2. Feature → wire 需求（AGENTS §2 Derivation Order、P14）

### 2.1 點名的功能從哪裡來

1. **使用者確認的 pv10 功能清單**（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`，使用者 2026-11-07「就這些」）七項，加 2026-11-11 追加兩項（Match 的 ready 事件、Peer 與 Player 分開）。
2. **master 的 PvP 現有功能**：使用者 2026-11-19「我們有一個完整遊戲作為對照」（`-infrastructure-before-products`），產品會回來。只取「遊戲做了什麼」，不取算法與常數（P8）。hitbox 是使用者 2026-10-31 點名的（pv8 的教訓）。
3. **使用者在 GYO2 點名的**：4 人 PvP（`-budget-scale-depth`）、「master 不單單是 4 人 PVP」（`-vision-scale`）、地圖 500×500 地面＋地下（`-map-500-two-layers`）、斷線重連與退出重入（`-nested-same-id-respawn-not-engine`）、格鬥 P2P 佔位（`-authority-is-game-design`）、跨時鐘事件要兼容網路（`-cross-clock-events-open`，例子是射擊和聲音）、多通道「需要留設計」（`-multichannel-shape`）、分段載入（`-streaming-load`）。

### 2.2 推導表

「用它的 Product」：Match＝伺服器的 World；Client＝玩家的模擬 World。

| # | 功能（出處） | 用它的 Product | wire 需求（欄位／通道／單位） | Contract | 參考實作的對照 | 落在第幾步 |
|---|---|---|---|---|---|---|
| F1 | 移動與預測（master PvP；rollback 頁的例子） | Client 送、Match 收；Client 預測 | 輸入訊息：`end_tick`（u32，Match 的 tick）＋連續 N 步的輸入（冗餘）；不可靠通道 | C-IN1：輸入以（Peer, tick）為鍵，重複送是冪等的 | lightyear：`end_tick`、冗餘 5、逐 tick Absent/Same/Snapshot；Unity：一包帶當前＋前 n 份 | 3 |
| F2 | 射擊與命中、hitbox 部位（master PvP；使用者 10-31 點名 hitbox） | Client 送、Match 判 | 輸入訊息帶**畫面的時間**：Client 畫面上看到的 Match tick＋小數（定點 1/65536 tick）。部位與命中是判定資料，經複製回去。hitbox 的歷史姿態由產品／collision 存，網路只帶時間 | C-IN2：每筆輸入帶畫面的時間，Match 用它查歷史 | Unity：command 標頭帶 interpolation delay；lightyear：輸入訊息帶 `interpolation_delay`；master：`ShotRequest.view_tick_q16`（只取「有這個欄位」，不取算法） | 3 |
| F3 | 互動物件：門、拉桿、陷阱、生成、邏輯組合（pv10 #1） | Match 送、Client 收 | 實體的出現、消失、加減元件走**可靠有序**；狀態值走不可靠；互動請求走輸入（F1 的鍵） | C-RP1：結構與值分流；值的訊息帶「至少要先套用到哪個結構 tick」 | replicon：Updates（Ordered）／Mutations（Unreliable，帶 min required tick）；lightyear：Actions／Updates | 2 |
| F4 | 物品／容器／背包，內容只給開啟者（pv10 #2） | Match 送給特定 Peer | **每收件者的元件範圍**：某實體的某元件只送給指定的 Peer（私人資料）；出現、消失走可靠 | C-VIS2：可見性可以細到元件 | replicon：visibility scope 可以是 components；Unity：relevancy 只到 ghost | 5 |
| F5 | 技能四類＋戰術動作（pv10 #3） | Client 送、Match 判 | 放技能＝「某 tick 要做某事」，走輸入（單次觸發要不會因掉包重複或漏掉）；冷卻＝私人狀態（F4）；結果＝事件或實體 | C-IN3：單次觸發的輸入，用（Peer, tick）去重，掉一個 tick 也只觸發一次 | Unity：`InputEvent`；pv10 的結構 4（動作請求的 oneof） | 3 |
| F6 | 寵物＝類似裝備（pv10 #4） | Match 送 | 玩家的表現狀態，一般的元件值 | 同 C-RP1 | — | 2 |
| F7 | AI 小怪 ≤20、Boss 1（pv10 #5、#6；`-vision-scale`） | Match 送 | 實體數量：Match 一端可能數百個；**一包放不下**，要分多包、要優先度、要相關性 | C-RP3：一個 tick 的值可以分成多個訊息，每個實體的資料不拆；C-VIS1：每收件者一份可見性 | replicon：依大小切 mutation、priority；Unity：importance、partial snapshot；lightyear：priority accumulator | 2（分包）、5（相關性） |
| F8 | 對局規則：計分、擊殺訊息、勝負與結算（pv10 #7） | Match 送 | **每收件者的可靠有序事件串流**；事件帶它依賴的結構 tick，不比它指到的實體先到 | C-MS1：伺服器的訊息綁 tick，Client 等到那個 tick 的結構套用完才發出 | replicon：server message 綁 tick、`MessageQueue`；N11 | 2 |
| F9 | Match 的 ready 事件（pv10 追加，11-11） | Match 送 | 可靠事件（同 F8），或握手完成的訊息的一欄；之後 tick 從 0 起算 | C-CN3：握手完成時帶 Match 的當前 tick 與步長；ready 是一般的伺服器訊息 | Unity：`ServerApprovedConnection` 帶 tick rate；lightyear：KeepAlive 後才給 client_id | 1、2 |
| F10 | Peer 與 Player 分開（11-11）；觀戰者等非 Player 的 Peer | Match | Peer 是連線的身分（伺服器配，連線內有效）；握手帶**不透明的使用者 id**（u64，產品對到 Player）；帳本與可見性掛在 Peer | C-CN2：Peer id 與使用者 id 分開 | replicon：client 是 server 上的 entity、持久 id 另用 `NetworkId`；lightyear：`PeerId`、token 的 u64 client_id；Unity：`NetworkId`＋`UniqueId` | 1 |
| F11 | 斷線重連、退出重入（使用者 11-15；P7） | 兩端 | 資料流當心跳：任何一包都刷新存活；只有閒置時才送 keepalive；逾時→斷線（理由 Timeout）；重連＝新的握手（帶使用者 id）＋重新對時＋完整的世界 | C-CN4：存活只看 UDP 上的資料流；C-CN5：重連不延續 session | lightyear：送 keepalive 前先看距上次送出夠不夠久（`server.rs:893`）；replicon：新 client 的 ticks 空、重送整份；Unity：command 一直送當 keepalive | 1（存活）、3（重連） |
| F12 | 對時（master PvP；時間設計 3.3） | Client | 每包的標頭帶送端的 tick；RTT 由封包確認推（不另開 ping 也行）；Client 調**倍率**追 Match，誤差大時跳 tick | C-TS1：標頭帶 tick；對時的輸出是送進時間 Module 的倍率與跳 tick，不進回滾歷史 | lightyear：header 帶 tick、`RemoteTimeline`、±5% 與 >10 tick snap；Unity：command age 推 time scale、>10 tick 跳；N6 | 3 |
| F13 | 預測的 Client 收權威值、回滾（DI-A 7、rollback 頁） | Client | 「第 C 步以前收齊」要能從 wire 判斷：每個 tick 的值訊息帶這個 tick 的分包數，收齊才確認（MC-4） | C-RP4：確認點只在那個 tick 的值訊息全到、結構也套用到所需 tick 之後前進 | lightyear 的 checkpoint 不變式（`rollback.rs:313-316`）；replicon `ServerMutateTicks`＋`track_mutate_messages` | 4 |
| F14 | 4 人 PvP、之後可能更多（`-budget-scale-depth`、`-vision-scale`） | 兩端 | 規模：Peer 數不寫死上限；每包 MTU 1200（第 6 節） | C-TR3：MTU 預設 1200、兩端可協商較小值 | lightyear `DEFAULT_MTU = 1200`；Unity：MTU 減標頭 | 1 |
| F15 | 一致性：兩端判定部分相同才開打（authority 的一致性指紋）；步長放指紋或握手（authority 4 延到這裡） | 兩端 | 握手比：協議版本、網路的登記雜湊、權威的一致性指紋、步長；任何一項不同→拒絕並寫明哪一項 | C-CN1：握手比四項 | Unity：四個 hash（NetCode、Game、Rpc、Component）不符斷線；replicon `ProtocolHash`；lightyear protocol | 1 |
| F16 | 跨時鐘事件要兼容網路（`-cross-clock-events-open`） | 兩端 | 本線只保證：伺服器訊息綁 Match 的 tick 送到 Client，Client 端寫進它的 `Message<M>`（F8 的形狀）；跨時鐘那一半照時間設計 6.4，不在本線 | 同 C-MS1 | — | 2 |
| F17 | 格鬥遊戲 P2P（佔位） | — | 不實作；Link 與通道不區分伺服器／Client，輸入的形狀（F1）兩個方向都能用 | 不定新契約 | lightyear `NetworkTopology::P2P` | — |
| F18 | 分段載入（`-streaming-load`）、多通道「需要留設計」（`-multichannel-shape`） | Match 送 | 分段載入的實體進出＝可見性的進出（C-VIS1）；多通道＝每種訊息一條通道、通道有優先度（C-TR2） | C-TR2：通道是索引＋保證等級＋優先度 | replicon：每種訊息一條 channel；lightyear：channel priority | 1（通道）、5（進出） |

### 2.3 推不出來的（未決，交使用者：第 9 節 Q4）

- 加密與認證（connect token、防偽造來源）：沒有點名。三家只有 lightyear（netcode.io）有，而且它也不替你發 token。
- 同一個行程裡的伺服器與 Client（host、listen server）：沒有點名；交換點修訂 1 已拿掉 E-9。
- 瀏覽器、WebSocket 的 Client（「多種 Client」的第二種）：Gateway 原意說「多種」，但沒點名是哪幾種。
- 每個 Client 的頻寬預算：沒有點名；第 6 節只能給大小的估算，不能給上限。
- 執行中的 digest 比對與不一致時怎麼辦（authority 4 延到這裡）：沒有點名的產品行為。
- 內插（別人的角色在畫面上平滑地動）：master PvP 有，三家都放在網路庫；但「內插歸網路 Module 還是產品」沒有點名。第 5 步先放「畫面的時間」（F2 要的），內插本身列未決。

---

## 3. 參考實作：為什麼這麼做、和我們的相似點、能用多少、要改多少

| 面向 | lightyear | bevy_replicon | Unity Netcode | 為什麼（他們寫的理由） | GYO 能直接用 | GYO 要改裝的 |
|---|---|---|---|---|---|---|
| 傳輸的抽象 | `Link` 元件：收、送兩條 byte 佇列；每個 IO 是「一個元件＋兩個 System」；作者說「刻意做窄」 | 不帶 transport；backend 只要往兩個 buffer 讀寫 bytes＋管狀態；不用 trait（Rust orphan rule） | `NetworkDriverStore` 最多 3 個 driver、pipeline 疊加；同一行程走 IPC | 換傳輸不動上層；多種傳輸同時開 | **窄的 Link：收發 datagram＋連線狀態** | Link 是 C++ 的 interface（不是元件）；socket 在 `core/os`（P3） |
| 封包與可靠性 | 13 B 標頭（type、packet_id u16、ack u16、bitfield u32、tick u32）；packet id 只增、重送在訊息層；AckOnly 不引發回 ack | 不做（交 backend） | Unity Transport 的 Reliable／Unreliable Sequenced／Fragmentation pipeline | Gaffer 式 ack 一包蓋 33 包；重送新值比重送舊包好 | **標頭的五欄、訊息層重送、AckOnly 不回 ack** | 標頭加版本與協議的魔數（握手前擋雜訊） |
| 通道 | 六種（Unreliable、Sequenced、Ordered／Unordered Reliable…）＋優先度＋token bucket | 三種保證：Unreliable／Unordered／Ordered，backend 給的可以更強 | 4 條 pipeline | 結構要可靠、值只要最新 | **三種保證＋sequenced unreliable；優先度** | 頻寬上限先不開（Q4） |
| 結構與值分流 | Actions（可靠有序）／Updates（sequenced）；同一實體的值跟著它的 action 一起送 | Updates（Ordered）／Mutations（Unreliable，帶 min required tick）；同 tick 有結構變更的實體併進 Updates | snapshot＋delta 對最多 3 個已確認的 baseline | 只送一部分會讓元件指到 Client 沒有的實體；全可靠會重送舊資料 | **replicon 的分流與「最新值＋min required tick」** | — |
| 每收件者的帳本 | 用 replicon 的 | `ClientTicks`：每實體上次確認的 tick、元件遮罩；ack 才前推；沒 ack 的下次用最新值重送 | 每連線每 chunk 32 份 history、ack mask 4096 tick | 帳本只決定重送，不是遊戲狀態的副本 | **帳本歸 Peer、只決定重送**（AGENTS §3 狀態的權威：帳本是網路自己的狀態） | — |
| 編號 | `ServerEntityMap`（replicon）；client 用自己的 allocator，不撞號 | 雙向對照表；元件裡的 entity 要標 `#[entities]` 才對映 | ghostId（回收）＋spawnTick 唯一；和本地 Entity 分開 | Bevy 的 entity 會回收（世代），兩端不可能同號 | 對照表的形狀 | **GYO 的配發器不重用（P13），可以兩端同號：第 9 節 Q1** |
| 可見性 | (entity, link) 的可見性＋Room；失去可見時三種處理 | 每 client 一份遮罩、filter 最多 32 個、scope 到元件；`WhileVisible` 等三種 | Relevancy 三種模式；變成不相關＝client 收到 destroyed | 決定一律在 server | **每 Peer 一份可見性、scope 到元件、失去可見＝刪除** | 失去可見要寫進回滾的 `Exited`（MC-3） |
| 輸入 | `end_tick`＋冗餘 5、64 格 ring、缺的用 hold-last、伺服器只收 ±64 tick | 不做 | 帶當前＋前 n 份、對當前做 delta、server 缺輸入沿用上一筆、`InputEvent` | 掉包不缺輸入；擋惡意的大 tick | **冗餘、hold-last、tick 窗、單次觸發** | 輸入寫進權威的 `Ingress`；Client 的紀錄已有 `rollback.Input` |
| 對時 | 標頭的 tick＋ping；±5% 調速、>10 tick snap；發 `LocalTimelineShift` | 不做 | command 在 server buffer 等多久推 time scale（0.9～1.1）、>10 tick 跳；IPC 時當 0 | 步長不動才不會破壞固定步 | **倍率追、太遠就跳** | 倍率與跳 tick 經時間 Module 的契約（D-9）；具體的增益與門檻在第 3 步宣告時推（P8） |
| 預測與回滾 | 單一 entity：live 值＋`ConfirmedHistory`＋`PredictionHistory`；policy Check/Always/Disabled | 不做，留 `ConfirmHistory`、receive marker 給外部 | 只回滾這次有更新的 ghost；上限 64 tick | — | GYO 的回滾 Module 已做（R-8） | 網路只寫 `RollbackInbox` |
| 單機的成本 | prediction 用 `run_if` 關 | 送只在 `ServerState::Running`、收只在 `Connected`；單機什麼都不跑 | — | — | **不選角色＝不掛 System** | — |
| 握手與協議檢查 | netcode.io：Request→Challenge→Response→KeepAlive；token 2048 B | `ProtocolHash`（型別名與順序） | 四個 hash 不符斷線；「別讓不相容的版本玩到一半才爆」 | — | **握手比雜湊、拒絕寫理由** | 比的是 GYO 自己的四項（F15） |
| host 模式 | HostClient：client 不收 replication，在共用 World 直接模擬 | 不在同一個 world 同時跑 client 與 server（延遲的資料會蓋掉最新值） | single-world host（預設）與 binary-worlds host（走 IPC，和 dedicated 同一條路） | — | 記錄，不做（沒點名） | — |

**master 的結構與坑**（只取結構，P8；`recon-master-structure.md`）：

1. **外部有損重複、內部恰好一次**：兩種送達語意的轉換放在 Gateway，它就得去重、就得有 Match 的狀態副本。2026-10-10 的事故就是 Gateway 照副本悄悄丟了命令。→ D-4：內部協議每一筆都帶冪等的鍵，任何中繼都能原樣轉送。
2. **路徑上三十多處無聲丟棄** → D-12：每個丟棄的出口都有理由與計數，交給上層（錯誤只告訴外面發生了什麼）。
3. **序號在唯一的寫出處、寫出前才指定** → 傳輸層的 packet id 只在 Link 送出前配。
4. **診斷不進判定、跨時鐘域的時間戳只照抄或在同域相減** → D-9。
5. **socket 錯誤分類留在產品、Windows 的 `WSAECONNRESET`／`SIO_UDP_CONNRESET` 沒定案** → D-1：分類在 `core/os` 的 adapter 裡定。

---

## 4. 設計（Contract）

### D-1 UDP socket 放 `core/os`

- `core/os` 加一個非阻塞的 UDP datagram socket（暫名 `DatagramSocket`）：bind、送一個 datagram 到某個位址、收一個（沒有就回「沒有」）、IPv4／IPv6 位址的型別。錯誤用 `OSError` 分類（暫時的錯、對方不在、封包太大、其他）。
- Windows 的 ICMP 回傳讓 `recvfrom` 回 `WSAECONNRESET`：adapter 裡用 `SIO_UDP_CONNRESET` 關掉，上層看不到。這是平台差異，放 `src/sys/windows/`。
- 網路 Module 不 include 任何平台標頭（AGENTS §6：一個外部組件一個 adapter）。
- **系譜**：`core/os` 原意是 OS 的 adapter（記憶 `os-layer-original-intent`）；master 的 socket 用 asio 寫在產品裡（`os.md:307`），這是要改掉的。
- **退掉的**：(a) socket 放網路 Module 的 `src/adapters/`：平台差異要放 `core/os`（P3），而且 socket 不是第三方庫。(b) 用 asio：第三方庫，要一個 adapter，只為一個 UDP socket 不值得。
- **需要它的**：F1～F18 全部（Match、Client）。

### D-2 傳輸層自己做，不用第三方網路庫

- 網路 Module 裡的傳輸層是**純的狀態機**：輸入是「收到的 datagram＋現在的時間」，輸出是「要送的 datagram」。不碰 socket、不讀時鐘，測試用記憶體裡的 Link 加可重現的掉包、重複、亂序（種子固定）。
- 標頭照 lightyear 的五欄，加魔數與協議版本；packet id 只增，重送在訊息層；ack＋32 位元 bitfield；分片；MTU 預設 1200。
- 通道：Unreliable、Sequenced Unreliable、Ordered Reliable、Unordered Reliable，每條通道有優先度。
- **系譜**：lightyear 的 transport（Gaffer 式），它自己也是自做；Unity 用自家的 Unity Transport；replicon 不做。
- **退掉的**：(a) ENet：連線與逾時的模型是它自己的（和「資料流當心跳」、標頭帶 tick 不合），每次更新都要重對。(b) GameNetworkingSockets：重、帶加密與 relay，大部分沒點名。(c) netcode.io＋reliable.io（C）：加密與 token 沒點名（Q4）；之後要加密時，握手留了位置（D-6 的 token 欄），可以那時再評估。
- **需要它的**：F1～F18。

### D-3 Link 與一套內部協議（Gateway 原意的落點）

- **Link**（interface）：送 datagram 給某個遠端、收 datagram（帶來源）、連線的狀態。第一個實作是 UDP（D-1），測試用記憶體裡的 Link。之後的 WebSocket、Steam 等等都是另一個 Link。
- **一套內部協議**：Link 之上只有一種標頭、通道、訊息的格式。不管 Client 從哪個 Link 進來，Match 看到的都一樣。這就是「外部多種 Client、內部一套抽象協議」。
- **無狀態**：Link 只持有 bytes 與連線狀態，沒有遊戲的狀態。判定、去重、重送的決定都在網路 Module 的 Peer 上（帳本），帳本只決定重送，不是遊戲狀態的副本。
- **接線**（P10）：
  - 傳輸物件（`NetTransport`：Link 們、Peer 表）放 Product 一份的共用處（O-15），由網路 Module 在 `Configure` 建。
  - 每個選了連線角色的 World，網路 Module 掛兩個套用點的 System：收件在 `Main`／`First`（回滾決定 `PreUpdate` 之前），送件在 `Main`／`Last`。
  - 收件的 System 從 socket 非阻塞地拉、解、寫進 World；送件的 System 收集、編、交給 socket。
  - 都不等別的執行緒（O-12 的套用點規則）。
  - 不用交換函式（O-12：同一件事只選一種）。
- **系譜**：lightyear 的 `Link`＋multi-transport（同一個 server 同時開 UDP、WebTransport、Steam）；master 的「兩份 proto＋adapter」是外部協議與內部協議兩套，這裡改成「外部只到 bytes，協議只有一套」。
- **退掉的**：(a) 另開 Gateway 行程、外部協議轉內部協議（master 的系譜）：這是第 9 節 Q2。(b) 交換函式收件：收件要在回滾決定之前、要和 System 排先後，O-12 定了用套用點。
- **需要它的**：F1～F18；F10、F11（Peer 與重連）。

### D-4 編號與冪等的鍵

**冪等的鍵**：內部協議的每一筆都帶身分，重複收到不會變成兩件事（P6 的「保留身分與冪等」）。

| 資料 | 鍵 | 重複收到時 |
|---|---|---|
| 輸入 | (Peer, tick) | 同一格寫同一個值 |
| 可靠訊息 | (Peer, 通道, 訊息序號) | 收過就丟（計數） |
| 結構更新 | (Peer, 結構 tick) | 已套用就丟 |
| 值 | (實體, 元件, tick) | 新的蓋舊的；比已確認的舊就丟（計數） |

**實體編號**（第 9 節 Q1 的建議做法）：

- **伺服器配的實體，在 Client 上用同一個編號**（`SpawnWithId`）。
- 配發器的範圍分兩半：
  - 選了伺服器角色的 World 用 `[1, 2^31−1]`。
  - 選了 Client 角色的 World，自己生成的（畫面用的、預測出來的）用 `[2^31, 2^32−1]`。
  - 網路 Module 在準備函式裡放 `EntityIdsOptions`；已經有值就不蓋（O-7），讀檔時作者可以照常改。
- **為什麼能同號**：`RangeAllotter` 單調、不重用（EI-2）。同一個編號不會先後指兩個實體，所以進出可見性、重連再送一次，用同一個編號都安全。Bevy、Unity 的編號會回收，所以它們要對照表。
- **好處**：
  - 元件裡存 `EntityId` 的欄位不用每個型別都寫對映函式。C++ 沒有反射，對照表的做法要每個作者替每個元件寫 `MapEntities`，這就是「機制分散在各個實作裡」。
  - `RollbackInbox`、digest、日誌在兩端講同一個編號。
- **代價**：
  - Client 預測出來的實體（預先生成）和伺服器之後生成的不同號，要配對。本線不做預先生成的配對（沒點名）；要的時候再加一張只管這種情況的小表（replicon 的 `Signature` 的形狀）。
  - 伺服器的 World 最多配 2^31−1 個（RC-NT2）。
- 分界 2^31 的系譜：Unity 預先生成的 ghost id 從 `0x80000000` 起（`PrespawnHelper.cs:12`）。

### D-5 複製（伺服器 → Client）

- **登記**：網路 Module 自己的入口（統合 :603「要不要複製是網路 Module 自己的登記」）。
  - 寫法：`NetEntry(s).Replicate<T>(name, options)`，`options` 帶編碼函式與範圍。
  - 數值與 enum 有預設的編碼；其他型別由作者給編碼函式，不用位元組照抄（padding 不上 wire，同 World 摘要的教訓）。
- **wire id 與協議雜湊**：wire id 是登記的順序。協議雜湊由名字、順序、編碼的版本算出，握手時比（C-CN1）。
- **結構與值**：照 replicon。
  - **結構更新**：生成、刪除、加元件、拿掉元件。每個 tick、每個 Peer 一則，可靠有序，整則一起套用。
  - **值**：自上次確認以來的最新值，不可靠，帶「至少要先套用到哪個結構 tick」。
  - 一個 tick 的值可以分成多則，每則帶「這個 tick 共幾則」（C-RP3、C-RP4）。每個實體的資料不拆開。
  - 同一個 tick 有結構變更的實體，它的值併進結構更新。
- **帳本**：每個 Peer 一份，記每個實體上次確認的 tick 和元件的遮罩。只決定送什麼，不是遊戲狀態的副本（AGENTS §3）。
- **Client 怎麼寫進 World**（收件的套用點）：
  - 結構更新：用同一個編號 `SpawnWithId`、加減元件。
  - 值，分兩種情況：
    - World 選了回滾策略、而且 `T` 在 `RollbackRegistry`（讀公開的登記，R-6）：寫 `RollbackInbox::Value`／`Removed`；照 C-RP4 收齊後寫 `ConfirmedThrough`（第 4 步）。
    - 其他：直接寫進 World 的元件。
  - 寫進 World 的 System 在權威登記標 `authority.OneEndOnly`（統合 L23）。
- **系譜**：replicon 的 Updates／Mutations、`ClientTicks`、`ServerMutateTicks`；lightyear 的 Actions／Updates（它現在用 replicon）。
- **退掉的**：
  - (a) Unity 的 snapshot＋baseline delta：一個 chunk 存 32 份 history，記憶體與複雜度高；GYO 沒有 chunk 的佈局可以利用。
  - (b) master 的 latest-wins 全量快照：放不進一包（F7），也沒有帳本。
- **需要它的**：F3、F6、F7、F8、F13（Match、Client）。

### D-6 連線與握手

- **握手**：Client 送「請求」→ Match 回「挑戰」（隨機數）→ Client 回「回應」→ Match 回「接受」。
  - 「請求」帶協議版本、網路的協議雜湊、權威的一致性指紋（`ConsistencyFingerprintOf`）、步長，以及不透明的使用者 id（u64）。
  - 「接受」帶 Peer id、Match 的當前 tick、步長。
  - 四項任一不同就拒絕，理由寫明是哪一項。
  - 挑戰的往返擋掉偽造來源的放大攻擊（lightyear 用 netcode.io 的 Challenge）。
- **token 欄**：先留著、不驗證（加密與認證是 Q4）。
- **斷線的理由**：照 Unity 的分類，拿掉沒用到的：`Closed`、`Timeout`、`ClosedByRemote`、`ProtocolMismatch`（帶哪一項）、`FingerprintMismatch`、`StepLengthMismatch`、`HandshakeTimeout`、`Refused`（產品拒絕）。
  - 斷線冗餘送幾包（lightyear 送 10 包；次數在第 1 步推）。
- 產品決定收不收：Match 在「接受」之前問產品，用 E2 的回傳，產品可以拒絕（例：房間滿了）。
- **需要它的**：F9、F10、F15（Match、Client）。

### D-7 確認點（MC-4，給回滾的保證）

- Client 只在兩個條件都成立之後，才寫 `ConfirmedThrough(C)`：
  1. 第 C 個 tick 的值訊息全部收到（依「共幾則」）；
  2. 結構更新已套用到這些值要求的 tick。
- 比 C 舊的值晚到：寫進 `RollbackInbox`（MC-1：晚到的不靜默丟，回滾照「C 當時或之前最後的值」用）。比回滾深度還舊的，丟掉並計數。
- 可見性外的實體在 C 是「這份沒帶」（`NotSent`），不推成「沒變」（R-8）。要回滾先做 `NotSent`、`Entered`、`Exited`（第 4、5 步）。
- **系譜**：lightyear 的 checkpoint 不變式；replicon 的 `track_mutate_messages`（「這個 tick 沒變化也算確認」）。
- **需要它的**：F13（預測的 Client）。

### D-8 存活、斷線、重連（P7、P12）

- **存活**：任何一個合法的 datagram 都刷新存活（不只是 snapshot）。距上次送出超過一個間隔才送 keepalive（lightyear `server.rs:893` 的寫法）。
- **Client 端的狀態**：
  - 「連線」。
  - 「中斷」：收不到超過第一個門檻。保留 Peer，產品可以顯示「連線不穩」。
  - 「斷線」：超過第二個門檻，理由 Timeout。
- 門檻用時間（IO 層的單調時鐘，不進判定），數值在第 1 步推，系譜照參考實作，不用 master 的數。
- **重連**：一律是新的連線。
  - 新的握手（帶同一個使用者 id）、新的 Peer、新的帳本，所以 Match 會重送整份世界。
  - Client 重新對時。
  - 舊連線的輸入用 tick 窗擋掉（C-IN1）。
  - Player 怎麼接回來，由產品依使用者 id 決定（P12：重入的資料可能在 Engine 外）。
- **不做**：同一條 session 換位址（NAT 重綁）、session 延續。master 有，三家都沒有（Unity 的 UniqueId 只給 host migration 標記），也沒點名。
- **需要它的**：F11（Match、Client）。

### D-9 時間與對時（P9）

- **兩種時間要分開**：
  - **判定用的時間**只有 Match 的 tick：標頭的 tick、輸入的 tick、值的 tick。
  - **牆鐘**（RTT、存活、對時的量測）只在網路的 IO 層：不寫進判定資料、不進 digest、不進回滾歷史。
  - 跨行程的時間戳只照抄回去，或在同一端相減（master 的坑 4）。
- **對時（第 3 步）**：Client 從標頭的 tick 與 RTT 推「Match 現在第幾步」，算出目標 tick（Match tick＋單程延遲＋安全邊際）。
  - 差得不多：調**倍率**，步長不動。
  - 差太多：跳 tick。
  - 兩者都經時間 Module。
- **時間 Module 要加的契約**（Architecture Delta，第 5 節）：
  - 一個「只有倍率、沒有暫停」的入口，和產品的 `TimeControl` 相乘。
  - 跳 tick 的寫者進 `TickWriters()`（world_time.hpp 的註解已寫「clock-sync System joins with the Client batch」）。
  - 形狀在第 3 步的宣告定，給使用者看。
- 時間設計 3.3 寫「移植 PvP 時定」，是 11-19 之前的排法；網路改成先做（P1），對時跟著網路提前，不是推翻。

### D-10 不選就沒有（P2、P16）

- 登記檔一列：`net`，`default=yes`、`depends=world_time;rollback;authority;entity_ids`。
- 產品在 `ProductDescription::options` 放連線的設定（bind 或連到哪裡），World 在 `WorldDescription::options` 放角色：`Server`、`Client`；不放就是 `None`。
- 沒有 Product 的連線設定：不建 socket、不建傳輸物件。World 沒有角色：不掛任何 System、不登記任何東西。單機遊戲的 World 和沒有這個 Module 時一樣（第 8 節的判定 G-1）。
- **依賴方向**：網路 → 權威、回滾、時間、entity_ids、編排層、Kernel、`core/os`、`core/time`、`core/base`。
  - 權威、回滾、時間、entity_ids 都不依賴網路（R-3、DI-A 9、WT-4、EI-3 不變）。
  - 網路只讀它們的公開登記與入口。

### D-11 輸入（客戶端 → 伺服器）

- **登記**：`NetEntry(s).Input<T>(name, options)`。
- **Client**：每一步把這一步的輸入放進待送，一包帶最後 N 步（N 在第 3 步推；系譜：lightyear 5、Unity 2+2）。單次觸發的輸入用（Peer, tick）去重（C-IN3）。每筆帶畫面的時間（C-IN2）。
- **Match**：每個 Peer 一份按 tick 的輸入緩衝。第 T 步的入口 System（經 `authority.Ingress`）從緩衝取第 T 格寫進那個 Peer 對應實體的輸入元件。
  - 沒有就沿用上一步（hold-last），並計數「缺」。
  - tick 窗外的丟掉並計數。窗＝`[T − L, T + L]`，L 取回滾 `InputHistoryLength::Long`（64）。系譜：lightyear ±64、Unity 回滾上限 64。
- Peer 對應到哪個實體：由產品寫（Peer 與 Player 分開，P11）。網路 Module 給一個「Peer → 實體」的寫入口，不認得 Player。
- **需要它的**：F1、F2、F5（Match、Client）。

### D-12 丟棄與診斷（錯誤只告訴外面發生了什麼）

- 每個丟棄的出口有一個理由碼與計數，每個 Peer 一份：太舊、窗外、重複、解碼失敗、緩衝滿、分片逾時、協議不符、未知的 Peer。
- 這些計數掛在 World 的 Resource 上，交日誌（`world_logging` 的寫法）或由產品讀，不寫判定資料。
- 錯誤分級照異常分級：
  - 解碼失敗、惡意的 tick 是 E2（回報、這一筆丟掉）。
  - socket 壞掉是 E2 回給 runner。
  - 不加 E0。
- 送的數、到的數、計數的丟棄：測試裡逐項對上（AGENTS §14「件數の突き合わせ」；第 8 節 G-2）。

---

## 5. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：P1 與第 2 節的 F1～F18。搬 PvP 之前要有網路的機制；時間、權威、回滾都留了給網路的空位（`RollbackInbox`、DI-A 7、`TickWriters` 的註解、authority 4 的延後項）。
2. **現在缺什麼**：
   - `gyo2` 沒有 socket、傳輸、連線、複製；
   - `RollbackInbox` 只有寫的一半、沒有寫的人；
   - 一致性指紋沒有交換的地方。
3. **變的邊界**：
   - 新的 Module `engine/modules/net`（新 Subsystem、Build Graph、登記檔多一列）；
   - `core/os` 的公開介面加 UDP socket；
   - 時間 Module 加對時的入口（第 3 步）；
   - 回滾 Module 實作 `NotSent`、`Entered`、`Exited`（第 4、5 步）。這三個形狀已在 R-8 核准、現在回 NotImplemented，這裡是實作、不改契約；
   - entity_ids 不改，網路只用 `EntityIdsOptions`。
4. **影響的**：`core/os`、world_time、rollback（實作）、網路（新）。權威、編排層、Kernel、entity_ids 不改。現有產品沒有（`gyo2` 上沒有產品）。
5. **依賴方向**：新邊只有「網路 → 其他」與「`core/os` 的 socket」。沒有反方向（D-10）。
6. **Ownership**：網路 Module 的 owner 是架構。帳本、可見性、輸入緩衝是網路自己的狀態（不判定、可重建）。判定資料的權威照舊在 Match 的 World（AGENTS §3 狀態的權威）。
7. **為什麼不能更小**：
   - 只做傳輸、不做複製：權威與回滾留的空位沒人接，PvP 移植時又要一次設計。
   - 先做複製、不做連線：沒有握手就沒地方比一致性指紋。
   - 本線照「薄的開始」：每一步只實作點名功能直近要的部分，其餘只定 Contract 的形狀（AGENTS §2）。

---

## 6. 規模與預算（AGENTS §2 根據 3）

- **已知的規模**：
  - 4 人 PvP（使用者），之後可能更多；
  - AI 小怪 ≤20、Boss 1（pv10）；
  - 數百個互動物件（pv10 的結構 2，架構師的估計）；
  - 64 Hz（時間 Module 預設步長 15.625 ms）；
  - 地圖 500×500 兩層。
- **狀態大小的估算**（照 master 的 protobuf 欄位推，P8 允許）：

  | 一筆 | 欄位 | 量化前、varint 後約 |
  |---|---|---|
  | `PlayerState` | 21 欄：浮點 7、整數與 enum 12、bool 3 | 60～80 B |
  | `CombatState` | 14 欄，多為 u32 | 約 40 B |
  | `PlayerAnimation` | 7 欄 | 約 20 B |

  - 4 人全部變動：約 0.5～0.6 KB／tick／Peer。
  - 加上數百個會動的物件（每個約 15～20 B）：數 KB，**放不進一包 1200 B**。
- **由此推出的要求**：一個 tick 可以分多則（C-RP3）、要優先度（C-TR2）、要相關性（C-VIS1，第 5 步）。
- **預算**：
  - 每個 Client 的頻寬上限沒有點名，是 Q4。
  - 本線的判定只量**確定的大小**：同一個情境逐位元組相同、可重現，不靠計時。
  - 第 3 步結束時在一台機器（macOS arm64）量收發 System 的 CPU 時間，記錄、不判定（G-8）。
  - 擴到別的平台（Windows Client、Linux Match）之前，把數字交使用者對照預算。

---

## 7. 給 threads／io 重審卡的需求（經主會話轉）

1. `core/os` 加非阻塞的 UDP socket（D-1）。threads／io 不必提供。
2. 本線的收發在套用點的 System 裡、在跑那個 World 的執行緒上做，非阻塞，不開 IO 執行緒。
3. 傳輸物件放 Product 一份的共用處。之後若要專門的 IO 執行緒，同步是傳輸物件自己的事（O-15）。本線不需要 threads 的新原語。
4. 編碼與解碼用自己的小緩衝（`std::span<std::byte>` 的讀寫），**不依賴** `core/io` 的 stream。若重審把 `SpanStream` 的形狀改成適合 datagram，再接。

---

## 8. 測試與判定（先宣告；結果出來之後不放寬，AGENTS §14.1）

所有判定用記憶體裡的 Link（`cpu` 標籤，CI 三平台）。G-7 用真的 loopback UDP。掉包、重複、亂序用固定的種子，可重現。

| 判定 | 內容 | 步 |
|---|---|---|
| G-1 單機零成本 | 選了網路 Module、World 沒有角色：Registry 的 System 數、Resource 數、Product 一份的共用處，和不選網路 Module 時完全相同；沒有 socket | 1 |
| G-2 傳輸 | 掉包 20%、重複 10%、亂序（種子固定，跑 10 000 則）：<br>• Ordered Reliable 每則恰好一次、照順序；<br>• Sequenced Unreliable 不交出比已交出更舊的；<br>• 分片的訊息只在全到時交出；<br>• **件數的突き合わせ**：送的＝交出的＋計數的丟棄（每個理由），逐項相等 | 1 |
| G-3 握手 | 四項各自不同時，各回對應的理由；挑戰回錯拒絕；產品拒絕回 `Refused` | 1 |
| G-4 存活 | 雙向有資料時不送 keepalive；閒置時照間隔送；收不到超過門檻依序進「中斷」「斷線（Timeout）」；時間由測試給（傳輸層不讀時鐘） | 1 |
| G-5 複製 | 伺服器的 World 跑 N 步（生成、刪除、加減元件、改值、實體互指），不預測的 Client 經有損的 Link：<br>• 每個確認的 tick，兩端的 digest 相同；<br>• 實體編號相同；<br>• 事件不比它指到的實體先到；<br>• 件數對上 | 2 |
| G-6 輸入 | 掉包下，Match 在每個 tick 拿到的輸入和 Client 送的一致（冗餘蓋得住的範圍）；缺的沿用並計數；窗外丟並計數；單次觸發只觸發一次 | 3 |
| G-7 UDP | loopback 上 G-3、G-5 的縮小版通過（三平台；Windows 的 `WSAECONNRESET` 不讓收件中斷） | 1、2 |
| G-8 大小與成本 | 固定情境的每 tick 位元組數：記錄，兩次執行逐位元組相同；CPU 時間只記錄 | 2、3 |
| G-9 重連 | 斷線後新連線：Match 重送整份，Client 的 digest 在確認後和 Match 相同；舊連線的輸入被窗擋掉並計數 | 3 |
| G-10 預測 | 選回滾的 Client：所有權威值到齊後，在確認點的 digest 和 Match 相同；網路生成的實體回滾到出生之前不被刪掉（`Entered`） | 4 |
| G-11 相關性 | 看不到的實體不送；再看到時用同一個編號；私人元件只送給指定的 Peer；回滾不把 `NotSent` 當「沒變」 | 5 |

- **scenario test**：第 2 步起加一段「伺服器＋兩個 Client」（2D 的樣本，`v8-arch-gyo2-migration-needs-consent` 允許自建樣本）。綁實作方式的守衛在那段通過之後立（AGENTS §3）。
- **突變**：每步只跑相關的（記憶 `mutation-rerun-scope`）。只加既有突變捕不到的誤差（§14.5）。不為誤用加守衛（P15）。守衛 Test 與受入判定的突變清單，交不同模型的敵對式檢查（§14.2）。

---

## 9. 要使用者定的

只列推翻舊稿、和三家都不同、或沒點名的。

### Q1 實體編號：兩端同號（建議）或對照表（三家的做法）

- **同號（建議）**：
  - 伺服器配的實體在 Client 上用同一個編號，配發器的範圍分兩半（D-4）。
  - 理由：GYO 的配發器不重用，可以安全同號；C++ 沒反射，對照表要每個作者替每個元件寫對映函式（機制分散在各實作）；兩端的 `RollbackInbox`、digest、日誌講同一個編號。
  - 代價：預先生成的實體要另配；伺服器最多 2^31−1 個。
- **對照表**：照 replicon／lightyear／Unity。Client 用自己的範圍，網路 Module 持雙向表；元件裡的 `EntityId` 要作者給對映函式。
- 生成世界時「先動配發器的範圍」（P13）是兩者共同的；同號時要分範圍，對照表時不用。stamp-order-spike:36 寫過「編號就是網路上的編號…不另設對照表」，權威第 4 批把「id 要不要對映」留給網路，所以交你定。

### Q2 Gateway：行程內的 Link（建議）或另開無狀態的 Gateway 行程

- **行程內（建議）**：每種外部 Client 是一個 Link，Link 之上一套協議，Match 自己持帳本（D-3）。
  - 系譜：lightyear 的 multi-transport。
  - 原意的三條都在：多種 Client、一套內部協議、邊緣無狀態。
  - 內部協議每一筆都帶冪等的鍵（D-4），之後若要另開 Gateway 行程，它只要原樣轉送 datagram 與來源，協議不用改。
- **另開行程**（master 的系譜，Go）：部署上多一個行程，Match 與 Gateway 之間多一條內部連線。
- 本線只做行程內，另開行程不在本線。

### Q3 wire 的格式：網路 Module 自己的編碼＋協議雜湊（建議）或 protobuf

- **自己的編碼（建議）**：
  - 三家都是登記順序的 id＋各型別的編碼函式（replicon postcard、Unity codegen）。
  - 握手比協議雜湊，也比權威的一致性指紋。
  - 標頭與封包的格式寫成文件（`docs/architecture/net-wire.md` 之類），可以 diff。
- **protobuf**：11-10 的網路設計稿 N2 認為「.proto 可以手寫、可以 diff，更適合 GYO」；master 用它。
  - 代價：ECS 元件的複製要每個型別寫 .proto 與轉換。
  - protobuf 是第三方庫，要一個 adapter；看版本更新。
- N2 是架構師的看法、不是使用者的決定，但會推翻舊稿，所以交你定。

### Q4 沒點名的功能：本線做不做

建議都不做，只保留不擋路的形狀：

| 功能 | 建議 | 保留的形狀 |
|---|---|---|
| 加密與認證（connect token） | 不做 | 握手的 token 欄 |
| host、listen server | 不做 | 記憶體 Link（測試用，就是同一行程的那條路） |
| WebSocket、瀏覽器 Client | 不做 | Link interface |
| 每個 Client 的頻寬上限 | 不做；第 3 步把 G-8 的數字交你 | 通道的優先度 |
| 執行中的 digest 比對 | 不做；只在測試比（G-5、G-10） | — |
| 內插（別人的角色平滑地動） | 第 5 步之後再問 | 輸入帶畫面的時間（F2） |
| 觀戰者 | 不做 | Peer 與 Player 分開 |
| P2P | 不做（權威是佔位） | Link 與通道不分角色 |

---

## 10. 本卡照 L17 判的（不是使用者的話，揭露）

- 傳輸層自己做（D-2）：三家裡 lightyear 自做、Unity 用自家的、replicon 不做，沒有分歧。
- socket 放 `core/os`（D-1）：照 P3 與 os 的原意。
- 套用點、不用交換函式（D-3）：照 O-12 的規則。
- `default=yes`、不選角色就沒有（D-10）：照 P16 與權威、回滾的先例。
- 值的分流、帳本、確認點（D-5、D-7）：照 replicon；R-8 的前提已經是這個。
- 輸入的窗用 `Long`（64）：系譜 lightyear ±64、Unity 64。
- 名字（`NetEntry`、`DatagramSocket`、`NetTransport` 等）在實作時照命名規範定，這裡是暫名。

---

## 11. 實作的步驟（動工前核准、使用者說「開始」之後）

所有步驟都是**上段**：觸及協議、資料形式、公開介面（navigator「上段と下段」）。

| 步 | 做什麼 | 實作者 | 審查者 | 檢證者（§14.4） | 判定 | PR |
|---|---|---|---|---|---|---|
| 0 | `core/os` 的 UDP socket（三平台）＋loopback 測試 | Opus／high | Opus／high | Opus／high | G-7（socket 部分） | PR-1 |
| 1 | 傳輸層（標頭、ack、通道、分片、記憶體 Link）＋連線（握手、存活、斷線理由、丟棄計數）；`net` 登記列、`default=yes`、不選就沒有 | Opus／high | Opus／high | Opus／high | G-1～G-4、G-7 | PR-1 |
| 2 | 複製（登記、結構與值、帳本、同號與範圍、分包）＋伺服器訊息綁 tick；scenario「伺服器＋兩個 Client」 | Opus／high | Opus／high | Opus／high | G-5、G-7、G-8 | PR-1 |
| 3 | 輸入（冗餘、緩衝、`Ingress`、畫面的時間、單次觸發）＋對時（時間 Module 的入口，形狀先給使用者看）＋重連；量 CPU（G-8） | Opus／high | Opus／high | Opus／high | G-6、G-8、G-9 | PR-1 |
| 4 | 預測的 Client：寫 `RollbackInbox`、確認點（MC-4）；回滾實作 `Entered`、`Exited` | Opus／high | Opus／high | Opus／high | G-10 | PR-2 |
| 5 | 相關性：每 Peer 的可見性、元件範圍（私人資料）、優先度；回滾實作 `NotSent` | Opus／high | Opus／high | Opus／high | G-11 | PR-2 |

- **合併前**：
  - Fable／high 做唯讀的設計審查（每個 PR 一次）。
  - 守衛與受入判定的突變清單，交不同模型的敵對式檢查（作者是 Opus 時用 Fable／high）。
- **PR**：
  - PR-1（第 0～3 步）結束時，一個不預測的 Client 能連、能玩、能重連。這是「基盤的反復在積上去之前做完」的點，量的數字交使用者後再開 PR-2。
  - base 都是 `gyo2`，等使用者說「合併」才合併。
- **ギア**：照記憶 `xhigh-only-for-investigation`，實作、審查一律 high。卡住要調查時，先停下回報再談。
- **dev_log**：每步一份，和那一步同一個變更。

---

## 12. Design Invariants 與 Replacement Criteria（草稿；最終文字在合併前核准定稿）

新頁 `docs/architecture/target/v2/net.zh-Hant.md`：

- **NT-1 職責**：網路 Module 的職責是
  1. 連線（握手、存活、斷線理由）；
  2. 傳輸（標頭、ack、通道、分片）；
  3. 複製（登記、結構與值、帳本、可見性）；
  4. 輸入的收送與緩衝；
  5. 伺服器訊息的收送；
  6. 對時的量測與給時間 Module 的輸入；
  7. 丟棄的計數。

  不做判定、不存判定資料的副本、不回滾、不認得 Player、不認得特定產品。
- **NT-2 一套內部協議**：Link 之上只有一種協議；Link 只有 bytes 與連線狀態。
- **NT-3 冪等的鍵**：每一筆帶第 4 節 D-4 表的鍵，重複收到不改結果。
- **NT-4 判定用的時間只有 tick**：牆鐘只在 IO 層，不寫進判定資料、digest、回滾歷史。
- **NT-5 確認點**：只在 D-7 的兩個條件成立後寫 `ConfirmedThrough`。
- **NT-6 不選就沒有**：不選角色時，World 和沒有這個 Module 時相同（G-1）。
- **NT-7 依賴**：網路依賴權威、回滾、時間、entity_ids；它們都不依賴網路。外部組件只經 `core/os`。
- **NT-8 丟棄有理由、有計數**。
- **NT-9 編號**（照 Q1 的結果寫）。

**Replacement Criteria**（寫在積上去之前）：

- **RC-NT1**：職責清單（NT-1）要多一項，或判不出某樣東西屬網路、權威、回滾、時間哪一個。
- **RC-NT2**：有點名的產品在一個 World 的生命內要超過伺服器那一半的編號。
- **RC-NT3**：要讓複製正確，必須在 Kernel 或編排層加網路專用的欄位、分支或觀察點。
- **RC-NT4**：每加一種 Link，上層（傳輸、複製）就要改。
- **RC-NT5**：同一個回避策（對映、去重、狀態的副本）要放進兩個以上的地方，或 Link、中繼開始需要遊戲的狀態才能正確。
- **RC-NT6**：G-8 的數字超過使用者給的預算，而且相關性與優先度都用上了。

---

## 13. 範圍的限界

- 預先生成（Client 預測出來的實體與伺服器的配對）、內插、lag compensation 的歷史姿態（產品／collision）、加密、host 模式、另開 Gateway 行程、WebSocket、P2P：不在本線（Q4）。
- 「`networking-systems.svg`」不動（`-network-diagram-after-mechanism`）。圖裡有 master 的常數，本頁一律不引用。
- 批次表第 6 列寫「master 的 `engine/net`、`services/gyo_gateway`（參考）」：本頁照 P8，只取結構與坑。master 的產品 Gateway 在 `apps/object_fps_pvp/gateway`，`services/gyo_gateway` 是公共層。
- 數值（存活的門檻、冗餘的份數、對時的增益與跳 tick 的門檻、斷線冗餘送的包數）：在各步的宣告裡從參考實作與 64 Hz 推，揭露系譜，不用 master 的數。
