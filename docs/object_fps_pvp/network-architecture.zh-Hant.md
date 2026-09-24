# Object_FPS_PVP 聯網架構與操作

本文件描述 Object_FPS_PVP 的第一個聯網切片。目標是兩個 Windows Client 經 LAN
加入同一場 Match，由 C++ 世界統一運算移動與地圖碰撞，並看見彼此。
這是 GYO 固定 Tick 與外部驅動邊界的實際案例，不是通用 Multiplayer Protocol。

## 1. 責任與依賴

```text
Client（SDL 輸入 / Lobby / GYO Render）
    │ HTTP JSON 控制 / UDP Client Protocol v1
    ▼
Object_FPS_PVP Go 組合層
    ├─ Room、容量、加入資格、Session → PlayerId
    ├─ ObjectFPS Adapter：schema / version / 欄位轉換
    └─ 使用 services/gyo_gateway 的 HTTP / Session / framing
    │ Runtime Protocol v1 / loopback TCP
    ▼
C++ IPC Host：讀寫、framing、Protobuf 轉換
    │ 有界控制佇列 / per-player latest-value / owning Snapshot
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
| Client Tick | 本地 60 Hz 輸入採樣序號 | Authority 的執行時刻或延遲 |
| Authority Tick | C++ 已完成的模擬步數 | Client 時鐘的直接對照 |
| Input sequence | 同一玩家輸入的新舊順序 | 模擬次數 |
| UDP sequence | 同一傳輸 session 封包新舊 | Game Tick |

兩種 Tick 的起點、追趕與停止歷史各自獨立。即使名義頻率相同，也禁止直接相減
或視為同步時鐘。本輪沒有 clock synchronization、回溯模擬或 lag compensation。
v1／v2 仍使用原有 variable-frame gameplay；本輪只遷移 PvP。

### Input：每位玩家一個最新值

Client 每兩個本地 Tick 形成一次輸入，名義 30 Hz；一次 frame 的 catch-up 形成
多份待送值時只保留最後一份。滑鼠 delta 每個 presentation frame 只消費一次，
累加成絕對 yaw／pitch，不在每次 catch-up 重播。

Go runtime link 與 C++ Host 都以 PlayerId 為鍵維持 latest-value；Gateway 的玩家
session 與 C++ Host 保存已接受的 sequence 高水位。Client 的待送槽消費後也保留
高水位。只允許新 sequence 覆蓋，沒有普通 Input FIFO；相同數值配新
sequence 是有效 heartbeat，舊包／重複包不刷新輸入 freshness。

Join／Leave 走獨立、有界、有序控制佇列。每 Tick 先處理控制，再消費有效玩家的
最新輸入，再模擬。Join 建立中立輸入，重複 Join 不重置位置；Leave 清除玩家與
held/latest 輸入。未知玩家的 Input 不會生成玩家。同一服務生命週期內 PlayerId
不重用，重新加入使用新 ID。

一份輸入最多作用十五個 Authority simulation ticks，沒有新輸入便歸零移動。
這是模擬 Tick 期限，不是 Client Tick 或 wall-clock 的 250 ms 保證。封包數量
不會增加世界更新次數。未來 fire／jump 等一次性動作必須另訂契約，不能直接
塞進可覆蓋的持續狀態。

### Snapshot cadence

只在完成 Authority Tick 3、6、9……後擷取完整世界，正常情況名義 20 Hz。
同一次 `Advance` 跨越多個取樣點，只保留最後候選，再供 I/O 取走；候選包含
取樣當時的 owning state，不能把較晚位置標上較早 Tick。

Host／IPC 與 Gateway 的待送 Snapshot 都允許合併最新值。Gateway 只在收到新
publication 後轉送，沒有自己的 20 Hz 發送計時器。Client 丟棄舊 Authority Tick。
背壓、catch-up 與丟包可能降低收到的頻率，不保證每秒二十份。
Welcome 不附送世界快照，Client 等待第一份週期 Snapshot 後才顯示世界。

跨執行緒只交接 owning 值；不把 `IRuntimePort` borrowed view 傳給 I/O。
socket 讀寫與序列化不在世界 Tick 內執行。

## 3. 兩份獨立 Protocol

來源為產品的 `protocol/client_v1.proto` 與 `protocol/runtime_v1.proto`，彼此不 import。
目前兩者版本都為 1，但不要求未來同步升級。Adapter 明確映射兩份生成型別，
Match 核心只接收普通 C++ domain 值。

Client Protocol 包含 Hello、Welcome、PlayerInput、WorldSnapshot、Error。
Hello 提交 session token；Welcome 回覆 PlayerId、MatchId、tick/snapshot rate 與
arena identity。PlayerInput 只有 sequence、client tick、移動軸、yaw／pitch，
沒有位置、傷害或擊殺結果。Snapshot 包含 Authority Tick 及完整玩家集合。

RuntimeEnvelope 包含獨立 `protocol_version` 和 oneof：Ready、PlayerJoin、
PlayerLeave、PlayerInput、JoinResult、RuntimeError、WorldSnapshot。
Ready 宣告 arena identity、60 Hz、snapshot interval 3 與容量 2。
Runtime PlayerInput 的 PlayerId 來自已驗證的 Session 映射，不信任 Client 任選 ID。

Adapter 驗證 Protobuf、版本、finite 數值、移動軸範圍與線上欄位範圍；Match 自行
裁定出生位置、速度、合法視角與碰撞。Arena identity 不符時 Client 拒絕加入。

### UDP v1

最大 datagram 為 1,200 bytes，包括以下 24-byte big-endian header：

| Offset | Bytes | 欄位 |
|---|---:|---|
| 0 | 4 | ASCII `GYOP` |
| 4 | 2 | Client protocol version，現為 1 |
| 6 | 2 | message type：Hello 1、Welcome 2、Input 3、Snapshot 4、Error 5 |
| 8 | 8 | Session ID |
| 16 | 4 | UDP sequence |
| 20 | 2 | Protobuf payload length |
| 22 | 2 | Channel，v1 只允許 0：unreliable sequenced |

Header 後面是對應 message 的 Protobuf payload。公共 framing 驗證長度與 Channel，
不解釋產品 message type；PvP 邊界驗證版本與類型。本輪只有 Channel 0；尚無
多 channel、ACK、ack_bits 或 Reliable Ordered。UDP sequence 用半範圍比較處理 32-bit wrap。
Hello 可重送，Welcome 可重發；完整 Snapshot 修復漏掉的加入／離開資訊。

### IPC v1

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
{"request_id":"a-client-generated-unique-id","protocol_version":1}
```

成功回覆包含 `match_id`、`player_id`、`session_id`、`session_token`、`udp_ip`、
`udp_port`、`protocol_version`、`arena_id`、`arena_version`。同一有效 request_id
重試回傳同一 reservation。保留與 joining 名額也計入最多兩人限制。

HTTP 成功只代表取得加入憑證。UDP Hello 驗證 token 並綁定 endpoint 後，Adapter
才提交 PlayerJoin；Runtime 接受後 Gateway 才回 Welcome。重複握手不重複出生。
Session 憑證隨機產生，UDP endpoint 不可用舊 session 靜默切換。

Reservation／有效 Session 的期限是 Gateway 單調 wall clock 五秒；等待 Runtime
JoinResult 的上限為三秒。正常 30 Hz Input 同時保持連線存活。這些連線期限與
十五個 Authority Tick 的 gameplay input freshness 是不同契約。

IPC 失聯使 Room unavailable 並撤銷 Session，Host 清場；Client 回 Lobby。
Gateway 可繼續提供 HTTP 狀態，但本輪不透明重接或恢復世界。復原流程是重啟
必要服務後重新加入。MVP 沒有帳號、TLS、可靠事件、Prediction、Reconciliation、
射擊、傷害、跳躍、玩家移動互阻或持久化。

## 5. 手動建置與啟動

以下 PowerShell 命令從 repository root 執行；C++ 使用 MSVC x64 Developer
Shell 或 MinGW-w64、CMake／Ninja 與專案原有 shader toolchain。普通 C++ build
不要求 Go。CMake 會在首次 configure 選定 compiler；更換 compiler 時使用不同
build directory，不混用兩種工具鏈的 cache／產物。

```powershell
cmake --preset dev -B build/target/_build/pvp -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build build/target/_build/pvp
cmake --build build/target/_build/pvp --target gyo_object_fps_pvp-gateway
```

MinGW GCC 15.2 的 `dev` 建置亦已通過。部分 MinGW-w64 headers 缺少
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
轉視角，ESC 離開；失焦送中立輸入，不暫停另一人的世界。

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
  apps/object_fps_pvp/protocol/client_v1.proto apps/object_fps_pvp/protocol/runtime_v1.proto
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

GUI acceptance executable 支援下列參數，create／join 必須是兩個獨立程序；SDL
event queue 屬於 process，不在同程序內輪流 pump 兩個 app：

```text
--arena-root <部署後的 assets/object_fps_pvp>
--gateway 127.0.0.1:8080 --role create|join --duration 5 --output <capture-directory>
[--gpu-driver auto|d3d12|vulkan] [--move]
```

Probe 經 SDL 鍵盤操作實際 Lobby 的 Create／Refresh／Join，等待兩位真實網路
玩家後保存 before/world BMP 與 report，最後驗證 ESC 回 Lobby；`--move` 經原有
SDL adapter／Client Tick 採樣路徑送 W 輸入，檢查權威位置變化。Renderer capture
是 UI overlay 前的實際場景，不是 Lobby 畫面驗證。Lobby JSON 另經 UI schema
驗證。測試程式位於產品外的
`build/acceptance/object_fps_pvp`，沒有向產品注入測試控制或假 Snapshot。

本機已驗證實際 Go Gateway／C++ Match／兩個 Client 探針，41 份共同 Authority
Snapshot 狀態一致；雙 GUI 程序渲染互見，SDL W 輸入產生 1.10 單位權威位移。
也已驗證隔離目錄的 Headless 啟動、非法 IPC 長度、Gateway 更換後清場、Match
失聯時兩位 Client 回 Lobby。這些證據使用 localhost，不等於實體雙機 LAN 驗收。
Engine-only、v2 與產品移除／複製等項目的個別結果、命令與限制記錄在
[本次開發日誌](../dev_logs/2026_09_24_object_fps_pvp.md)。
