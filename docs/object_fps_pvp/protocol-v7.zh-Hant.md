# PvP Protocol v7：檢測、無狀態邊緣、丟棄回報、射擊步序與 hitbox 的契約支持

更新：2026-10-16（定稿，D96；§1 兩處條文依 D99 修改）；初稿 2026-10-15。**狀態：定稿，尚未生效。** 由 v7.1 的 `pv7-design` 產出（[v7.1 交接](plans/v7.1/HANDOFF.md) D92、D94～D96；設計紀錄見 [pv7-design](plans/v7.1/pv7-design.md)，Gateway 方向見 [gateway-direction](plans/v7.1/gateway-direction.md)）。D-1～D-15 都已決定，§2a 已做 xhigh 對抗式檢查。定稿之後，任何 wire 的新增、刪除、改號、改型別或改語意，都要先徵求使用者同意。
本文件的 vN 指網路協議版本 pvN。現行 wire 仍是 pv6（[protocol-v6](protocol-v6.zh-Hant.md)）；本契約在 v7.1 的 pv7-line 分批實作，`pv7-merge` 時一次生效。
pv4～pv6 中沒有在此改寫的條款，pv7 原樣沿用；改寫的條款標明原出處。程式行號以 `95bac31` 為準。

## 0. 原則

- **pv7 是 v7.1 唯一一次 wire 變更**（v7.1 D90 的做法 (a)）。本文件列出的欄位、列舉值、語意與 HTTP 鍵，照 pv7-line 的批次順序在同一條 PR 線加入，`pv7-merge` 時一次合併。合併之後，任何新增、刪除、改號、改型別或改語意（包括 DropReason 新增值、W、K 的值）都要先徵求使用者同意。版本嚴格相等、三個角色同一個 PR、舊程序重新啟動（v7 D32）。
- **v7.1 是 hotfix：先檢測、再評估。** 新增的大多是檢測用的欄位；它們只供觀察，不進任何判定（§2）。
- **Gateway 是無狀態的邊緣**（v7.1 D94）：Gateway 只做翻譯、單則訊息的檢查，然後原樣轉送；它不持有 Match 決策狀態的副本，不做跨訊息、依內容的判斷，也不排程重送。內部訊息保留身分並且冪等，可靠送達只由端點（Client 與 Match）負責（§2a）。
- **只有 `a1-shot-step` 改變判定規則。** 三個 edge 行為批（`edge-input-passthrough`、`edge-results-passthrough`、`edge-actions-passthrough`）改變端到端的到達時序，各自事前宣告並前後對照；其他批次都維持行為不變（§9）。
- **骨骼 hitbox 只做契約支持**（v7.1 D87）：欄位與語意現在定下，v7.1 填等同現行行為的值；實作在 v8。v8 的其他候選不在 pv7（§10）。
- 每個欄位都寫出它支撐的功能（AGENTS.md §2）。

## 1. 版本與錯誤（P1）

### 版本值

- `ProtocolVersion`＝7，只接受 7。四處定義一起改，`test_protocol_version.py` 檢查四者一致，拒絕集合由常數導出（定義表照 protocol-v6 §1）。
- `proto-rename-v7` 先把 `client_v6.proto`／`runtime_v6.proto` 改名為 `client_v7.proto`／`runtime_v7.proto`，Go 套件改為 `clientv7`／`runtimev7`，CMake target 一起改；這時版本值仍是 6。`version-errors` 再把值改成 7。

### 版本位置不變式

- 三個版本位置在之後的任何版本都不改號、不改型別、不搬位置：GYOP 標頭的 `version`、`RuntimeEnvelope` 的欄位 1（uint32，永遠第一個序列化）、HTTP JSON 的 `protocol_version` 鍵。
- 任何 JSON 中的 `protocol_version` 都是送方自己的版本。
- 兩端一律先檢查版本，再解讀內容。版本不符的錯誤要寫出期望值與收到值，並且和解析失敗分開。

### 各位置的行為（取代 protocol-v6 §1 的版本表）

| 位置 | 版本不是 7 時 |
|---|---|
| HTTP join 請求（Gateway） | 在保留名額之前回 409，body 為 `{"error":"protocol_version","protocol_version":7}`。缺少視為 0；不是 0～65535 的整數照舊回 400 `invalid_join`。Gateway 寫一行 log：`join refused remote=<addr> code=protocol_version expected=7 received=<n>`（和既有的 `join refused remote=<addr> code=<code>` 同一系，D99） |
| HTTP 回覆的錯誤（Client） | **以 body 的 `error` 判斷，不看狀態碼**：`error`＝`"protocol_version"`（不論 409 或 400）才產生 `protocol_version_mismatch: Client 7, Gateway <n\|unknown>`，n 取 body 的 `protocol_version`（只接受 JSON 無號整數，沒有就寫 unknown）。其他錯誤（例如 409 `room_full`）照舊 |
| HTTP join 成功回覆（Client） | 回覆的版本不是 7 時，錯誤為 `protocol_version_mismatch: Client 7, Gateway <n>`，取代 `Client protocol mismatch`。保留還在 reserved 階段，Gateway 不會送 PlayerLeave；Client 照舊在下一個動作以 HTTP leave 釋放 |
| GYOP 標頭（Client、Gateway） | 靜默丟棄：不回覆，不改序號與活性。Gateway 計入 `drop_other_version_datagrams`，經 Gateway 串流的 `DROP_OTHER_VERSION_DATAGRAMS` 讓 Match 知道（§4）。Client 新增本機計數，把版本不符和其他解碼失敗分開，寫進 worker 統計行 |
| IPC Ready（Gateway） | 版本不符獨立成一種啟動失敗，訊息保留既有前綴：`runtime readiness contract mismatch: protocol version expected 7, received <n>` |
| 之後的 IPC envelope | 兩端都把「版本不符」和「解析失敗、訊息為空」分開，未知 oneof 要寫出欄位號，然後走既有的關閉路徑。既有的 close 原因字串（`eof`、`read: …`、`pending controls over 64`）逐字不變 |
| 版本不符時的告知 | 偵測到版本不符的一方，關閉前盡力寫出一個只含欄位 1（自己的版本）的 envelope。收方先檢查版本，再寫出期望值與收到值。Match 對「Connected 之後還沒收到任何框架就 eof」另寫一行提示。**實作範圍**（D99）：Match 先送 Ready，所以實際會偵測到不符的只有 Gateway 讀 Ready 的時候；告知只在這裡送（寫入逾時 500 ms）。Gateway 接受 Ready 之後兩端版本已一致，之後的不符只會來自實作錯誤：偵測方寫出期望值與收到值後直接關閉，不送告知，避免 link 上出現第二個寫入者 |
| `GET`／`POST /rooms`、leave | 照舊不帶版本 |

### 驗收工具

- `version-errors` 凍結 pv7 版分析器：只改版本常數。凍結前在 pv6 語料上核對，除了事前列出的版本比對項（例如比對 probe 報告的 `protocol` 欄位）之外，逐值一致。
- `run_quad` 的 mixed 模式（舊 Match 對新 Gateway、舊 Client）在 `version-errors` 改寫成 pv7 版，或明寫退役。
- `mutations.json` 的 `09-version-python` 改寫 find 字串。

## 2. 診斷的共通規則

### 診斷不進判定（D32）

- 下列值只供觀察：ClockSample 算出的 offset、delay、漂移；dwell、hold；Client 的 GYOP 序號（`client_transport_sequence`）；丟棄回報；動作拒收回覆；Leave 原因；v7.1 的 `view_tick_q16`、`hit_part`。
- 它們絕不被下列任何一處讀取：250 ms 的 InvalidReference／Expired；160 ms 的品質門檻與逐出；TrackSlack 與 slack 樣本；backlog／starvation；5 秒的 session／runtime／Client 逾時；A1 的判定與冷卻；Client 的相位追蹤、token 規則、射擊閘門與 SnapshotTimeline；Gateway 的 admission 與速率。
- **結構隔離**：時鐘估計器、dwell、hold 與丟棄回報只存在於 I/O 層（Gateway、`IpcHost`、`ClientConnection`），由 `IpcHost` 自己寫對時與 `gw_` 前綴的日誌。`MatchRuntimeHost` 的公開介面不新增任何帶這些值的參數；若一定要經過它，只收不透明的 `DiagnosticCounts`，而且只准在 `TakeIngressStatistics` 內使用。
- **守衛**：原始碼守衛 CTest 涵蓋 `PvpMatch.cpp`、`LocalPlayerPrediction.cpp`、`MatchRuntimeHost.cpp`／`.hpp`（至少 `SubmitInput`、`Step`、`TrackSlack`、`JudgeConnectionQuality`）、`SnapshotTimeline.hpp`、`PvpApplication.cpp`，以及 `a1-shot-step` 之前的 `FireGate.cpp`。Go 端加測試，確認 session 套件與 `server.go` 的逾時、admission 不引用 ClockSample、dwell、hold 的符號。
- **差分測試（每個相關突變的 oracle）**：同一個 IpcHost＋Host 情境跑兩次，一次診斷值全缺，一次給極端值（offset ±10 s、跳變、dwell 與 hold 都是 10,000,000）。ShotDecision、參考年齡、slack 樣本與逐出必須逐位元相同。
- 例外：ShotRequest 的 E、S、`view_tick_q16` 是 Client 提供的不可變請求內容，要進 `EqualShot` 與 `ShotRequest::operator==`，參與重送的內容比對（誠實 Client 的重送逐位元相同）；但在 `a1-shot-step` 之前不參與任何判定，Host 只寫影子紀錄。

### 診斷沒有否決權（依 D-5）

- ClockSample、GatewayDropReport、動作拒收回覆、dwell、hold、Leave 原因、`view_tick_q16` 發生語意違規（序號跳號、累計倒退、範圍外、比對不到 origin、未知列舉值）時：記錄、計數，忽略該值或把該串流標為不可信；不關閉 link，也不讓載體失效。
- protobuf 解析失敗、未知 oneof、版本不符、方向錯誤的已知 oneof，照舊關閉 link（結構違規）。
- Leave 在任何情況下都照常處理。遊戲語意欄位的既有驗證不變，例如受擊紀錄違規時整份 snapshot 無效。
- 這些都屬 Runtime Error（`docs/architecture/error-handling.md`），本端不 assert。

### 診斷訊框不刷新活性

- Gateway 的 runtime 讀取期限 5 秒，只由 Snapshot、JoinResult、Evicted、Error 刷新。pv7 的 ActionResults 由 Match 的 IPC 執行緒以計時器重送（§2a），和心跳同一類，所以也不刷新。Match 每個 Tick 都發布 snapshot（0 位玩家時也一樣，`MatchRuntimeHost.cpp:450-451`），所以模擬正常時不會誤判。
- 實作：記下最後一個這類框架的時刻，以剩餘時間呼叫 Read。**剩餘時間 ≤0 時不得呼叫 Read**（可重用層的 framing 遇到 timeout≤0 會取消期限），直接走既有的逾時失效路徑。
- 理由：Match 的 IPC 由獨立的 `gyo-match-ipc` 執行緒服務（`IpcHost.cpp:187`），模擬卡住時照樣回覆心跳、照樣重送結果；若這些框架刷新期限，原本 5 秒就失效的 link 會一直存活。
- Hello、Welcome 帶的 clock 不改變 session 與 Client 的活性規則。

### 診斷訊框的計時與佇列

- **Gateway**：由 Server 擁有的 1 Hz 診斷 goroutine 組裝 ClockSample，鎖順序固定為先 `s.mu` 再 `l.mu`。組好的 envelope 放進 link 的單槽診斷佇列（不是 `l.controls`；新蓋舊時計數）。GatewayDropReport 不經過這個槽：計時器只標記「該送一輪」（距離上一輪不到 1 秒就延後），writer 在寫出前一刻才從丟棄帳本（葉鎖）組每條串流的回報，所以 `report_sequence`、樣本與 dwell 的終點都在寫出時決定，不會有被取代的回報，也不會有序號空洞（D104）。link 的 writer 只多一個分支：取診斷槽與這一輪的回報並寫出。這個分支不改變資料 FIFO 的順序與上限（§2a），不動 `l.wake` 與輸入 token；在 `edge-actions-passthrough` 之前，也不動 action 的 nextSend 與 timer。
- **Match**：ClockSample 回覆用獨立的有界槽（1 則，新蓋舊），不進 `session.controls`（超過 64 會關閉連線）；寫出優先順序排在 controls、action lane、snapshot 之後；診斷槽有內容時也要觸發寫出排程；transmit 在選中框架時才蓋。動作拒收回覆依 D-6 搭在既有的 ActionResults 上，不另設訊框。
- 被新蓋舊取代的診斷訊框要計數（D57：沒有無聲的出口）。

### 時鐘域與時間戳

| 域 | 來源 | wire 值 |
|---|---|---|
| M（Match） | `std::chrono::steady_clock`（`Engine::Time::MonotonicClock`），和 Match trace 的 `time_ns` 同源 | `time_since_epoch` 的 µs |
| C（Client） | 同上（`ClientConnection.cpp:29`） | 同上 |
| G（Gateway） | Go 的單調讀數 | `time.Since(程序啟動時取的唯一參考點)` 的 µs；runtime link 與所有 session 共用 |

- SDL ticks 與牆鐘不得上 wire。
- 跨時鐘域的時間戳只能被照抄回去（`ClockSample.origin_us`），或在四時間戳公式中和同域的值相減。其他時間一律用單一時鐘域內量的持續時間上 wire（µs、uint32、夾在 10 s）。接收端不拿對方的時間戳直接和自己的時鐘比，也不用固定偏移換算。
- macOS 上系統睡眠時，Go 的單調時鐘（G）停止，Match 與 Client 的 `steady_clock`（M、C）繼續走（2026-10-17 實測，只記錄：睡了約 27 秒，Match 的時鐘視窗出現一次約 26.7 秒的 offset 跳變、`jump=1`；Gateway 對 Client 的回聲出現 `negative_delay`）。它影響解讀，不影響欄位定義。
- Tick 與序號不是時鐘：Authority Tick、`view_tick_q16`（Tick×65536）、命令序號 (player, epoch, life, sequence)。

### dwell 只有一個定義

- dwell＝從 Gateway 讀到帶來該項目的資料包（`receivePacket` 的 now，`server.go:373`），到**承載它的 IPC 框架在 `write()` 中開始 Marshal／`runtimeWrite` 的時刻**；用 G 域量，夾在 10,000,000 µs。
- 依 §2a，一則 IPC 訊息對應一個 Client 資料包，所以 dwell 是一對一的，沒有合併規則。
- 沒有資料包的事件（snapshot 類、下行 control 與 results）以事件發生的時刻為起點；各原因的起點寫在 §4 的列舉表。
- 共用這個定義的欄位：`PlayerInput.gateway_dwell_us`、`ActionBatch.gateway_dwell_us`、`DropSample.dwell_us`。

### 日誌只加鍵

- `version-errors` 凍結 pv7 版分析器之後，同一條線的批次只在既有行的行尾加鍵；新紀錄寫到新檔或新的行族；不在 `commands.jsonl` 加事件種類；不改既有的 close 原因字串。
- Hello／Welcome 的回聲不改變 `received_datagrams` 的組成。
- 日誌格式在 `log-contract` 之前都是暫定。

## 2a. IPC 送達語意（v7.1 D94、D95）

Gateway 是無狀態的邊緣：內部訊息保留身分並且冪等，可靠送達只由端點負責。這一節改寫 runtime proto 的送達註解（`runtime_v6.proto:31-33`、`:39`、`:123-131`）與 protocol-v4 中由 Gateway 合併、重送的條款，包括「結果不能套用 latest-wins」（`protocol-v4.zh-Hant.md:193-195`）：pv7 的結果框架由 Match 重送到 ACK 為止，被較新的框架取代不會造成永久遺失，所以 Gateway 的單槽持有（下述）可以 latest-wins。

runtime proto 的 `ActionBatch`／`ActionResults` 要寫入送達註解：批可能重複；ACK-only 而且 ack ≤ 已退役值，表示要求補送退役；結果會重送，而且冪等。

### 各訊息的語意

| 訊息 | pv6 | pv7 |
|---|---|---|
| `PlayerInput`（Gateway→Match） | Gateway 依 `lastResolved` 副本合併、去重、截短，只送這條 link 沒寫過的命令；另有拒絕線 | **一個 Client 資料包的窗口原樣轉送**，可能重複，也可能含已解析命令的副本；Match 以 (player, epoch, life, sequence) 去重（既有的 AdmitInput 與暫存去重）；`observed_authority_tick` 取該資料包自己的值 |
| `ActionBatch`（Gateway→Match） | Gateway 的 link 動作視窗聚合請求，依帳本驗證（A3、A5），以 30 Hz 持有與重送 | **一則 Client 批次原樣轉送**（請求加累計 ACK），可能重複；Match 既有的接納檢查判斷並依原因計數 |
| `ActionResults`（Match→Gateway→Client） | Match 每個裁決與退役只送一次，由 Gateway 的 session 帳本對 Client 重送 | **Match 對每位玩家重送**：Client ACK 之前，以 ≤`ActionSendRate` 重送未確認的裁決，每框架 ≤8 筆，**先放最小的 4 筆未確認 id，其餘槽位循環**（Client 只能連續 ACK，最小 id 送不到會卡住退役）；退役前進時送一次；收到 ACK ≤ 已退役值的 ACK-only 批時，再補送一次退役；拒收也觸發一次送出（D-6）。Gateway 逐則檢查後轉給 Client，送出節奏見下面的單槽持有 |
| `WorldSnapshot`、Join／Leave、JoinResult、Evicted | — | 語意不變。Gateway 仍負責把 snapshot 分送給每位玩家（latest-wins） |

### 順序與背壓（D-10）

- link 依到達順序寫出；**同一玩家的 controls 與資料保持到達順序**，Leave 不會超車同一玩家還在佇列中的輸入。
- controls 不丟：待寫超過 64 則照舊讓 link 失效。
- 資料（輸入、動作）每位玩家一個有界 FIFO；滿了丟最舊的，計入 §4 的佇列溢出原因並回報。**上限準則：每位玩家不少於 64 則輸入窗口**（pv6 的對應上限是 32 個已解析命令，`runtime_link.go:182-185`）。被替代的命令只存在於最舊的窗口，所以溢出＝遲到證據的損失；這和 2026-10-10 事故同一類，差別是現在有計數與回報。深度與回報用替換 `runtimeWrite` 的 Go 測試驗證（既有的延遲注入在 relay 端吸收，量不到 Gateway 的 FIFO 深度）。
- **結果的「最多 30 Hz」由 Gateway 的單槽持有執行**：Gateway 對每位玩家記「上次送出結果的時刻」，並至多持有 1 則待送的結果框架（latest-wins）；在「上次送出＋1/30 s」時放出，被較新的框架取代時，舊的那則計入 `DROP_RESULTS_OVER_RATE_PACKETS` 並回報。它不依內容選擇，也不重送；延後送出不是重送。這和 snapshot 的 latest-wins 佇列同一類，屬於允許的有界外送佇列。
  - 不採「間隔內到達就丟」：它和 Match 的節拍同值（兩邊都是 33,333,334 ns），只要有抖動就會丟，新裁決最多晚約一個間隔；丟棄若和循環游標的相位耦合，固定一部分裁決可能長時間送不到（xhigh 檢查第 1 項）。
- **唯一的 UDP 寫出者**：每位玩家的 GYOP 序號，在唯一的 UDP 寫出處、寫出前才指定；30 Hz 的放出判斷也在同一處。Client 會無聲丟掉序號不比上一則新的封包（`ClientConnection.cpp:453`、`:590`），所以序號不能在 reader goroutine 先指定。
- 逐出或 Leave 時，佇列中這位玩家的資料照樣轉給 Match，由 Match 依「未知玩家」計數；和 Leave 同一個 Tick 到達的資料，Match 先處理 controls，所以仍會被丟（計數位置從 Gateway 的 `drop_discarded_on_leave_*` 移到 Match 的 `leave_discarded_*`）。link 失效或寫出失敗時的清除，結構上無法回報，只寫 Gateway log。
- 給已移除玩家的結果（Match 讀到 Leave 之前可能還在重送）：歸入出口完整性的 (d)「不算損失」，只寫 Gateway log；Match 已經知道這位玩家離開了（和 pv6 的 D7 同類）。

### session 活性（D-11）

- 解碼成功就提交序號與活性（呼叫公共層 Session 的既有介面），不看內容；舊序號照現行規則不提交。
- Gateway 不再依內容路由，所以 snapshot 的兩個守衛（D1 tick 單調、D2 倒退檢查）移除（D-12）：IPC 本來就有序，Client 也只接受更新的 tick（`ClientConnection.cpp:533-535`）。v7 的 L2 語料中，兩者的計數都是 0。

### Gateway 的約束（D-14；由 `edge-contract` 寫進 network-architecture 的 `Design Invariants`）

- **允許**（可以重建、不參與遊戲判定的軟狀態）：連線與 session（token、endpoint、傳輸序號、速率、活性、生命週期）、房間與保留、Ready 的設定副本、有界的外送與 IPC 佇列（溢出要計數並回報）、每位玩家上次送出結果的時刻與至多 1 則待送的結果框架、診斷計數與時鐘樣本。對每則訊息只做翻譯，以及單則訊息內的版本、格式、範圍與授權檢查。依傳輸序號、速率與 session 生命週期的跨訊息判斷（例如舊序號、未認證、非 active）是允許的。
- **禁止**（Gateway 不得做的事；括號內是負責的一方）：
  - Gateway 不得持有 Match 決策狀態的副本，例如游標、epoch、life、命令表、動作帳本、裁決、退役（這些狀態的權威是 Match）。
  - Gateway 不得**依遊戲項目的身分或內容做跨訊息的判斷**，例如去重、合併、判定過時、解決衝突（由 Match 判定）。
  - Gateway 不得排程重送（可靠送達由端點負責：結果由 Match 重送到 Client 確認為止，輸入與動作由 Client 重送）。
- **白名單的分類**（D105）：診斷用的欄位分三類，各有限制。
  - 診斷狀態（時鐘同步、回聲、丟棄帳本與其觸發）：允許；判定路徑不得讀取（§2「診斷不進判定」）。
  - 單次呼叫的脈絡（處理一個資料包期間記下的收包時刻與序號）：不得跨資料包保留。
  - 測試縫隙（套件變數與注入的時鐘）：正式路徑只用預設值。
- 判斷準則：這個 Gateway 欄位或判斷，是否要知道「Match 已經有什麼」才能做對？是的話，它屬於 Match。新增任何 Gateway 狀態欄位都是 Architecture Delta，要經使用者同意。
- 守衛：Go 測試遞迴走訪 Gateway 狀態型別的欄位（含 map、slice 的元素、具名型別與 any／interface 欄位），並用 `go/ast` 掃描套件層級的變數，以及以 `go` 啟動的函式與閉包裡的區域 map、slice，和分類表比對；配一個「新增未分類欄位」的突變。依內容的判斷測不到，靠審查，並從 AGENTS.md 連到這份約束（設計事故行動 4）。實作在 `apps/object_fps_pvp/gateway/state_whitelist_test.go`（v7.1 `edge-contract`），條文在 network-architecture §7 的「Gateway：無狀態邊緣」（D120 核准）。

### 兩份 proto（D-15）

Client 與 runtime 的兩份 proto 加 adapter 的轉換保留：這就是「每種外部 Client 一個接口」。

## 3. 對時與回聲（P2、P3）

### ClockSample

`client_v7.proto` 與 `runtime_v7.proto` 各定義一份，同號同型（延續兩份契約各自獨立的原則）。≤35 B。

| 欄位 | 號 | 型別 | 域 | 語意 | 驗證 |
|---|---|---|---|---|---|
| `origin_us` | 1 | optional uint64 | 對端（照抄） | 本端最近收到的那則對端 ClockSample 的 `transmit_us`，逐位元照抄 | 和 `receive_us` 同時有或同時沒有；收方要在自己最近送出的 8 個 `transmit_us` 中找到它，找不到就丟棄樣本並計數 |
| `receive_us` | 2 | optional uint64 | 送方 | 送方讀到上述那則訊息的時刻 | ≤ 同一則的 `transmit_us` |
| `transmit_us` | 3 | uint64 | 送方 | 送方序列化這則訊息的時刻（Match 在選中框架時才蓋；Welcome 在編碼時蓋） | ≥1；同一送方、同一串流嚴格遞增（送方用 max(now, 上一則＋1)） |

公式：t1＝A 端送出的 transmit；t2＝B 端的 receive；t3＝B 端的 transmit；t4＝A 端讀到 B 回覆的時刻。delay＝(t4−t1)−(t3−t2)；offset(B−A)＝((t2−t1)+(t3−t4))/2。ClockSample 是對稱的，兩端都能當 A。

### runtime link 心跳（P2，`heartbeat-sync`）

- `RuntimeEnvelope.clock`（oneof 20）。Gateway 由 1 Hz 診斷計時器送出；Match 對每則心跳回覆 1 則（單槽，新蓋舊）。
- 只作量測與診斷（§2）。不共用輸入 token、nextSend、`nextActionSendAt` 或 link 寫出迴圈的喚醒來源（FireGate 重推條件 4 在 `a1-shot-step` 之前仍適用）。`edge-actions-passthrough` 之後 link 沒有動作計時器，nextSend 這一項失去對象，其餘照舊。
- 日誌：每 10 秒視窗取 delay 最小的樣本，記 offset、delay 最小與最大、樣本數、不合法數、漂移（ppm，在跳變處分段回歸）、跳變旗標；負 delay 的樣本丟棄並計數；整個視窗沒有樣本時記為缺樣本。Gateway 與 Match 各寫一行。

### Client↔Gateway 時間回聲（P3，`time-echo`）

- `Hello.clock`（2）：origin＝最近收到的 `Welcome.clock.transmit_us`，receive＝Client 讀到那則 Welcome 的時刻，transmit＝序列化這則 Hello 的時刻。Gateway 要等 Hello 通過認證才讀；樣本不合法就忽略並計數，Hello 的 admission、liveness、Join、Welcome 一律照常。
- `Welcome.clock`（11）：origin＝該 session 最新一則已認證 Hello 的 `clock.transmit_us`，receive＝Gateway 讀到那則 Hello 的時刻，transmit＝編碼這則 Welcome 的時刻。Client 只當診斷用，不影響 `CheckArena`、規則檢查與 welcomed。
- 不新增封包與 GYOP 種類，沿用 Hello 的節拍（welcomed 之後每 1000 ms）。Welcome 走有損的 control 佇列，遺失只是少樣本。
- `ReadyFitsWelcome` 要把 ClockSample 的最大編碼算進去。
- 驗收 probe 可以不帶 clock。

## 4. Gateway 丟棄回報（P4，`drop-report`）

Gateway 執行了、但沒能轉送內容的丟棄，依原因回報給 Match（D49 第二階段）。只帶身分，不帶內容；回報有界，計數不失數。

依 §2a，Gateway 不再持有 Match 決策狀態，所以這裡只剩傳輸與 session 層級的丟棄。`drop-report` 排在 edge 批之前（D90、D95），先照現行出口實作；那時的過渡原因（A3／A5、截短、拒絕線溢出、Leave 時清除、D1、D2）只存在於 pv7-line 的中間 commit，edge 批刪掉對應的程式時一起刪除，不進入合併後的 pv7。

### GatewayDropReport（`RuntimeEnvelope.drop_report`，oneof 21，Gateway→Match）

| 欄位 | 號 | 型別 | 語意與驗證 |
|---|---|---|---|
| `player_id` | 1 | uint64 | 串流身分。0＝Gateway 範圍；其他值＝該玩家，包括未認證的宣稱與已被移除的玩家。Match 不要求自己持有這位玩家 |
| `report_sequence` | 2 | uint64 | 同一 link、同一串流從 1 起算，寫出時才指定，每則加 1；也是兩端日誌的關聯鍵 |
| `final` | 3 | bool | 該串流的最後一則（見「串流的起訖」） |
| `counts` | 4 | repeated DropCount | 完整的累計狀態，不是增量：列出所有非 0 計數，依 reason 遞增排列；每個 reason 最多一次，要在該串流的範圍內；和前一則相比不得倒退或消失 |
| `samples` | 5 | repeated DropSample | 自上一則已寫出的回報之後，最早發生的至多 4 個事件；樣本的 reason 必須也出現在 counts |
| `samples_omitted` | 6 | uint64 | 累計沒有被取樣的事件數；不得倒退 |
| `merged_streams` | 7 | uint64 | 只在 Gateway 串流：因為延後佇列已滿，被合併掉、沒有單獨送 final 的串流累計數；不得倒退 |

### DropCount

| 欄位 | 號 | 型別 | 語意 |
|---|---|---|---|
| `reason` | 1 | DropReason | 丟棄出口 |
| `total` | 2 | uint64 | 累計值，等於 Gateway 寫出這則回報時對應 `drop_` 鍵的累計；≥1、不得倒退 |

### DropSample

| 欄位 | 號 | 型別 | 語意 |
|---|---|---|---|
| `reason` | 1 | DropReason | 這個事件的出口 |
| `dwell_us` | 2 | uint32 | §2 的 dwell 定義；起點依下表 |
| `transport_sequence` | 3 | uint32 | 上行資料包的 GYOP 序號，或下行封包的 Gateway 序號；沒有時為 0 |
| `movement_epoch` | 4 | uint64 | 命令類原因所屬的 epoch；其他原因填 0 |
| `life_generation` | 5 | uint64 | 命令類原因所屬的 life；其他原因填 0 |
| `first_command_sequence` | 6 | uint64 | 命令類原因：被丟命令的涵蓋範圍下界（範圍內的序號不一定全被丟，實際數量看 `item_count`） |
| `last_command_sequence` | 7 | uint64 | 涵蓋範圍上界 |
| `first_action_id` | 8 | uint64 | 動作類原因：action id 涵蓋範圍下界；ACK 類兩欄都填被丟的 `acknowledged_through` |
| `last_action_id` | 9 | uint64 | 涵蓋範圍上界 |
| `snapshot_tick` | 10 | uint64 | snapshot 類原因的 tick；其他原因填 0 |
| `item_count` | 11 | uint32 | 這個事件丟掉的項目數（命令、請求、封包） |

### DropReason

值名＝ingress v1 的 `drop_` 鍵轉大寫，一個值對應一個鍵；原因字只在 `gateway/ingress_statistics.go` 的名稱表定義一次，Go、C++、Python 讀同一份跨語言 golden。1～34 是玩家範圍，40～45 是 Gateway 範圍；16、17、20～26、43、44 在 proto 宣告為 `reserved`（D94 之前的草案用過，定稿時刪除，之後不得重用）。新增值就是 wire 變更（D-7）。

| 值 | 名稱 | 出口 | dwell 起點 |
|---|---|---|---|
| 0 | `DROP_REASON_UNSPECIFIED` | 違規值 | — |
| 1～4 | `DROP_RATE_LIMITED_{HELLO,INPUT,ACTIONS,OTHER}_PACKETS` | 超過 Session 的速率限制 | 收包 |
| 5～8 | `DROP_UNAUTHORIZED_{HELLO,INPUT,ACTIONS,OTHER}_PACKETS` | 未認證（記在宣稱的 session，可被偽造，只能記錄） | 收包 |
| 9 | `DROP_STALE_SEQUENCE_ACTIONS_PACKETS` | 舊序號的 Actions（傳輸序號層級；照 ★ 轉送屬行為改變，留給 re-evaluate） | 收包 |
| 10 | `DROP_STALE_SEQUENCE_OTHER_PACKETS` | 舊序號的其他種類 | 收包 |
| 11～13 | `DROP_MALFORMED_{HELLO,INPUT,ACTIONS}_PACKETS` | 解碼或單則訊息的檢查失敗 | 收包 |
| 14、15 | `DROP_INACTIVE_{INPUT,ACTIONS}_PACKETS` | session 不是 active | 收包 |
| 18 | `DROP_UNKNOWN_KIND_PACKETS` | 未知的 GYOP 種類 | 收包 |
| 19 | `DROP_ADMISSION_UNKNOWN_PACKETS` | Session 回傳未知的接納錯誤（應為 0） | 收包 |
| 27～29 | `DROP_WRITE_FAILED_{SNAPSHOT,RESULTS,CONTROL}_PACKETS` | D5：寫 UDP 給這位玩家時出錯（新鍵） | 事件 |
| 30 | `DROP_CONTROL_QUEUE_FULL_CONTROL_PACKETS` | D6：control 佇列滿，Welcome 或 Error 被丟（新鍵） | 事件 |
| 31 | `DROP_REPLACED_SNAPSHOT_PACKETS` | 送出迴圈中途 snapshot 被較新的取代：還沒拿到被取代那一份的每位 active 玩家各計 1；樣本帶 `snapshot_tick`（新鍵） | 事件 |
| 32 | `DROP_IPC_QUEUE_OVERFLOW_INPUT_PACKETS` | §2a：這位玩家的 IPC 資料佇列滿，最舊的輸入被丟；樣本帶命令身分（新鍵） | 收包 |
| 33 | `DROP_IPC_QUEUE_OVERFLOW_ACTIONS_PACKETS` | 同上，最舊的動作批被丟；樣本帶 action id 範圍（新鍵） | 收包 |
| 34 | `DROP_RESULTS_OVER_RATE_PACKETS` | §2a：為了守 30 Hz 而沒有送出的結果框架，包括在單槽中被較新的框架取代的那一則（Match 會再送）；樣本帶 action id 範圍（新鍵） | 事件 |
| 40 | `DROP_UNDECODABLE_DATAGRAMS` | Gateway 範圍：GYOP 標頭無法解碼 | 收包 |
| 41 | `DROP_OTHER_VERSION_DATAGRAMS` | Gateway 範圍：標頭版本不是 7 | 收包 |
| 42 | `DROP_UNKNOWN_SESSION_DATAGRAMS` | Gateway 範圍：不屬於任何 session | 收包 |
| 45 | `DROP_COALESCED_SNAPSHOTS` | Gateway 範圍：**只計讀取端**的新蓋舊（`server.go:618-630`），被取代的 snapshot 沒有送給任何 Client（新鍵；送出迴圈中途的取代計在 31） | 事件 |

不上 wire、只寫 Gateway log：`room_unavailable`、`cleared_on_close_*`、`abandoned_on_close_*`（發生時 link 已失效，結構上無法回報）；Gateway 的 `drop_rate_limited_packets` 總和（可由玩家計數導出）；kernel 接收緩衝溢出（Gateway 看不到）。

### 串流的起訖與上限

- 串流：每位玩家一條（從保留名額起算），加上 Gateway 一條（player_id 0，從 link 建立起算）。
- 每條串流每秒最多 1 則，只在 counts 或 samples 有變化時送；樣本最多 4 筆，其餘計入 `samples_omitted`；計數一律是累計值（被合併的回報不會少算）。
- Gateway 的 ingress 計數每 10 秒歸零，所以回報要另存累計值；Server 與 link 兩處計數都要由同一個入口同時更新 10 秒行與累計值。
- **final**：玩家被移除（Leave、逐出、逾時）後送 1 則 final；之後不會再有這個 id 的回報。只有曾經有非 0 計數或樣本的串流才送 final；空串流不送，也不建立。
- **每次計時器觸發的上限**：送出的串流數最多 MaxPlayers×2；超出的 final 延後到下一次。延後佇列也滿時，合併成 Gateway 串流的 `merged_streams` 計數。惡意的 join／leave 只能推高數值，推不高訊息的數量或大小。
- **優雅關閉**：Gateway 收到關閉訊號（ctx 取消或 Close）時，先對每條有內容的串流寫出最後一則回報（全部標 final，包括 Gateway 串流），再關 link。只在關閉時發生，不碰遊戲資料。
- **回報日誌**：Gateway 每寫出一則回報，記一行（player、report_sequence、各 reason 的累計）。L2 判定「Match 收到的回報與 Gateway 寫出的回報依 report_sequence 逐則相等」，以及「最後一則的累計 ≤ ingress 累計，差額只來自最後一次寫出之後的事件」。
- 違規（序號跳號、累計倒退、final 之後又出現、範圍外）依 D-5 處置。

### Gateway 出口的完整性

Gateway 處理的每個項目恰好有一個結果：(a) 寫出到 link（`fwd_` 鍵）；(b) 可回報的丟棄（上表）；(c) 只寫 Gateway log（link 已失效、可導出的總和）；(d) 不算損失（D4、D7、D8、D9 等轉送結果，以及給已移除玩家的結果，§2a）。新增出口時，要同時更新名稱表、列舉、範圍與測試。

### 動作內容的判斷（D-1 不再需要）

Gateway 不驗證動作內容，也不持有動作帳本（§2a）；動作批原樣轉送，由 Match 既有的接納檢查判斷並依原因計數。D-1（A3／A5 要不要轉送）隨 Gateway 帳本的刪除而不必決定。

### 未認證的計數

`DROP_UNAUTHORIZED_*` 與 `DROP_MALFORMED_HELLO_PACKETS` 記在宣稱的 session 下，任何知道 session id 的人都能灌高。Match 只能記錄，不得拿來處罰、逐出或改變行為（輸入防偽造已取消，v7.1 D90）。

## 5. 原因紀錄（E2、動作拒收回覆、Leave 原因，`cause-records`）

### Gateway 停留時間與 Client 的 GYOP 序號

- `PlayerInput.gateway_dwell_us`（runtime，6，optional uint32）：承載這則 IPC 輸入的那個 Client 資料包在 Gateway 的停留時間（§2 的定義，一對一）。
- `ActionBatch.gateway_dwell_us`（runtime，4，optional uint32）：同上，承載這則動作批的 Client 資料包。
- `PlayerInput.client_transport_sequence`（runtime，7，optional uint32）與 `ActionBatch.client_transport_sequence`（runtime，5，optional uint32）：承載這則訊息的 Client 資料包的 GYOP 序號（Client 的序號空間，每個 session 從 1 起算；Hello、Input、Actions 共用）。只作診斷：Match 用它逐位玩家觀察 Client→Gateway 的序號缺口。缺口由三種來源組成：Hello（Gateway 自己處理、不轉送）、Gateway 回報的丟棄（`DropSample.transport_sequence` 可以逐筆對上）、網路遺失；逐窗的精確核對要配合 Gateway 的 ingress 計數離線做。Match 絕不拿它去重或判斷新舊（去重只看命令與動作的身分）。
- 以上欄位都放在 IPC 訊息層級，不放進 ShotRequest 或 MovementCommand，不參與任何身分或內容比對。
- 缺少或超界時只記錄，視為缺少，輸入照常處理。

### 端到端 snapshot 年齡（依 D-4）

- `WorldSnapshot.match_hold_us`（runtime 4、client 4，optional uint32，M 域持續時間）：從 Host 發布這個 Tick（參考年齡的起點），到 IpcHost 選中這份 snapshot 的框架開始寫出。起點與終點用同一個時鐘來源（Host 的可注入時鐘也交給 IpcHost）。被 latest-wins 取代、沒有寫出的 snapshot 不帶。Gateway 原樣轉給 Client。
- `WorldSnapshot.gateway_hold_us`（client 5，optional uint32，G 域持續時間）：從 Gateway 讀到這份 snapshot 的 IPC 框架，到**寫給這位玩家**之前；每位玩家寫出前各自填，送出迴圈中途被取代時重新編碼。
- Client 可以把每份 snapshot 的年齡分段：Match 停留＋IPC 單程（runtime link 的 delay 最小值／2）＋Gateway 停留＋網路單程（回聲的 delay 最小值／2）。160 ms 逐出與 250 ms Expired 的往返因此可以拆開來看。
- 缺少或超界時只記錄，snapshot 照常有效。

### 動作拒收回覆（依 D-6）

- Match 整批拒收動作時，在 runtime `ActionResults` 帶 `refused_batches`（4，uint64，該玩家從 link 建立起被整批拒收的累計批數）與 `last_refusal`（5，ActionRefusal）；Gateway 原樣轉入 client `ActionResults` 的 3、4。v7.1 的 Client 只記錄，不改重送。之後若要讓 Client 停止重送，只改行為、不改 wire。
- ActionRefusal（client 與 runtime 各一份，同號）：`ACTION_REFUSAL_UNSPECIFIED` 0、`OVER_BATCH` 1、`MALFORMED` 2、`RESETTING` 3、`INVALID_PLAYER` 4、`INVALID_BATCH` 5、`CONFLICT` 6、`OUTSIDE_WINDOW` 7、`FULL` 8（值名＝`IngressActionRejection` 的名稱）。Resetting 需要 host 把 `IngressActionRejection` 交給 IpcHost 才分得出來。
- 拒收觸發一次結果送出，由 IpcHost 的結果通道負責，和 §2a 的重送放在一起。

### Leave 原因

- `PlayerLeave.reason`（runtime，2，LeaveReason）：`LEAVE_REASON_UNSPECIFIED` 0、`CLIENT_REQUEST` 1（Client 經 HTTP leave 離開，包括 Welcome 時 arena 不符後的釋放）、`SESSION_TIMEOUT` 2、`JOIN_TIMEOUT` 3。逐出、join 被拒、runtime 失效本來就不送 Leave。
- Match 收到 UNSPECIFIED 或未知值時照常移除玩家，記為 unspecified 並計一次違規。Match 處理 Leave 的方式不因原因不同而改變。

### 實作與紀錄（`cause-records`，2026-10-18）

- proto 列舉名稱一律帶前綴：`LEAVE_REASON_*`、`ACTION_REFUSAL_*`（上面的簡寫是同一組值）。`ActionRefusal` 的值是 `IngressActionRejection` 的索引加 1，由 IpcHost 的 `static_assert` 逐值守住。
- Gateway：dwell 在 link 的 writer 開始 Marshal 這則訊息時填（`stampDwell`，與 `DropSample.dwell_us` 共用 `dwellMicros`）；Client 的 GYOP 序號在交給 link 時從資料包的 header 填；`gateway_hold_us` 在送給每位玩家前填並重新編碼；snapshot 的大小檢查以 hold 的上限（10 s）計算。`match_hold_us`、拒收兩欄由 adapter 原樣照抄（不檢查，D-5）。
- Match：Host 的 `TakeSnapshot` 帶回發布時刻，`Now()` 提供 Host 的時鐘；IpcHost 選中 snapshot 要寫出時才編碼並填 `match_hold_us`。`SubmitActionBatch` 以輸出參數交出拒收原因（Resetting 只在這裡分得出來）。Host 的判定與介面上的判定參數不變。
- 日誌（只加鍵或新行族）：Match 的 `leave player=<id>` 行尾加 `reason=<client_request|session_timeout|join_timeout|unspecified>`；缺少或未知時另寫 `leave violation player=<id> value=<n> count=<累計>`。Match 新行族 `gw_transport player=<id> window_ms final messages sequence_missing sequence_gaps sequence_late largest_sequence dwell_samples dwell_missing dwell_range dwell_max_us dwell_mean_us`（每 10 秒與 session 結束時；序號缺口是原始值，扣掉 Hello 與回報的丟棄要配合 Gateway 的計數離線做）。Client 新行族 `[ObjectFPS/PvP] cause records player window_s snapshots match_hold_{samples,missing,range,max_us,mean_us} gateway_hold_{…} refused_batches last_refusal`（每個統計窗口）。
- 4 人 snapshot 的最大 datagram 由 1055 B 變成 1065 B（兩段停留都在 10 s 上限時各 5 B；Go 的 capacity 測試與 worker probe 同步）。

## 6. 射擊照命令步判定（A1）

判定語意在 `a1-shot-step` 生效（v7.1 D137）；欄位依 D-2 在 `hitbox-fields` 加入，先做影子量測（下面「影子量測」一節是 `hitbox-fields` 到 `pv7-checkpoint` 的歷史）。實作：判定在 `PvpMatch::ResolveActions`（到期 `ActionDue`、保留與重判 `Decide`），Client 規則在 `ShotGate.hpp`（`LocalShotGate`）。

### 欄位

- `ShotRequest.movement_epoch`（7，uint64，E）與 `command_sequence`（8，uint64，S），client 與 runtime 同號同型。Shot 與 Reload 都帶。
- E、S 是不可變內容（§2 的例外）。**解碼層不驗證 E**：E＝0 時視為 E≠Ecur，走 pv6 的到達判定，影子紀錄記為 epoch 不符。S 任何值都合法。

### 名詞

- A＝交接 Tick（host 把動作交給 PvpMatch 時的 `tick_+1`），等同 pv6 的 acceptedTick，也是第一次機會。
- L(t)、Ecur(t)、life(t)＝Tick t 移動之後的 `lastResolvedCommand`、movement epoch、life generation。
- T(S)＝解析命令 S 的 Tick。同一 epoch 開始後每個 Tick 恰好推進一個序號，所以 T(S) 只由序號決定。
- W＝`MaxShotStepLead`＝`MaxPendingCommands`＋1＝13，是契約常數。

### 到期與判定

- due(t) ⇔ t≥A 且下列任一條成立：E≠Ecur(t)、請求的 life≠life(t)、L(t)≥S、t≥A＋W。
- D＝第一個 due 的 Tick，在該 Tick 的移動之後判定。
  - 準時：同一 epoch 且 S>L(A)，D＝T(S)。誠實 Client 的 S−L(A)≤12。
  - 退回（late）：S≤L(A)，D＝A，等同 pv6。
  - 退回（epoch／life）：E 或 life 不同，在 A 或之後 epoch 改變的那個 Tick 判定。
  - 上限：epoch 還沒開始，或 S 宣稱過遠，D＝A＋W。
- 非法或偽造的 E、S 只會讓判定落在 [A, A＋W]，不可能早於 pv6，所以不新增拒絕原因。
- **T(S) 只由序號決定**：命令 S 是實際送達的命令，還是被 Held／Neutral 替代，都一樣；射線起點取該 Tick 的權威狀態。「起點和 Client 點擊時看到的自己一致」只在命令 S 是實際命令時成立。
- 在 D 依 pv5 §4 的順序檢查：生命 → Dead → 參考（InvalidReference／Expired）→ Reloading → Reload 的 MagazineFull → Shot 的 EmptyMagazine → 冷卻。

### 參考年齡與冷卻保留（依 D-3）

- 參考年齡在 A 量（和 pv6 同一個時點），存在帳本裡，判定時使用存下的值；保留期間不吃 250 ms。
- 冷卻保留（只限 Shot）：D 時其他檢查都通過、只差冷卻，而且 nextAllowedShotTick−D≤K，保留到 nextAllowedShotTick，在該 Tick 完整重跑一次檢查，不再保留第二次；差距 >K 時回 `REJECTION_COOLDOWN`。
- **K＝min(K_max, cooldownTicks)**（v7.1 D136）：K_max＝2 Tick 是契約常數，不上 wire；cooldownTicks 是射手當時武器的權威規則值（`CombatRules`）。所以 0≤K≤cooldownTicks 對任何武器資料都成立，射速改成資料驅動也不必改這條。
  - 為什麼是 2：`pv7-checkpoint` 的影子資料裡，退回（late）的晚到量 d＝L(A)−S 有 52% 在 0～2 Tick（98 筆，故障情境才有更大的值）；判定最多因此延後約 33 ms。
  - 已知的盲區（只記錄）：該次量測沒有「退回」與「壓著冷卻邊緣連射」同時出現的情境；全自動武器上線時，用故障情境＋全自動的量測重新評估 K_max。
- 冷卻仍在 Tick 空間，`next_allowed_shot_tick` 的語意不變。Client 的「冷卻中的點擊丟棄，不形成稍後自動射擊」（pv5）不變。

### 順序與上界

- 同一位玩家：Match 持有的未裁決動作依 ActionId 裁決，較大的 id 不會比較小的 id 早判定（例如 Reload 排在保留中的 Shot 之後）。只限 Match 已持有的動作，缺號不等待。
- 同一個 Tick 跨玩家：沿用 (A, PlayerId, ActionId)。
- 上界：J−A≤W＋K。已知限制：較大的 id 先到並在等待時，較小的 id 之後才到又各自等 W＋K，可以連鎖；只有 S 不隨 id 單調的對抗式 Client 會遇到，受影響的只有它自己。
- 保留期間 Client 與 link 照舊重送未決請求，Match 視為重複。

### 效果

- 射線起點＝J 時射手移動後的權威腳點＋eyeHeight；目標＝J 的當前權威姿勢（v7.1 的 R＝0，見 §7）。
- `ShotDecision.resolved_tick`（既有欄位，語意「判定 Tick」不變，改的是決定規則）與 `last_shot_tick`＝J；next_allowed＝J＋cooldown。
- 射手在 T(S) 之前死亡時，以 Dead 拒絕。

### Client 規則（改寫 pv5 的 Client 射擊閘門）

- E＝處理點擊那一幀所用預測的 epoch；S＝該 epoch 已產生的最新命令 N（v7.1 D136 確定取 N：`pv7-checkpoint` 的 clean 退回率 0、準時餘裕 S−L(A) 至少 1 Tick）；**E 與 S 必須取自同一個發布的模擬步**。還沒有命令時 S 填 0。
- 預測未啟用或凍結時不送，點擊不排隊（這是 `a1-shot-step` 的行為改變之一）。
- 本機閘門改在序號空間：同一 epoch 內，T̂(S)＝snapshot.tick＋(S−snapshot 的 last_resolved_command)；放行條件是 T̂(S) ≥ max(next_allowed_shot_tick, 已收到的接受裁決的 resolved_tick＋cooldown, 未決射擊的 T̂＋cooldown)。跨 epoch 時保守退回用 snapshot 的 Tick 判斷。
- FireGate 的 earliest、guard、spread 退役。改用 N＋1 屬於 Client 規則修訂，W 已經涵蓋。

### 影子量測（`hitbox-fields` 到 `pv7-checkpoint`，依 D-2）

- Client 照實填 E、S；判定仍在 A，digest 不變。
- Host 對每個裁決記錄：A、E、S、Ecur(A)、L(A)、life 是否相符、若採 A1 會走的路徑（on_time／late／epoch／life／hold_limit）、預計的 T(S)、需要的冷卻保留量。
- 命令 S 的來源（Actual／Held／Neutral）與命令 S 是否之後才到，不由 Host 記錄（v7.1 D127）：離線以 (player, E, life, S) 加上連線或時間順序，對上 `match-commands.jsonl` 的 resolved 事件（帶 `source`）與 `match-ingress.jsonl` 的替代紀錄（第一份晚到的時刻）。對接率是 `pv7-checkpoint` 的判定：每個 on_time 路徑的裁決都要對得到 resolved 事件；對不到的先在同一份軌跡找產品行為的解釋（T(S) 之前該 epoch 被重設、同鍵的 `lifecycle_cancelled`、先到 `trace_end`），有解釋的只記錄，找不到的才是工具無效（D132；軌跡掉資料時不得悄悄少算，軌跡的 `trace_end` 與 `dropped`＝0 另外要求）。
- 用途：`pv7-checkpoint` 產出 K 與 S 取法所需的資料。
- 實作（`hitbox-fields`，2026-10-18）：影子紀錄寫在 `match-shots.jsonl`（版本 1；由 `--movement-trace` 推出路徑：`-commands.jsonl` → `-shots.jsonl`，同 `match-ingress.jsonl` 的先例；`MatchShotTrace.hpp`）。每個裁決一行 `kind=decision`：`time_ns`（Host 記錄的時刻，和 `match-commands.jsonl` 同一個時鐘 `MovementTraceNowNs`；D128）、`player_id`、`action_id`、`action_kind`、`a`、`e`、`s`、`request_life`、`epoch_at_a`、`last_resolved_at_a`、`life_at_a`、`path`（on_time／late／epoch／life／hold_limit）、`step_tick`（A1 會判定的 Tick：on_time 為 T(S)＝A＋(S−L)，late、epoch、life 為 A，hold_limit 為 A＋W）、`cooldown_hold_ticks`（Shot：判定前的 nextAllowedShotTick−step_tick，不足為 0；Reload 為 null）、`rejection`、`view_tick_q16`、`view_valid`、`view_lag_q16`（A×65536−view，有效且非 0 時）。結尾 `kind=trace_end` 帶 `records`、`dropped`、`view_tick_invalid`。`epoch_at_a` 尚未解析任何命令（L＝0）時記為 hold_limit（「epoch 還沒開始」）。
- 命令 S 的來源與是否之後才到：照上面的離線對接（D127）。
- 版本 2（`a1-shot-step`，2026-10-19）：判定改照本節的 A1 規則之後，同一個檔案記錄實際的判定。A 時先記 Ecur(A)、L(A)、life(A) 與 A 時的路徑分類（`path`、`step_tick` 不變），判定發生時（Tick J）才寫一行，多三個欄位：`due_tick`（第一個到期的 Tick D）、`resolved_tick`（J）、`held`（Shot 是否因冷卻保留）；`cooldown_hold_ticks` 改成實際保留的 Tick 數（held 時 J−D，否則 0；Reload 為 null）。`time_ns` 是寫出時（J 之後）的時刻。`path` 只是 A 時的分類：例如 S＝0、L(A)＝0 記為 hold_limit，但依 due 規則在 A 就判定。

## 7. 骨骼 hitbox 的契約支持

pv7 只定欄位與語意；實作（Match 權威姿勢、射擊回溯、hitbox 內容）在 v8。

### 欄位（`hitbox-fields`）

| 欄位 | 號 | 型別 | v7.1 的值 |
|---|---|---|---|
| `ShotDecision.hit_part` | 11 | HitPart | 命中玩家時一律 BODY，其他情況 UNSPECIFIED |
| `CombatState.last_hit_part` | 14 | HitPart | 有受擊時一律 BODY，否則 UNSPECIFIED |
| `ShotRequest.view_tick_q16` | 9 | uint64 | Client 照實填 |
| `CombatRules.maximum_shot_damage` | 9 | uint32 | 25（＝`shot_damage`） |

HitPart（client 與 runtime 各一份，同號）：`HIT_PART_UNSPECIFIED` 0、`HIT_PART_BODY` 1、`HIT_PART_HEAD` 2。

### 語意

- `hit_part`：`HIT_PLAYER` ⇔ BODY 或 HEAD；其他情況一律 UNSPECIFIED；未知值＝這個結果批無效（既有路徑）。Gateway 與 Client 的驗證都接受 HEAD；v7.1 的 Match 不產生（產品測試鎖住）。v8 開始產生時，驗證不必改。
- `last_hit_part`：受擊紀錄的第 4 欄，和 `last_damage_tick`、`damage_count`、`last_attacker_id` 在同一處寫入，重生時一起歸零。protocol-v6 §2 的第 1、4 條擴成四欄：四欄同時為 0 或同時非 0；Dead 時非 0；非 0 時∈{BODY, HEAD}。只供呈現，不進 DigestFields v1。v8 依它選 Hit_Chest 或 Hit_Head（改寫 protocol-v6 §6）。
- `view_tick_q16`：射手看到的遠端時刻＝點擊之前最後一次呈現時，SnapshotTimeline 的**全域插值游標**（`cursor_`＋`baseTick_`），換成 Tick×65536 並 floor；不是逐目標的呈現時刻。沒有呈現任何遠端玩家時填 0。解碼層不驗證（以免改變凍結 `action_probe` 的 `conflicting_kind` 歸屬）；Match 只做紀錄性檢查（Shot：0，或 65536≤v≤observed_authority_tick×65536；Reload：0），不符就視為缺少並計入 `view_tick_invalid`，永遠不因此拒絕動作。v7.1 的 Match 記錄 view_lag＝J−view_tick/65536，作為 v8 回溯窗口的參考。
- `maximum_shot_damage`：單次裁決的傷害上限，包含 v8 的部位倍率；`shot_damage` 仍是基本（BODY）傷害。驗證：`shot_damage` ≤ `maximum_shot_damage` ≤ `maximum_hp`；`ShotDecision.damage` 的上限改成它。
- 實作（`hitbox-fields`）：`view_tick_q16` 的全域游標是 `SnapshotTimeline::GlobalCursor()`；Client 在每一幀抽樣遠端玩家後換算（`ViewTickQ16`），該幀確實 Presented 時記下，點擊時用最後一次呈現的值；Reload 一律 0。E、S 取自同一份 `ClientPresented` 的 observation（`ActionContext`）。紀錄性檢查與 view_lag 寫在影子紀錄（§6），不加在 Match 的 ingress 統計行。讀取端共用 `ValidDecisionHitPart`、`ValidShotDamageBounds`（`Combat.hpp`）；HitPart 的 wire 值與 Domain 由 IpcHost 的 `static_assert` 守住。

### 回溯的界限（取代 protocol-v4 §5「命中查詢不回退」與 §6「改回溯模型時須另改契約」）

- 目標姿勢的取用時刻 τ 由 Match 的政策決定，界限是 J−R ≤ τ ≤ J。R 是 Match 的產品常數，不上 wire；換算成時間不得超過 `CombatRules.maximum_reference_age_ms`。v7.1 的 R＝0。
- Match 可以把每個目標各自夾到該目標目前生命或 epoch 的起點（因為 `view_tick_q16` 是全域游標，目標若在它之後重生，射手畫面上的目標其實停在新區段的起點）。
- τ 時與 J 時目標的生命不同時，該命中不得傷害目標目前的生命。
- 射手側的時刻由 A1 的 S 提供，所以 v8 回溯需要的兩個時刻在 pv7 都有了。命中幾何來自 Match 擁有的內容，只讀權威狀態；Renderer 與呈現動畫不參與。
- v8 調整 R、hitbox 內容與倍率，屬於 Match 的政策與內容，不是 wire 變更。

## 8. 大小預算

Client 面向的 UDP 訊息（含 24 B 標頭）在契約最大值與型別最大值下都要 ≤1200 B，由 Go 與 C++ 的測試釘住：

| 訊息 | 預期最大值 |
|---|---|
| 4 人 Snapshot | 契約最大 1073 B／型別最大 1155 B（含 D-4 的兩個 hold 與 `last_hit_part`） |
| ActionBatch 8 筆 | ≤680 B |
| ActionResults 8 筆 | 約 603 B（依 D-6 再加 ≤13 B） |
| Welcome | 約 254 B |
| Hello | 約 125 B |

4 人 Snapshot 只剩約 45 B 的餘裕。GatewayDropReport 的最大組合要 ≤ IPC 的 MaxFrame。

## 9. 各批的範圍與證明

順序守住 D90（v7.1 D94、D95）：檢測（P1～P4）在前，Gateway 改動在後，A1 最後。

| 批次 | 加入或改變的 wire | 行為與證明 |
|---|---|---|
| `proto-rename-v7` | 只改名（版本仍 6） | 不變；digest 兩樹比較 |
| `version-errors` | 版本 7、409 body 的 `protocol_version`、版本不符的告知 envelope | 不變 |
| `heartbeat-sync` | ClockSample（runtime）、oneof 20、活性規則、診斷計時與槽 | 不變 |
| `time-echo` | `Hello.clock`、`Welcome.clock` | 不變 |
| `drop-report` | oneof 21、GatewayDropReport、DropCount、DropSample、DropReason（先含過渡原因，§4） | 不變 |
| `edge-results-resend` | ActionResults 的重送語意（欄位不變，最小 id 優先）；Gateway 這時仍有帳本，會冪等合併 | Client 收到的內容不變；新裁決最多晚 1 個間隔（未確認超過 8 筆時，新 id 要等循環槽位）（v7.1 D107）：IpcHost 測試，加上 Go 測試（Match 的副本在 Gateway 不改變任何東西：假時鐘下 Client 的資料包逐則相同；忠實的通道模型和「立刻送」比較 Client 第一次收到每個裁決的時刻） |
| `edge-input-passthrough` | PlayerInput 原樣轉送（§2a）；FIFO、順序、活性（D-10、D-11）；移除 D1、D2；刪除對應的過渡原因 | **行為改變**：事前宣告，L2 前後對照（命令證據、替代率、連線品質與逐出、Match 的副本計數；IpcHost 層新增 host mutex 的鎖等待直方圖，門檻事前宣告）。宣告中要列出的次要變化：參考年齡的樣本權重（pv6 合併時只抽一次樣，pv7 每個帶新命令的資料包各抽一次）、LateFirst 的參考年齡改用該資料包自己的 tick、copy 類計數大增 |
| `edge-results-passthrough` | 結果轉送，Gateway 的單槽持有與唯一寫出者（D-10）；刪除 session 帳本與 A3；**同一批讓 link 原樣轉送 ACK-only 批**（拿掉 link 對 ACK 的過濾，否則 Match 收不到補送退役的要求，最後一次退役遺失就不再送） | **行為改變**：事前宣告，L2 前後對照（裁決到 Client 的延遲，含 `edge-results-resend` 帶來的「新裁決最多晚 1 個間隔」（D107）；送出間隔 ≤30 Hz、射擊時序；中間 commit 也跑 loss 與 network 模式）。Go 測試：退役框架被丟後，Client 在 2 個間隔內收到退役；框架以 I±0.5 ms 到達時 0 丟棄；「每隔一則丟一則」時，未確認 16 或 32 筆都在 ≤2⌈n/8⌉＋2 則內送達（D112：Client 只能連續 ACK，有缺口時循環槽位會重送 Client 已有的 id；原寫 ≤2⌈n/8⌉） |
| `edge-actions-passthrough` | ActionBatch 原樣轉送；刪除 link 動作視窗、A5、30 Hz 持有與重送 | **行為改變**：事前宣告，L2 前後對照（射擊到達、權威 Cooldown＝0、Expired 率） |
| `edge-analyzers` | 無 | 改寫並重新凍結 ingress 的守恆式（見下面的鍵對應） |
| `edge-contract` | 無（約束條文與守衛，§2a） | 不變 |
| `cause-records` | dwell、Client 的 GYOP 序號（D96）、snapshot hold（D-4）、動作拒收回覆（D-6）、Leave 原因 | 不變 |
| `hitbox-fields` | HitPart、`hit_part`、`last_hit_part`、`view_tick_q16`、`maximum_shot_damage`；E、S 與影子紀錄（D-2） | 不變 |
| `pv7-checkpoint` | 無 | 先扣掉三個 edge 批事前宣告的差異帶，再和 v7 基準比較 |
| `a1-shot-step` | 只改判定語意與 Client 閘門 | **依事前宣告改變權威結果**（golden 重產、舊檔保留） |

- 權威 digest 不經過 Gateway 與 IpcHost（`AuthorityDigest.cpp:1-10`）。只改 PvpMatch、Host、Client 預測的批，以 `compare_authority_trees.py` 同機兩樹比較（scale 1、3 各 35／35 情境 0 差分）、golden 30／30 證明行為不變；只改 Gateway 或 IpcHost 的批，digest 不是有效證據，行為不變的批以 Go 測試與 IpcHostTests 證明，行為改變的批以事前宣告的 L2 前後對照證明（宣告前做 D45 的門檻來源檢查）。
- **ingress 鍵的對應**（從 `edge-input-passthrough` 起適用，`edge-analyzers` 重新凍結）：
  - 直通的轉送沿用 `fwd_main_input_packets`、`fwd_handed_main_commands`、`fwd_link_written_main_commands`，IG 與 I1 照常成立。
  - 被刪路徑的 v1 鍵照原順序輸出 0；`fwd_handed_actions_shots`（D59 的「不同的請求」）與 ACK 類鍵輸出 0，另加逐批的新鍵（行尾）。
  - 事前宣告的例外：I3（Match 的副本數：clean-60 每個跑次約 3.8k、quad 約 7.8k，對照的 Gateway 鍵是 0；原寫「上萬」，見 gateway-direction 的更正）、I0（只在有佇列溢出時）、`link_shots`。
  - `edge-analyzers` 重新凍結（v7.1，分析器 `pvp-v71-ingress-3`）：I0 的出口加上 FIFO 溢出的命令；新增 `removed_paths`（被刪路徑的鍵必須是 0）、`link_actions`（動作批的 link 守恆）、I1a（Gateway 寫出的動作批＝Match 收到的動作批，前提同 I1）；直通的 Gateway 上 I3 與 `link_shots` 不適用。上面三個例外因此不再需要。定義見 `tests/object_fps_pvp/fixtures/ingress_v1/README.zh-Hant.md`。
- 每批的測試清單見 [pv7-design](plans/v7.1/pv7-design.md)。

## 10. 不在 pv7 的項目（D-7）

下列項目刻意不上 wire。之後要做，就是需要使用者同意的 wire 變更：

1. slack 樣本的到達即發布（B7）與一份 snapshot 帶多個樣本（W11）：B7 會改 `movement_slack` 的語意。
2. 逐命令的完整丟棄身分（樣本只有 4 筆）。
3. 每個 Input／ActionBatch 帶 Client 送出時刻。
4. ShotDecision 帶判定路徑。
5. 武器、出生保護、回合、隊伍等 v8 欄位（v7.1 D87）。
6. 輸入防偽造（v7.1 D90）。
7. 房間列表的版本鍵、HTTP leave 帶 Client 端原因。
8. DropReason 的新值（Gateway 之後新增丟棄出口時）。

（定稿時決定納入 pv7、不在本清單：`PlayerInput`／`ActionBatch` 帶 Client 的 GYOP 序號，§5，D96。）

## 附錄：Architecture Delta

- **Gateway 的 Ownership 改變**（v7.1 D94）：可靠送達與去重從 Gateway 移到端點（Match 的 IpcHost 與 Host、Client）；Gateway 刪除 Match 決策狀態的副本（`lastResolved`、命令表、動作帳本、結果帳本、拒絕線、已寫出去重、截短、D1、D2）。約束寫進 network-architecture 的 `Design Invariants`，並由白名單測試守住（`edge-contract`）。
- **runtime Data Contract 的送達語意改變**（§2a）：PlayerInput、ActionBatch 原樣轉送且可能重複；ActionResults 由 Match 重送。
- 新增的產品內 Data Contract：IPC oneof 20～21、ActionResults 的拒收欄位（D-6）、PlayerInput／ActionBatch 的 dwell 與 Client 的 GYOP 序號、Hello／Welcome 的 clock、ShotRequest／ShotDecision／CombatState／CombatRules 的新欄位、HTTP 409 body 的鍵。
- Build Graph：proto、Go 套件、CMake target 改名（`proto-rename-v7`，單獨一個 commit）。
- Gateway：Server 擁有的診斷 goroutine、link 的診斷槽；runtime 活性只看 pv6 種類；link 改成依到達順序的有界 FIFO。
- IpcHost：新增結果重送（循環游標、退役補送）、診斷槽與對時估計器；`MatchRuntimeHost` 的介面不接收診斷值。
- `a1-shot-step`：FireGate 退役，射擊判定語意改變。
- 沒有新的 Top-level 目錄、子系統或跨 owner 依賴；Engine 與 `services/gyo_gateway` 不改（GYOP 標頭配置與種類範圍不變）。
