# 事前宣告：網路 Module（`v8-arch-gyo2-net-module`）

狀態：修訂 2。
- 修訂 0 經敵對式檢查（Fable／high：擋核准 5、應改 12、小事 5），處理對照在第 15 節。
- 修訂 1 經窄複查（Fable／high：擋核准 2、應改 5、小事 2），處理對照在第 16 節。
- 修訂 2 加了狀態機卡轉來的「狀態怎麼複製到 Client」（Q8）。

**2026-11-22 使用者動工前核准**（「照建議，全部核准」；第 17 節）。2026-11-22 使用者說「維持，開始」，實作開始。

性質：批次表第 6 列「網路 Module」的事前宣告（AGENTS §2 基盤 Architecture、§3 Architecture Delta、§4 系譜）。網路是機制、在搬產品之前做（`v8-arch-gyo2-net-is-mechanism-first`）；開卡：`v8-arch-gyo2-next-batches-start`（2026-11-22「網路 Module 可以開卡做了。」）。

讀的版本：`gyo2` eacab4d7（含 #270 交換點、#272 生成世界）；主會話的 HANDOFF 在 `origin/claude/gyo2-time-design` 817f78c5。

證據（主 checkout `build/evidence/gyo2-net-module/`）：
- 偵察原文五份，各帶檔名行號：`recon-gyo-docs.md`、`recon-lightyear.md`、`recon-bevy_replicon.md`、`recon-unity-netcode.md`、`recon-master-structure.md`；
- 修訂 0 的敵對式檢查：`adversarial-rev0-fable.md`。

參考實作的版本：
- lightyear 736e1695（0.31；複製層已換成 bevy_replicon 0.45）；
- bevy_replicon d80c849（0.45）；
- Unity Netcode for Entities 7.2.0（`com.unity.transport` 本機沒有，transport 的預設值查不到）。

---

## 0. 一個例子（先用遊戲講）

**PvP 射擊**：Match 是伺服器，四個 Client 連進來。

- **連線**：
  - Client 經 UDP 送「我要連」，Match 回挑戰。
  - 兩邊比協議雜湊、步長、配發器範圍，Client 也模擬時再比一致性指紋。對不上就拒絕，並寫明是哪一項。
  - 連上後，Match 的 World 裡多一個 **Peer 實體**，帶這條連線的帳本與可見性。產品拿握手帶來的使用者 id，對到自己的 Player（Peer 與 Player 分開，11-11）。
- **每一步**：
  - Client 把「第 100 步：前進、開槍、我畫面上看到的是第 94 步多一點」連同前幾步的輸入一起送出（冗餘，掉一包也不缺）。
  - Match 在第 100 步把它寫進那個玩家實體的輸入元件；沒到就沿用上一步的，並記一筆「缺了」。
  - Match 跑完後，網路把這一步改過的值送給每個 Client：
    - 生成、刪除、加減元件走可靠有序；
    - 值走不可靠、只送最新的，每個 tick 都送一則（沒變也送空的）。
- **Client 收到**（Client 分三種，D-5）：
  - 只顯示的 Client（例：觀戰）：把值直接寫進 World。
  - 模擬但不回滾的 Client：本機照跑，收到的值直接蓋上。
  - 模擬又回滾的 Client：權威值寫進 `RollbackInbox`，收齊後說「第 100 步以前都收齊了」，回滾 Module 比對、重跑。
  - 「門開了」「誰擊殺了誰」這類事件走可靠的事件串流，不會比它指到的實體先到。
- **斷線**：
  - 資料流本身就是心跳。一陣子收不到就進「中斷」，再久就斷線。
  - 重連是新的連線：產品換一個新的 Client World、重新握手、重新對時，Match 重送一份完整的世界。

**2D 橫版單機**：World 不選連線的角色，網路什麼都不登記、不建 socket、不掛 System。作者一行網路的程式都不寫。

**格鬥遊戲的 P2P**：權威是佔位（AU-E1）。網路的 Link 與輸入的形狀不分伺服器、Client，留著 P2P 交換輸入的位置；本線不實作。

---

## 1. 前提表

每條：誰定的、哪些決定掛在它上面。這些前提一變，掛在上面的決定要重看。

| # | 前提 | 誰定的（出處） | 掛在上面的決定 |
|---|---|---|---|
| P1 | 網路是機制、不是某個遊戲的附屬品，搬產品之前做 | 使用者 2026-11-19 `-net-is-mechanism-first` | 本線存在；Feature 從會用它的產品推（第 2 節） |
| P2 | 目標是通用引擎：PvP 的強化不影響別的遊戲，單機不因此變複雜 | 使用者 2026-11-14 `v8-arch-engine-scope-general` | D-10、G-1 |
| P3 | 網路是 Module，放 Engine 內；外部組件只在一個 adapter；平台差異放 `core/os` 的 `src/sys/<平台>` | 使用者 2026-11-16 `-module-plugin-owner-definition`；AGENTS §6；`-os-in-core-platform-dirs` | D-1、D-2、Q5 |
| P4 | `core/os` 的現行約定：socket 留在用它的產品；哪些 socket 錯誤是暫時的由呼叫者決定（DI 7）；`os-net-errors` 沒有觀測的壓力（D78） | 使用者 2026-10-28 的 OS 收口（D78）；`docs/architecture/os.md:46`、`:334`、`:377` | D-1 要改這條，交使用者（Q5） |
| P5 | 權威是作者選的策略式 Module；伺服器權威下判定資料以伺服器為準，規定在權威、搬運在網路、重算在回滾（DI-A 7）；一致性指紋收寫判定資料的 System，只扣 `OneEndOnly` | 使用者 2026-11-16 `-authority-is-game-design`；authority.zh-Hant.md DI-A 5、DI-A 7；`ServerAuthorityStrategy.cpp:309-315` | D-5、D-6、D-11 |
| P6 | 回滾不認得網路（R-3）；權威值的入口 `RollbackInbox` 的形狀 R-8；**「收齊」只算 snapshot 通道，可靠事件串流不算進確認點**；步外生成要不要回滾「等 Client 那一批」；輸入紀錄只有 Resource | 使用者 2026-11-17／11-20 核准；rollback.zh-Hant.md :53、:76-86、:102-103；`authority-rollback-declaration.zh-Hant.md:319`；`Registrations.hpp:314` | D-5、D-7、D-11；Q6 |
| P7 | Gateway 原意：外部可接多種 Client、內部一套抽象協議、Gateway 無狀態；內部協議保留身分與冪等 | 使用者 2026-10-15 說明（記憶 `gateway-original-intent`；repo 只有 HANDOFF 與批次表引用） | D-3、D-4、Q2 |
| P8 | 斷線偵測與短線重連只在 UDP 上，資料流當心跳，重連＝重新對時；不加 TCP 心跳 | 使用者 2026-10-23（記憶 `udp-only-liveness`） | D-8 |
| P9 | 權威／回滾不參考 master 的算法、常數、數據；09-25 dev_log 不引用；11-01 後的數據只當比較基準；master 的 protobuf 只估狀態大小 | 使用者 2026-11-20 `-authority-rollback-no-master-reference`、`-budget-baseline-research` | 本頁的常數只寫參考實作的系譜或推導；第 6 節 |
| P10 | 時間只來自 tick 與被記錄的 boundary 輸入（歸伺服器權威策略）；Client 對時走倍率、步長不動 | 使用者 K.3-11（11-11）、11-17 改寫 `-clock-out-of-kernel`；時間設計 3.3 | D-9 |
| P11 | 外部服務只經交換點進出 World；套用點是 Module 的外部服務在幀裡進出 World 的預設；收件在回滾決定（`PreUpdate`）之前（`First`）；傳輸物件放 Product 一份的共用處，同步是它自己的事 | 使用者 2026-11-22 核准的交換點（O-12、O-13、O-15） | D-3、第 7 節 |
| P12 | **Peer 與 Player 分開**：Peer 是獨立的實體，帶每收件者的帳本與可見性；Link 是外部的 Resource，不是實體 | 使用者 2026-11-11（`plans/kernel/networking.zh-Hant.md:75-79`；pv10-requirements 追加） | D-3、D-5、D-11 |
| P13 | 11-10 的網路設計稿留給使用者的兩件：可靠串流的帳本放 Match 還是 Gateway；要不要「結構可靠、值不可靠」的分流 | 架構師 2026-11-10 稿 §4（`networking.zh-Hant.md:83`） | Q2、Q3 |
| P14 | **LIM-7**：編號只由權威端給；只在本機存在的實體不和權威共用編號空間，**怎麼分開由產品決定** | 使用者 2026-11-15（`docs/architecture/target/limits.zh-Hant.md:17`） | D-4、Q1 |
| P15 | Engine 不為「斷線重入用同一個編號」加機制；重入的資料可能在 Engine 外的服務 | 使用者 2026-11-15 `-nested-same-id-respawn-not-engine` | D-8 |
| P16 | 預設的配發器 `RangeAllotter` 單調、不重用、不回繞；網路的編號對映不歸它（EI-1） | 使用者 2026-11-22 核准生成世界 | D-4 |
| P17 | 回滾入口替多通道「留設計」（MC-1～MC-6） | 使用者 2026-11-20 `-authority-rollback-multichannel-shape` | F18、D-7 |
| P18 | 協議設計時點名的功能逐一推 wire 需求；推不出的列未決，不寫成假設 | 使用者 2026-10-31（記憶 `protocol-design-covers-named-features`） | 第 2 節、Q7 |
| P19 | 不為防誤用加守衛；守衛只守寫明的契約；底層的設計合不合理靠每批的設計審查 | 使用者 2026-11-21／22（記憶 `no-guards-against-misuse`、`design-review-not-guards`） | 第 8 節 |
| P20 | Module 預設全帶、產品可以關 | 主會話照 L17 判（批次表「不擋路的小事」） | D-10 |

「網路複製的外來編號也要先動配發器的範圍」（`world-gen-merge-approval.zh-Hant.md:85`）是生成世界的設計審查記下、交之後的批的事，**不是使用者的決定**；本頁在 D-4 處理。

---

## 2. Feature → wire 需求（AGENTS §2 Derivation Order、P18）

### 2.1 點名的功能從哪裡來

1. **使用者確認的 pv10 功能清單**（`docs/object_fps_pvp/plans/v8/pv10-requirements.md`，使用者 2026-11-07「就這些」）：七項，加 2026-11-11 追加兩項（Match 的 ready 事件、Peer 與 Player 分開）。
2. **master 的 PvP 現有功能**：使用者 2026-11-19「我們有一個完整遊戲作為對照」（`-infrastructure-before-products`），產品會回來。只取「遊戲做了什麼」，不取算法與常數（P9）。hitbox 是使用者 2026-10-31 點名的（pv8 的教訓）。
3. **使用者在 GYO2 點名的**：
   - 4 人 PvP（`-budget-scale-depth`）；
   - 「master 不單單是 4 人 PVP」（`-vision-scale`）；
   - 地圖 500×500 地面＋地下（`-map-500-two-layers`）；
   - 斷線重連與退出重入（`-nested-same-id-respawn-not-engine`）；
   - 格鬥 P2P 佔位（`-authority-is-game-design`）；
   - 跨時鐘事件要兼容網路（`-cross-clock-events-open`，例子是射擊和聲音）；
   - 回滾入口替多通道留設計（`-multichannel-shape`）；
   - 分段載入（`-streaming-load`）。
4. **研究卡整理的規模**（不是使用者的話，來源是 master `entity-requirements.md`，11-01 定的產品需求）：之後可能 8 人、同時約 400 個實體（AI 20、Boss 1、物品 200、門 50…）、視距 100 m、Client 同層看得到約 315 個、每 tick 狀態約 4.9 KB；pv10 的「設計時的規則」：事件每局約兩萬件。只當規模的根據（AGENTS §2 根據 3），不當設計。

### 2.2 推導表

用它的 Product：Match＝伺服器的 World；Client＝玩家的模擬 World。

| # | 功能（出處） | 用它的 Product | wire 需求（欄位／通道／單位） | Contract | 參考實作的對照 | 步 |
|---|---|---|---|---|---|---|
| F1 | 移動與預測（master PvP；rollback 頁的例子） | Client 送、Match 收；Client 模擬 | 輸入訊息：`end_tick`（u32，Match 的 tick）＋連續 N 步的輸入（冗餘）；不可靠 | C-IN1：輸入以（Peer, tick）為鍵，重複送是冪等的；輸入是**玩家實體上的元件**，兩端相同 | lightyear：`end_tick`、冗餘 5、ActionState 是 entity 上的元件；Unity：`ICommandData` 是 entity 上的 buffer，一包帶當前＋前 n 份 | 3 |
| F2 | 射擊與命中、hitbox 部位（master PvP；使用者 10-31 點名 hitbox） | Client 送、Match 判 | 每筆輸入帶**畫面的時間**：玩家按下時畫面看到的 Match tick＋小數（小數的編碼在第 3 步照 lightyear 的 overstep 定）。部位與命中是判定資料，經複製回去。hitbox 的歷史姿態由產品／collision 存，網路只帶時間 | C-IN2 | Unity：command 標頭帶 interpolation delay；lightyear：輸入訊息帶 `interpolation_delay`、tick＋overstep | 3 |
| F3 | 互動物件：門、拉桿、陷阱、生成、邏輯組合（pv10 #1） | Match 送、Client 收 | 實體出現、消失、加減元件走**可靠有序**；狀態值走不可靠；互動請求走輸入（F1 的鍵） | C-RP1：結構與值分流（Q3）；值的訊息帶「至少要先套用到哪個結構 tick」 | replicon：Updates（Ordered）／Mutations（Unreliable、min required tick）；lightyear：Actions／Updates | 2 |
| F4 | 物品／容器／背包，內容只給開啟者（pv10 #2） | Match 送給特定 Peer | **每收件者的元件範圍**：某實體的某元件只送給指定的 Peer（私人資料） | C-VIS2 | replicon：visibility scope 到 components；Unity：relevancy 只到 ghost | 5 |
| F5 | 技能四類＋戰術動作（pv10 #3） | Client 送、Match 判 | 放技能是「某 tick 要做某事」，走輸入；單次觸發不因掉包重複或漏掉；冷卻是私人狀態（F4）；結果是事件或實體 | C-IN3：單次觸發以（Peer, tick）去重 | Unity：`InputEvent`；pv10 結構 4 | 3 |
| F6 | 寵物＝類似裝備（pv10 #4） | Match 送 | 玩家的表現狀態，一般的元件值 | 同 C-RP1 | — | 2 |
| F7 | AI 小怪 ≤20、Boss 1（pv10 #5、#6）；約 400 個實體（研究卡） | Match 送 | 一包放不下：一個 tick 的值分多則、每則帶「這個 tick 共幾則」、實體的資料不拆；要優先度與相關性 | C-RP3、C-VIS1 | replicon：依大小切 mutation、priority；Unity：importance、partial snapshot | 2（分則）、5（相關性、優先度） |
| F8 | 對局規則：計分、擊殺訊息、勝負與結算（pv10 #7） | Match 送 | **每收件者的可靠有序事件串流**；事件帶它依賴的結構 tick，不比它指到的實體先到 | C-MS1 | replicon：server message 綁 tick、`MessageQueue`；N11 | 2 |
| F9 | Match 的 ready 事件（pv10 追加） | Match 送 | ready 是一般的伺服器訊息（F8）；握手的「接受」帶 Match 的當前 tick 與步長 | C-CN3 | Unity：`ServerApprovedConnection` 帶 tick rate | 1、2 |
| F10 | Peer 與 Player 分開；觀戰等非 Player 的 Peer（P12） | Match | Peer 是 Match World 裡的實體（帳本、可見性）；握手帶**不透明的使用者 id**（u64），產品對到 Player；Peer 控制哪個實體由產品寫 | C-CN2 | replicon：client 是 server 上的 entity，per-client 狀態掛在它上面；lightyear：`PeerId`、`ControlledBy`；Unity：`NetworkId`＋`UniqueId` | 1 |
| F11 | 斷線重連、退出重入（使用者 11-15；P8） | 兩端 | 任何合法的 datagram 都刷新存活；只有閒置時才送 keepalive；逾時就斷線（Timeout）；重連＝新的握手（帶使用者 id）＋新的 Client World＋重新對時＋完整的世界 | C-CN4、C-CN5 | lightyear：送 keepalive 前先看距上次送出夠不夠久（`server.rs:893`）；replicon：新 client 的 ticks 空、重送整份 | 1（存活）、3（重連） |
| F12 | 對時（master PvP；時間設計 3.3） | Client | 標頭帶送端的 tick；RTT 由封包確認推；Client 調倍率追 Match，差太多就跳 tick | C-TS1 | lightyear：header 帶 tick、`RemoteTimeline`、1.05 倍、>10 tick snap；Unity：command age 推 time scale、>10 tick 跳 | 3 |
| F13 | 模擬的 Client 收權威值、回滾（DI-A 7） | Client | 「第 C 步以前收齊」要能從 wire 判斷：每個 tick 每個 Peer 都有值訊息（沒變也送），帶「共幾則」 | C-RP4 | lightyear checkpoint 不變式（`rollback.rs:313-316`）；replicon `track_mutate_messages`（`server.rs:1141`） | 4 |
| F14 | 4 人 PvP、之後可能 8 人 | 兩端 | Peer 數不寫死；MTU 1200 | C-TR3 | lightyear `DEFAULT_MTU = 1200` | 1 |
| F15 | 一致性：兩端判定部分相同才開打；步長放指紋或握手（authority 4 延到這裡） | 兩端 | 握手比：協議版本、網路的登記雜湊、步長、配發器範圍；Client 模擬時（有權威策略）再比一致性指紋 | C-CN1 | Unity：四個 hash 不符斷線；replicon `ProtocolHash` | 1 |
| F16 | 跨時鐘事件要兼容網路 | 兩端 | 本線只保證：伺服器訊息綁 Match 的 tick 送到 Client，寫進 Client 的 `Message<M>`；跨時鐘那一半照時間設計 6.4，不在本線 | 同 C-MS1 | — | 2 |
| F17 | 格鬥遊戲 P2P（佔位） | — | 不實作；Link 不分角色，輸入的形狀（F1）兩個方向都能用 | 不定新契約 | lightyear `NetworkTopology::P2P` | — |
| F18 | 回滾入口的多通道留設計（MC-1～MC-6） | Client | MC-1 晚到的值寫進入口不丟；MC-3 值三態（有值、確定刪除、這份沒帶）；MC-4 確認點；MC-5 全域確認點；每筆值帶自己的 tick | 對到 D-7 | — | 4、5 |
| F19 | 分段載入（`-streaming-load`） | Match 送 | 段的實體進出是可見性的進出（C-VIS1），Client 端是生成、刪除（模擬的 Client 是 `Entered`、`Exited`） | C-VIS1 | replicon `ScopeLifetime` | 5 |
| F20 | **地圖上預先放好的實體**（門、拉桿、生成點；pv10 #1） | 兩端載同一張地圖 | 會動、會變的實體只由 Match 生成，經複製送到 Client；Client 不自己生成它們。地形等不變的資料兩端各自從 asset 讀，不是複製的實體 | C-RP5：複製的實體只由 Match 生成 | replicon、lightyear：預設 server 生成；Unity：可選的 prespawn（場景預放的 ghost 兩端同號，id 從 `0x80000000`）是最佳化 | 2 |
| F21 | **模擬的 Client 在步裡生成**（召喚、建造、投射物；pv10 #3） | Client 模擬 | 兩條路：(a) 生成的 System 只在 Match 跑，Client 等複製來（晚一個 RTT 才看到）；(b) Client 先生成（預先生成），Match 的實體到時配對 | (a) 不用新契約；(b) 要配對的表 | replicon `Signature`、lightyear prespawn hash、Unity `PredictedGhostSpawnRequest` | (a) 2；(b) 未決（Q7） |

### 2.3 推不出來的（未決，交使用者：Q7）

- 加密與認證（connect token、防偽造來源）。
- 同一行程的伺服器與 Client（host、listen server）。交換點修訂 1 已拿掉 E-9。
- 瀏覽器、WebSocket 的 Client（「多種 Client」的第二種）。
- 每個 Client 的頻寬預算。第 6 節給候選。
- 執行中的 digest 比對與不一致時怎麼辦（authority 4 延到這裡）。
- 內插（遠端的角色在畫面上用延後的時間平滑地動）。F2 的「畫面的時間」不依賴它：它定義成「玩家按下時畫面看到的 tick」，畫面怎麼來不影響這個欄位。
- F21 的 (b) 預先生成。
- 地圖實體的 prespawn 最佳化（F20 兩端同號、不經複製）。

---

## 3. 參考實作：為什麼這麼做、和我們的相似點、能用多少、要改多少

| 面向 | lightyear | bevy_replicon | Unity Netcode | 為什麼（他們寫的理由） | GYO 能直接用 | GYO 要改裝的 |
|---|---|---|---|---|---|---|
| 傳輸的抽象 | `Link` 元件：收、送兩條 byte 佇列；作者說「刻意做窄」 | 不帶 transport；backend 往兩個 buffer 讀寫 bytes＋管狀態 | `NetworkDriverStore`、pipeline 疊加；同一行程走 IPC | 換傳輸不動上層；多種傳輸同時開 | **窄的 Link：收發 datagram＋連線狀態** | Link 是 C++ 的 interface，放 Product 一份的共用處（P12：Link 不是實體）；socket 在 `core/os`（Q5） |
| 封包與可靠性 | 13 B 標頭（type、packet_id u16、ack u16、bitfield u32、tick u32）；packet id 只增、重送在訊息層；AckOnly 不引發回 ack | 不做 | Unity Transport 的 pipeline | Gaffer 式 ack 一包蓋 33 包；重送新值比重送舊包好 | **標頭五欄、訊息層重送、AckOnly** | 標頭加魔數與協議版本、World 世代（D-5） |
| 通道 | 六種＋優先度＋token bucket | 三種保證；每種 message 一條 | 4 條 pipeline | 結構要可靠、值只要最新 | **兩種：Unreliable、Ordered Reliable**（第 2 節沒有 F 列要別的） | 頻寬上限先不開 |
| 結構與值分流 | Actions（可靠有序）／Updates（sequenced） | Updates（Ordered）／Mutations（Unreliable、min required tick）；同 tick 有結構變更的實體併進 Updates；沒變的 tick 也送 | snapshot＋delta 對最多 3 個已確認的 baseline | 只送一部分會讓元件指到 Client 沒有的實體；全可靠會重送舊資料 | **replicon 的分流**（Q3） | — |
| 每收件者的帳本與確認 | 用 replicon 的 | `ClientTicks` 掛在 client entity：每實體上次確認的 tick、元件遮罩；**Client 處理完值訊息才回 `MutationAcks`（Ordered）**，伺服器收到才前推 | 每連線每 chunk 32 份 history、ack mask 4096 tick | 帳本只決定重送，不是遊戲狀態的副本 | **帳本掛在 Peer 實體（P12）、應用層的確認** | — |
| 編號 | `ServerEntityMap`（replicon）；client 用自己的 allocator | 雙向對照表；元件裡的 entity 要標 `#[entities]` 才對映 | ghostId（回收）＋spawnTick；和本地 Entity 分開 | Bevy、Unity 的 entity 會回收，兩端不可能同號 | 對照表的形狀 | **GYO 的配發器不重用（P16），LIM-7 已定「編號只由權威端給」：兩端同號（Q1）** |
| 可見性 | (entity, link)＋Room；三種失去可見的處理 | 每 client 一份、scope 到元件；三種 `ScopeLifetime`；後兩種「隱藏期間的刪除不會重放」的坑 | Relevancy 三種模式；不相關＝destroyed | 決定一律在 server | **每 Peer 一份、scope 到元件、失去可見＝刪除**（只用 `WhileVisible`） | 模擬的 Client 寫 `Exited`／`NotSent`（MC-3） |
| 輸入 | `end_tick`＋冗餘 5、64 格 ring、hold-last、伺服器只收 ±64 tick | 不做 | 當前＋前 n 份、server 缺輸入沿用上一筆、`InputEvent` | 掉包不缺輸入；擋惡意的大 tick | **冗餘、hold-last、tick 窗、單次觸發、輸入在實體上** | Client 的重跑要每個實體的輸入紀錄（回滾的延伸，第 5 節） |
| 對時 | header tick＋ping；1.05 倍、>10 tick snap、`LocalTimelineShift` | 不做 | command age 推 time scale（0.9～1.1）、>10 tick 跳 | 步長不動才不會破壞固定步 | **倍率追、太遠就跳** | 經時間 Module 的入口（D-9）；增益與門檻在第 3 步推 |
| 預測與回滾 | 單一 entity：live 值＋`ConfirmedHistory`＋`PredictionHistory` | 不做，留 `ConfirmHistory`、receive marker | 只回滾這次有更新的 ghost；上限 64 tick | — | GYO 的回滾 Module 已做（R-8） | 網路只寫 `RollbackInbox` |
| 單機的成本 | prediction 用 `run_if` 關 | 送只在 `ServerState::Running`、收只在 `Connected`；單機什麼都不跑 | — | — | **不選角色＝什麼都不登記** | — |
| 握手與協議檢查 | netcode.io：Request→Challenge→Response→KeepAlive | `ProtocolHash`（型別名與順序） | 四個 hash 不符斷線：「別讓不相容的版本玩到一半才爆」 | — | **比雜湊、拒絕寫理由** | 比 GYO 自己的項目（F15） |
| 複製的單位 | 元件 | 元件 | ghost＝entity | — | **元件的複製** | Resource 形的狀態（狀態機）三家都沒有：Q8 |
| host 模式 | HostClient | 不在同一個 world 同時跑 client 與 server（延遲的資料會蓋掉最新值） | single-world／binary-worlds host | — | 記錄，不做（沒點名） | — |

**master 的結構與坑**（只取結構，P9；`recon-master-structure.md`）：

1. **外部有損重複、內部恰好一次**。兩種送達語意的轉換放在 Gateway，它就得去重、就得有 Match 的狀態副本。2026-10-10 的事故就是 Gateway 照副本悄悄丟了命令。→ D-4：內部協議每一筆都帶冪等的鍵，任何中繼都能原樣轉送。
2. **路徑上三十多處無聲丟棄**。→ D-12：每個丟棄的出口都有理由與計數，交給上層。
3. **序號在唯一的寫出處、寫出前才指定**。→ packet id 只在 Link 送出前配。
4. **診斷不進判定**；跨時鐘域的時間戳只照抄或在同域相減。→ D-9。
5. **socket 錯誤分類留在產品**；Windows 的 `WSAECONNRESET`／`SIO_UDP_CONNRESET` 沒定案。→ D-1、Q5。

---

## 4. 設計（Contract）

每個基盤選項寫三件事（AGENTS §4）：系譜、退掉的選項與理由、需要它的 Feature 與 Product。

### D-1 UDP socket 放 `core/os`（Q5）

- `core/os` 加非阻塞的 UDP datagram socket（暫名 `DatagramSocket`）：bind、送一個 datagram 到某個位址、收一個（沒有就回「沒有」）、IPv4／IPv6 位址。
- 錯誤回中立的分類碼（`OSError`）：會阻塞、對方不可達、封包太大、位址不能用、其他。
  - **哪些是暫時的，由呼叫者（網路 Module）決定**，`core/os` 的 DI 7 不變。
- Windows 對未連線的 UDP 收件會因 ICMP 回 `WSAECONNRESET`：adapter 用 `SIO_UDP_CONNRESET` 關掉，三平台收件的行為一致。這是平台差異，放 `src/sys/windows/`。
- 網路 Module 不 include 任何平台標頭。
- **要改 os 的**：os.md 的 Out of scope（:334「socket 留在用它的產品」）、Net 那一列（:46，D78 的「沒有觀測的壓力」）。理由：網路先做（P1）是「名指的 Feature」的予見壓力（AGENTS §2）。是 Architecture Delta，交使用者（Q5）。
- **系譜**：`core/os` 原意是 OS 的 adapter（記憶 `os-layer-original-intent`）；master 的 socket 用 asio 寫在產品裡。
- **退掉的**：
  - (a) 放網路 Module 的 `src/adapters/socket/`：平台差異要放 `core/os`（P3）；socket 不是第三方庫。
  - (b) 用 asio：第三方庫，只為一個 UDP socket 加一個 adapter 不值得。
- **需要它的**：全部的 F（Match、Client）。

### D-2 傳輸層自己做，不用第三方網路庫

- 網路 Module 裡的傳輸層是**純的狀態機**：輸入是「收到的 datagram＋現在的時間」，輸出是「要送的 datagram」。不碰 socket、不讀時鐘。
- 測試用記憶體裡的 Link，加可重現的掉包、重複、亂序（種子固定），並計數它丟了幾個。
- 標頭照 lightyear 的五欄，加魔數、協議版本、**連線識別**（握手產生的隨機 32 位元）、World 世代。
  - packet id 只增，重送在訊息層；ack＋32 位元 bitfield；分片；MTU 預設 1200。
  - 連線識別不符的 datagram（例：同一個位址上舊連線還在飛的包）丟掉並計數。
- 通道兩種：Unreliable、Ordered Reliable。
- **系譜**：lightyear 的 transport（Gaffer 式，它自做）；Unity 用自家的 Unity Transport；replicon 不做。
- **退掉的**：
  - (a) ENet：連線與逾時是它自己的模型，和「資料流當心跳」、標頭帶 tick 不合。
  - (b) GameNetworkingSockets：重；加密與 relay 沒點名。
  - (c) netcode.io＋reliable.io：加密與 token 沒點名（Q7）。之後要時，握手留了 token 欄，那時再評估。
- **需要它的**：全部的 F。

### D-3 Link、Peer 實體、一套內部協議（Gateway 原意的落點；Q2）

- **Link**（interface）：送 datagram 給某個遠端、收 datagram（帶來源）、連線的狀態。第一個實作是 UDP，測試用記憶體裡的 Link。WebSocket、Steam 等是另一個 Link。
- **一套內部協議**：Link 之上只有一種標頭、通道、訊息的格式。Client 從哪個 Link 進來，Match 看到的都一樣。這是「外部多種 Client、內部一套抽象協議」。
- **無狀態的邊緣**：Link 只有 bytes 與連線狀態。
  - 去重、重送、可見性的決定在 Match World 裡的 **Peer 實體**（P12），用網路自己的元件：帳本、可見性、輸入緩衝、控制的實體。
  - Client World 裡也有一個 Peer 實體，代表它對 Match 的那條連線：帶本機的輸入緩衝與控制的實體，所以兩端的入口 System 讀寫同一組元件（D-11）。
  - 這些元件不是判定資料（不進權威登記、不進 digest 與指紋），是網路自己可以重建的狀態（AGENTS §3）。
- **傳輸物件**（暫名 `NetTransport`：Link 們、連線表）放 Product 一份的共用處（O-15），網路 Module 在 `Configure` 建。
- **接線**（P11）：
  - 每個選了角色的 World，網路 Module 掛兩個套用點的 System：收件在 `Main`／`First`，送件在 `Main`／`Last`。
  - 都非阻塞、不等別的執行緒。
  - 不用交換函式（O-12：同一件事只選一種）。
- 一個 Product 一個連線對到一個 World：產品在 `WorldDescription::options` 指定哪個 World 有角色。本線只做一個伺服器 World、一個 Client World 的情形。
- **系譜**：
  - lightyear 的 `Link`＋multi-transport（同一個 server 同時開 UDP、WebTransport、Steam）；
  - replicon 的 per-client 狀態掛在 client entity；
  - master 的「兩份 proto＋adapter」是外部、內部兩套協議，這裡改成外部只到 bytes、協議只有一套。
- **退掉的**：
  - (a) 另開 Gateway 行程（master 的系譜；Q2）。
  - (b) 帳本放 `NetTransport` 的表（修訂 0）：和 P12「Peer 是實體」不合。
  - (c) 交換函式收件：收件要在回滾決定之前、要和 System 排先後（O-12）。
- **需要它的**：全部的 F；F10、F11（Peer 與重連）。

### D-4 冪等的鍵與實體編號（Q1）

**冪等的鍵**：每一筆都帶身分，重複收到不會變成兩件事（P7）。

| 資料 | 鍵 | 重複收到時 |
|---|---|---|
| 輸入 | (Peer, tick) | 同一格寫同一個值 |
| 可靠訊息 | (連線, 通道, 訊息序號) | 收過就丟（計數） |
| 結構更新 | (World 世代, 結構 tick) | 已套用就丟 |
| 值 | (World 世代, 實體, 元件, tick) | 新的蓋舊的；比已確認的舊就丟（計數） |

**實體編號**（建議做法；照 LIM-7）：

- 伺服器配的實體在 Client 上用同一個編號（`SpawnWithId`）。這就是 LIM-7 的「編號只由權威端給」。
- **範圍由產品分開**（LIM-7「怎麼分開由產品決定」）：
  - 產品在每個 World 的 `WorldDescription::options` 放 `EntityIdsOptions`。
  - 網路不放範圍、entity_ids 不改。
  - 握手時 Match 帶自己的配發器範圍，Client 檢查自己 World 的範圍和它不重疊；重疊就拒絕（`IdRangeOverlap`，寫明兩個範圍）。
  - 範圍判斷讀 World 的 `RangeAllotter` Resource；作者換了別的配發器時，由作者用網路的選項給出範圍。
- **為什麼能同號**：`RangeAllotter` 單調、不重用（EI-2）。同一個編號不會先後指兩個實體，所以進出可見性用同一個編號安全。重連另換新的 World（D-8），也安全。
  - Match 換成別的配發器（例：會回收編號的）時，「不重用」由作者保證；回收的話，Client 可能把新實體當成舊的。
- **好處**：
  - 元件裡存 `EntityId` 的欄位不用每個型別寫對映函式。C++ 沒有反射，對照表要每個作者替每個元件寫 `MapEntities`，這就是「機制分散在各個實作裡」。
  - `RollbackInbox`、digest、日誌在兩端講同一個編號。
- **代價**：
  - Client 先生成的實體（F21 的 (b)）和伺服器的不同號，要另一張配對表（未決）。
  - 範圍要產品分；忘了分，握手時就被拒絕，看得到、不會悄悄錯。
- **對照表**（三家的做法）會推翻 LIM-7 的形式。stamp-order-spike:36 也寫了「需要對照表時觸發 World 的 RC」。所以交使用者（Q1）。

### D-5 複製（伺服器 → Client）

- **登記**：網路 Module 自己的入口（統合 :603「要不要複製是網路 Module 自己的登記」）。
  - 寫法：`NetEntry(s).Replicate<T>(name, options)`，`options` 帶編碼函式與範圍。
  - 元件照本節。Resource 形的狀態（狀態機、作者的判定 Resource）怎麼到 Client 是 Q8；三家都只複製元件。
  - 數值與 enum 有預設的編碼；其他型別由作者給編碼函式，不用位元組照抄（padding 不上 wire）。
- **wire id 與協議雜湊**：wire id 是登記的順序；協議雜湊由名字、順序、編碼的版本算出，握手時比。
- **結構與值**：照 replicon。
  - **結構更新**：每個 tick、每個 Peer 一則，可靠有序，整則一起套用。
  - **值**：自上次確認以來的最新值，不可靠，帶「至少要先套用到哪個結構 tick」。
  - 每個 tick 每個 Peer 都送值訊息：分多則時每則帶「共幾則」，沒變時送一則空的（C-RP4）。
  - 同一個 tick 有結構變更的實體，值併進結構更新。
- **確認**（應用層）：
  - Client **處理完**一則值訊息（寫進 World 或入口）才回確認，經 Ordered Reliable。
  - Match 收到確認才在 Peer 的帳本前推。
  - 封包層的 ack 只管傳輸重送，不前推帳本。所以收到之後又因緩衝滿、解碼失敗丟掉的，帳本不前推，下次會重送最新值。
- **帳本**：Peer 實體上，記每個實體上次確認的 tick 和元件的遮罩。只決定送什麼，不是遊戲狀態的副本。
- **World 世代**：Match 的 World 被替換（`ReplaceWorld`）時世代加一，標頭帶它。
  - Peer 實體在舊 World 裡，隨 World 一起沒了；新 World 裡由網路重建 Peer 實體、帳本從空開始，所以重送整份。
  - Client 收到新世代：丟掉舊世代的訊息，並發一則網路的訊息「伺服器換了 World」。換不換 Client 的 World 由產品決定（錯誤與事件只告訴外面發生了什麼）。
- **Client 怎麼寫進 World**（收件的套用點，`First`）。Client 分三種，由 World 選的策略決定：

  | Client | World 的選擇 | 登記 | 結構與值怎麼寫 | 比指紋 |
  |---|---|---|---|---|
  | K1 只顯示 | 沒有權威策略、沒有權威登記 | 網路不經權威登記任何東西（入口與收件 System 直接 `AddSystem`；否則是 AU-E2） | `SpawnWithId`、加減元件；值直接寫進元件 | 不比 |
  | K2 模擬、不回滾 | 伺服器權威、沒有回滾策略（權威頁「不預測的 Client」） | 照權威登記 | 同 K1；本機的模擬寫的判定資料被收到的值蓋掉 | 比 |
  | K3 模擬、回滾 | 伺服器權威＋回滾 | 照權威與回滾登記 | 見下 | 比 |

  **K3 的寫法**：
  - 登記成可回滾的元件（`RollbackRegistry` 裡有的）：值寫 `RollbackInbox::Value`／`Removed`，實體的出現與消失寫 `Entered`／`Exited`，照 D-7 寫 `ConfirmedThrough`。**不在 `First` 直接加減可回滾的元件**（R-2 前提 5：可回滾的資料只被步與入口寫）。
  - 不可回滾的複製元件（表現用、`NoRollback`）：直接寫進元件，同 K1。
  - 輸入元件：遠端玩家的輸入本線不送（Q7「遠端輸入」）。Client 上只有本機玩家的輸入，由入口寫（D-11）。
  - 要改回滾的已核准文字（第 5 節），第 4 步開工前另給使用者看。
- 收件、送件的 System 只在一端：K2、K3 標 `authority.OneEndOnly`（統合 L23）；K1 沒有權威登記，不用標。
- **系譜**：replicon 的 Updates／Mutations、`ClientTicks`、`MutationAcks`、`track_mutate_messages`；lightyear 的 Actions／Updates。
- **退掉的**：
  - (a) Unity 的 snapshot＋baseline delta：每連線每 chunk 32 份 history，記憶體與複雜度高；GYO 沒有 chunk 的佈局可以利用。
  - (b) master 的 latest-wins 全量快照：放不進一包（F7），沒有帳本。
- **需要它的**：F3、F6、F7、F8、F13、F20（Match、Client）。

### D-6 連線與握手

- **握手**：Client 送「請求」→ Match 回「挑戰」（隨機數）→ Client 回「回應」→ Match 回「接受」。
  - 「請求」帶協議版本、網路的協議雜湊、步長、Client World 的配發器範圍、不透明的使用者 id（u64）、token 欄（先留、不驗證）、一致性指紋（Client 的 World 有權威策略時）。
  - 「接受」帶 Match 的當前 tick、步長、World 世代、Match 的配發器範圍。
- **比對**：任何一項不同就拒絕，寫明哪一項。
  - K1（沒有權威策略，不模擬，只收值）：不比指紋。它不寫判定資料，指紋沒有意義。
  - K2、K3（有權威策略）：兩端的指紋要相同。
  - 步長放握手、不放指紋（authority 4 延到這裡的二選一）：步長是時間 Module 的設定，不是判定的 System；放握手時拒絕的理由寫得出「步長不同」。
  - 兩端都有的同名入口 System 不必標 `OneEndOnly`（D-11）。
- **產品決定收不收**：Match 在「接受」之前問產品（E2 的回傳），產品可以拒絕（例：房間滿了）。
- **斷線的理由**：`Closed`、`Timeout`、`ClosedByRemote`、`ProtocolMismatch`（帶哪一項）、`FingerprintMismatch`、`StepLengthMismatch`、`IdRangeOverlap`、`HandshakeTimeout`、`Refused`。斷線冗餘送幾包（次數在第 1 步從參考實作推）。
- **系譜**：
  - netcode.io 的 Request→Challenge→Response（lightyear）：挑戰的往返擋偽造來源的放大攻擊；
  - Unity 的四個 hash 與斷線理由的分類；
  - replicon 的 `ProtocolHash`。
- **退掉的**：
  - (a) 不挑戰、一包就接受：偽造來源可以讓 Match 對別人送大量資料。
  - (b) TCP 的控制通道：P8。
  - (c) 不比指紋、靠執行中的 digest：要到玩到一半才發現（Unity 的理由），而且執行中的 digest 沒點名。
- **需要它的**：F9、F10、F15（Match、Client）。

### D-7 確認點（MC-4；Q6）

- Client 寫 `ConfirmedThrough(C)` 要兩個條件都成立：
  1. 第 C 個 tick 的值訊息全部收到（依「共幾則」）；
  2. 這些值要求的結構更新已套用。
- 比 C 舊的值晚到：寫進 `RollbackInbox`（MC-1：不靜默丟，回滾照「C 當時或之前最後的值」用）。比回滾深度還舊的丟掉、計數。
- 可見性外的實體在 C 是「這份沒帶」（`NotSent`），不推成「沒變」（R-8）。
- 伺服器的事件訊息（F8）**不算進確認點**：一條卡住的事件串流不讓確認點停（核准的形狀）。
- **和核准形狀的關係**：
  - 核准的是「收齊只算 snapshot 通道」。
  - 本頁把**結構更新算成 snapshot 的一部分**：它和值是同一份狀態，值要等它才寫得進去（replicon 的 min required tick）。
  - 所以卡住的結構更新會讓確認點停。這是讀法，交使用者確認（Q6）。
- **系譜**：lightyear 的 checkpoint 不變式；replicon 的 `track_mutate_messages`。
- **退掉的**：只看值訊息、不等結構更新。值指到 Client 還沒有的實體時無處可寫，確認了也比不了。
- **需要它的**：F13、F18（模擬的 Client）。

### D-8 存活、斷線、重連（P8、P15）

- **存活**：任何一個合法的 datagram 都刷新存活。距上次送出超過一個間隔才送 keepalive（lightyear `server.rs:893`）。
- **Client 端的狀態**：
  - 「連線」。
  - 「中斷」：收不到超過第一個門檻，保留連線。
  - 「斷線」：超過第二個門檻，理由 Timeout。
- 門檻是設定值，用 IO 層的單調時鐘（不進判定）。預設值在第 1 步從參考實作推，揭露系譜，不用 master 的數。
- **重連**：一律是新的連線。
  - 產品先換一個新的 Client World（`ReplaceWorld`），再用同一個使用者 id 握手。網路給一則「斷線了」的訊息，不替產品換。
  - 新 World 裡沒有舊的複製實體、回滾沒有舊歷史（R-5），Match 會重送整份。
  - 斷線期間 Match 刪掉的實體不會殘留。舊的編號不會被 `SpawnWithId` 撞到（DI-KS 18）。
  - Client 重新對時。舊連線還在飛的 datagram，因連線識別不符被丟、計數（D-2）。
  - Player 怎麼接回來，由產品依使用者 id 決定（P15）。
- **系譜**：lightyear（重新 Connect、sync reset）、replicon（新 client 的 ticks 空、整份重送）；讀檔一律在新的 World（restore-entry，使用者 11-16）。
- **退掉的**：
  - (a) 網路自己刪掉上一個連線的複製實體：在模擬的 Client 上和回滾的存活集合打架，而且是 Engine 替產品決定。
  - (b) session 延續、NAT 換位址的重綁：master 有，三家都沒有（Unity 的 UniqueId 只給 host migration 標記），也沒點名。
- **需要它的**：F11（Match、Client）。

### D-9 時間與對時（P10）

- **判定用的時間只有 Match 的 tick**：標頭的 tick、輸入的 tick、值的 tick。
- **牆鐘只在網路的 IO 層**（RTT、存活、對時的量測）：不寫進判定資料、不進 digest、不進回滾歷史。跨行程的時間戳只照抄回去，或在同一端相減。
- **對時**（第 3 步）：
  - Client 從標頭的 tick 與 RTT 推「Match 現在第幾步」，算目標 tick（Match tick＋單程延遲＋安全邊際；lightyear 要留 1 tick，不然 server 會在輸入到之前先模擬）。
  - 差得不多：調倍率，步長不動。差太多：跳 tick。
  - 都經時間 Module 的函式，網路不直接寫 `Tick`（時間 RC (b)：tick 只由時間 Module 推）。
- **時間 Module 要改的**（第 5 節）：
  - 一個「只有倍率、沒有暫停」的入口，和產品的 `TimeControl` 相乘（WT-6 第 (3) 項「倍率」的範圍內）。
  - 前跳 k 步的函式（WT-3 的文字要加一句；WT-6 第 (4) 項「退回 k 步」旁加「前跳 k 步」）。
  - 對時的 System 進 `TickWriters()`，在 Client 標 `authority.OneEndOnly`，權威不必改。
  - 形狀與 WT 文字在第 3 步開工前給使用者看。若判斷 WT-6 要多一項，就是時間 RC (d)，停下交使用者。
- **系譜**：lightyear 的 `RemoteTimeline`＋倍率＋snap；Unity 的 command age＋time scale＋跳；N6。
- **退掉的**：
  - (a) 改步長：破壞固定步與回滾的等價。
  - (b) 只跳不調速：每次跳都是一次可見的跳動。
- **需要它的**：F12（Client）。

### D-10 不選就沒有（P2、P20）

- 登記檔一列：`net`，`default=yes`，`depends=world_time;rollback;authority;entity_ids`。
- **設定**：
  - 產品在 `ProductDescription::options` 放連線的設定（bind 或連到哪裡）。
  - World 在 `WorldDescription::options` 放角色：`Server` 或 `Client`；不放就是沒有角色。
- **不選時**：沒有連線設定就不建 socket、不建傳輸物件；World 沒有角色就不登記任何東西、不掛 System（G-1）。
- **依賴方向**：網路 → 權威、回滾、時間、entity_ids、編排層、Kernel、`core/os`、`core/time`、`core/base`。反方向沒有（R-3、DI-A 9、WT-4、EI-3 不變）。
- **系譜**：replicon（送只在 `ServerState::Running`、收只在 `Connected`）；權威、回滾的 `default=yes`。
- **退掉的**：`default=no`。和 P20 的「預設全帶」不合，而且不選角色已經是零成本。
- **需要它的**：P2 的單機（2D 橫版）。

### D-11 輸入（客戶端 → 伺服器）

- **登記**：`NetEntry(s).Input<T>(name, options)`。`T` 是玩家實體上的元件，兩端都是（C-IN1）。
- **權威端的登記**：作者照常寫 `authority.Component<T>(name, {.input = true})`。
  - 伺服器權威要替**元件**的輸入代為登記輸入紀錄。現在只對 Resource 做（`authority.zh-Hant.md:101`）。
  - 回滾要支援元件的輸入紀錄：每個實體按 tick 一份，重跑每一步前寫回（`Registrations.hpp:314` 只有 Resource）。
  - 兩個都是延伸、不改既有的契約，列在第 5 節。
- **入口 System**：網路 Module 在兩端都掛同一個名字、同樣存取、同樣掛載的入口 System。
  - 有權威策略的 World 經 `authority.Ingress`（重跑時不跑）；K1 直接 `AddSystem`。
  - **兩端讀寫同一組元件**：讀 Peer 實體上的輸入緩衝 `PeerInput<T>` 與控制的實體 `PeerControls`，寫控制的實體上的 `T`（D-3：Client World 也有一個 Peer 實體）。
  - Match：輸入緩衝由收件的 System 從 wire 填。
  - Client：產品經網路的函式把本機的輸入放進自己的 Peer 的緩衝；入口 System 取第 T 格寫進本機玩家的實體，送件的 System 從緩衝送出。
  - 兩端的入口 System 名字、存取、掛載都相同，所以一致性指紋相同。
- **Client**：一包帶最後 N 步（N 在第 3 步推；系譜：lightyear 5、Unity 2+2）。單次觸發用（Peer, tick）去重（C-IN3）。每筆帶畫面的時間（C-IN2）。
- **Match**：
  - 沒有就沿用上一步（hold-last），並計數「缺」。hold-last 是 Match 的判定的一部分，在入口 System 裡、在 Match World 裡（不是副本）。
  - 收的窗：`(已消耗的 tick, T + L]`；L 取回滾 `InputHistoryLength::Long`（64，系譜 lightyear ±64）。
  - 已消耗的 tick 晚到、窗外的，丟掉並各自計數。
- **Peer 控制哪個實體**：產品在 Peer 實體上寫一個「控制的實體」元件（lightyear `ControlledBy` 的反方向）。網路不認得 Player。
- **系譜**：lightyear 的 InputBuffer、ActionState；Unity 的 `ICommandData`、`InputEvent`。
- **退掉的**：
  - (a) Client 用 Resource、Match 用元件（修訂 0）：權威登記的種類兩端不同，指紋永遠不同。
  - (b) 兩端各自的入口 System 標 `OneEndOnly`：指紋就看不到輸入怎麼進判定，失去指紋的意義。
- **需要它的**：F1、F2、F5（Match、Client）。

### D-13 只在一端跑的遊戲邏輯（F20、F21 (a)）

- 例：生成門、生成召喚物的 System 只該在 Match 跑。
- 寫法：**兩端都登記同一個 System，加網路給的執行條件** `OnServer()`（另有 `OnClient()`）。條件讀 World 的角色 Resource。
  - 兩端的登記與條件的名字相同，所以一致性指紋相同；在 Client 上它不跑。
  - 不用 `OneEndOnly`。`OneEndOnly` 拿來標一般的遊戲邏輯，會觸發權威 RC 5（「作者要為了過檢查而標記一般的遊戲邏輯」）。
- **代價**：Client World 裡多一個永遠不跑的 System。
- **系譜**：lightyear 依 `NetworkTopology` 的 run condition；replicon 的 `ServerState`／`ClientState` 條件。
- **退掉的**：
  - (a) 只在 Match 登記：指紋不同，握手一律拒絕。
  - (b) 標 `OneEndOnly`：觸發權威 RC 5。
- **需要它的**：F20、F21 (a)（Match、Client）。

### D-12 丟棄與診斷

- 每個丟棄的出口有理由碼與計數，每個連線一份：
  - 太舊、窗外、已消耗、重複；
  - 解碼失敗、緩衝滿、分片逾時；
  - 協議不符、未知的來源、World 世代不符。
- 計數放在 World 的 Resource（Client）或 Peer 實體（Match），交日誌（`world_logging`）或由產品讀，不寫判定資料。
- **錯誤分級照異常分級**：
  - 解碼失敗、惡意的 tick：E2，回報、這一筆丟掉；
  - socket 壞掉：E2 回給 runner；
  - 不加 E0。
- **系譜**：master 10-10 事故的教訓（每個丟棄出口有理由並回報，`GatewayDropReport`）；replicon：server 端反序列化錯誤用 debug 級，避免被灌爆日誌。
- **退掉的**：丟棄只記日誌、不計數。測試沒有辦法對件數（AGENTS §14 的突き合わせ）。
- **需要它的**：全部的 F；判定 G-2、G-5。

---

## 5. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：P1 與第 2 節的 F1～F21。時間、權威、回滾都留了給網路的空位（`RollbackInbox`、DI-A 7、`TickWriters` 的註解、authority 4 的延後項）。
2. **現在缺什麼**：
   - `gyo2` 沒有 socket、傳輸、連線、複製；
   - `RollbackInbox` 有入口、沒有寫的人；
   - 一致性指紋沒有交換的地方；
   - 元件的輸入沒有紀錄（模擬的 Client 重跑時寫不回）。
3. **變的邊界**（逐一列，每一條都改既有文件）：

   | 改的地方 | 改什麼 | 步 | 交使用者 |
   |---|---|---|---|
   | 新 Module `engine/modules/net` | 新 Subsystem、Build Graph、登記檔多一列 | 1 | 本宣告 |
   | `core/os` | 公開介面加 UDP socket；os.md 的 Out of scope 與 Net 列改寫（DI 7 不變） | 0 | **Q5** |
   | 回滾 Module | 元件的輸入紀錄：新的登記，不改 R-1 的掛點；R-6 的「六種登記」多一種、只認 Resource 的 `InputRecord` 要延伸（`Registrations.hpp:314`） | 3 | 本宣告 |
   | 權威 Module | 元件輸入的代為登記（`authority.zh-Hant.md` 第 4 節的限界那一條改寫） | 3 | 本宣告 |
   | 時間 Module | 對時的倍率入口、前跳 k 步；WT-3、WT-6 的文字 | 3 | 第 3 步開工前給文字 |
   | 回滾 Module | 實作 `Entered`、`Exited`、`NotSent`；**改 R-8 的已核准文字**（「只看 C 當時存活」要加「進入、退出由呼叫者明說時照它」）與限界（「步外生成要不要回滾，等 Client 那一批」由本線回答：網路生成的走 `Entered`） | 4、5 | 第 4 步開工前給文字 |
   | 回滾 Module（Q8 選 (b) 時） | 入口收 Resource 的權威值（改 rollback 限界「Resource 本批只從歷史還原」） | 4 | **Q8** |
   | 狀態機 Module（Q8 選 (b) 時） | `RequestSilent` 與兩個套用函式（狀態機修訂 8 第 2.1 節留給本線） | 2 | **Q8** |
   | entity_ids、編排層、Kernel | 不改 | — | — |

4. **影響的**：`core/os`、world_time、rollback、authority、網路（新）。編排層、Kernel、entity_ids 不改。現有產品沒有（`gyo2` 上沒有產品）。
5. **依賴方向**：新邊只有「網路 → 其他」與「`core/os` 的 socket」。沒有反方向（D-10）。
6. **Ownership**：
   - 網路 Module 的 owner 是架構。
   - 帳本、可見性、輸入緩衝是網路自己的狀態，掛在 Peer 實體：不判定、可重建。
   - hold-last 是 Match 的入口 System 的判定（D-11）。判定資料的權威照舊在 Match 的 World。
7. **為什麼不能更小**：
   - 只做傳輸不做複製：權威與回滾留的空位沒人接。
   - 不做握手：沒地方比指紋。
   - 輸入不改回滾、權威：模擬的 Client 重跑時沒有別人的、也沒有自己的實體輸入。
   - 本線照「薄的開始」：每一步只實作點名功能直近要的部分；預先生成、內插、加密、host 模式、另開 Gateway 只定不擋路的形狀（AGENTS §2）。

---

## 6. 規模與預算（AGENTS §2 根據 3）

- **已知的規模**：
  - 4 人 PvP（使用者），之後可能 8 人；
  - 同時約 400 個實體，Client 同層看得到約 315 個（研究卡）；
  - 64 Hz（時間 Module 預設步長 15.625 ms）；
  - 地圖 500×500 兩層；
  - 事件每局約兩萬件（pv10）。
- **狀態大小的估算**（照 master 的 protobuf 欄位推，P9 允許）：

  | 一筆 | 欄位 | varint 後約 |
  |---|---|---|
  | `PlayerState` | 21 欄 | 60～80 B |
  | `CombatState` | 14 欄 | 約 40 B |
  | `PlayerAnimation` | 7 欄 | 約 20 B |

  - 4 人全部變動：約 0.5～0.6 KB／tick／Peer。
  - 加上看得到的實體：研究卡估約 4.9 KB／tick。**放不進一包 1200 B**。
- **由此推出的要求**：一個 tick 分多則（C-RP3）、優先度與相關性（第 5 步）。
- **每個 Client 下行頻寬的預算候選**（交使用者選，Q7）：

  | 候選 | 下行 | 推法 |
  |---|---|---|
  | (a) | 約 2.5 Mbps | 看得到的狀態每 tick 全送：4.9 KB × 64 Hz ≈ 313 KB/s。上限，壓力測試用 |
  | (b) | 約 450 kbps | lightyear 預設 token bucket 56 KB/s（預設不開）。要靠「只送變的」與優先度才放得進 |
  | (c) | 使用者另給 | — |

  選定後寫進 G-8 的判定與 RC-NT6。
- **CPU**：第 3 步結束時，在一台機器（macOS arm64）量收發 System 每 tick 的時間（4 Peer、400 實體的情境），記錄。擴到 Windows Client、Linux Match 之前交使用者對照。CPU 的門檻沒點名，不判定。
- **判定只量確定的大小**：同一個情境逐位元組相同、可重現，不靠計時。

---

## 7. 給 threads／io 重審卡的需求（經主會話轉）

1. `core/os` 加非阻塞的 UDP socket（D-1）。threads／io 不必提供。
2. 本線的收發在套用點的 System 裡、在跑那個 World 的執行緒上做，非阻塞，不開 IO 執行緒。
3. 傳輸物件放 Product 一份的共用處。之後若要專門的 IO 執行緒，同步是傳輸物件自己的事（O-15）。本線不需要 threads 的新原語。
4. 編碼與解碼用自己的小緩衝（`std::span<std::byte>` 的讀寫），不依賴 `core/io` 的 stream。

---

## 8. 測試與判定（先宣告；結果出來之後不放寬，AGENTS §14.1）

- 判定用記憶體裡的 Link，掉包、重複、亂序用固定的種子，可重現。標籤 `cpu`，L1 三平台。
- G-7 用真的 loopback UDP，標籤也是 `cpu`、L1 三平台：Windows 的 adapter 是平台程式，CI 一定要跑。
- 門檻（存活的間隔、冗餘份數等）都是設定值；判定看「在設定值的邊界上行為對不對」，不寫死數字。

| 判定 | 內容 | 觀測的時點與方法 | 步 |
|---|---|---|---|
| G-1 單機零成本 | 選了網路 Module、World 沒有角色、Product 沒有連線設定 | 唯讀登記表的 System 數、Resource 型別、元件型別，和不選網路 Module 時逐項相同；Product 一份的共用處沒有網路的物件；沒有開 socket（測試的 socket 計數 0） | 1 |
| G-2 傳輸 | 測試 Link 掉包 20%、重複 10%、亂序（種子固定，10 000 則）：<br>• Ordered Reliable 每則恰好一次、照順序；<br>• Unreliable 不交出重複的；<br>• 分片的訊息只在全到時交出 | **件數的突き合わせ**：送的＝交出的＋各理由的丟棄計數＋測試 Link 注入的掉包數，逐項相等 | 1 |
| G-3 握手 | 協議版本、協議雜湊、步長、配發器範圍、指紋（有權威策略時）各自不同時，回對應的理由；挑戰回錯拒絕；產品拒絕回 `Refused`；沒有權威策略的 Client 不比指紋 | 每個案例只改一項 | 1 |
| G-4 存活 | 雙向有資料時不送 keepalive；閒置時照設定的間隔送；收不到時，超過第一個門檻進「中斷」，超過第二個進「斷線（Timeout）」 | 時間由測試給（傳輸層不讀時鐘）；在門檻前一刻與後一刻各看一次 | 1 |
| G-5 複製（K1 只顯示的 Client） | 伺服器的 World 跑 N 步：生成、刪除、加減元件、改值、實體互指、地圖實體（F20）、事件。Client 經有損的 Link 收 | (1) **每次 D-7 的兩個條件對某個 tick T 成立、而且當時還沒套用過任何 tick > T 的值或結構時**（K1 也算，只是不寫入口），Client World 的複製元件（逐型別逐實體）和實體編號集合，等於測試替伺服器記下的第 T 步。這種時點要佔全部 tick 的 10% 以上，不到就是「觀測不成立」：停下回報，不放寬；<br>(2) 結束時伺服器停止變動，Link 排空、全部確認後再比一次；<br>(3) 事件交出時，它指到的實體在 Client 已經存在；<br>(4) 件數對上 | 2 |
| G-6 輸入 | 掉包下，Match 在每個 tick 拿到的輸入等於 Client 送的（冗餘蓋得住的範圍內）；超出的沿用並計數；已消耗與窗外的丟並計數；單次觸發只觸發一次 | 測試記下 Client 每 tick 送的與 Match 每 tick 入口寫的，逐 tick 比；件數對上 | 3 |
| G-7 UDP | loopback 上 G-3、G-5 的縮小版通過；Windows 送給沒在聽的位址之後，收件不中斷 | 三平台 L1 | 0、1、2 |
| G-8 大小 | 固定情境的每 tick、每 Peer 位元組數 | 兩次執行逐位元組相同；數字記錄，選定預算後（Q7）再判 | 2、3 |
| G-9 重連 | 斷線後產品換新 World、新連線 | Match 重送整份；Client 在確認後和 Match 相同（G-5 的方法）；斷線期間 Match 刪掉的實體不在 Client；舊連線還在飛的 datagram 因連線識別不符被丟、計數 | 3 |
| G-10 模擬的 Client | 選回滾的 Client，對一個確定性的 2D 樣本 | 前提：情境最後一段（≥ 回滾深度的步數）所有玩家送的都是預設的輸入值。遠端玩家的輸入本線不送，Client 上它們的輸入元件一直是預設值；最後一段讓兩端相同。結束時 Client 在它的第 P 步的 `Digest` 等於 Match 記下的第 P 步的 `Digest`；網路生成的實體回滾到它出生之前不被刪掉（`Entered`）；件數對上 | 4 |
| G-11 相關性 | 看不到的實體不送；再看到時用同一個編號；私人元件只送給指定的 Peer；模擬的 Client 不把 `NotSent` 當「沒變」 | 逐 Peer 記下收到的實體與元件，對照測試給的可見性規則 | 5 |

- **scenario test**：第 2 步起加一段「伺服器＋兩個 Client」（2D 樣本，`v8-arch-gyo2-migration-needs-consent` 允許自建樣本）。綁實作方式的守衛在那段通過之後立（AGENTS §3）。
- **突變**：
  - 每步只跑相關的（記憶 `mutation-rerun-scope`）。
  - 只加既有突變捕不到的誤差（§14.5）。
  - 不為誤用加守衛（P19）。
  - 守衛 Test 與受入判定的突變清單，交不同模型的敵對式檢查（§14.2）。

---

## 9. 要使用者定的

只列推翻舊決定或舊稿、和三家都不同、或沒點名的。

### Q1 實體編號：兩端同號（建議，照 LIM-7）或對照表（三家的做法）

- **同號**：
  - 伺服器配的在 Client 上用同一個編號；範圍由產品分開，握手時檢查不重疊（D-4）。
  - 這是 LIM-7（使用者 11-15）「編號只由權威端給，本機的不共用編號空間，怎麼分開由產品決定」的形式。
  - 理由：GYO 的配發器不重用、可以安全同號；C++ 沒反射，對照表要每個作者替每個元件寫對映函式；兩端講同一個編號。
- **對照表**：照 replicon／lightyear／Unity。會推翻 LIM-7 的形式，觸發 stamp-order-spike:36 寫的 World RC。

### Q2 Gateway：行程內的 Link＋帳本在 Match（建議）或另開無狀態的 Gateway 行程

- **行程內**：每種外部 Client 是一個 Link，Link 之上一套協議，帳本在 Match World 的 Peer 實體（D-3）。
  - 系譜：lightyear 的 multi-transport、replicon 的 per-client 狀態。
  - 原意的三條都在：多種 Client、一套內部協議、邊緣無狀態。
  - 內部協議每筆帶冪等的鍵（D-4）。之後若另開 Gateway 行程，它只要原樣轉送 datagram 與來源，協議不用改。
- **另開行程**：master 的系譜（Go）。部署多一個行程。
- 11-10 稿 §4 把「帳本放 Match 還是 Gateway」留給你。本線只做行程內。

### Q3 結構可靠、值不可靠的分流（建議採用）

- 結構（生成、刪除、加減元件）走可靠有序、整則套用；值走不可靠、只送最新的、帶最低的結構 tick（D-5）。
- 系譜：replicon 的 Updates／Mutations，lightyear 的 Actions／Updates，理由寫在 replicon `channels.rs:97-120`。
  - 全走可靠：遺失時重送的是舊資料。
  - 全走不可靠：只到一部分會讓元件指到 Client 沒有的實體。
- 11-10 稿 §4 留給你。

### Q4 wire 的格式：自己的編碼＋協議雜湊（建議）或 protobuf

- **自己的編碼**：
  - 三家都是登記順序的 id＋各型別的編碼函式（replicon postcard、Unity codegen）。
  - 握手比協議雜湊。
  - 標頭與封包的格式寫成文件，可以 diff。
- **protobuf**：11-10 稿 N2 認為「.proto 可以手寫、可以 diff，更適合 GYO」；master 用它。
  - 代價：ECS 元件的複製要每個型別寫 .proto 與轉換。
  - 第三方庫要一個 adapter、跟著版本更新。

### Q5 `core/os` 收 UDP socket（建議）

- 改 os.md 的 Out of scope（「socket 留在用它的產品」）與 Net 列（D78「沒有觀測的壓力」）。DI 7（哪些錯誤是暫時的由呼叫者決定）不變（D-1）。
- 理由：
  - 你給這張卡的方向是「平台差異放 `core/os` 的 `src/sys/<平台>`」；
  - 網路先做是名指的 Feature 帶來的予見壓力（AGENTS §2 基盤）；
  - master 把 asio 寫在產品裡，是 GYO2 不要的形狀。
- 會推翻 D78 的一部分，所以交你。

### Q6 確認點：結構更新算 snapshot 的一部分（建議）

- 核准的形狀是「收齊只算 snapshot 通道；可靠事件串流不算進確認點」。本頁的讀法：
  - 結構更新和值是同一份狀態，算進確認點；
  - 伺服器的事件訊息（擊殺、計分）不算（D-7）。
- 代價：結構更新卡住時確認點會停。值要等結構才寫得進去，不等也比不了。
- 你同意這個讀法，第 4 步才照做。

### Q7 沒點名的功能：本線做不做

| 功能 | 建議 | 保留的形狀 |
|---|---|---|
| 頻寬預算 | 從第 6 節 (a)／(b)／(c) 選一個 | 判定 G-8、RC-NT6 |
| 加密與認證（connect token） | 不做 | 握手的 token 欄 |
| host、listen server | 不做 | 記憶體 Link |
| WebSocket、瀏覽器 Client | 不做 | Link interface |
| 執行中的 digest 比對 | 不做，只在測試比（G-10） | — |
| 內插 | 不做；PvP Client 移植時再問 | F2 的欄位不依賴它 |
| F21 (b) 預先生成 | 不做；作者用 `OnServer()` 讓生成的 System 只在 Match 跑（F21 (a)、D-13） | — |
| 遠端玩家的輸入送給 Client（lightyear 的 rebroadcast） | 不做；遠端玩家在 Client 上用預設的輸入預測，靠權威值修正 | 輸入的形狀兩個方向都能用 |
| 地圖實體的 prespawn 最佳化 | 不做；地圖實體由 Match 生成、經複製 | — |
| 觀戰者 | 不做 | Peer 與 Player 分開 |
| P2P | 不做（權威是佔位） | Link 與輸入不分角色 |

### Q8 Resource 形的狀態怎麼到 Client（狀態機卡轉來；含 `RequestSilent`、`Change` 參數）

**用遊戲講**：聯機的天氣 `Weather` 是一個狀態機。它的值在 Resource `StateMachine<Weather>`，型別封閉，只有狀態機 Module 建得出來。伺服器在第 3600 步換成起霧，Client 的畫面要跟著起霧。作者的判定資料若放在 Resource（例：計分），也是同一個問題。

**為什麼現在要定**：
- 狀態機修訂 8（使用者 2026-11-22 核准「照建議，三件都核准，開始」）第 2.1 節把 `RequestSilent` 留到本線：Engine 的放回都有型別，網路要把線上來的值放進 World，就得有一條建出 `StateMachine<S>` 的路。
- 窄複查 N7：只複製元件的話，`authority.Resource` 的判定資料沒辦法由伺服器校正。

**要重問的舊決定**（原話在 HANDOFF）：
- `v8-arch-gyo2-state-machine-silent-request-and-child-reset` 一：要求分兩種，`Request`（跑離開與進入）與 `RequestSilent`（不跑）。使用者比過四個選項選了它，理由「狀態機不能手動換，相當於遊戲邏輯必須完全依託於狀態機了」。
- `v8-arch-gyo2-state-machine-silent-parent-keeps-child`：父 silent 換成同一個值時，子不動。
- `v8-arch-gyo2-state-machine-remaining-eight` 四：(c) 區另外選「怎麼換」（權威端換／兩端同規則），例 `RegisterStateMachine<Weather>(r, 插在哪, Level::C, Change::Authority)`。
- 同一則的七：快照蓋掉值時跑不跑離開與進入，「這個也是看遊戲設計的」。

| | (a) 兩端同規則、不複製 | (b) 網路複製 Resource，型別的擁有者給套用函式（建議） | (c) 狀態機開「依快照設值」 | (d) 不複製 Resource，作者放在實體上 |
|---|---|---|---|---|
| 做法 | K2、K3 照同樣的規則算出狀態；K3 回滾時重算（狀態機的兩個 Resource 已登記回滾） | `NetEntry(s).ReplicateResource<T>(name, {codec, apply})`；每個 tick 的值訊息多一段單例；Client 收到時呼叫 `apply`（預設 `SetResource<T>`）。狀態機給兩個套用函式：照 `Request`（跑離開與進入）、照 `RequestSilent`（不跑；子不動） | 網路的套用直接建出 `StateMachine<S>`，不經套用的 System | 作者把要同步的值放在一個實體的元件上，自己寫 System 收值後 `Request` |
| K1（只顯示）拿得到狀態嗎 | 拿不到 | 拿得到 | 拿得到 | 拿得到 |
| 跑／不跑離開與進入（七） | 跑（本機自己換） | 作者每個狀態機選 | 不跑 | 作者自己寫 |
| `Change` 參數（四） | 沒有路 | **移到網路的登記**：不登記複製＝兩端同規則；登記複製並選套用函式＝權威端換。狀態機不依賴網路，所以不放在 `RegisterStateMachine` | 移到網路的登記 | 沒有 |
| 舊決定 | `RequestSilent` 不需要：推翻一；`-silent-parent-keeps-child` 用不到；四的「權威端換」沒有路 | 一、`-silent-parent-keeps-child`、七都保留；四的形狀從狀態機的登記移到網路的登記（要你同意） | 推翻一（你當時比過、沒選這個） | 同 (a)，另外每個作者自己寫轉接 |
| K3 要改什麼 | 不改 | 回滾的入口收 Resource 的權威值（rollback 限界「Resource 本批只從歷史還原，不收權威值」要改；第 4 步） | 同 (b) | 不改 |
| 系譜 | Bevy States（replicon、lightyear 都不複製 Resource） | 三家都沒有 Resource 的複製；套用函式的形狀照 replicon 的 `RuleFns` 的 `with_consume`（收到時由規則決定怎麼寫） | 修訂 7 前比過的「Module 開依快照設值的入口」 | replicon、lightyear 的慣例：要同步的全局狀態放在單例實體上 |
| 代價 | 規則依賴的資料若有不同，只靠 digest 發現 | 網路多一種登記；狀態機多兩個套用函式與 silent 對子的規則 | 子的存在跟不上父 | 機制分散在每個作者 |

**建議 (b)**：
- 你 11-16 選 `RequestSilent` 的理由（「遊戲邏輯不必完全依託狀態機」）在 (b) 裡還成立。
- 「跑不跑看遊戲設計」（七）由作者選套用函式。
- `authority.Resource` 的判定資料也有了校正的路。
- 和三家不同（它們不複製 Resource），所以交你。

**(b) 的生效時點**（狀態機卡 2026-11-22 補）：
- 兩個套用函式都經「要求」換值，所以收到快照的當下不換，下一次套用的 System 跑時才生效。套用的 System 預設在 `Main` 的 `PreUpdate` 之後。
- 網路的收件在 `Main`／`First`，所以收到與生效在同一幀：第 N 幀的 `First` 收到「回合結束」，套用的 System 那一步才換。同一幀裡比它早跑的 System（`First`、`PreUpdate`）看到的還是舊狀態。
- 作者要「收到就看得到」，就把讀它的 System 排在套用的 System 之後。
- 不衝突的確認：修訂 8 的型別封閉（DI 1：進入之後只能經要求改值）照樣成立。`RequestSilent` 只在 `StateMachineRequest` 多一個「種類」欄，修訂 8 的介面不用改（狀態機卡回覆）。
- K3 經回滾的入口時，在權威值的 tick 比對、重跑，不是這條路，時點照回滾。

選 (b) 時：網路的 Resource 複製在第 2 步（K1、K2 直接套用）、第 4 步（K3 經回滾的入口）；狀態機的兩個套用函式與 `RequestSilent` 由兩邊誰後合併的一方做（第 14 節）。

---

## 10. 本卡照 L17 判的（不是使用者的話，揭露）

- 傳輸層自己做（D-2）：lightyear 自做、Unity 用自家的、replicon 不做，沒有分歧。
- 通道只兩種：第 2 節沒有 F 列要別的。
- 套用點、不用交換函式（D-3）：照 O-12 的規則。
- Peer 是 Match World 的實體（D-3）：照 P12（使用者 11-11）。
- `default=yes`、不選角色就沒有（D-10）：照 P20 與權威、回滾的先例。
- 輸入在實體上、兩端同名的入口 System（D-11）：照 lightyear、Unity；為了指紋。
- 輸入窗用 `Long`（64）：系譜 lightyear ±64、Unity 回滾上限 64。
- K1 不比指紋（D-6）：它不寫判定資料。
- 步長放握手、不放指紋（D-6；authority 4 延到這裡的二選一）。
- 只在一端跑的邏輯用兩端同登記＋`OnServer()` 條件（D-13）：照 lightyear、replicon 的 run condition；不用 `OneEndOnly`（權威 RC 5）。
- 遠端玩家的輸入不送（Q7 有列）。
- 換 World 的世代欄（D-5）、重連換新的 Client World（D-8）：照讀檔一律在新的 World 的既有決定。
- 名字（`NetEntry`、`DatagramSocket`、`NetTransport` 等）實作時照命名規範定，這裡是暫名。

---

## 11. 實作的步驟（動工前核准、使用者說「開始」之後）

所有步驟都是**上段**：觸及協議、資料形式、公開介面（navigator「上段と下段」）。

| 步 | 做什麼 | 實作者 | 審查者 | 檢證者（§14.4） | 判定 | PR |
|---|---|---|---|---|---|---|
| 0 | `core/os` 的 UDP socket（三平台）＋loopback 測試；os.md 改寫（Q5 核准後） | Opus／high | Opus／high | Opus／high | G-7（socket） | PR-1 |
| 1 | 傳輸層（標頭、ack、兩種通道、分片、記憶體 Link）＋連線（握手、存活、斷線理由、丟棄計數）＋Peer 實體；`net` 登記列、不選就沒有 | Opus／high | Opus／high | Opus／high | G-1～G-4、G-7 | PR-1 |
| 2 | 複製（登記、結構與值、應用層確認、帳本、同號、分則、世代）＋伺服器訊息綁 tick；scenario「伺服器＋兩個 Client」 | Opus／high | Opus／high | Opus／high | G-5、G-7、G-8 | PR-1 |
| 3 | 輸入（實體上的輸入、冗餘、緩衝、兩端同名的入口、畫面的時間、單次觸發）＋回滾的元件輸入紀錄＋權威的元件輸入代為登記＋對時（時間 Module 的入口；文字先給使用者看）＋重連；量 CPU | Opus／high | Opus／high | Opus／high | G-6、G-8、G-9 | PR-1 |
| 4 | 模擬的 Client：寫 `RollbackInbox`、確認點；回滾實作 `Entered`、`Exited`（R-8 文字先給使用者看） | Opus／high | Opus／high | Opus／high | G-10 | PR-2 |
| 5 | 相關性：每 Peer 的可見性、元件範圍、優先度；回滾實作 `NotSent` | Opus／high | Opus／high | Opus／high | G-11 | PR-2 |

- **模型的理由**：
  - 上段不用小的模型（navigator 的下限）。
  - 實作、審查、檢證用 Opus：表外的實作與審查是 Opus（navigator）。
  - 審查者與實作者同一個模型，規則允許；換模型的要求在敵對式檢查與合併前的設計審查。
- **合併前**：
  - Fable／high 做唯讀的設計審查，每個 PR 一次（作者是 Opus，換模型）。
  - 守衛與受入判定的突變清單交 Fable／high 的敵對式檢查（§14.2）。
- **ギア**：照記憶 `xhigh-only-for-investigation`，實作、審查一律 high。卡住要調查時先停下回報。
- **PR**：
  - PR-1（第 0～3 步）結束時，一個不模擬的 Client 能連、能收、能送輸入、能重連。這是「基盤的反覆在積上去之前做完」的點：量的數字交使用者後再開 PR-2。
  - base 都是 `gyo2`；等使用者說「合併」才合併。
- **dev_log**：每步一份，和那一步同一個變更。
- **閘口**：第 3 步的時間 Module 文字、第 4 步的回滾 R-8 文字，開工前交使用者。

---

## 12. Design Invariants 與 Replacement Criteria（草稿；最終文字在合併前核准定稿）

新頁 `docs/architecture/target/v2/net.zh-Hant.md`：

- **NT-1 職責**：網路 Module 的職責是
  1. 連線（握手、存活、斷線理由）；
  2. 傳輸（標頭、ack、通道、分片）；
  3. 複製（登記、結構與值、確認、帳本、可見性）；
  4. 輸入的收送與緩衝、入口；
  5. 伺服器訊息的收送；
  6. 對時的量測與給時間 Module 的輸入；
  7. 丟棄的計數。

  不存判定資料的副本、不回滾、不認得 Player、不認得特定產品。
- **NT-2 一套內部協議**：Link 之上只有一種協議；Link 只有 bytes 與連線狀態。
- **NT-3 冪等的鍵**：每一筆帶 D-4 表的鍵，重複收到不改結果。
- **NT-4 判定用的時間只有 tick**：牆鐘只在 IO 層。
- **NT-5 確認點**：只在 D-7 的兩個條件成立後寫 `ConfirmedThrough`。
- **NT-6 不選就沒有**（G-1）。
- **NT-7 依賴**：網路依賴權威、回滾、時間、entity_ids；它們都不依賴網路；外部組件只經 `core/os`。
- **NT-8 丟棄有理由、有計數**。
- **NT-9 編號**：照 Q1 的結果寫。
- **NT-10 每收件者的狀態在 Peer 實體**：帳本、可見性、輸入緩衝；不進判定、可重建。

**Replacement Criteria**（寫在積上去之前）：

- **RC-NT1**：職責清單（NT-1）要多一項，或判不出某樣東西屬網路、權威、回滾、時間哪一個。
- **RC-NT2**：有點名的產品在一個 World 的生命內用完產品分給伺服器的編號範圍。
- **RC-NT3**：要讓複製正確，必須在 Kernel 或編排層加網路專用的欄位、分支或觀察點。
- **RC-NT4**：每加一種 Link，上層（傳輸、複製）就要改。
- **RC-NT5**：同一個回避策（對映、去重、狀態的副本）要放進兩個以上的地方，或 Link、中繼開始需要遊戲的狀態才能正確。
- **RC-NT6**：G-8 的數字超過使用者選的頻寬預算，而且相關性與優先度都已用上。
- **RC-NT7**：每加一種要複製的元件，就要改網路 Module 的程式（不只是登記）。

---

## 13. 範圍的限界

- 預先生成（F21 (b)）、內插、lag compensation 的歷史姿態（產品／collision）、加密、host 模式、另開 Gateway 行程、WebSocket、P2P、地圖實體的 prespawn：不在本線（Q7）。
- 一個 Product 只有一個伺服器 World 或一個 Client World 有角色。多個 World 共用連線不在本線。
- `PartialValue`（只帶一部分元件）不用：值以（實體、元件）為單位寫入口，本來就是分開的。
- `FromEvent`（事件寫進歷史，MC-2）不用：MC-2 待使用者定（`-multichannel-shape` 的「仍待定」）。
- `networking-systems.svg` 不動（`-network-diagram-after-mechanism`）。圖裡有 master 的常數，本頁不引用。
- 批次表第 6 列寫「master 的 `engine/net`、`services/gyo_gateway`（參考）」：本頁照 P9 只取結構與坑。master 的產品 Gateway 在 `apps/object_fps_pvp/gateway`，`services/gyo_gateway` 是公共層。
- 數值（存活的門檻、冗餘份數、對時的增益與跳 tick 的門檻、斷線冗餘送的包數）：在各步從參考實作與 64 Hz 推，揭露系譜，不用 master 的數。

---

## 14. 和同時在跑的卡的交界

- **讀檔**（`gyo2-restore-impl`）：
  - 讀檔與重連都換新的 World，用同一個 `ReplaceWorld`。
  - 範圍由產品放的 `EntityIdsOptions`，讀檔時照常蓋。
  - 網路不改它們。
- **狀態機**（`gyo2-state-machine-r8`）：修訂 8 第 2.1 節把 `RequestSilent`、「權威端換、Client 採用」、`Change` 參數交給本宣告（Q8）。Q8 選 (b) 時，`RequestSilent` 與套用函式改在狀態機 Module。由兩邊誰後合併的一方做，經主會話協調；本卡做的話排在第 2 步、狀態機修訂 8 合併之後。
- **asset**（`gyo2-asset`）：都用 Product 一份的共用處、都是套用點；不碰同一批檔案。
- **threads／io 重審**（`gyo2-threads-io-review`）：第 7 節的需求經主會話轉。

---

## 15. 修訂 0 的敵對式檢查：處理對照

檢查者 Fable／high（作者 Opus）；原文 `build/evidence/gyo2-net-module/adversarial-rev0-fable.md`。

| # | 嚴重度 | 指出的 | 處理 |
|---|---|---|---|
| B1 | 擋核准 | D-1 推翻 `core/os` 的約定卻列在自己判 | 前提表加 P4；D-1 寫明改 os 的哪兩處、DI 7 不變；Q5 交使用者；第 5 節列入 |
| B2 | 擋核准 | 輸入接不上回滾（只有 Resource）與指紋（Match 的入口在 Client 沒有） | D-11 改成實體上的輸入、兩端同名的入口；回滾的元件輸入紀錄、權威的元件輸入代為登記列進第 5 節；對時的 System 用 `OneEndOnly`（權威不改） |
| B3 | 擋核准 | 範圍由網路放做不出來；Q1 漏引 LIM-7 | 照 LIM-7：範圍由產品放、握手檢查不重疊（D-4）；entity_ids 真的不改；Q1 補 LIM-7 與 stamp-order-spike:36 |
| B4 | 擋核准 | 漏推地圖預放的實體與步內生成；Unity 0x80000000 的系譜用反 | 加 F20、F21；F21 (b) 列 Q7；拿掉 0x80000000 的引用（範圍改由產品分） |
| B5 | 擋核准 | G-5、G-10 判不了 | 第 8 節加「觀測的時點與方法」：G-5 在 D-7 條件成立的 tick 比、結束排空再比；G-10 在輸入不變的最後一段比 `Digest`；D-6 定「沒有權威策略的 Client 不比指紋」 |
| S1 | 應改 | `Entered`／`Exited` 要改 R-8 文字；結構更新直接加減可回滾元件違反 R-2 前提 5 | D-5 改成模擬的 Client 走 `Value`／`Removed`／`Entered`／`Exited`；第 5 節列「改 R-8 文字」，第 4 步開工前交使用者 |
| S2 | 應改 | D-7 和「收齊只算 snapshot」衝突 | D-7 寫明讀法（結構算 snapshot、事件不算）；Q6 交使用者 |
| S3 | 應改 | 帳本的確認沒定義；沒變的 tick 要送空訊息 | D-5 加應用層確認（處理完才確認，Ordered Reliable）與每 tick 都送值訊息 |
| S4 | 應改 | 沒處理換 World | 標頭加 World 世代；Peer 隨 World 重建；Client 收到新世代發訊息、由產品決定 |
| S5 | 應改 | 重連沒清舊的複製實體 | D-8：重連前產品換新的 Client World；G-9 加斷線期間刪掉的實體 |
| S6 | 應改 | Peer 是實體（使用者 11-11）；分流是留給使用者的 | D-3 改成 Peer 實體；前提表加 P12、P13；Q2 含帳本的位置、Q3 交分流 |
| S7 | 應改 | 對時要改 WT-3、可能觸發時間 RC (d) | D-9 與第 5 節寫明要改的 WT 文字，第 3 步開工前交使用者；WT-6 要多一項時停下 |
| S8 | 應改 | 1/65536 來自 master | 改成「小數的編碼照 lightyear 的 overstep，第 3 步定」 |
| S9 | 應改 | 「多通道」的歸屬錯、通道不薄 | F18 改成回滾入口的 MC-1～MC-6；通道只留兩種 |
| S10 | 應改 | 沒有預算 | 第 6 節用研究卡的規模、給頻寬預算候選 (a)(b)(c)，交 Q7；G-8 在選定後判 |
| S11 | 應改 | 內插、Resource 是否複製沒寫 | F2 的欄位定義成不依賴內插；只複製元件（三家都是）；內插列 Q7 |
| S12 | 應改 | 判定門檻、標籤、系譜的缺口 | G-2 加注入的掉包數；門檻改成設定值的邊界；G-7 寫標籤 `cpu`、L1 三平台；D-6～D-12 補系譜與退掉的選項 |
| M1 | 小事 | P13 把「記下」寫成使用者定的 | 前提表下方更正 |
| M2 | 小事 | 窗的下界沒計數 | D-11 窗改成 `(已消耗, T+L]`，已消耗另計 |
| M3 | 小事 | `PartialValue`、`FromEvent` 沒交代 | 第 13 節交代 |
| M4 | 小事 | 連線對到哪個 World | D-3：一個 Product 一個連線對到一個 World；多 World 列限界 |
| M5 | 小事 | 第 11 節沒寫模型的理由 | 補上 |

---

## 16. 修訂 1 的窄複查：處理對照

檢查者 Fable／high；原文 `build/evidence/gyo2-net-module/narrow-recheck-rev1-fable.md`。其餘指摘的處理，複查者查過、成立。

| # | 嚴重度 | 指出的 | 處理 |
|---|---|---|---|
| N1 | 仍擋核准 | G-5 (1) 的觀測時點在亂序下不成立 | G-5 (1) 加條件「當時沒套用過任何 > T 的值或結構」，並要求這種時點佔 10% 以上，不到就是觀測不成立、停下回報 |
| N2 | 仍擋核准 | 「不模擬＝沒有權威策略」和自己的權威登記衝突（AU-E2）；權威無回滾的 Client 沒寫 | D-5 把 Client 分成 K1／K2／K3，逐種寫登記、寫入、比不比指紋；K1 網路不經權威登記（直接 `AddSystem`） |
| N3 | 應改 | 兩端「同樣存取」不成立 | Client World 也有 Peer 實體（D-3）；兩端的入口都讀 `PeerInput<T>`、`PeerControls`，寫 `T`（D-11） |
| N4 | 應改 | K3 上不可回滾的複製元件沒有寫入路徑；遠端輸入沒寫；G-10 的前提 | D-5：不可回滾的直接寫；遠端輸入本線不送（Q7 列）；G-10 的前提改成最後一段都送預設輸入 |
| N5 | 應改 | 只在 Match 跑會撞指紋 | 新增 D-13：兩端同登記＋`OnServer()` 條件，不用 `OneEndOnly`（權威 RC 5） |
| N6 | 應改 | 重連沒有連線識別 | 標頭加連線識別（D-2），D-8、G-9 改成依它丟舊連線的包 |
| N7 | 應改 | 第 9、10 節漏列；Resource 不複製的影響 | 第 10 節補 K1 不比指紋、步長放握手；Resource 形的狀態併進 Q8（和狀態機卡轉來的題目同一件） |
| M-a | 小事 | 換配發器時「不重用」由誰保證 | D-4 補一句 |
| M-b | 小事 | 元件輸入紀錄碰 R-6 與 `InputRecord` | 第 5 節回滾列補上 |

---

## 17. 決定的紀錄

**2026-11-22 動工前核准**（使用者，本卡的對話）：
- **原話**：「照建議，全部核准」。
- **核准的內容**：修訂 2（aca5fd46）加 Q8 的生效時點一段，第 9 節 Q1～Q8 都照建議：
  - Q1 兩端同號（照 LIM-7）；
  - Q2 行程內的 Link、帳本在 Match 的 Peer 實體；
  - Q3 結構可靠、值不可靠的分流；
  - Q4 自己的編碼＋協議雜湊；
  - Q5 `core/os` 收 UDP socket；
  - Q6 結構更新算進確認點、事件不算；
  - Q7 頻寬預算待選，其餘不做；
  - Q8 (b) 網路複製 Resource、型別的擁有者給套用函式。
- **比過的選項**：各題表裡的選項。
- **理由**：使用者沒有另外說。
- **之後**：使用者核准後追問「Q4 為何不用 protobuf？」，本卡回答了五個理由（通用的元件格式、增量、格式演進用不到、第三方的成本、手寫規格加協議雜湊）。使用者回覆「維持，開始」：Q4 維持自己的編碼，開始實作。
- **還沒定的**：Q7 的頻寬預算從第 6 節 (a)／(b)／(c) 選哪一個，使用者沒有指定，第 3 步交數字時再問。

---

## 18. 第 3 步後半（3b 對時）：時間 Module 的形狀（開工前交使用者，2026-11-23）

宣告 D-9 與第 11 節的閘口：對時要改時間 Module 的文字，先給使用者定。

**用遊戲講**：Client 剛連上，握手告訴它「Match 在第 5000 步」，而 Client 的 World 才跑到第 30 步。之後網路一直在量，發現 Client 比目標慢了 3 步、或快了 2 步。

**兩件事要定**：(1) 平常的小誤差怎麼追；(2) 剛連上、或誤差很大時怎麼對上。

(1) 兩案都一樣：**調倍率、步長不動**（三家都是）。
- 時間 Module 加一個 Resource `TimeSyncScale{scale = 1.0}`，只由網路的對時 System 寫。
- `AdvanceTime` 用「產品的 `TimeControl.scale` × `TimeSyncScale.scale`」。
- 落在 WT-6 第 (3) 項「倍率」的範圍內，不多一項。

(2) 兩案不同：

| | (A) 時間 Module 加「跳 k 步」（建議） | (B) 不跳，大誤差就換新的 World |
|---|---|---|
| 做法 | 時間 Module 加 `JumpFixedSteps(world, k)`：`Tick` 與 `FixedTime.elapsed` 一起改 k 步（k 可正可負），不跑步的 schedule。只由網路的對時 System 呼叫；它登記進 `TickWriters()`，所以伺服器權威的 AU-E5 照樣看得到 | 只調倍率。剛連上時，網路發「已連上，Match 在第 N 步」，產品換一個新的 Client World，在準備函式放回 `Tick`（WT-3 已允許讀檔這樣放回）。誤差大到倍率追不上時，網路發「失去同步」，產品重連（「重連＝重新對時」） |
| 要改的時間 Module 文字 | WT-3 加一句（`Tick` 也可經 `JumpFixedSteps` 改）；WT-6 第 (4) 項加「跳 k 步」；`TickWriters()` 從固定名單改成可以登記（WT-5） | 只加 `TimeSyncScale`（WT-5 寫者表、WT-6 第 (3) 項的說明） |
| 回滾 | 跳之後，回滾的歷史不涵蓋跳過的 tick；確認點早於跳的點時，回滾照「C 早於第一筆記錄」放棄（現有的規則） | 新 World 沒有舊歷史（R-5），不碰回滾 |
| 產品的負擔 | 不用管 | 第一次連上也要換 World |
| 系譜 | lightyear：誤差 >10 tick 就 snap，發 `LocalTimelineShift`；Unity：>10 tick 直接跳 | GYO 自己的；用到「讀檔一律在新的 World」與「重連＝重新對時」兩個既有決定 |
| 代價 | 時間 Module 多一個寫 `Tick` 的入口、`TickWriters()` 變成可登記；但時間 RC (b)「tick 只由時間 Module 推」不觸發（跳還是時間 Module 的函式） | 和三家不同；剛連上時的流程繞一圈 |

**建議 (A)**：三家都這樣做，產品不用為了連線換 World；時間 Module 多的那個函式仍在它自己的職責裡（WT-6 第 (4) 項，tick 的驅動）。跳的門檻、倍率的增益與上下限，第 3 步照 lightyear（10 tick snap、1.05 倍、連續 3 次同方向才調）推，寫系譜。

選定後，時間 Module 的文字（WT-3、WT-5、WT-6）照選的案寫，合併前核准定稿。

**2026-11-23 使用者決定**（本卡的對話）：
- **原話**：「照建議，選 A」。
- **決定**：對時用倍率（`TimeSyncScale`）；剛連上與大誤差時，時間 Module 用 `JumpFixedSteps(world, k)` 跳。
- **要改的時間 Module 文字**：WT-3、WT-5（`TickWriters()` 可登記）、WT-6 第 (4) 項；照 (A) 寫，合併前核准定稿。
- **比過的選項**：(A) 跳 k 步／(B) 不跳、換新的 World。
- **理由**：使用者照建議（三家都這樣做；產品不用為了連線換 World）。

---

## 19. 第 3c 步：送件的效能（2026-11-23 加；判定先宣告）

**使用者的決定**（經主會話轉，四樣）：
- **原話**：「照建議，加 3c，預算選 (b)」。
- **比過的選項**：加 3c ←選／先合 PR-1、效能之後再修。預算 (a) ≤ 1.56 ms／(b) ≤ 3.9 ms ←選／(c) 另給或不設門檻。
- **理由**（主會話的建議）：網路併進 gyo2 後會被往上積，基盤的反覆先做完（AGENTS §2）；規模沒點名，先取做得到又能解掉「8 個 Peer 就超過一步」的門檻，規模點名後照 §2 重訂。
- **出處**：主會話對話 2026-11-23。

**起因**：第 3b 步量到 Match 每 tick 平均 8.19 ms，其中 `ReplicateOut` 8.03 ms（4 個 Peer、400 個實體；dev_log step3b）。取樣看到大多是記憶體配置：每個值一個暫時的 `std::vector` 編碼、快照用 `std::map`。

**起因的更正**（3c 量測後補）：用改前的執行檔重量 4 個 Peer 是 2.70 ms，不是 8.19 ms；8 個 Peer 是 5.49 ms（3 次）。推測 3b 量測時機器上有別的負載，沒有查證。G-12 以 3c 改前的 3 次為比較基準；「8 個 Peer 就超過一步」的判斷不成立，但 5.49 ms 仍超過門檻 3.9 ms，3c 照做。

**改法**：編碼緩衝重用、快照改用連續陣列。不改協議、wire 上的位元組、判定（G-2～G-9 照舊要過，G-8 的位元組逐位元相同）。

**判定 G-12**（結果出來之後不放寬，AGENTS §14.1）：
- **情境**：一個 Match、8 個 Client（K1），MemoryLink 不掉包；Match 上 400 個複製的實體每 tick 都改值；64 Hz，一幀一個 tick。
- **量什麼**：Match World 上網路 Module 的全部 System（收件、輸入、複製、伺服器訊息、送件；名字以實作為準，全部列進證據）每 tick 的合計時間，用 trace 的段計時。
- **建置**：`core` preset（RelWithDebInfo），本機 macOS x86_64。
- **次數**：暖機 300 tick，量 2 000 tick，跑 3 次。
- **門檻**：3 次的**平均**都 ≤ 3.9 ms／tick。p50、p99 記錄、不判定。
- **改前也量**：同一個情境在改之前量 3 次，記錄，當比較基準。
- **範圍的限界**：只在這台機器量。Windows Client、Linux Match 沒量，擴到它們之前另量（AGENTS §2）。
- **不成立時**：停下回報（§14），不放寬、不改情境。

**對本機的影響**（量之前告知使用者）：9 個 Product 在同一個行程，400 個實體；記憶體估計 < 500 MB、CPU 一核；每次約 30～60 秒，改前改後共 6 次，約 3～6 分鐘。沒有 GPU。

**G-12 的結果**（2026-11-23，dev_log step3c）：改後 3 次平均 1 122.1／1 117.9／1 092.4 µs，都 ≤ 3.9 ms，成立。改前 5 504.5／5 482.6／5 478.8 µs。G-8 的位元組改前改後逐位元相同。

**頻寬的基準**（G-12、G-8，dev_log step3c；只記錄、不判定）：

| 情境 | 每 Client 每 tick 的下行 | 64 Hz |
|---|---|---|
| G-12（8 Peer、400 個實體每 tick 都動，沒有相關性） | 平均 9 743 B、最大 9 791 B | 約 5.0 Mbps（含 IP／UDP 約 5.1） |
| G-8（固定的小情境） | 平均 338 B、最大 625 B | 173～320 kbps |

**Q7 頻寬預算：使用者 2026-11-23 決定等第 5 步再定**（經主會話轉，四樣）：
- **原話**：「照建議，Q7 等第 5 步再定」。
- **比過的選項**：現在選 (a) 約 2.5 Mbps／(b) 約 450 kbps／(c) 另給；等第 5 步（相關性）再定 ←選。
- **理由**（主會話的建議）：G-12 是最壞的情境（沒有相關性、全部每 tick 都變），用它定門檻等於拿之後一定會改的情境當門檻。
- **出處**：主會話對話 2026-11-23。
- **之後**：第 5 步動工前，在宣告裡把典型情境（實體數、每 tick 變動的比例、一個 Client 看得到的範圍）與預算候選一起交使用者，量之前先定（§14.1）。
