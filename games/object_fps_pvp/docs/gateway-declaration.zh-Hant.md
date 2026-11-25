# Object_FPS_PVP Gateway 移植的事前宣告（`v8-product-pvp-gateway`）

日期：2026-11-25。GYO2 主會話開的任務卡（PvP Gateway 移植）。作者 Opus 5.5／high；每段的設計審查也是 Opus／high（Fable 暫停，作者與審查者同模型，報告寫明）。
基準 `gyo2` `eed6e2ed`。這份宣告屬於產品；刪掉 Object_FPS_PVP 時跟著刪（AGENTS §1-8）。新 AGENTS 不等核准，寫完就動手；要推翻使用者決定或動 AGENTS §1 時停下問主會話。

## 0. 前提

| # | 前提 | 誰定的 | 掛在上面的決定 |
|---|---|---|---|
| G1 | Gateway 是 Match 以外所有事情的入口：大廳房間列表、建房與加入、準備與鎖房、配對與分配 Match、存檔讀寫、身分與連線憑證；有狀態的放在它後面的服務（微服務） | 使用者 2026-11-25（`v8-arch-gyo2-game-flow-and-gateway`） | D1、D2 |
| G2 | Gateway 原意：外部可接多種 Client、內部一套抽象協議、Gateway 本身無狀態，內部協議保留身分與冪等 | 使用者 2026-10-15 | D2、D3、D4 |
| G3 | Client 直接連 Match（行程內 Link、帳本在 Match 的 Peer）；Gateway 只交連線目標與 token，token 由 Match 的 `NetProductOptions::admission` 驗 | 網路宣告 Q2（使用者 2026-11-22 核准） | D5 |
| G4 | World＝場景；房間 World 與遊戲 World 分開；多人的遊戲 World 在「開始遊戲」時才帶 `NetConnectTarget{server, userId, token}`，單人不帶 | 使用者 2026-11-25（`v8-arch-gyo2-net-connect-target-user`） | D9 |
| G5 | Gateway 算在 PvP 移植裡；合作／生存的規則與結算、商人、任務是之後的新功能 | 使用者 2026-11-25（`v8-arch-gyo2-gateway-in-pvp-port`） | 第 1 節的「不做」 |
| G6 | 權威、回滾不參考 master 的算法、常數、數據；master 綁舊事故的診斷、ingress 統計、drop report、送件預算不搬 | 使用者 2026-11-20；盤點第 2.4 節 | D2 |
| G7 | engine/core/os 只有觀測到壓力（有 Feature 要）才長 | 使用者 D78（`os.md` Constraints 9） | D8 |
| G8 | Gateway、Match、Player 是產品的詞；Engine 名義登記的 gateway 已刪，Go module 的機制留著 | 詞彙表 :173；R0 帶過清單 :274 | D1 |

**master 的 Gateway 搬什麼**：master 的 Gateway 是 Client 與 Match 之間的 UDP 中繼（`apps/object_fps_pvp/gateway/server.go`：session、rate limit、rebinding、轉送 action 與 snapshot、時鐘同步），外加單一房間的 HTTP（`server.go:168-171` `/rooms`、`/rooms/1/join`、`/rooms/1/leave`）。G3 之後中繼那一半沒有位置（盤點 2.4 已判不搬）；搬的是 HTTP 房間那一半的**語意**（列表、建立、加入帶 `request_id` 的冪等、離開要憑證、版本不合時說出自己的版本 `server.go:282-291`、body 上限 4096 與拒絕未知欄位 `server.go:229-240`）。session token 不照搬：master 是隨機字串存在 Gateway、常數時間比較（`session/session.go:62-64`），要查表；這裡改成自含的 HMAC（D5），Gateway 不存狀態。PV19 的 rate limit 與 rebinding 是 UDP 中繼的，不搬；Match 的 UDP 由網路 Module 負責。PV14（內容比對）：放進 token 的欄位之後可加（見 D5），本卡不做。

## 1. 分段（每段一個 draft PR，後一個疊在前一個上）

```text
v8-product-pvp-gateway
├─ 第 1 段 services   high｜Go：gateway、identity、rooms、matches、saves；token 簽發；建房→加入→準備→開始→拿到連線目標
├─ 第 2 段 admission  high｜Match：驗 token（HMAC）、照 matches 給的參數起來、人走光就結束；matches 真的起 Match 行程
├─ 第 3 段 http       high｜Engine：core/os 的 TCP（StreamSocket）＋ library-only Module `http`（非阻塞的 HTTP/1.1 client）
├─ 第 4 段 saves      high｜Match 進場讀檔、離場（撤離）寫檔，經 saves 服務
└─ 第 5 段 client     high｜PvP 的大廳換成走 Gateway：房間列表、建房、房間（準備、開始）→ 帶 NetConnectTarget 的遊戲 World（等 #332）
```

| 段 | 範圍 | 不做 | 測試 |
|---|---|---|---|
| 1 services | `games/object_fps_pvp/services/`（Go module，`gyo_game_add_go_service`）：五個服務的套件、一個執行檔帶子命令（`gateway`、`identity`、`rooms`、`matches`、`saves`、`all`）；外部 HTTP 協議 v1（D3）、內部協議（D4）、token（D5）；matches 的 provider 介面與「本機行程」實作（第 2 段才真的起 Match） | Client、Match 的 C++；快速配對 | `go test`：各服務單元；`all` 在行程內起五個服務（各自 listener），兩個使用者經 Gateway 建房→加入→準備→開始→各拿到自己的連線目標；token 驗得過、換 user／換 Match／過期都驗不過；冪等鍵重送回同一結果 |
| 2 admission | Match：`--match-id`、`--capacity`、`--admission-key-stdin`（金鑰是標準輸入的第一行 hex）；admission 驗 token（D5）並拒絕同一 userId 第二條連線；`--exit-when-empty`；matches 的本機 provider 起 Match、看行程結束、回收 port | 存檔 | C++：token 驗證的外部測試（Go 產生的向量檔，兩邊對同一份）、admission 與座位的單元；network 標籤：本機 provider 起真的 Match（金鑰經 stdin）、它在 port 上 listen；壞的金鑰拒絕啟動。〔動工後改〕Client 帶 token 連上真的 Match 的測試移到第 5 段（Client 的連線參數等 #332 改成 `NetConnectTarget`，現在寫會在合併後重寫） |
| 3 http | `engine/core/os` 加 `StreamSocket`（非阻塞 IPv4 TCP client：Open/Connect、State、Send、Receive、Close；錯誤是中性碼）；新 library-only Module `http`：純的 HTTP/1.1 請求組裝與回應解析（Content-Length、chunked），加上用 `StreamSocket` 的 `HttpRequest`（呼叫者每幀 `Poll`） | DNS（只收 IPv4 字面值，和現在的大廳一樣）、TLS、keep-alive、HTTP/2 | os：本機 loopback 的連線、送收、對方關閉、連不上（network 標籤）；http：解析的單元（分段到達、chunked、錯的格式）＋對本機 Go 測試伺服器的請求（network） |
| 4 saves | Match：玩家連上後向 saves 讀檔（非阻塞），離場時寫回（冪等鍵＝matchId＋userId＋離場序號）；存檔格式 v1（D7），`save_version` 的 `Check` | 合作／生存的結算規則 | Match＋saves 的 network 測試：連上→讀到→離場→寫回→再連讀到新值 |
| 5 client | 大廳 World（房間列表、建房：模式、地圖）→ 房間 World（成員、準備、開始）→ 遊戲 World 帶 `NetConnectTarget`；回到大廳；用 U（`ui`、`world_ui`、`ui_render`，ui 頁第 9 節）；裝置 id 存在使用者資料夾 | 商人、任務、合作／生存 | client：大廳與房間的流程經本行程的 Go stack（network）；`--connect` 照舊；最後在這台 Mac 兩個 Client 經 Gateway 對戰、撤離後存檔寫回再讀到（實機試玩交使用者） |

先後：1 → 2 → 3 → 4 → 5。第 5 段要等 #332 合併；碰到 PvP 卡正在改的 Client 檔案時 SendMessage 協調。第 3 段改 Engine，單獨 commit，PR 本文寫明。

PR 標題 `type(scope): 說明（v8-product-pvp-gateway）`；draft、base 前一段的分支（第 1 段 base `gyo2`）；每段手動 dispatch L1。

## 2. 決定

### D1 放哪裡 → 產品的 Go module `games/object_fps_pvp/services/`

- 前提：作者能處理（G8：Gateway 是產品的詞，Engine 名義的 gateway 已刪；G1 的服務都是這個產品現在要的）。
- master 的共通層 `services/gyo_gateway`（UDP framing、peer session、HTTP 預設值）在 G3 之後只剩 HTTP 預設值有用，不另立共通層。第二個產品要用時再抽，抽的對象以那時兩個產品的重疊為準。
- 第三方 Go 套件：不用（標準庫的 `net/http`、`crypto/hmac`、`crypto/sha256`、`encoding/json`）。master 用 protobuf 是為了 IPC 與 Client 協議，這裡都不需要。

### D2 服務怎麼切

| 服務 | 狀態 | 做什麼 | 誰呼叫它 |
|---|---|---|---|
| gateway | **無** | 外部協議的 adapter（現在只有 HTTP/JSON v1）：驗 session token（自含、HMAC，不查表）、把身分與冪等鍵放進內部請求、轉給後面的服務、把回應原樣轉回 | Client |
| identity（憑證） | 裝置 id → userId 的表（檔案） | 裝置登入：給 userId 與 session token | gateway |
| rooms（房間） | 記憶體裡的房間（短命，重啟就沒了） | 列表、建房（模式、地圖、容量）、加入、離開、準備、開始（鎖房）；開始時向 matches 要 Match；保存每位成員的連線目標，成員來問時給自己那一份 | gateway |
| matches（配對與分配） | 執行中的 Match 與它的位址、金鑰、玩家 | 分配：用 provider 起（或取）一個 Match，給每位玩家簽 admission token；Match 還在時給玩家重簽（token 短命，D5）；Match 結束就忘掉 | rooms |
| saves（存檔） | 每位使用者一份存檔（檔案）與版本號 | 讀、寫（樂觀鎖＋冪等鍵）；內容是產品的格式，服務不解讀 | Match（讀寫）、gateway（使用者讀自己的） |

- 「配對」本卡只做房間制的分配；自動配對（快速加入）之後放在 matches 或 rooms 前面，協議不用改。
- 本機部署：一個執行檔 `gyo_object_fps_pvp_services`，子命令起一個服務；`all` 在同一個行程裡起全部，但每個服務各有自己的 loopback listener、彼此照樣走 HTTP，所以邊界和分開部署時一樣（測試與本機試玩用）。
- 商人、任務、合作／生存的結算：之後是 rooms 後面或旁邊的新服務（或 Match 的規則），經 gateway 對外；存檔格式加欄位（D7 的版本號）。本卡不留空殼。

### D3 Client ↔ Gateway 的外部協議（HTTP/1.1＋JSON，`/v1`）

| 方法與路徑 | 內容 | 回應 |
|---|---|---|
| `POST /v1/session` | `{"deviceId"}` | `{"userId","sessionToken","expiresAt"}` |
| `GET /v1/rooms` | — | `{"rooms":[{"id","mode","map","players","capacity","state"}]}`（只列 `open`） |
| `POST /v1/rooms` | `{"mode","map"}` | 房間（建的人自動加入、是房主） |
| `GET /v1/rooms/{id}` | — | 房間：成員與準備、狀態；`started` 後多一個 `connect`：**呼叫者自己的** `{"server","userId","token"}`（token 是 admission token 的 hex） |
| `POST /v1/rooms/{id}/join`、`/ready`（`{"ready"}`）、`/start` | — | 房間 |
| `POST /v1/rooms/{id}/leave` | — | `{"left":true}` |
| `GET /v1/saves/me` | — | 自己的存檔（唯讀） |

- 身分：`Authorization: Bearer <sessionToken>`（`/v1/session` 以外都要）。改變狀態的請求帶 `Idempotency-Key`（Client 產生）；同一使用者同一個鍵重送，回第一次的結果（master `request_id` 的推廣，`server.go:330-332`）。
- 錯誤：`{"error":"<code>"}` 加 HTTP 狀態；碼是說「發生了什麼」（`room_full`、`room_started`、`not_owner`、`not_all_ready`、`invalid_session`…），怎麼處理由 Client 決定（AGENTS §1-9）。
- 未知的路徑回 404 `{"error":"unknown_route"}`；別的版本的路徑回 404 `{"error":"unsupported_version","versions":[1]}`（Client 看得到伺服器說哪些版本，master 的語意）。〔審查 S5〕
- started 房間的 Match 結束後再問房間：410 `match_ended`（房間刪掉，之後是 404 `room_not_found`）。
- 版本：路徑的第一段（`/v1`）。〔動工後改〕原本照 master 在 body 帶 `protocol_version`、不合回 409（`server.go:282-291`）；改成路徑版本（Nakama `/v2`、UGS `/v1` 的作法），別的版本的 Client 打到不存在的路徑（404 `unsupported_version`），舊版本的路徑可以並存。
- 請求 body 上限 4096、拒絕未知欄位（`server.go:229-240`）。
- 房間的變化用輪詢 `GET /v1/rooms/{id}`（Unity Lobby 也是輪詢；推送之後可加另一個外部 adapter）。
- 開始後房間無法進入：`started` 的房間 `join` 回 `room_started`，也不在列表裡。

### D4 內部協議（服務之間，HTTP/1.1＋JSON，`/internal/v1`）

- 每個請求帶 `X-GYO-User`（gateway 驗過的 userId；`0`＝系統，服務或 Match 發起的）與 `Idempotency-Key`（gateway 原樣轉 Client 的；服務自己發起時自己產生）。登入前的請求（`/v1/session`）不帶 `X-GYO-User`。服務只信任這兩個標頭，不再看 session token；Client 自己送的 `X-GYO-User` 不會被轉送。〔審查 S2〕
- 冪等：rooms、saves、matches 以（使用者, 路由, 鍵）存第一次**成功（2xx）**的結果，重送回同一結果，進行中的同鍵請求等第一個做完；被拒絕的（4xx、5xx）不存，同一把鍵可以再試（master 也只記成功）。〔審查 S7〕
- 發內部請求只有一個出口（`internal/api` 的 `Send`），gateway 的轉送與服務之間的呼叫都用它。〔審查 S6〕
- rooms 向 matches 要 Match：進 `starting` 時產生一把鍵，沒有回應（傳輸錯誤）時用同一把鍵最多送三次，所以一次開始最多起一個 Match。〔審查 S1〕
- 內部埠只綁 loopback（或部署時的私網）；內部協議不做認證——外部只經 gateway 進來。
- 新的外部 adapter（例如 WebSocket）只換 gateway 的前面，內部協議不變。

### D5 token 的形式與 Match 端的驗證

兩種 token，都是「自含、HMAC-SHA256 簽名、不查表就能驗」：

- **session token**（Client ↔ gateway）：identity 簽、gateway 驗，共用 identity 金鑰；內容 userId、到期時間。
- **admission token**（Client → Match，裝在 `NetConnectTarget::token`，網路的 `ConnectRequest::token` 原樣帶到 Match）：matches 簽，金鑰是**每個 Match 各一把**（matches 起 Match 時隨機產生，寫進 Match 行程的標準輸入；不放命令列與環境，別的使用者讀不到）。〔動工後改〕原本寫環境變數：C++ 讀環境要 `getenv`，MSVC 會警告（C4996），改成 stdin。二進位、固定長度 57 bytes：

```text
version u8 (=1) | matchId u64 | userId u64 | expiresAt u64 (Unix 秒) | mac [32] = HMAC-SHA256(key, 前 25 bytes)
（整數皆 big-endian）
```

- Match 的 admission：長度與版本對、MAC 對（常數時間比較）、`userId == request.userId`、`matchId` 是自己的、沒過期（Match 的 wall clock；admission 不是決策資料，D-9 不受影響）、這個 userId 沒有連線中，最後是現有的座位檢查（`SeatAdmission`）。拒絕的 detail 說是哪一條。
- 沒設金鑰的 Match（`--connect` 直連、現有測試）照舊只看座位；直連是開發用的入口，不加守衛（AGENTS §3）。
- 對照 netcode.io（lightyear `crates/connection/netcode/src/token.rs:135-142`）：connect token 由後端用和伺服器共用的私鑰加密，含 client id、伺服器位址、到期、user data；伺服器不問後端就能驗。GYO 取同一個思想（後端簽、Match 自己驗、到期）。不加密：netcode 加密是為了帶封包加密的金鑰，GYO 的網路沒有加密（網路 Q7），token 內容不是秘密，簽名就夠。重放：同一個 token 被別人搶先用，Match 只收同一 userId 一條連線；在沒有封包加密的線上這擋不完，登記為限制（第 4 節 L1）。
- 之後要加的（PV14 內容比對、合作／生存的隊伍）一律升 version；Match 對未知 version 拒絕。〔審查 S3：原本留 `reserved u32`，不升 version 就填進去的話，舊 Match 驗得過 MAC 卻不看新欄位，等於默默放寬，所以拿掉〕
- 壽命：token 短命（預設 2 分鐘）。started 房間的成員每次問房間，rooms 就向 matches 要一張新的（matches 保存執行中 Match 的金鑰與玩家），所以比賽多長都能重連（D-8：重連是新連線）；Match 結束後 matches 回 `match_ended`，rooms 回 Client 410 並刪掉房間。netcode 也是短 token 加後端重發。〔審查 S4〕
- 到期由 matches 與 Match 各自的 wall clock 判斷；跨機時兩邊的時鐘差會吃掉一部分壽命（NTP 同步的機器差在秒以下，2 分鐘夠用）。〔審查 N12〕

### D6 Gateway 怎麼找到並分配 Match

- matches 有 provider 介面：`Start(spec{matchId, key, capacity, mode, map}) → Running{address, ended, stop}`（Match 結束時 `ended` 關閉；`stop` 提早結束）。〔審查 S8：原本寫 `Allocate`／`Release`，實作的形狀比較好，改宣告〕本卡的實作是**本機行程**：從設定的 port 範圍挑空的，起 `gyo_object_fps_pvp-match --port P --match-id M --capacity N --arena A --admission-key-stdin --exit-when-empty`（金鑰寫進 stdin），看行程結束就回收。之後換成機群（Agones、PlayFab MPS 那類）只換 provider。
- 地址：provider 回 `advertise IP:port`（本機預設 127.0.0.1；跨機時設定 advertise）。
- Match 起來前就交出連線目標：Client 的網路握手會重送，Match 晚一點 listen 不影響（網路的握手逾時內）。第 2 段量本機起 Match 的時間，若接近握手逾時再改成等 Match 回報。

### D7 存檔放哪裡、誰寫

- 存在 saves 服務的資料夾 `<data>/saves/<userId>.json`（`--data` 指定；本機 stack 的腳本放在建置輸出下，不放 home）；檔內 `{"version": <樂觀鎖的號碼>, "body": <產品的 JSON>}`，寫入先寫暫存檔再改名。
- **只有 Match 寫**（權威在 Match）；Client 經 gateway 只能讀自己的。
- PvP 的存檔格式 v1（產品的）：`{"saveVersion":1,"matches":n,"kills":k,"deaths":d}`，用 `save_version` 的 `Policy{1,1}` 檢查。撤離＝玩家離開這場 Match（現在 PvP 沒有撤離點；生存模式「撤離立刻結算」的最小版本）；合作「活著的人全部撤離才結算」是之後的規則，寫入點同一個。

### D8 C++ 端怎麼講 HTTP → core/os 的 `StreamSocket`＋Engine 的 `http` Module

- 前提：只有 Engine 做得到（OS 的 socket 只能在 core/os，G7、`os.md` DI 1）；HTTP/1.1 的組裝與解析是作者能做的，但每個要接後端的遊戲都會要，屬於機制。
- 壓力：本卡的 Feature（Client 要講 Gateway、Match 要講 saves）。和 D78 一致：有 Feature 要才長。
- 形狀照 core/os 現有的 `DatagramSocket`（非阻塞、中性錯誤碼、不判斷哪些可重試）；`http` 是 library-only Module（像 `save_version`），請求物件放在 Resource，System 每幀 `Poll`。
- 別家：Godot 的 `HTTPClient` 是輪詢式的非阻塞 client（`~/Code/GitHome/godot/core/io/http_client.h:37`、`:192` `poll()`），Unity 的 `UnityWebRequest`、Unreal 的 `FHttpModule` 也是 Engine 內建；Bevy 核心沒有 HTTP（生態裡的 `bevy_http_client` 包 ehttp）。偏離 Bevy 的理由：GYO 的 OS socket 只能在 core/os，產品自己包第三方 HTTP 庫要開 Plugin，比一個小 Module 的膠水多。
- 不用 cpp-httplib＋asio（master 的作法）：要下載第三方、阻塞式、要包成 Plugin。

### D9 Client 的流程（第 5 段）

```text
多人 → 大廳 World（房間列表；建房：模式、地圖）→ 房間 World（成員、準備；房主按開始）
     → rooms 回 started＋自己的 connect → 遊戲 World（prepare 放 NetConnectTarget{server,userId,token}）
     → Match 結束或離開 → 回大廳
```

- 大廳與房間是沒有 net 的 Product（現在的大廳就是）；遊戲 World 只在拿到 connect 時才帶 `NetConnectTarget`（G4）。HTTP 請求放在大廳／房間 World 的 Resource，System 每幀 `Poll`。
- 裝置 id：使用者資料夾裡的一個檔（`--profile NAME` 換一份，讓同一台 Mac 開兩個 Client 是兩個使用者）。

## 3. 別家怎麼分，GYO 為什麼這樣分

| | 身分 | 房間／大廳 | 分配伺服器 | 進伺服器的憑證 | 存檔 |
|---|---|---|---|---|---|
| Nakama | 裝置／帳號登入給 session token（JWT HS256） | 內建的 party、matchmaker | 權威 match 跑在 Nakama 行程裡（Lua／Go／TS） | 同一個 session | storage engine（物件、版本號樂觀鎖） |
| Unity Gaming Services | Authentication（匿名登入） | Lobby 服務（輪詢、房主、自訂資料） | Matchmaker → Multiplay（或 Relay）分配 | allocation／join code | Cloud Save |
| Epic Online Services | Connect（裝置 id 登入） | Lobbies | 自己的伺服器註冊到 Sessions | 遊戲自己的 | Player Data Storage |
| PlayFab | 自訂 id 登入 | Lobby、Matchmaking | Multiplayer Servers 的 RequestMultiplayerServer 回 IP:port | 遊戲自己的 | Player Data／Entity Files |
| netcode.io（lightyear） | — | — | — | 後端簽的 connect token，伺服器不問後端就驗 | — |
| **GYO（本卡）** | identity：裝置 id → userId、session token（HMAC） | rooms：輪詢 | matches：provider（本機行程） | admission token（每個 Match 一把金鑰，HMAC） | saves：版本號樂觀鎖，只有 Match 寫 |

（Nakama、UGS、EOS、PlayFab 本機沒有原始碼，依公開文件，未對照源碼；netcode 對照了本機 lightyear。）

- 共通的分法：身分、大廳、分配、存檔是分開的責任；遊戲伺服器只做遊戲，進場憑證由後端簽、伺服器自己驗。GYO 照這個分法，對應使用者的微服務（G1）。
- 和 Nakama 不同：Nakama 把權威 match 放在後端行程裡；GYO 的 Match 是獨立的 C++ 行程（網路 Q2、權威在 Match），後端只交連線目標，所以分配要經 provider 起行程（像 UGS／PlayFab）。
- 存檔只讓 Match 寫：Nakama 預設 Client 可寫自己的物件（權限可關）；PvP 的結算在權威端，Client 寫會被竄改，所以只開讀。

## 4. 限制（本卡結束時仍在）

- L1 token 重放：沒有封包加密，同一網路上看得到 token 的人可以搶先用（Match 只收同一 userId 一條連線，搶先的人會占走）。解除：網路加密（Q7）。
- L2 rooms 的狀態在記憶體：rooms 重啟，房間全沒；已開始的 Match 不受影響（Client 已拿到連線目標）。
- L3 只收 IPv4 字面值（沒有 DNS），沿用現在的大廳。
- L4 內部協議沒有認證，只靠綁 loopback／私網。
- L5 同一 userId 只收一條連線：Client 在握手中途關掉又馬上重連時，Match 上舊的那條要等逾時（現在 10 秒）才移除，這段時間新的會被拒絕（「the user is already in the match」）。PvP 卡 2026-11-25 指出的既有行為（握手中途沒有 Disconnect 可送）。
- L6 rooms 的地圖：`--content` 給了就只收 content 的 `maps/arenas.json` 列的 arena id；沒給時收任何 `[a-z0-9_]{1,64}`，選到 Match 沒有的地圖時 Match 啟動失敗、Client 連不上（`all` 的本機腳本會給 `--content`）。
- L7 裝置 id 就是憑證：知道別人的裝置 id 就能登入成那個人。Client 要用高熵亂數產生並存好（第 5 段）；不加守衛（AGENTS §3）。〔審查 N6〕
- L8 gateway 與 identity 共用 session 金鑰，所以 gateway 也簽得出 session。要分開時換成非對稱簽章（例如 ed25519：identity 拿私鑰、gateway 只拿公鑰）。〔審查 N7〕
- L9 本機 provider 不先檢查 port 有沒有被別的程式占用：占用時 Match 啟動失敗、行程結束、port 回收，這一場的 Client 連不上。〔審查 N8〕
- L10 分開部署時每個子命令的 `--listen` 預設是隨機 port、`rooms --matches` 沒給也能啟動，部署腳本要自己給齊；服務的組裝在 `stack` 與 `main.go` 各一份。〔審查 N5〕

## 5. 測試與突變

- Go：`go test ./...`（CTest 的 `object_fps_pvp.go.vet`、`.go.test`，Linux 另有 `.go.race`；CI 只在 Linux 跑 Go）。network 類（真的起 Match）用 CTest 的 `network` 標籤，只在本機。
- C++：照各段表格。本機 `ctest -L "cpu|gpu"`，網路 `-L network`。
- 突變：登記在 `games/object_fps_pvp/docs/mutations.json`（id `pvp-gw-`）；Go 的用執行器的 `command`（`go -C games/object_fps_pvp/services test …`）。每段只跑相關的：token 的簽與驗（每個欄位、MAC、到期）、Match admission 的每一條拒絕、rooms 的狀態轉移（開始後不能加入、沒全準備不能開始、非房主不能開始）、冪等重送、saves 的樂觀鎖。誤用（例如不經 gateway 直接打內部埠）不做突變（AGENTS §3）。

## 6. 設計審查

每段照批次表〈每批的設計審查〉，Opus／high 子 Agent（作者同模型，報告寫明）。
