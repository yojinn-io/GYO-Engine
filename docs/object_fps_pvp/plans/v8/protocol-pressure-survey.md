# 協議壓力調研：哪些遊戲行為會佔用協議

更新：2026-10-30。Owner：`object_fps_pvp`。**性質：規劃輸入，不是設計決定。** 起因：使用者觀察到「現在壓力來到了協議層」，要一份調研報告（[HANDOFF](HANDOFF.md) D254），參考主流射擊遊戲、鬥陣特工風格的戰鬥系統，以及塔科夫類的地圖、背包、裝備、簡單武器（沒有改裝）。

## 這份報告怎麼來的

- 調研由 1 位背景子 agent 執行（general-purpose，引擎與 effort 繼承主會話，只讀網路公開資料，沒有讀或改 repo）。主會話在整理時換上本產品的實測數字（[protocol-v8](../../protocol-v8.zh-Hant.md) §6，2026-10-30），並補上第 5、6 節。
- **其他遊戲的敘述由子 agent 收集，主會話沒有逐一打開來源核對。** 標記：
  - 「二手」：來自第三方報導或搜尋摘要，沒有讀到原文。
  - 「推論」：子 agent 或主會話的估算，沒有外部來源。
  - 沒有標記的：子 agent 表示讀過一手來源（附 URL）。
- 估算的數字都是用來看數量級，不是本產品的設計值。

## 摘要

1. **本產品的餘裕**：pv8 的 4 人 snapshot 契約最大 1101 B、型別最大 1175 B（實測），離 1200 B 只剩 99 B／25 B。每人只要再多約 6～25 B 的「每 Tick 都送」的狀態，就會超出上限。
2. **鬥陣特工風格**最先撐爆的是「每人每 Tick 都送」的欄位：技能、狀態效果、護盾池。接著是隨實體數成長的投射物與可放置物件。粗估 4 人共增加 0.4～0.6 KB，超過單一資料包（推論）。
3. **塔科夫類**的壓力不在玩家，而在**實體數**（AI、地上物品、門）與**私人資料**（背包、容器內容）。沿用「全員、全量、每 Tick」的話，20 個 AI 加上數百件物品會到數 KB 到數十 KB（推論）。
4. **主流做法**是把資料分成幾條通道：
   - 高頻、不可靠、只送最新值的狀態，用差分與量化壓縮。
   - 可靠的事件串流。
   - 只給本人的私人狀態。
   - 依相關性與優先度篩選要送的實體（relevancy、dormancy、priority）。
5. **有一手資料的做法**：
   - Valorant：改用推送事件，取代輪詢式的變數同步；用 PVS 過濾敵人資訊。
   - UE 的 Replication Graph：用空間格子篩選實體。
   - Quake 3：對已確認的 snapshot 做差分。
   - Gaffer：priority accumulator 在固定的封包預算內輪流送實體。
6. **塔科夫與同類撤離遊戲**幾乎沒有公開的 netcode 資料：tick rate 沒有官方數字，物品同步的方式找不到來源。第 4 節對塔科夫類的推估全部是推論。

---

## 1. 行為分類總表

位元組是**每個實體**的量級，假設欄位已量化；未量化時約為 1.5～2 倍。全部是推論，只用來看數量級。

| 類別 | 要同步什麼 | 給誰 | 頻率 | 可靠性 | 每個實體約多少 | 隨什麼成長 |
|---|---|---|---|---|---|---|
| 玩家移動與視角 | 位置、yaw/pitch、速度或姿態（蹲、衝刺）、grounded、命令序號 | 所有相關的人 | 每 Tick | 不可靠，只要最新值 | 未量化 20～30 B；量化後 10～14 B | 人數 |
| 射擊與命中 | 開火事件（時間、方向）、命中結果、傷害 | 開火者、被擊者、附近的人（槍口焰、彈道） | 事件 | 結果要可靠；視覺效果可以不可靠 | 8～20 B／事件 | 射速 × 人數 |
| 投射物 | 生成（起點、方向、類型、tick），以及空中的位置或爆炸事件 | 附近或看得見的人 | 生成與結束時；不可預測時每 Tick | 生成與爆炸可靠；飛行中不可靠 | 生成 16～24 B；逐 Tick 位置 12～27 B | 空中的投射物數 |
| 技能與冷卻 | 剩餘冷卻、充能次數、啟用中的旗標 | 精確數值給本人；「是否啟用」給所有人 | 變化時（可由起始 tick 推算） | 可靠事件，或包在狀態裡 | 3～5 B／技能 | 人數 × 技能數 |
| 狀態效果與 buff | 效果 id、剩餘時間或結束 tick、來源、層數 | 看得見的人（影響畫面）；數值細節給本人 | 加上與移除時；用結束 tick，不必每 Tick 送 | 可靠事件，或包在狀態裡 | 4～6 B／效果 | 人數 × 同時存在的效果數 |
| 護盾與護甲 | 多個生命池（HP、護甲、護盾）、護盾耐久 | 所有人（血條），或只有本隊 | 變化時 | 送最新值即可 | 2 B／池 | 人數 × 池數 |
| 大招充能 | 0～100% | 本人與隊友 | 變化時 | 送最新值即可 | 1～2 B | 人數 |
| 可放置物件（砲台、屏障） | 生成、位置、朝向、HP、擁有者、狀態 | 附近或看得見的人 | 生成與破壞時；砲台轉向時高頻 | 生成與破壞可靠 | 12～24 B | 放置數 |
| 目標點與計分 | 佔點進度、分數、回合計時 | 所有人 | 變化時，或低頻 | 送最新值即可 | 數 B／目標 | 目標數（很少） |
| AI 敵人 | 同玩家移動，加上動畫或行為狀態、HP | 附近或看得見的人 | 每 Tick（在相關範圍內） | 不可靠，只要最新值 | 10～25 B | AI 數 × 被多少人看見 |
| 地上物品與容器 | 物品 id、類型、位置、數量；容器是否開啟 | 附近的人；容器內容只給開啟的人 | 生成、拾取、掉落時；不動時不送 | 可靠 | 10～20 B／物品 | 物品數 × 地圖大小 |
| 背包與裝備 | 格子位置、旋轉、堆疊、耐久、彈匣內容 | 只有本人；外觀裝備給所有人 | 變化時 | 可靠且有順序 | 8～12 B／格中物品；外觀 1～2 B／欄位 | 每人的物品數 |
| 門與互動物件 | 開關、上鎖、角度 | 附近的人，或全圖 | 變化時 | 可靠 | 1～4 B | 互動物件數 × 地圖大小 |
| 世界事件 | 空投、Boss 出現、警報 | 全體或區域 | 事件 | 可靠 | 8～16 B | 事件數（很少） |
| 撤離計時 | 撤離點開放狀態、倒數 | 本人（個人撤離點），或全體 | 變化時（送結束 tick 即可） | 可靠 | 2～6 B | 撤離點數 |
| 聲音事件 | 槍聲、爆炸、開門；腳步聲多半可由移動推出 | 聲音範圍內的人（塔科夫類範圍很大） | 事件 | 不可靠，可以丟 | 6～12 B | 事件率 × 範圍內人數 |

成長軸的歸納（推論）：

- 鬥陣特工風格：大多是「人數 × 每人欄位數」，人數固定是 4 人。
- 塔科夫類：大多是「實體數 × 相關性」。地圖越大，實體越多，但每個人看得到的部分不一定變多。所以要靠篩選，不能靠全量。

---

## 2. 各遊戲的做法

### Overwatch（Blizzard）

- Tim Ford 在 GDC 2017 的演講主題是 ECS 架構，以及用確定性同時取得反應速度與精準度。來源：[GDC Vault](https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and)。
- 二手（[Edgegap 摘要](https://edgegap.com/blog/game-backend-deep-dive-overwatch-2016-netcode-architecture-rollback)，引用演講的 YouTube 影片）：
  - 負責遊戲性 netcode 的系統只有三個：移動、武器、state script。
  - 模擬以固定的 16 ms command frame（約 60 Hz）執行。
  - 移動、技能、武器預設都在 Client 端預測，包括 Pharah 的火箭；需要時再逐個技能關掉預測。
  - Client 每個封包都附上所有尚未被確認的輸入，用來補上遺失的封包。
- Ford 與 Orwig 後來拍了影片說明一次 netcode 更新。來源：[PCGamesN](https://www.pcgamesn.com/overwatch/overwatch-netcode)。搜尋摘要提到 PC 的更新率從約 21 Hz 提到 60 Hz，原文沒有確認（二手）。
- Philip Orwig 的 GDC 2017〈Replay Technology in Overwatch〉講和網路模型一起開發的重播系統，以及在重生期限內產生並傳送擊殺鏡頭。來源：[GDC Vault](https://gdcvault.com/play/1024053/Replay-Technology-in-Overwatch-Kill)。投影片細節沒有公開。
- 啟示（推論）：大量技能沒有讓 Overwatch 的 netcode 系統變多。技能邏輯放在共用的 state script，由預測與回滾處理。

### Valorant（Riot）

- 128 tick，每幀 7.8125 ms。扣掉 10% 留給 OS，目標每幀 2.34 ms、每個核心跑 3 場以上。來源：[Riot〈VALORANT's 128-Tick Servers〉](https://www.riotgames.com/en/news/valorants-128-tick-servers)、[technology.riotgames.com/node/112](https://technology.riotgames.com/node/112)。
- 同一篇提到：把輪詢式的同步變數改成狀態變化時推送的 RPC，很多情況改善 100～10,000 倍。例子是送一個帶傷害值的命中事件，而不是同步 HP。
- 同一篇也提到：伺服器保留玩家位置與動畫狀態的歷史，用來回溯驗證命中。這和本產品 v8 的 hitbox-rewind（Match 端的權威姿勢歷史）是同一類需求。
- Fog of War（[Riot 文章](https://www.riotgames.com/en/news/demolishing-wallhacks-valorants-fog-war)、[tech blog node/108](https://technology.riotgames.com/node/108)）：
  - 伺服器對每個玩家判斷每個網路實體是否「相關」，不相關就停止送更新。
  - 最後採用預先計算的 PVS 格子可見表，取代射線檢測。判定是樂觀的：兩個格子只要有任何部分互相看得到，就算看得到。
  - 用速度乘上一段大於預期 ping 的前瞻時間擴大包圍盒，讓資訊提早送到 Client。
  - 附掛在角色上的物件（例如武器）繼承父物件的可見性。
  - 這套系統只佔伺服器幀時間不到 2%。
- 延伸閱讀：[Peeking into VALORANT's Netcode](https://technology.riotgames.com/node/111)（子 agent 沒有逐項讀完）。

### Counter-Strike 2（Valve）

- 官方伺服器 64 Hz。sub-tick 讓輸入（開火、跳躍、移動）帶上 Tick 之間的精確時間點，伺服器依那個時刻判定。sub-tick 處理的是**輸入的時間精度**，不是 snapshot 的大小。
- 本產品的對照：pv7 的 A1（射擊照命令步判定）、`view_tick_q16`（射手看到的遠端時刻）屬於同一類「輸入的時間精度」問題，不增加 snapshot 的大小。
- 來源都是二手：Valve 的發表影片〈Moving Beyond Tick Rate〉的轉述，見 [Dexerto](https://dexerto.com/counter-strike-2/counter-strike-2-sub-tick-updates-explained-2094004)、[GameRevolution](https://www.gamerevolution.com/guides/937479-cs2-tick-rate-changes-counter-strike-2-sub-tick-updates-csgo)。

### Apex Legends（Respawn）

- 伺服器 20 Hz，每個 Tick 結束時把世界狀態同步給所有 Client。來源：[EA〈Servers & Netcode Developer Deep Dive〉](https://www.ea.com/games/apex-legends/news/servers-netcode-developer-deep-dive)。
- 同一篇的頻寬數字：開局約 60 kB/s；改成 60 Hz 約 180 kB/s，大致是三倍。
- 同一篇提到，每個 Tick 包含戰術技能、被動、大招、補給箱、無人機位置等大量資訊。
- 同一篇的取捨：在 50 ms ping 下，60 Hz 只省約兩幀延遲，卻要約三倍的頻寬與 CPU，所以沒有提高 tick rate。
- Titanfall 的部分沒有找到可用的一手來源。

### Fortnite／Unreal Engine 的 Replication Graph

- **Priority**：連線頻寬飽和時，依優先度分配頻寬，只有優先度之間的比例有意義。來源：[UE〈Actor Priority〉](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-priority-in-unreal-engine)。
- **Dormancy**：很少變動的 actor 可以設為休眠，伺服器就不再逐次檢查它有沒有變，省 CPU。修改前要先喚醒；常常變動的 pawn 不適合。來源：[UE〈Actor Network Dormancy〉](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-network-dormancy-in-unreal-engine)。
- **Replication Graph**（Epic 2018 技術部落格，[連結](https://www.unrealengine.com/tech-blog/replication-graph-overview-and-proper-replication-methods)；抓取時被 403 擋下，內容來自搜尋摘要，二手）：
  - 透過減少重複工作與快取資料降低伺服器 CPU，目標是 100 個以上的連線。
  - Grid Spatialization 2D 節點把世界切成格子，記錄每個格子可見的 actor。
- 格子節點的 API 參數有 CellSize、GridBounds 等。來源：[API 文件](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ReplicationGraph/UReplicationGraphNode_GridSpatia-)。

### Escape from Tarkov（Battlestate Games）

公開資訊很少。

- **Tick rate**：官方沒有公布。第三方比較表寫「30，可變」，說明是隨伺服器負載變化（[diamondlobby](https://diamondlobby.com/server-tick-rates/)，二手，可信度低）。
- **Desync**：
  - 2018 年有一份分析，在兩名玩家之間量到最高 941 ms、平均約 200 ms 的落差，BSG 當時承認 netcode 需要改善。來源：[Mein-MMO](https://mein-mmo.de/en/escape-from-tarkov-entwickler-arbeiten-am-grossen-lag-problem,209596)。
  - 0.13.1.0 的更新說明提到提高伺服器與 Client 的位置一致性、降低 desync 的機率（[TechRaptor](https://Techraptor.net/gaming/news/escape-from-tarkov-update-01310-aims-to-increase-accuracy-reduce-desync)、[Shazoo](https://shazoo.ru/2023/07/06/145404/escape-from-tarkov-polucila-patc-ulucsaiushhii-pozicionirovanie-i-sinxronizaciiu-igrokov-na-kliente-i-ix-sviaz-s-serverom)；抓取原文被擋，內容來自搜尋摘要，二手）。
- **物品、容器、AI 的同步方式**：找不到可信來源。

### 同類撤離遊戲：Hunt: Showdown（Crytek），附 Arena Breakout

- **Hunt: Showdown**：
  - 沒有官方的 tick rate。有玩家在 Steam 討論串說詢問客服時被告知不能透露（[Steam 討論](https://steamcommunity.com/app/594650/discussions/8/1746770563031395786)）。
  - Update 1.9 同時改了伺服器與 Client，目的是減少 desync（[PCGamesN](https://www.pcgamesn.com/hunt-showdown/update-1-9-patch-notes)）。
- **Arena Breakout Infinite**：也沒有官方數字。玩家說 24 tick，行銷頁面說 72 tick，無法查證。

結論：撤離類遊戲**沒有可引用的 netcode 架構資料**。

---

## 3. 通用技術

### Snapshot 差分（Quake 3）

- 伺服器為每個 Client 保留最近 32 份 snapshot，只對該 Client **最後確認收到**的那一份做差分；每個欄位用 1 bit 標記有沒有變。
- 沒有可用的基準時，對全零的 dummy 狀態做差分，結果就是全量。
- 因為基準是「最後確認的版本」，遺失的變化會自動帶進下一份，狀態本身不需要另外重送。
- 必須送達的資料（例如離開、換關）走 netchan 的可靠機制。
- 大訊息切成 1400 B 的片段，再用預先計算的 Huffman 表壓縮。
- 來源：[Fabien Sanglard，Quake 3 Network Model](https://mail.fabiensanglard.net/quake3/network.php)、[The Quake3 Networking Model](https://fabiensanglard.net/quake3/The%20Quake3%20Networking%20Mode.html)。

### 量化（Gaffer on Games〈Snapshot Compression〉）

來源：[gafferongames.com/post/snapshot_compression](https://gafferongames.com/post/snapshot_compression/)。

- **朝向**：smallest-three 編碼，2 bit 記最大分量的索引，另外三個分量各 9 bit，共 29 bit（原本 128 bit）。
- **位置**：x、y 範圍 [-256, 255] m 各 18 bit；z 範圍 [0, 32] m 14 bit；每公尺 512 階（約 2 mm）。共 50 bit（原本 96 bit）。
- **每個物體**：321 bit 壓到 80 bit。再對約一個 RTT 前已確認的基準做差分，只送變化的物體索引，位置與朝向送小範圍的差值。
- 901 個方塊、60 Hz 時，原始頻寬約 17 Mbps，目標是 256 kbps；全部靜止時約 15 kbps，主要是標頭成本。

### State synchronization 與 priority accumulator（Gaffer）

來源：[gafferongames.com/post/state_synchronization](https://gafferongames.com/post/state_synchronization/)。

- 每個封包帶最多 64 個物體的狀態，加上冗餘的輸入。
- 每幀把每個物體當下的優先度加到它的累加器上，依累加值由大到小排序，在預算內盡量放進封包。送出的物體累加器歸零，沒送的保留累加值，下次更優先。
- 靜止的物體優先度低，而且不送速度。
- 結果是在**固定的封包預算**內，所有物體輪流得到更新。

### Interest management（相關性篩選）

- **距離**：UE 的 net cull distance。
- **空間格子**：Replication Graph 的 Grid Spatialization。
- **視線或 PVS**：Valorant 的 Fog of War。
- **休眠**：UE 的 dormancy。
- 前三種決定「送給誰」，dormancy 決定「要不要檢查」。

### 可靠事件串流

- 把「會發生一次、必須到達」的事情做成有序號、會重送的訊息串流，和「只要最新值」的狀態分開。例子：Valorant 的命中 RPC、Quake 3 的 netchan。
- 延伸閱讀（子 agent 沒有逐項讀）：Gaffer〈[Reliable Ordered Messages](https://gafferongames.com/post/reliable_ordered_messages/)〉、〈[Packet Fragmentation and Reassembly](https://gafferongames.com/post/packet_fragmentation_and_reassembly/)〉。

### 私人通道（推論，沒有找到單一來源）

- 只有本人需要的資料（背包、精確冷卻、容器內容）只送給本人。其他人的 snapshot 不會變大，作弊者也拿不到。
- Valorant 的 Fog of War 是同一個原則用在「敵人資訊」上：依連線分別過濾。

### Snapshot 分片的取捨（推論，由機率推導）

- snapshot 拆成 n 個片段時，任何一片遺失整份就作廢，到達率是 (1−p)^n。p＝2%、n＝3 時約 94%。
- 差分的基準也要依賴整份都收到。
- 替代方式：
  - 每個封包是一份可以獨立使用的「部分 snapshot」，搭配 priority accumulator 輪流送。
  - 把大量資料移到可靠串流，分片放在那裡。

---

## 4. 對本產品的意義（粗估）

只寫壓力與對應的技術，不做設計決定。數字全部是推論。

### 現況的餘裕（pv8，實測）

- 4 人 snapshot：契約最大 1101 B（餘裕 99 B）、型別最大 1175 B（餘裕 25 B）。來源：[protocol-v8](../../protocol-v8.zh-Hant.md) §6。
- 每人約 264 B（PlayerState＋CombatState，含長度前綴），snapshot 本身約 45 B。
- 下行頻寬：1101 B × 60 Hz ≈ 66 kB/s，加上 UDP/IP 標頭約 28 B × 60 ≈ 1.7 kB/s。和 Apex 開局約 60 kB/s 同一量級，而 Apex 是 20 Hz、人數多很多。可見「每 Tick 全量」的單位成本很高（推論）。
- pv8 的蹲下與衝刺只是旗標，每人 +7 B。真正的壓力是下面兩個方向。

### 鬥陣特工風格（4 人，每人 N＝4 個技能含大招、M＝4 個狀態效果）

| 項目 | 如果每 Tick 給所有人全量 | 估算 |
|---|---|---|
| 技能冷卻、充能、啟用 | 4 人 × 4 × 4 B | 64 B |
| 大招充能 | 4 × 2 B | 8 B |
| 狀態效果 | 4 人 × 4 × 5 B | 80 B |
| 護盾與護甲池 | 4 × 3 池 × 2 B | 24 B |
| 英雄 id 與姿態旗標 | 4 × 2 B | 8 B |
| 可放置物件（共 6 個） | 6 × 20 B | 120 B |
| 空中的投射物（共 8 個，逐 Tick 送位置） | 8 × 27 B | 216 B |
| **合計增加** |  | **約 520 B，總計約 1.6 KB，超過 1200 B** |

- **最先撐爆的**是每人固定欄位：技能、狀態、護盾。光是這一組（約 180 B）就超過 99 B 的餘裕。接著是隨實體數成長的投射物與放置物。
- **對應的技術與通道**：
  - 冷卻與狀態效果的時間只送「開始 tick 或結束 tick」的可靠事件，由兩端自己推算剩餘時間。
  - 精確冷卻與大招充能只給本人或本隊，走私人通道。
  - 投射物只送「生成事件與結果事件」，中間由確定性模擬（Overwatch 預測投射物是同類思路，二手）。不能預測的投射物才算進高頻狀態。
  - 放置物件用可靠事件送生成與破壞，HP 變化時才送。
  - 玩家位置與角度量化：參考 Gaffer 的位元配置，位置 12 B 可降到約 6～7 B，yaw/pitch 各 16 bit。

### 塔科夫類（4 人，K＝20 個 AI、L＝200 件地上物品、50 扇門）

| 項目 | 如果每 Tick 給所有人全量 | 估算 |
|---|---|---|
| AI（用 pv8 的玩家格式，約 264 B） | 20 × 264 B | 約 5,300 B |
| AI（量化後的精簡格式，約 25 B） | 20 × 25 B | 約 500 B |
| 地上物品 | 200 × 20 B | 約 4,000 B |
| 門 | 50 × 2 B | 100 B |
| 背包（每人 30 件 × 10 B，若放在共用 snapshot） | 4 × 300 B | 1,200 B |
| 容器內容 | 依開啟次數 | 數百 B／次 |

- **最先撐爆的是 AI**：它和玩家一樣需要高頻更新，用 pv8 的玩家格式時，加 1 個 AI 就超過 99 B 的餘裕。
- 地上物品、門、背包的數量很大，但幾乎不會動。它們的問題是「每 Tick 重送不變的資料」，不是「真的需要頻寬」。
- **對應的技術與通道**：
  - AI 是高頻狀態，需要 interest management（距離、格子、視線或 PVS）、量化，以及差分或 priority accumulator，才能放進固定預算。
  - 地上物品與門是低頻的可靠事件，再加上「進入相關範圍時送一次全量」，對應 UE 的 dormancy 與 relevancy，或 Quake 3 對已確認基準的差分。
  - 背包、裝備、容器內容走私人可靠通道，只給本人、只在變化時送；對其他人只需要外觀裝備，每個欄位 1～2 B。
  - 拾取、移動物品、開門、開容器是「動作」，需要伺服器判定後回覆結果，和現有的動作通道（射擊、換彈）性質相同，但次數與種類會多很多。
  - 撤離計時與世界事件：低頻可靠事件，送結束 tick 即可。
  - 聲音事件：塔科夫類的聽覺範圍大，槍聲要送到遠處的人，可以用不可靠事件並依距離篩選。
  - 「簡單的武器（沒有改裝）」讓武器狀態保持在少數欄位（武器 id、彈匣、換彈）。武器資料本身是規則資料，可以像 pv8 的移動參數一樣在進地圖時下發，不必每 Tick 送（推論）。

### 各類需要的通道（推論）

| 通道 | 鬥陣特工風格 | 塔科夫類 |
|---|---|---|
| 高頻狀態（不可靠，只要最新值，需要壓縮與篩選） | 玩家、不能預測的投射物、砲台朝向 | 玩家、AI |
| 可靠事件（共用） | 技能啟用、狀態加上與移除、生成與破壞、計分 | 物品生成與拾取、門、世界事件、撤離 |
| 私人狀態（只給本人） | 精確冷卻、大招充能 | 背包、容器內容、個人撤離點 |
| 動作（Client 到伺服器，可靠） | 技能施放 | 拾取、整理背包、互動 |

共同的結論（推論）：pv7／pv8 的「全員、全量、每 Tick、單一資料包」只適合「人數少，而且每人欄位少」的情況。兩個方向都會先撞到同一件事：資料要依「頻率 × 對象 × 可靠性」分到不同通道。差別在於：

- 鬥陣特工風格的主要問題是**每人欄位**，靠事件化與私人通道就能處理大部分。
- 塔科夫類的主要問題是**實體數**，必須加入 interest management，以及差分或優先度排程。

---

## 5. 和本產品現有元件的對照（主會話補充）

| 調研中的技術 | 本產品現在有沒有 | 位置與說明 |
|---|---|---|
| 可靠的動作通道（有序號、ACK、重送、去重） | 有 | 射擊與換彈的動作通道（30 Hz），Match 在 ACK 前重送結果（v7.1 `edge-results-resend`）。v8 README「網路協議升級」一項已經把它當成可靠通道的樣板 |
| 規則在進地圖時下發 | 有 | `CombatRules`、`jump_height`／`gravity`，pv8 加上蹲下與衝刺參數（protocol-v8 §3） |
| 伺服器端的歷史回溯 | 部分 | A1 的命令步判定；hitbox-rewind（v8 ①）要在 Match 保存姿勢歷史，和 Valorant 的做法同類 |
| 輸入的時間精度 | 有 | `view_tick_q16`、A1 的 (E,S)；和 CS2 的 sub-tick 同一類問題，不增加 snapshot 大小 |
| 高頻狀態的差分 | 沒有 | 每 Tick 全量 |
| 量化 | 沒有 | 位置、角度是 float |
| interest management | 沒有 | 每個 Client 收全部玩家 |
| 私人通道 | 沒有 | CombatState 對所有人送，彈匣等數值所有人都看得到 |
| 可靠事件串流（世界事件） | 沒有 | 目前沒有世界事件 |
| 分片 | 沒有 | snapshot 必須塞進一個資料包（protocol-v8 §6） |

和既有決定的關係：

- v8 README「網路協議升級」一項記了「要使用者決定的架構點」：可靠串流與差分的基準需要記狀態，放在 Match 還是 Gateway。放在 Gateway 會讓它變成有狀態，違背「無狀態邊緣」（v7.1 D94）。本調研的差分（Quake 3）、priority accumulator、interest management 都需要**每個 Client 一份**的狀態（最後確認的基準、累加器、相關集合），所以這個架構點在協議升級時一定會碰到。
- 依 AGENTS §2「Feature → Contract → Architecture」，上面的技術要在具體的玩法需求（同時存在的 AI、投射物、地上物品的數量，背包格數，技能數）寫出來之後，才能選。本調研只提供選擇的依據。

## 6. 這份報告會影響哪些決定

| 決定 | 什麼時候 | 這份報告提供什麼 |
|---|---|---|
| 塔科夫類的玩法需求（D243：v8 只寫需求） | 使用者撰寫需求時 | 第 1 節的分類可以當成需求的清單：每類要寫數量、可見範圍、更新頻率 |
| 鬥陣特工風格要不要做、做到多少 | 尚未排定 | 第 4 節的粗估：每人固定欄位一加就會超過現有的餘裕 |
| 下一次協議升級的範圍（⑤ 的對局規則會再升一次，D246） | v8 ⑤ 開始前 | 計分、回合是低頻資料，可以借這次升級先引入「可靠事件串流」或「低頻狀態」，不必放進每 Tick 的 snapshot（推論，是否這樣做由使用者決定） |
| 差分與基準的狀態放在哪裡 | 協議升級時 | 第 5 節：這些技術都需要每個 Client 一份的狀態，和 Gateway 無狀態的約束直接相關 |
| pv8 之後還能不能在 PlayerState／CombatState 加欄位 | 每次要加欄位時 | 型別最大只剩 25 B（protocol-v8 §6） |

## 限制

- Overwatch 的演講影片、Valve 的原文、Titanfall 的 netcode 資料、塔科夫的物品與 AI 同步方式，都沒有取得一手內容；相關敘述已標為二手或推論。
- 主會話沒有逐一打開來源核對；引用其中的數字做決定之前，要先打開來源確認。

## 來源清單

**Overwatch**
- Overwatch Gameplay Architecture and Netcode（GDC 2017）：https://gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and
- Replay Technology in Overwatch（GDC 2017）：https://gdcvault.com/play/1024053/Replay-Technology-in-Overwatch-Kill
- Edgegap，Overwatch netcode 摘要（二手）：https://edgegap.com/blog/game-backend-deep-dive-overwatch-2016-netcode-architecture-rollback
- PCGamesN，Overwatch netcode 更新：https://www.pcgamesn.com/overwatch/overwatch-netcode

**Valorant**
- 128-Tick Servers：https://www.riotgames.com/en/news/valorants-128-tick-servers ／ https://technology.riotgames.com/node/112
- Fog of War：https://www.riotgames.com/en/news/demolishing-wallhacks-valorants-fog-war ／ https://technology.riotgames.com/node/108
- Peeking into VALORANT's Netcode：https://technology.riotgames.com/node/111

**Counter-Strike 2（二手）**
- Dexerto：https://dexerto.com/counter-strike-2/counter-strike-2-sub-tick-updates-explained-2094004
- GameRevolution：https://www.gamerevolution.com/guides/937479-cs2-tick-rate-changes-counter-strike-2-sub-tick-updates-csgo

**Apex Legends**
- Servers & Netcode Developer Deep Dive：https://www.ea.com/games/apex-legends/news/servers-netcode-developer-deep-dive

**Unreal Engine**
- Actor Priority：https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-priority-in-unreal-engine
- Actor Network Dormancy：https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-network-dormancy-in-unreal-engine
- Replication Graph 技術部落格（抓取時 403，內容來自搜尋摘要）：https://www.unrealengine.com/tech-blog/replication-graph-overview-and-proper-replication-methods
- Grid Spatialization API：https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ReplicationGraph/UReplicationGraphNode_GridSpatia-

**Escape from Tarkov（可信度低或二手）**
- diamondlobby，tick rate 比較表：https://diamondlobby.com/server-tick-rates/
- Mein-MMO，2018 年 desync：https://mein-mmo.de/en/escape-from-tarkov-entwickler-arbeiten-am-grossen-lag-problem,209596
- TechRaptor，0.13.1.0：https://Techraptor.net/gaming/news/escape-from-tarkov-update-01310-aims-to-increase-accuracy-reduce-desync
- Shazoo，0.13.1.0：https://shazoo.ru/2023/07/06/145404/escape-from-tarkov-polucila-patc-ulucsaiushhii-pozicionirovanie-i-sinxronizaciiu-igrokov-na-kliente-i-ix-sviaz-s-serverom

**Hunt: Showdown**
- Steam 討論串：https://steamcommunity.com/app/594650/discussions/8/1746770563031395786
- PCGamesN，Update 1.9：https://www.pcgamesn.com/hunt-showdown/update-1-9-patch-notes

**Quake 3**
- Network Model：https://mail.fabiensanglard.net/quake3/network.php
- The Quake3 Networking Model：https://fabiensanglard.net/quake3/The%20Quake3%20Networking%20Mode.html

**Gaffer on Games**
- Snapshot Compression：https://gafferongames.com/post/snapshot_compression/
- State Synchronization：https://gafferongames.com/post/state_synchronization/
- Reliable Ordered Messages（延伸閱讀）：https://gafferongames.com/post/reliable_ordered_messages/
- Packet Fragmentation and Reassembly（延伸閱讀）：https://gafferongames.com/post/packet_fragmentation_and_reassembly/
