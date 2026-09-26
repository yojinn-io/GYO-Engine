# Object_FPS_PVP 聯網架構與操作

本文件描述 Object_FPS_PVP 的 LAN 聯網與本機移動預測。Linux／Windows Client 經 LAN
加入同一場 Match，Client 即時預測自己的移動，C++ Match 統一裁定移動與地圖碰撞。
這是 GYO 固定 Tick 與外部驅動邊界的實際案例，不是通用 Multiplayer Protocol。

## 1. 責任與依賴

```text
Client（SDL 輸入 / Lobby / GYO Render）
    │ HTTP JSON 控制 / UDP Client Protocol v3
    ▼
Object_FPS_PVP Go 組合層
    ├─ Room、容量、加入資格、Session → PlayerId
    ├─ ObjectFPS Adapter：schema / version / 欄位轉換
    └─ 使用 services/gyo_gateway 的 HTTP / Session / framing
    │ Runtime Protocol v3 / loopback TCP
    ▼
C++ IPC Host：讀寫、framing、Protobuf 轉換
    │ 有界控制佇列 / per-player command window / owning Snapshot
    ▼
MatchRuntimeHost：程序生命週期、clock、Wait、停止與 reset
    ├─ GYO FixedTickRuntime.Advance(elapsed)
    └─ PvpMatch.Tick(TickContext)
           ├─ 純 Movement / Collision 規則
           └─ GYO Collision 幾何查詢
```

Engine 決定時間如何切成 Tick；Host 決定程序如何存活；Match 決定世界如何變化；
IPC 只交換資料。Go 不計算移動結果、碰撞、合法出生或其他遊戲規則。

`services/gyo_gateway` 的公共 Go module 不定義 Room／Match，也不 import PvP。
Room 是產品語意，路由與容量留在 `apps/object_fps_pvp/gateway`。產品 module 以
`replace gyo.local/gateway => ../../services/gyo_gateway` 使用公共基礎設施，沒有
列舉產品的根目錄 `go.work`。CPP Engine 不依賴 Gateway 或任何具體產品。

`match_domain` 不連 `PlayerController`、Client `Player`、`PlayerSettings`、SDL、UI
或 Renderer。`ComputePlanarInput`／`ComputePlanarDisplacement` 與 numeric
`MoveCharacterBody`／`CanPlaceCharacterBody` 承擔純規則。地圖到 AABB 的轉換獨立，
不把滑鼠 delta、Camera 或 Campaign 狀態帶入權威世界。

## 2. Tick 與 handoff 契約

### 固定步進

`Engine::Runtime::TickSettings` 有 `tickRate` 與 `maximumCatchUpSteps`；PvP 使用
60 Hz、最多五步追趕。`TickContext` 有 `tickId` 與固定 `deltaSeconds`。
`FixedTickRuntime::Advance(elapsed, callback)` 維持 accumulator，回傳
`FixedTickAdvance{steps, droppedSeconds, secondsUntilNextTick}`。

Tick ID 從 1 開始，只計算真正執行的步進。超額整步積欠被捨棄，保留小於一步的
餘量。Engine 核心沒有 `Run()`、clock、sleep 或 thread。
`MatchRuntimeHost::Run()` 使用 `steady_clock` 讀 elapsed，呼叫 `Advance`，再等待
下一步或停止／重置。離線測試直接呼叫同一 Host 的 `Advance`。

| 名稱 | 意義 | 不可作為 |
|---|---|---|
| Local sampling Tick | Client 60 Hz 採樣步數 | Authority 的執行時刻或延遲 |
| Authority Tick | C++ 已完成的世界模擬步數 | Client 時鐘的直接對照 |
| Command sequence | 每位玩家獨立、從 1 起的固定一步命令 | Authority Tick |
| lastResolvedCommand | 權威位置已完成的最後命令，含缺包替代步驟 | 最近收到封包的序號 |
| UDP sequence | 同一傳輸 session 封包新舊 | Game Tick |

兩端的 Tick 起點、追趕與停止歷史各自獨立，不視為同步時鐘。原有 object_fps／
object_fps_v2 的 variable-frame gameplay 不受影響；本次只改 PvP。

### Input：不可修改的單步命令窗口

Client 60 Hz 產生命令，每個序號固定代表 1/60 秒，欄位只有移動軸與絕對 yaw／pitch。
滑鼠 delta 每個 presentation frame 只消費一次，不在 catch-up 重複消費。
Network worker 以獨立 60 Hz deadline 重送完整未確認窗口，首次窗口立即送出；
新 epoch 的 ACK=0 階段，首次窗口等第一個合法固定步形成後才發布，包含原來的
兩個中立命令及已產生的當前命令，避免僅有 neutral 的窗口提前耗掉 headroom。
這項 bootstrap 補充已由使用者批准；沒有額外產生步數或增加中立命令。
每批最多 12 個，含 24-byte header 的完整 UDP 不超過 1,200 bytes。延遲 deadline
不補送過期批次，worker 不產生命令，主執行緒正常 30 FPS 每幀可產生兩步。
Gateway／Host 合併不可變命令，未來命令數量及距離游標都不超過 32。
命令識別為 `(playerId, movementEpoch, sequence)`：epoch 只由 Match 提升，從 1
開始；每個 epoch 的 sequence 從 1 開始，兩者不回繞。重複未完成命令為無操作；
相同識別但內容衝突則原子拒絕整批；已完成／舊 epoch 不重執行，未來 epoch 拒絕。

Match 等到當前 epoch 的序號 1 後，每 Tick 恰好完成下一序號。缺命令時沿用最近
**實際執行的真實命令**最多 15 Tick，之後中立移動。Actual／Held／Neutral 都推進
`lastResolvedCommand`，重送不刷新期限。控制、命令交接、模擬依序進行。

每步後記錄游標後連續待執行命令數，最近 30 Tick 總和達 105、且重設冷卻至少
60 Tick，於下一個 Tick 邊界提升 epoch。該邊界不移動，保留身分、位置、視角與
世界 Tick，清除命令／替代輸入／統計，游標歸零等新序號 1。後續每份 Snapshot
均攜帶新 epoch，首份遺失仍可恢復。各層失效舊窗口，Host 交接再次驗證。
另針對已證實的停頓後永久晚到：最近 30 個 Running Tick 的連續待執行數總和為 0、
同窗曾使用 Held／Neutral、且整個未來命令佇列為空時，視為提前量耗盡；同樣經
60 Tick 冷卻、於下一邊界重設 epoch。全為 Actual 時 queue 為 0 不觸發此條件，
Awaiting seq1 期間不重複重設。這項補充已由使用者批准，理由及反例見 v3 dev_log。
ACK 遺失、窗口滿或 Server 超前本身仍不直接觸發重設。

### 本機預測、校正與鏡頭

產品內 `StepMovement` 讓 Client／Match 共用方向正規化、速度、視角限制及靜態牆壁碰撞。
`LocalPlayerPrediction` 負責本機 60 Hz 模擬及重播，遠端玩家使用一 Tick（16.67 ms）延遲插值。
Application 在實際呼叫 Advance 前取樣 steady_clock 間隔，避免幀內停頓被下一個
FrameContext delta 重複計入；domain 本身仍不讀時鐘。
Snapshot 到達時還原自己的權威狀態，移除已完成命令並依序重播剩餘命令；鏡頭使用
相鄰預測狀態的逐幀插值，不直接讀權威快照位置。小校正的顯示偏移在 100 ms 內消除，
誤差達 1 世界單位則直接重新定位。完整 ACK 也保留相鄰命令的插值配對，
校正只比較同一預測終點的真正差異，不能將正常插值相位當成位置誤差。
呈現插值及偏移也受角色牆壁碰撞限制。

首次啟動、ACK 超過本機 tip、或較新 epoch 時，清除失效歷史與累積時間，建立
**兩個中立命令**提前量。固定 lead 會形成穩態序號差，是約 33.3 ms 的持續延遲成本，
並非只影響啟動。ACK 小於 tip 時還原並重播；**ACK 等於 tip 是正常確認**，保留
計時餘數，不補中立命令。新播種的首次 Advance 最多计入一步；pending 為空且
frame gap 超過 50 ms 時亦最多計入一步，保留既有餘數，避免重播權威已涵蓋的停頓。
新epoch先保留seed窗口，首個合法固定步形成後才一起發布。若尚未發布前的累積
時間會在一次Advance產生三個以上操作命令，同樣限制本次計入一步並記錄省略時間，
避免首批載入停頓把lead永久墊高；正常30FPS首批兩步仍保留。
窗口達 12 時凍結位置預測，持續視角與重送。失焦產生中立新命令；既有命令不可改。
加入、離開、斷線及身分變更清除預測、歷史與校正。

worker 保存最多 64 份實際接收時間戳快照，`Drain()` 原子取得狀態、連線 generation
及接收歷史。本機只校正最新狀態；遠端 `SnapshotTimeline` 使用相對 Authority Tick、
固定 60 Hz 斜率、最近 64 樣本的 `min(receiveTime − q/60)` 作時間原點。
顯示游標落後一 Tick、單調且限制於歷史；位置線性插值、yaw 最短角度插值，缺未來
資料保持最新姿態、不外推。玩家 epoch 切換只分割其呈現區段，不跨世代插值。
主執行緒停頓／歷史溢位不重設原點；連續三個樣本偏移超過原點 100 ms 才重新定位。
這是到達時間線估計，不是跨主機時鐘同步。

`PvpApplication::LocalMovement()` 提供唯讀 predicted／render position、correction offset、
權威 Tick、確認／最新命令、窗口長度和 frozen 狀態，供產品 GUI probe 記錄；
`RemoteMovement()` 回報選取的遠端姿態與時間線。`PresentedMovement()` 僅在
`PresentStatus::Presented` 後回報該幀真正提交的本機／遠端姿態、frame ID及時間；
Skipped 不產生樣本。原始缺少未來 Snapshot 與移動中的 hold 分別觀測，靜止角色
不增加動作停頓次數；只有同一玩家／epoch的相鄰成功呈現姿態確實重複，才累加
hold。連續選用新的最新快照而位置前進時不算hold。正式遊戲沒有測試輸入或時間控制接口。
產品日誌記錄首次世界繪製、超過 250 ms 的事件／更新／繪製、滑鼠釋放與網路斷線原因，
不記錄 Session token。最小化時略過繪製的 frame 會短暫等待，避免忙迴圈。

### Snapshot cadence

每個完成的 Authority Tick 產生完整 Snapshot，正常名義 60 Hz。同一次 catch-up
只保留最後 owning candidate，不把較晚位置標成較早 Tick。Host／IPC／Gateway
對尚未開始寫出的完整 Snapshot 採 latest-wins；已部分寫出的 TCP frame 必須完成
或由連線失敗處理，不可替換剩餘內容。Gateway亦在每位peer實際送出前重新選取
最新候選，前一位peer的阻塞不能固定下一位未送出的舊datagram。已進入OS／網路
的資料不能撤回。
傳輸不阻塞模擬，待送狀態有界。Gateway 依新 publication 轉送，沒有額外發送 Tick。
Client 丟棄舊 Authority Tick；背壓／catch-up／掉包會降低實際接收頻率。
Welcome 不附送世界，Client 等待第一份有效 Snapshot。

跨執行緒只交接 owning 值；不把 `IRuntimePort` borrowed view 傳給 I/O。
socket 讀寫與序列化不在世界 Tick 內執行。

## 3. 兩份獨立 Protocol

來源為產品的 `protocol/client_v3.proto` 與 `protocol/runtime_v3.proto`，彼此不 import。
目前兩者版本都為 3，Client／Gateway／Match 必須一起升級，明確拒絕 v1／v2。Adapter 明確映射兩份生成型別，
Match 核心只接收普通 C++ domain 值。

Client Protocol 包含 Hello、Welcome、PlayerInput、WorldSnapshot、Error。
Hello 提交 session token；Welcome 回覆 PlayerId、MatchId、tick/snapshot rate 與
arena identity。PlayerInput 包含 movement_epoch 及 commands，每個命令只有 sequence、移動軸、yaw／pitch，
沒有 Client 指定時間、位置、傷害或擊殺結果。Snapshot 包含 Authority Tick、完整玩家集合
與每位玩家的 last_resolved_command、movement_epoch、contiguous_pending_commands。

RuntimeEnvelope 包含獨立 `protocol_version` 和 oneof：Ready、PlayerJoin、
PlayerLeave、PlayerInput、JoinResult、RuntimeError、WorldSnapshot。
Ready 宣告 arena identity、60 Hz、snapshot interval 1 與容量 2。
Runtime PlayerInput 的 PlayerId 來自已驗證的 Session 映射，不信任 Client 任選 ID。

Adapter 驗證 Protobuf、版本、finite 數值、移動軸範圍與線上欄位範圍；Match 自行
裁定出生位置、速度、合法視角與碰撞。Arena identity 不符時 Client 拒絕加入。

### UDP v3

最大 datagram 為 1,200 bytes，包括以下 24-byte big-endian header：

| Offset | Bytes | 欄位 |
|---|---:|---|
| 0 | 4 | ASCII `GYOP` |
| 4 | 2 | Client protocol version，現為 3 |
| 6 | 2 | message type：Hello 1、Welcome 2、Input 3、Snapshot 4、Error 5 |
| 8 | 8 | Session ID |
| 16 | 4 | UDP sequence |
| 20 | 2 | Protobuf payload length |
| 22 | 2 | Channel，v3 只允許 0：unreliable sequenced |

Header 後面是對應 message 的 Protobuf payload。公共 framing 驗證長度與 Channel，
不解釋產品 message type；PvP 邊界驗證版本與類型。本輪只有 Channel 0；尚無
多 channel、ACK、ack_bits 或 Reliable Ordered。UDP sequence 用半範圍比較處理 32-bit wrap。
Hello 可重送，Welcome 可重發；完整 Snapshot 修復漏掉的加入／離開資訊。

### IPC v3

Match 僅監聽 loopback TCP，預設 `127.0.0.1:27016`。每個 Protobuf
RuntimeEnvelope 前有四個 bytes 的 big-endian 長度；payload 必須為 1–65,536
bytes。讀取處理半包／黏包；截斷、錯版本、無效 frame 或控制交接超載屬於
連線失敗，不能靜默遺失 Join／Leave 後繼續假裝同步。

## 4. Room、Session 與失敗

此 MVP 最多一個固定 Room 1／Match 1。建立前 `GET /rooms` 回傳空陣列；
Runtime Ready 後 `POST /rooms` 建立房間，重試回傳同一個房間。
不做程序配置或動態建立其他 Match。

| HTTP | 行為 |
|---|---|
| `GET /rooms` | 回傳 `rooms` 陣列，含 id、match_id、players、capacity、status、arena identity |
| `POST /rooms` | 建立／回傳固定房間，Runtime 不可用時 503 |
| `POST /rooms/1/join` | 以 `request_id`、`protocol_version` 保留名額 |
| `POST /rooms/1/leave` | 以 `session_id`、`session_token` 驗證離開 |

Join request 範例：

```json
{"request_id":"a-client-generated-unique-id","protocol_version":3}
```

成功回覆包含 `match_id`、`player_id`、`session_id`、`session_token`、`udp_ip`、
`udp_port`、`protocol_version`、`arena_id`、`arena_version`。同一有效 request_id
重試回傳同一 reservation。保留與 joining 名額也計入最多兩人限制。

`player_id` 是這次加入的角色實例身分，不是固定席位或帳號。離開後重入會取得新 ID
並重新出生，舊 ID 不重用；沒有角色保存／續玩。HUD 只顯示權威快照的在線人數，
診斷 ID 留在日誌。Lobby 的 `players` 是占用名額（含握手中），以 slots occupied 標示。

Esc 立即清除本機世界；Client worker 必須確認 HTTP Leave 成功且 `left:true`，
再更新房間列表，才能完成返回 Lobby。失敗時停止舊 UDP 與保活、保留待清理憑證，
下一次操作先重試冪等 Leave，不得直接建立第二個角色。Lobby 約每秒更新房間列表；
非 Lobby 或仍有待清理憑證時不做自動查詢，過期操作的結果不能覆蓋新狀態。
所有人離開後房間仍保留為零占用名額，未實作空房逾時關閉。

HTTP 成功只代表取得加入憑證。UDP Hello 驗證 token 並綁定 endpoint 後，Adapter
才提交 PlayerJoin；Runtime 接受後 Gateway 才回 Welcome。重複握手不重複出生。
Session 憑證隨機產生，UDP endpoint 不可用舊 session 靜默切換。

Reservation／有效 Session 的期限是 Gateway 單調 wall clock 五秒；等待 Runtime
JoinResult 的上限為三秒。Network worker 每秒用既有 Hello／Welcome 保活，即使畫面停止產生 Input 仍可存活；
60 Hz Input 也可保持連線存活。Hello 不延長遊戲移動期限。這些連線期限與
十五個缺包 Authority Tick 的替代輸入期限 是不同契約。

IPC 失聯使 Room unavailable 並撤銷 Session，Host 清場；Client 回 Lobby。
Gateway 可繼續提供 HTTP 狀態，但本輪不透明重接或恢復世界。復原流程是重啟
必要服務後重新加入。MVP 沒有帳號、TLS、可靠事件、射擊、傷害、跳躍、玩家移動互阻、
高延遲 Internet 最佳化或持久化。

## 5. 手動建置與啟動

本次 v3 在 Linux／GCC 14 完成建置與驗收；Windows／MinGW 的歷史結果不代表
本次 v3 已重新完成跨平台驗收。Linux 從 repository root 執行：

```bash
cmake --preset test
cmake --build --preset test --parallel 6
cmake --build build/target/_build/test --target gyo_object_fps_pvp-gateway
```

以下保留 PowerShell 操作方式，亦從 repository root 執行；C++ 使用 MSVC x64 Developer
Shell 或 MinGW-w64、CMake／Ninja 與專案原有 shader toolchain。普通 C++ build
不要求 Go。CMake 會在首次 configure 選定 compiler；更換 compiler 時使用不同
build directory，不混用兩種工具鏈的 cache／產物。

```powershell
cmake --preset dev -B build/target/_build/pvp -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build build/target/_build/pvp
cmake --build build/target/_build/pvp --target gyo_object_fps_pvp-gateway
```

過往基線的 MinGW GCC 15.2 `dev` 建置曾通過。部分 MinGW-w64 headers 缺少
`GetAddrInfoExCancel`；httplib wrapper 會檢查 API 是否可編譯／連結，缺少時
使用上游同步 `getaddrinfo` fallback。此時 hostname 的 DNS 解析本身不受 HTTP
連線 timeout 約束；LAN 範例使用 literal IPv4，不需 DNS 查詢。靜態 SDL_ttf
在 wrapper 明確連結所選 SDL3，讓 MinGW 取得正確 library 順序。

Gateway convenience target 需要 Go 1.23 或更新版。也可獨立建置：

```powershell
New-Item -ItemType Directory -Force build/target/_services/object_fps_pvp/bin
Push-Location apps/object_fps_pvp
$env:GOWORK = 'off'
go build -o ../../build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway.exe ./gateway/cmd
Pop-Location
```

Native Client／Match 隨產品 role 組裝至 `build/target/object_fps_pvp/bin`；Gateway
獨立放在 `_services/object_fps_pvp/bin`，不混入 native archive。服務分別手動啟動，
不使用程序池或 supervisor。主機具備相應 C++ runtime 時，Match 可只攜帶有效
Arena JSON 與其執行檔運行，不需要 SDL 或 asset catalog；
GUI Client 需要部署的 fonts、UI 與 shader bundle。

第一個終端啟動 Match，明確指定 Arena 可避免工作目錄混淆：

```powershell
& ./build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match.exe `
  --arena ./build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json `
  --listen 127.0.0.1:27016
```

第二個終端啟動 Gateway。雙機 LAN 必須將下例 IP 換成服務主機的 LAN IP：

```powershell
& ./build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway.exe `
  --http 0.0.0.0:8080 --udp 0.0.0.0:27015 `
  --runtime 127.0.0.1:27016 --advertise-ip 192.168.1.20
```

Client 在各機器啟動 `gyo_object_fps_pvp.exe --gateway 192.168.1.20:8080`。
Lobby 也可點擊地址欄編輯 `host:port`；Enter 完成，Ctrl+A 清空，Ctrl+V 貼上。
第一個 Client 選 Create + Join；另一個選 Refresh、Join Room。WASD 移動，Mouse
轉視角，ESC 離開。進場先保留自由游標，點擊遊戲內容區或按 Tab 才鎖定；Tab 可釋放游標，
方便拖曳標題列。視窗移動／失焦會釋放滑鼠並以中立輸入停止後續移動，重新聚焦不會
自動鎖回游標，也不暫停另一人的世界。

只做同機驗證時可使用 `--advertise-ip 127.0.0.1` 與 `--gateway 127.0.0.1:8080`。
LAN 主機需允許該應用的 TCP 8080 與 UDP 27015；27016 保持 loopback。
本輪 C++ Client 的 UDP 端點只解析 IPv4；`--advertise-ip` 必須給可達的 IPv4。
Gateway 的底層 API 可接受 IPv6，不代表這個產品已完成 IPv6 互通。
啟動順序錯誤或 Runtime 未 Ready 時 Gateway 啟動會失敗，不自動生成 Match。

### Protobuf 生成

C++ compiler 與 runtime 都固定 Protobuf **v36.2**；C++ bindings 由 CMake 生成到
build tree。Go plugin/runtime 固定 **v1.36.11**，生成的 `.pb.go` 由產品持有。
Asio 固定 **1.38.2**，cpp-httplib 固定 **0.56.0**；這些 wrapper 由所選產品引入。

修改 `.proto` 後，用 CMake 建好的 v36.2 `protoc.exe` 執行：

```powershell
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11
protoc --version  # 必須為 libprotoc 36.2；PATH 指向本次 CMake 建置的 protoc
protoc --proto_path=apps/object_fps_pvp/protocol `
  --go_out=apps/object_fps_pvp --go_opt=module=gyo.local/object_fps_pvp `
  apps/object_fps_pvp/protocol/client_v3.proto apps/object_fps_pvp/protocol/runtime_v3.proto
git diff -- apps/object_fps_pvp/protocol
```

`protoc-gen-go` 所在的 Go bin 目錄也須在 PATH。未變更 schema 時重生成應無差異。
生成／測試期間使用固定版本，不把未知全域 protoc 產物當作相容保證。

## 6. 驗證與證據邊界

Go 測試分別在公共 module 與產品 module 執行 `go test ./...`。C++ testing
configuration 執行 engine Tick、Match、handoff 與 cadence 測試；完整網路驗收
使用實際 Gateway／Match，不能把 codec 單測當成端到端結果。

```powershell
cmake --preset test -B build/target/_build/pvp-test -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build build/target/_build/pvp-test --target gyo_object_fps_pvp_tests `
  gyo_object_fps_pvp_network_probe gyo_object_fps_pvp_gui_probe
ctest --test-dir build/target/_build/pvp-test -R object_fps_pvp --output-on-failure
```

`build/acceptance/object_fps_pvp/run_network.py` 以 `--match`、`--gateway`、`--probe`、
`--arena`、`--output` 接收實際檔案路徑，選取 localhost ports，啟動服務與 network
probe 並保存結果。缺少 executable 或 Arena 會直接失敗。GUI probe target 為
`gyo_object_fps_pvp_gui_probe`；probe 位於 build tree 的 acceptance 目錄，不部署
到產品 bin 或 release archive。

Runner 另接受 `--gui-probe <executable>` 與 `--arena-root <deployed assets>`，可把
兩個 GUI 程序納入同一次驗收；無論成功或失敗都清理本次啟動的服務與 probe。
這個選項需要可用 GPU／視窗環境，不是 Headless network test 的替代品。

`--network-impairments` 額外執行三組實際 socket 驗收：RTT 0／20／40 ms，單向排程
抖動最多 10 ms、5% 隨機丟包，以及每位玩家的首批與停止附近兩個連續輸入批次遺失。
產品專屬 HTTP 代理只重寫 join 回覆的 UDP 目的端點，UDP 代理逐 byte 轉送原始資料；
正式 Client／Gateway／Match 沒有測試開關。每組輸出 `network-rtt-*.log/json`，
記錄丟包、排程／觀測延遲、封包大小與覆蓋條件。OS 排程可能增加實際延遲，
因此原始觀測值也保留，不將配置值當成真實網路量測。

GUI acceptance executable 支援下列參數，create／join 必須是兩個獨立程序；SDL
event queue 屬於 process，不在同程序內輪流 pump 兩個 app：

```text
--arena-root <部署後的 assets/object_fps_pvp>
--gateway 127.0.0.1:8080 --role create|join --duration 5 --output <capture-directory>
[--gpu-driver auto|d3d12|vulkan] [--move]
```

Probe 經 SDL 鍵盤操作實際 Lobby 的 Create／Refresh／Join，等待兩位真實網路
玩家後保存 before/world BMP 與 report，最後驗證 ESC 回 Lobby；`--move` 經原有
SDL adapter／60 Hz 命令採樣路徑送 W 輸入，檢查權威位置變化。另輸出每幀
`*-movement.csv` 的預測、顯示與校正資料，斷言沒有新 Snapshot 時鏡頭仍移動、
穩定畫面幀中至少 80% 持續位移，以及 ESC 清空預測。`*-presentation.csv` 紀錄兩個
程序的主機 monotonic 時間與本機／遠端實際顯示位置，產生 `presentation-latency.json`
以相同位移門檻比較兩個視窗，要求 localhost 中位延遲不超過 150 ms。Renderer capture
是 UI overlay 前的實際場景，不是 Lobby 畫面驗證。Lobby JSON 另經 UI schema
驗證。測試程式位於產品外的
`build/acceptance/object_fps_pvp`，沒有向產品注入測試控制或假 Snapshot。

初始 v1 切片曾驗證實際 Go Gateway／C++ Match／兩個 Client 探針，41 份共同 Authority
Snapshot 狀態一致；雙 GUI 程序渲染互見，SDL W 輸入產生 1.10 單位權威位移。
也已驗證隔離目錄的 Headless 啟動、非法 IPC 長度、Gateway 更換後清場、Match
失聯時兩位 Client 回 Lobby。這些證據使用 localhost，不等於實體雙機 LAN 驗收。
Engine-only、v2 與產品移除／複製等項目的個別結果、命令與限制記錄在
[本次開發日誌](../dev_logs/2026_09_24_object_fps_pvp.md)。


### v2 Architecture Delta

本機 WASD 鏡頭原先等待 20 Hz Snapshot，造成可見階梯移動；只增加快照插值會增加
操作延遲，不能達成即時本機反應。因此 Client 新增自身移動預測責任，產品輸入契約
由持續狀態變成可確認的固定步命令。變更限於 object_fps_pvp 的 domain、Client、
Runtime、Gateway adapter、兩份 schema 與 owner 專屬驗收。共同移動步驟和預測器
編入既有產品 domain library，沒有新增公共 target、Subsystem 或 Top-level Directory。
Engine 與 services/gyo_gateway 不加入遊戲邏輯或反向依賴，Product Ownership 與
Arena JSON Data Contract 維持不變。v2 測試證據見
[本機預測驗收日誌](../dev_logs/2026_09_25_pvp_prediction.zh-Hant.md)。


### v3 命令時序與呈現驗收

`--movement-trace path` 啟用有界唯讀事件輸出，預設關閉。熱路徑只寫入記憶體佇列，
獨立診斷執行緒輸出 JSONL；溢位、缺結尾或不合法資料使驗收失敗。時間戳不在線上
協定內，不影響移動。事件涵蓋產生、成功送出、Host 接受、Actual／Held／Neutral
執行、epoch 重設、快照接收及成功 Presented。`PresentedMovement()` 只在
`PresentStatus::Presented` 後產生一份觀測，Skipped 不冒充呈現。

產品專屬 `run_timing.py` 支援雙 GUI 三輪 120 秒／200 個预先安排事件，以及
`timing_probe` 的 60／144 Hz 真實網路／預測 30 分鐘長測。命令延遲以相同識別
配對；呈現以位移交越配對。未配對／不明事件以無限延遲計入 nearest-rank 分位數，
不排除慢幀，所有呈現間隔均保留。GUI 計時期間不 readback 或存圖；GPU 正確性另測。
僅比较同機單調時鐘，不稱為 input-to-photon。固定門檻、執行命令、實測結果與限制見
[命令時序 v3 開發日誌](../dev_logs/2026_09_25_pvp_command_timing_v3.zh-Hant.md)。
