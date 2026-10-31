# Object_FPS_PVP 聯網架構與操作

本文件描述 Object_FPS_PVP 的 LAN 聯網與本機移動預測。macOS／Linux／Windows Client 經 LAN
加入同一場 Match，Client 即時預測自己的移動，C++ Match 統一裁定移動與地圖碰撞。
這是 GYO 固定 Tick 與外部驅動邊界的實際案例，不是通用 Multiplayer Protocol。

2026-10-21：目前 wire 為 pv7（[v7.1](plans/v7.1/README.md) 的 `pv7-merge`，PR #78 合併起），規則見
[protocol-v7](protocol-v7.zh-Hant.md)；沒有在其中改寫的條款沿用 [pv6 契約](protocol-v6.zh-Hant.md) 與
[v5 契約](protocol-v5.zh-Hant.md)。日誌的格式見 [日誌 Data Contract](log-contract.zh-Hant.md)。程序與執行緒結構以
[v7.1 穩定基線](plans/v7.1/STABLE_BASELINE.md)（2026-10-21 升格；v7 的結構加上 pv7 變更：Gateway 回到無狀態邊緣、
丟棄回報、對時與回聲）為準，第 1 節依此繪製。各版變更見文末
「v6、v7、v7.1 的聯網架構變更」。第 0 節與第 6 節的 v3 量測／長測、第 8 節成本回顧、第 9 節 v4 記錄及
v5 一節保留為歷史證據，不視為 v6／v7／v7.1 的驗收。Go Gateway 的建置平台宣告目前只有
linux-x64／windows-x64；v7 驗收在 macOS 同機三角色執行，範圍見 v7 穩定基線。
（歷史：v7 停在 pv6，pv7 移到 v7.1，v7 D73。）

## 0. 本次修復總結：做了什麼、證明了什麼

本節於 2026-09-26 整理。本文件保留可持續維護的設計與結論；逐次實驗、失敗跑次、
重現命令與版本指紋保留在 [v3 dev_log](../dev_logs/2026_09_25_pvp_command_timing_v3.zh-Hant.md)。
下列正式成果僅引用最終 `pvp-v3-release-*`，不把中間候選的較好數字當成交付結果。

### 工作範圍與根因

| 問題 | 已確認的原因／風險 | 完成的工作 |
|---|---|---|
| 自己移動時鏡頭跳動 | 原本本機位置等待 20 Hz 權威快照 | v2 起加入產品內共用移動、本機預測、ACK 重播與逐幀顯示插值 |
| 另一個視窗看玩家明顯落後 | 三步 lead 形成持續序號差；傳送、快照、遠端插值另有等待 | v3 固定兩步 lead，worker／Snapshot 60 Hz，遠端時間線落後一 Tick |
| ACK 後累積或憑空增加延遲 | 完整 ACK 被當成重新播種；相鄰顯示端點被合併，正常插值差又被當成校正 | 分開 `ACK < / == / > tip`，保留計時餘數及相鄰端點，按同一預測終點比較誤差 |
| 畫面停頓後多生或持續晚到的命令 | 舊 frame delta 重複計入權威已涵蓋的停頓；Running 游標可永久領先恢復後命令 | Advance 呼叫點計時、啟動／長幀保護、Match 積欠與提前量耗盡的 epoch 復原 |
| 偶發丟失原始操作但角色仍會走 | Held 可掩蓋命令晚到；完整 ACK 後舊 worker deadline 擋住新窗口 | 記錄 Actual／Held／Neutral；窗口清空後下個可用窗口立即送出 |
| 啟動時提前量不穩定 | neutral-only 首包太早啟動 Match；載入積累也可能讓首批多生三步以上 | 第一個合法操作步形成後才發布完整窗口；保留正常 30 FPS 的兩步產生 |
| 通訊恢復後仍播放舊狀態 | FIFO 快照及已取出的雙 peer 批次可保留舊資料 | 尚未寫出的完整快照 latest-wins、逐 peer 重新選取、64 份接收歷史選目前區間 |
| Esc 重入的編號／人數混淆 | PlayerId 是一次加入的身分；Leave 未確認與 Lobby 占位資訊容易被混用 | 確認 Leave 後完成返回、失敗清理屏障、Lobby 更新、HUD 在線人數與診斷 ID 分離 |

另完成 Client／Runtime Protobuf、C++ 型別、Go bindings、HTTP join、Ready／Welcome
和轉接器的 v3 同步升級；Client、Gateway、Match 必須整套更新。診斷、故障注入探針、
證據分析器與 owner 移除驗證也屬於交付內容。焦點處理、滑鼠釋放與獨立保活改善了
拖窗／停頓的產品行為，但**拖曳標題列造成整個 OS 無回應的根因仍未證實**。

### 最終結果與適用範圍

| 指標 | v2 基線 | 最終 v3 |
|---|---:|---:|
| 命令產生 → 首次成功送出 P95 | 18.65 ms，舊觀測限制見 dev_log | 15.79–16.12 ms |
| 命令產生 → Actual P50／P95 | 55.94／63.44 ms | 34.72–36.77／42.91–46.18 ms |
| 跨視窗位移交越 P50／P95 | 108.24／115.49 ms | 45.02–48.13／45.75–48.75 ms |
| 乾淨 GUI 原始命令實際執行率／事件配對率 | 100%／200:200 | 三輪均 100%／200:200 |

v3 每輪 120 秒、200 個預先安排事件，三輪獨立通過；實際平均 59.74–59.75 FPS，
保留所有慢幀，沒有 epoch 重設或診斷遺失。此處範圍是三輪各自的分位數，不是
合併樣本後的分位數。這是同機成功提交畫面的位移交越，**不是 input-to-photon**，
也不等同於「命令產生 → 執行」；兩種量測有不同起點，不應相減拼出網路耗時。

- CTest 31／31、PvP 67 cases／1,389,737 assertions、Go race、shader、GPU 通過。
- 固定 RTT 0／20／40 ms 與停頓九組通過；解除停頓後最慢 550.12 ms 開始恢復，
  最慢 800.12 ms 完成連續 250 ms 穩定觀察，符合 1.5 秒契約。
- Host／Gateway／downstream 六組阻塞通過；最慢 70.02 ms 開始恢復、336.69 ms
  完成穩定觀察。丟包、抖動、重送、重入及斷線另經完整網路測試。
- 60／144 Hz **headless 網路／預測**各實時 30 分鐘：431,999 個非 seed 命令全為
  Actual，零重設、診斷遺失、掉時或窗口凍結；30 Tick 積欠總和最高 60，門檻仍為 105。
- GUI 的 30／144 FPS 另測，144 模式觀察端實際約 142.79 FPS。長測不冒充 GUI 長測。

原始摘要位於 `build/target/_build/test/logs/pvp-v3-release-gui-latency/summary.json`
及 `pvp-v3-release-soak-summary.json`；這些是本機建置證據，Git 內的永久說明以本文件
及 dev_log 為準。尚未完成實體雙機 LAN、最新 v3 的 Windows 回歸或跨主機時鐘漂移
保證；代理分片與阻塞也沒有證明 kernel send buffer 曾飽和。人工手感與真實視窗
管理員拖曳仍需人工確認。射擊補償、跳躍、移動互撞與高延遲 Internet 不在本輪範圍。

## 1. 責任與依賴

### 程序、執行緒與雙向資料流

v7.1 穩定基線的結構（master `5ee499a`）：v7 的結構（`0f0bc91`）加上 pv7 的變更（PR #78；Gateway 與 runtime link 依
[protocol-v7](protocol-v7.zh-Hant.md) §2a～§4 改繪）。Client A、B 是各自的程序，共用同一 Gateway／Match。
箭頭表示資料流，不表示底層程式碼依賴上層。執行緒名稱是 `GYO::Threads` 的角色名稱
（`engine/threads/include/engine/threads/RoleThread.hpp`）；固定步期限以 `GYO::Time` 的 Waiter
等待（`engine/time/include/engine/time/Waiter.hpp`），晚醒依 `LateWakeStats` 計數。
v5 以前的結構（主執行緒產生命令、worker 每 ≤2 ms 輪詢、IPC 以短休眠輪詢）見 git 歷史與
[v5 計畫](plans/v5/README.md)、[v7 盤點](plans/v7/INVENTORY.md)。

```text
Client A / Client B (each process; v7 batch 04: three roles)
  Main thread (SDL events + render; PvpApplication)
    keys / mouse -> PublishIntent(latest intent: axes, absolute yaw/pitch,
                    jump presses, owner)          -- never waits for the role
    PresentAt(now) <- local pose + ShotGateView (copy published by the role)
    Drain() -> HUD, action results, SnapshotTimeline -> remote pose
    click -> LocalShotGate -> SubmitAction / SubmitShot -> wakes worker
         |                                  ^
         | intent mailbox (latest wins)     | presentation copy
         v                                  |
  gyo-client-sim (ClientSimulationRole, library client_simulation)
    WaitUntil(next fixed-step deadline, 60 Hz)
    DrainSimulation() -> per-snapshot phase samples + reconcile newest
    ClientSimulationLoop -> ClientSimulation -> LocalPlayerPrediction
    changed window -> SendInput(window, generation) -> wakes worker
         |
         v
  gyo-client-net (ClientConnection worker; asio io_context, v7 batch 08)
    woken by: socket readable, SendInput / SubmitAction / ACK post,
              one timer at the earliest deadline (last 2 ms on the Waiter)
    HTTP lifecycle (may block) + Hello keepalive (Hello.clock: time echo, pv7 §3) + lobby poll
    input: 60/s token bucket, burst 2; waited sends charged less, <= 80/s (09a)
    actions + ACK: 30 Hz (ActionSendRate); resent until the Match decides
    snapshot history <= 64 -> Drain() (main) / DrainSimulation() (sim role)
         |
         v
  HTTP control / UDP Client Protocol pv8 (client_v9.proto)
         |
         v
  Product Go Gateway (goroutines; apps/object_fps_pvp/gateway) -- stateless edge (pv7 §2a)
    receiveUDP: Session -> PlayerId, adapter (one message at a time), admission
      + session token bucket (120/s, 120 deep; services/gyo_gateway/session)
    runtime link writer: woken by input / control / action (wake chan)
      forwards each Client window / batch as it came: no merging, no dedup
      controls in order, never dropped; data: per-player FIFO <= 64,
      full -> drop oldest + count + GatewayDropReport (oneof 21, pv7 §4)
      diagnostic slot: ClockSample heartbeat (oneof 20, 1 Hz, pv7 §3)
    runtime link reader: JoinResult / Snapshot / ActionResults / Evicted / Clock
    sendUDP (the one UDP writer): latest snapshot per peer;
      results: one slot per player, released >= 1/30 s apart (latest-wins)
    expire, logStatistics (ingress / drop counts), runDiagnostics, HTTP serve
         |
         v
  loopback TCP / Runtime Protocol pv8 (runtime_v9.proto)
         |
         v
  Match process (v7 batch 07)
    gyo-match-ipc (IpcHost; asio io_context)
      accept / read / write; action lanes + laneTimer (<= 30 Hz):
        resend unacknowledged decisions until the Client ACKs (pv7 §2a)
      clock reply (one slot); reads GatewayDropReport (diagnostics only)
      SubmitInput / SubmitActionBatch / QueueJoin -> bounded handoff
        (Match alone dedups, judges stale / conflicting windows)
      Pump on publish listener -> latest-wins Snapshot, results, control
         ^ publish listener (post only)         | owning values
         |                                      v
    gyo-match-sim (MatchRuntimeHost::Body)
      Advance(elapsed) -> FixedTickRuntime -> PvpMatch::Tick (60 Hz)
      WaitUntil(absolute next-tick deadline); RequestReset notifies
```

回程：`gyo-match-sim` 發布 Snapshot／結果 → 發布通知只 post 一次 Pump → `gyo-match-ipc` 寫出 →
Gateway reader → `sendUDP` → `gyo-client-net` 收進歷史 → 模擬角色以 `DrainSimulation()` 校正與追蹤相位，
主執行緒以 `Drain()` 取 HUD、動作結果與遠端時間線 → Presented。三個 Client 角色都不等待彼此：
畫面或事件卡住時，模擬角色照固定步期限產生命令（v7 任務 1、2）。

### Ownership 與程式碼依賴

```text
Client 程序（產品 CMake 庫，apps/object_fps_pvp/CMakeLists.txt）
    app_support：PvpApplication（SDL 輸入 / Lobby / GYO Render）、LocalShotGate、呈現
        │ PublishIntent / PresentAt
        ▼
    client_simulation：ClientSimulationRole → ClientSimulationLoop → ClientSimulation
        │ SendInput / DrainSimulation          （GYO::Time、GYO::Threads）
        ▼
    client_network：ClientConnection（asio、httplib、client_v7 Protobuf）
    │ HTTP JSON 控制 / UDP Client Protocol pv7
    ▼
Object_FPS_PVP Go 組合層（無狀態邊緣，§7「Gateway：無狀態邊緣」）
    ├─ Room、容量、加入資格、Session → PlayerId
    ├─ ObjectFPS Adapter：schema / version / 欄位轉換（逐則檢查）
    ├─ runtime link：原樣轉送、每位玩家有界 FIFO、ingress／丟棄計數與 GatewayDropReport、心跳對時
    └─ 使用 services/gyo_gateway 的 HTTP / Session（含 token bucket 限速）/ framing
    │ Runtime Protocol pv7 / loopback TCP
    ▼
Match 程序
    ipc：IpcHost（asio、runtime_v7 Protobuf）：讀寫、framing、action lane（結果重送到 ACK）
        │ 有界控制佇列 / per-player command window / owning Snapshot / 發布通知
        ▼
    runtime_host：MatchRuntimeHost：模擬角色、絕對期限、停止與 reset（GYO::Threads）
        ├─ GYO FixedTickRuntime.Advance(elapsed)
        └─ match_domain：PvpMatch.Tick(TickContext)
               ├─ 純 Movement / Collision 規則
               └─ GYO Collision 幾何查詢
```

Engine 決定時間如何切成 Tick 與如何等到期限；Host 與兩個 Client 角色決定執行緒如何存活；
Match 決定世界如何變化；IPC 與 worker 只交換資料。Go 不計算移動結果、碰撞、合法出生或其他遊戲規則。
`match_domain` 以外的庫都依賴它，`client_simulation` 依賴 `client_network`，反方向沒有依賴；
Match 執行檔只連 `ipc`，不連 SDL。

```text
  object_fps_pvp owner
    Client prediction (gyo-client-sim) --+
                                         +--> StepMovement(Arena, State, Command)
    Match authority (gyo-match-sim) -----+    pure product movement / collision policy
                                              |
                                              v
                      reusable Engine geometry / fixed-tick / time / threads / rendering

    Product Gateway adapter ----> services/gyo_gateway transport / Session tools
    Product probes -------------> product libraries (client_simulation) + read-only observations

  Forbidden reverse edges: Engine -> PvP; public Gateway -> PvP rules
```

共用的是純移動規則，不是 Client 的鏡頭、滑鼠輸入或 Match 的生命週期。
預測與權威各自持有狀態並呼叫同一移動步驟；一致結果來自相同起始狀態、Arena
及命令，而不是交換 Client 算出的座標。跨平台浮點逐 bit 一致性不在本輪保證內。

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
餘量。`FixedTickRuntime` 本身沒有 `Run()`、clock、sleep 或 thread；等待與角色執行緒由
`GYO::Time`、`GYO::Threads` 另外提供（v7 TT-1）。
Match 的模擬角色 `gyo-match-sim`（`MatchRuntimeHost::Body`）使用 `steady_clock` 讀 elapsed，呼叫
`Advance`，再以 Waiter 等到下一個 Tick 格點的絕對期限（Advance 的耗時不延後它），停止或
`RequestReset` 會提前喚醒（v7 第 07 批）。離線測試直接呼叫同一 Host 的 `Advance`。

| 名稱 | 意義 | 不可作為 |
|---|---|---|
| Local sampling Tick | Client 60 Hz 採樣步數 | Authority 的執行時刻或延遲 |
| Authority Tick | C++ 已完成的世界模擬步數 | Client 時鐘的直接對照 |
| Command sequence | 每位玩家獨立、從 1 起的固定一步命令 | Authority Tick |
| lastResolvedCommand | 權威位置已完成的最後命令，含缺包替代步驟 | 最近收到封包的序號 |
| UDP sequence | 同一傳輸 session 封包新舊 | Game Tick |

兩端的 Tick 起點、追趕與停止歷史各自獨立，不視為同步時鐘。原有 object_fps／
object_fps_v2 的 variable-frame gameplay 不受影響；本次只改 PvP。

### 為什麼 initial lead 是持續成本

令 `T` 為 Client 最新產生命令、`C` 為 Match 已完成游標。同為 60 Hz 時，正常一步
使兩者各加一：`(T + 1) - (C + 1) = T - C`。初始序號差沒有自然消失的機制；
在穩定產消下，三步的名義成本約 50 ms，兩步約 33.3 ms。實際延遲還受相位、
傳輸及停頓影響，不能把這個常數與幾個 P95 直接相加。兩步是本機／LAN 的固定
取捨：保留一些到達餘裕；一步候選在抖動下 Actual 比例不穩定，沒有採用。

```text
  First legal input step exists:
    Client window: [1 neutral] [2 neutral] [3 current input]
                              publish together -> worker -> Match

  Match after startup (one command per authority tick):
    tick a          tick a+1        tick a+2        tick a+3
    resolve #1  ->  resolve #2  ->  resolve #3  ->  resolve #4 ...
    ACK = 1         ACK = 2         ACK = 3         ACK = 4

  Client continues generating at the same 60 Hz rate; lead does not drain
  merely because the startup neutral commands have already executed.
```

必須區分三種數值：Client pending 是「尚未收到確認」，包含已執行但 ACK 仍在
路上的命令；`T-C` 是序號差，不能在未同步時刻直接當 queue 深度；Match 的
`contiguousPendingCommands` 才是游標之後已收到且能連續執行的命令數。
復原門檻使用第三種，避免將 ACK 返程延遲誤判為 Server 積欠。

### Input：不可修改的單步命令窗口

Client 60 Hz 產生命令，每個序號固定代表 1/60 秒，欄位只有移動軸與絕對 yaw／pitch。
滑鼠 delta 每個 presentation frame 只消費一次，不在 catch-up 重複消費。
Network worker 以每秒 60 個、容量 2 的 token bucket 送出完整未確認窗口：含從未送出命令的窗口有 token 即送
（v7 第 08 批起由 `SendInput` 喚醒 worker，沒有 token 時以計時器等到 token 出現；v5／v6 是下一次 ≤2 ms 的輪詢），
沒有新命令的窗口在 60 Hz 期限（上次送出＋1/60 秒）重送並保留一個 token 給下一個新命令
（2026-10-02，見 [v5 fix/03](plans/v5/fix/03-a1-missed-frame-starvation.md)）；
新 epoch 的 ACK=0 階段，首次窗口等第一個合法固定步形成後才發布，包含原來的
兩個中立命令及已產生的當前命令，避免僅有 neutral 的窗口提前耗掉 headroom。
這項 bootstrap 補充已由使用者批准；沒有額外產生步數或增加中立命令。
每批最多 12 個，含 24-byte header 的完整 UDP 不超過 1,200 bytes。延遲 deadline
不補送過期批次，worker 不產生命令。v7 第 04 批起由模擬角色在固定步期限產生命令，
不再跟著畫面幀（v5／v6 是主執行緒正常 30 FPS 每幀產生兩步）。
（補充，2026-10-14，v7 第 09a 批）：新命令等過 token 時，依等待時間比例少扣（最多少扣 0.25 個；
等待只算到 token 出現為止；送出後不超過 1 個），60 Hz 產生下 4 個命令內還清，任何 1 秒最多 82 個
輸入封包（`Movement.hpp` 的 `InputSendDrainRate`＝80、`InputSendBurst`；`ClientConnection.cpp` 的送出區塊）。
往前修相位的 slew 期間命令以 75 Hz 產生，每個命令仍固定等約 3.3 ms（1/60 秒×(1−60/75)），slew 結束後回到 0
（證據：`build/target/_build/test/logs/pvp-v7-09ab-impl-20261013/rev/xhigh-review.md` §1-a）。
Gateway／Host 合併不可變命令，未來命令數量及距離游標都不超過 32；32 只算游標以後的
命令，游標以 Match 為準。Gateway 只用自己的游標副本限制合併窗口的大小（已解析的部分
最多保留 32 個），超出上限的窗口原樣轉送，不丟棄。
命令識別為 `(playerId, movementEpoch, sequence)`：epoch 只由 Match 提升，從 1
開始；每個 epoch 的 sequence 從 1 開始，兩者不回繞。重複未完成命令為無操作；
相同識別但內容衝突則原子拒絕整批；已完成／舊 epoch 不重執行，未來 epoch 拒絕。
這些判斷都由 Match 做（第 08c 批，D49）：Gateway 照常轉送已解析的命令，讓 Match
量測遲到；舊或未來 epoch／life、衝突與超過上限的窗口走拒絕線，不合併、單獨轉送，
每位玩家最多 4 個窗口。Gateway 的 runtime link 做已寫出去重：同一（玩家、epoch、
life、序號）而且內容相同、已經寫出過的命令不再寫出；保留期是寫出過的最大序號往回
632 個，超過的照常轉送；換代與 Leave 時清空（每個 epoch 的序號從 1 重新開始）；
拒絕線不記入。去重是最佳化，Match 不得依賴它，仍把重複的命令當成無操作。
每則 IPC 的 `PlayerInput` 最多 12 個命令。Gateway 仍執行的丟棄（拒絕線溢出、
已解析部分的截短，以及拿不到內容的丟棄）依玩家與原因計數，寫進 ingress 統計行；
Match 不知道這些丟棄，是到 pv7 為止的已知缺口（[pv6 契約](protocol-v6.zh-Hant.md)
§1「Runtime 輸入流」）。
補充（2026-10-14，v7 D73）：pv7 移到 v7.1（[v7.1 計畫](plans/v7.1/README.md)），v7 停在 pv6，
所以這個缺口在整個 v7 期間都存在。
補充（2026-10-21，v7.1 `incident-close`）：上面兩段的 Gateway 行為（Gateway／Host 合併、游標副本、拒絕線、已寫出去重、
已解析部分的截短）是 v7 第 08c 批（pv6）的狀態。pv7 起（PR #78）Gateway 原樣轉送每個輸入窗口：不合併、不持有游標副本、
沒有拒絕線與已寫出去重；命令是否已完成、舊或未來 epoch、衝突與重複，都只由 Match 判定；Gateway 仍執行的丟棄依原因回報
Match（`GatewayDropReport`）。現行的規則以 [protocol-v7](protocol-v7.zh-Hant.md) §2a 與本文件 Design Invariants 的
Gateway 一節為準（INVENTORY `N-netarch-ambiguity`）。

（日誌上的對應見 [日誌 Data Contract](log-contract.zh-Hant.md) 第 5 節。）

死信原則（D57）：Client→Gateway→Match 的每一則訊息離開「待處理」時，只有兩種結果：執行，或帶原因的死信（至少依玩家與原因計數，能拿到內容時寫細節紀錄）；沒有第三種（無聲）。移動命令的序號連續、每 Tick 到期就替代，所以 Match 偵測得到缺號（替代紀錄就是缺號的死信）；每個收下的移動命令最多 32 Tick 內一定有結果。死信有界：細節紀錄與保留期都有上限，超過的只計數。死信只用來觀察與記帳，不可重放。新增出口、改丟棄路徑或審查時，依這三條檢查（Architecture Fitness）；第 10 批的 log 契約與第 11 批的 `GatewayDropReport` 以這些原則對齊，不另開死信的子系統或 Data Contract 概念。補充（2026-10-14，v7 D73）：第 11 批與 `GatewayDropReport` 移到 v7.1；本節之後要寫的「Gateway 職責」條文也在 v7.1 寫。補充（2026-10-14，v7 D76）：第 10 批的 log 契約也移到 v7.1。補充（2026-10-20，v7.1 D145）：兩份契約都已實作，維持分開（讀者、單位、版本規則不同）；兩者已用同一張名稱表（`gateway/ingress_statistics.go`）與跨語言 golden（`tests/object_fps_pvp/fixtures/drop_reasons.json`）對齊，沒有 `DropReason` 的 `drop_` 鍵由 Go 測試雙向檢查分類（對應表見 [日誌 Data Contract](log-contract.zh-Hant.md) 第 5.1 節）。

Match 等到當前 epoch 的序號 1 後，每 Tick 恰好完成下一序號。缺命令時沿用最近
**實際執行的真實命令**最多 15 Tick，之後中立移動。Actual／Held／Neutral 都推進
`lastResolvedCommand`，重送不刷新期限。控制、命令交接、模擬依序進行。

每步後記錄游標後連續待執行命令數，最近 30 Tick 總和達 105、且重設冷卻至少
60 Tick，於下一個 Tick 邊界提升 epoch。該邊界不移動，保留身分、位置、視角與
世界 Tick，清除命令／替代輸入／統計，游標歸零等新序號 1。後續每份 Snapshot
均攜帶新 epoch，首份遺失仍可恢復。舊窗口由 Match 失效：Gateway 把換代時待送的
舊窗口移到拒絕線單獨轉送，Match 拒絕並依原因計數；Host 交接再次驗證。
另針對已證實的停頓後永久晚到：最近 30 個 Running Tick 的連續待執行數總和為 0、
同窗曾使用 Held／Neutral、且整個未來命令佇列為空時，視為提前量耗盡；同樣經
60 Tick 冷卻、於下一邊界重設 epoch。全為 Actual 時 queue 為 0 不觸發此條件，
Awaiting seq1 期間不重複重設。這項補充已由使用者批准，理由及反例見 v3 dev_log。
ACK 遺失、窗口滿或 Server 超前本身仍不直接觸發重設。

以下是每位玩家的邏輯狀態；實作由 started／scheduled 與佇列欄位表示，並非新建
通用狀態機。Authority Tick 在三種狀態下都繼續。

```text
  Join -> AwaitingFirstCommand -- receive seq 1 --> Running
          no movement                              |
          wait in current epoch                    | each tick: resolve C+1
                                                   | Actual / Held / Neutral
                                                   |
           +---------------------------------------+
           | backlog OR lead exhaustion (cooldown permits)
           v
        Reset scheduled -- next authority boundary --> ResetBoundary
                                                       epoch += 1; C = 0
                                                       clear old commands
                                                       keep identity + pose
                                                       no movement this tick
                                                             |
                                                             v
                                                   AwaitingFirstCommand

  Leave / disconnect: remove the player; this is not an epoch reset.
```

105／30 表示最近窗口平均 3.5 個連續待執行命令，是本輪固定的異常門檻，不是 RTT
估計。積欠重設處理「過多」，提前量耗盡重設處理「一直趕不上」；後者必須同時有
sum=0、曾替代、整個 future map 空，避免把 queue=0 但全為 Actual 的健康執行誤殺。
升 epoch 是建立新的命令命名空間及啟動邊界，不是快轉世界或把舊操作再走一次。
它保留既有位置，也不能追回已被 Held／Neutral 取代的原始操作。

### 本機預測、校正與鏡頭

產品內 `StepMovement` 讓 Client／Match 共用方向正規化、速度、視角限制及靜態牆壁碰撞。
`LocalPlayerPrediction` 負責本機 60 Hz 模擬及重播，遠端玩家使用一 Tick（16.67 ms）延遲插值。
`ClientSimulation` 在實際呼叫 Advance 前取樣 steady_clock 間隔（v7 起在模擬角色上；v5／v6 在
Application 的幀內），避免停頓被下一次間隔重複計入；domain 本身仍不讀時鐘。主執行緒以
`PresentAt(now)` 把模擬角色發布的呈現狀態放到畫面時刻；意圖超過過期門檻（D30，
`ClientSimulationRole.hpp`）時移動改為中立、保留視角。
Snapshot 到達時還原自己的權威狀態，移除已完成命令並依序重播剩餘命令；鏡頭使用
相鄰預測狀態的逐幀插值，不直接讀權威快照位置。小校正的顯示偏移在 100 ms 內消除，
誤差達 1 世界單位則直接重新定位。完整 ACK 也保留相鄰命令的插值配對，
校正只比較同一預測終點的真正差異，不能將正常插值相位當成位置誤差。
呈現插值及偏移也受角色牆壁碰撞限制。

首次啟動、ACK 超過本機 tip、或較新 epoch 時，清除失效歷史與累積時間，建立
**兩個中立命令**提前量。固定 lead 會形成穩態序號差，是約 33.3 ms 的持續延遲成本，
並非只影響啟動。ACK 小於 tip 時還原並重播；**ACK 等於 tip 是正常確認**，保留
計時餘數，不補中立命令。新播種的首次 Advance 最多計入一步；pending 為空且
frame gap 超過 50 ms 時亦最多計入一步，保留既有餘數，避免重播權威已涵蓋的停頓。
新epoch先保留seed窗口，首個合法固定步形成後才一起發布。若尚未發布前的累積
時間會在一次Advance產生三個以上操作命令，同樣限制本次計入一步並記錄省略時間，
避免首批載入停頓把lead永久墊高；正常30FPS首批兩步仍保留。
窗口達 12 時凍結位置預測，持續視角與重送。失焦產生中立新命令；既有命令不可改。
加入、離開、斷線及身分變更清除預測、歷史與校正。

worker 保存最多 64 份實際接收時間戳快照，`Drain()`（主執行緒）原子取得狀態、連線 generation
及接收歷史；模擬角色另以 `DrainSimulation()` 取得自己的副本，每份 snapshot 都交相位追蹤樣本（v7 第 03 批）。
本機只校正最新狀態；遠端 `SnapshotTimeline` 使用相對 Authority Tick、
固定 60 Hz 斜率、最近 64 樣本的 `min(receiveTime − q/60)` 作時間原點。
顯示游標落後一 Tick、單調且限制於歷史；位置線性插值、yaw 最短角度插值，缺未來
資料保持最新姿態、不外推。玩家 epoch 切換只分割其呈現區段，不跨世代插值。
主執行緒停頓／歷史溢位不重設原點；連續三個樣本偏移超過原點 100 ms 才重新定位。
這是到達時間線估計，不是跨主機時鐘同步。

本機與遠端使用不同時間基準，不能共用「收到快照就覆蓋畫面」的處理：

```text
  Local player
    authority pose at ACK n
          + replay unacked n+1 ... tip
          = new predicted endpoint
                  |
      compare SAME endpoint before / after replay
                  |
    interpolate previous <-> current + decaying display correction
                  |
          collision-limited camera pose

  Remote player
    (authority tick, actual receive time, player epoch) history
                  |
    origin = min(receiveTime - relativeTick / 60), last 64 samples
    target = estimated authority timeline - one tick
                  |
    select [older tick, newer tick] in same player epoch
                  |
    position lerp / shortest-angle yaw -> remote pose
    no newer sample: hold latest; never replay old backlog frame by frame

  Both poses -> renderer -> Presented -> read-only frame observation
```

本機預測解決操作反應；相鄰狀態插值解決 60 Hz 模擬與不同呈現 FPS 的間隙；
校正偏移只掩平真正的權威差異。遠端沒有對方尚未送達的輸入，所以使用過去快照
插值，付出一 Tick 的平滑成本。這三種延遲來源必須分開觀測和調整。

`PvpApplication::LocalMovement()` 提供唯讀 predicted／render position、correction offset、
權威 Tick、確認／最新命令、窗口長度和 frozen 狀態，供產品 GUI probe 記錄；
`RemoteMovement()` 回報選取的遠端姿態與時間線。`PresentedMovement()` 僅在
`PresentStatus::Presented` 後回報該幀真正提交的本機／遠端姿態、frame ID及時間；
Skipped 不產生樣本。原始缺少未來 Snapshot 與移動中的 hold 分別觀測，靜止角色
不增加動作停頓次數；只有同一玩家／epoch的相鄰成功呈現姿態確實重複，才累加
hold。連續選用新的最新快照而位置前進時不算hold。正式遊戲沒有測試輸入或時間控制介面。
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

來源為產品的 `protocol/client_v9.proto` 與 `protocol/runtime_v9.proto`（Go 套件 `clientv9`／`runtimev9`），彼此不 import。
目前兩者版本都為 8（pv8，v8 的 pv8-line；規則見 [protocol-v8](protocol-v8.zh-Hant.md)，沒有改寫的條款沿用 pv7），Client／Gateway／Match 必須一起升級，明確拒絕其他所有版本
（C++ `wire::ProtocolVersion`、Go `adapter.ProtocolVersion`）。Adapter 明確映射兩份生成型別，
Match 核心只接收普通 C++ domain 值。本節只列結構；送達語意、Gateway 的約束、對時與回聲、丟棄回報的細節見
[protocol-v7](protocol-v7.zh-Hant.md) §2a～§4。

Client Protocol 包含 Hello、Welcome、PlayerInput、WorldSnapshot、Error、ActionBatch、ActionResults。
Hello 提交 session token；Welcome 回覆 PlayerId、tick/snapshot rate 與
arena identity 及 Match CombatRules。pv7 起 Hello 與 Welcome 各帶一份 `ClockSample`，做 Client↔Gateway 的時間回聲（只供診斷）。PlayerInput 包含 movement_epoch 及 commands，每個命令只有 sequence、移動軸、yaw／pitch，
沒有 Client 指定時間、位置、傷害或擊殺結果。Snapshot 包含 Authority Tick、完整玩家集合
與每位玩家的 last_resolved_command、movement_epoch、contiguous_pending_commands，
另有獨立 combat 集合。動作身分、裁決及消費確認見第 9 節。

RuntimeEnvelope 包含獨立 `protocol_version` 和 oneof：Ready、PlayerJoin、
PlayerLeave、PlayerInput、JoinResult、RuntimeError、WorldSnapshot、ActionBatch、ActionResults、
PlayerEvicted（v5 連線品質移出，僅 Runtime）、ClockSample（oneof 20，runtime link 心跳對時）、
GatewayDropReport（oneof 21，Gateway 依原因回報自己執行的丟棄）；後兩者是 pv7 新增，只供診斷，不進判定。
Ready 宣告 arena identity、60 Hz、snapshot interval 1、容量 4（`max_players`；v6 第 16 批起）及 Match CombatRules。
Runtime PlayerInput 的 PlayerId 來自已驗證的 Session 映射，不信任 Client 任選 ID。

Adapter 驗證 Protobuf、版本、finite 數值、移動軸範圍與線上欄位範圍；Match 自行
裁定出生位置、速度、合法視角與碰撞。Arena identity 不符時 Client 拒絕加入。

### UDP（pv7；標頭配置自 v5 起不變）

最大 datagram 為 1,200 bytes，包括以下 24-byte big-endian header：

| Offset | Bytes | 欄位 |
|---|---:|---|
| 0 | 4 | ASCII `GYOP` |
| 4 | 2 | Client protocol version，現為 7 |
| 6 | 2 | message type：Hello 1、Welcome 2、Input 3、Snapshot 4、Error 5、Actions 6、ActionResults 7 |
| 8 | 8 | Session ID |
| 16 | 4 | UDP sequence |
| 20 | 2 | Protobuf payload length |
| 22 | 2 | Channel，只允許 0：unreliable sequenced |

Header 後面是對應 message 的 Protobuf payload。公共 framing 驗證長度與 Channel，
不解釋產品 message type；PvP 邊界驗證版本與類型。本輪只有 Channel 0；尚無
多 channel、傳輸層 ACK／ack_bits 或 Reliable Ordered；動作層另有消費 ACK。UDP sequence 用半範圍比較處理 32-bit wrap。
Hello 可重送，Welcome 可重發；完整 Snapshot 修復漏掉的加入／離開資訊。

### IPC（pv7）

Match 僅監聽 loopback TCP，預設 `127.0.0.1:27016`。每個 Protobuf
RuntimeEnvelope 前有四個 bytes 的 big-endian 長度；payload 必須為 1–65,536
bytes。讀取處理半包／黏包；截斷、錯版本、無效 frame 或控制交接超載屬於
連線失敗，不能靜默遺失 Join／Leave 後繼續假裝同步。移動輸入與動作批次被 Match
略過或拒絕時連線不中斷，但每一筆都依原因計數（Match 的 ingress 統計行），不得無聲
丟棄；有 `--movement-trace` 時，移動輸入的拒絕另在 `match-ingress.jsonl` 寫有界的細節。
pv7 的各訊息送達語意（輸入與動作可能重複、結果由 Match 重送到 ACK、順序與背壓）見 [protocol-v7](protocol-v7.zh-Hant.md) §2a。

## 4. Room、Session 與失敗

此 MVP 最多一個固定 Room 1／Match 1。建立前 `GET /rooms` 回傳空陣列；
Runtime Ready 後 `POST /rooms` 建立房間，重試回傳同一個房間。
不做程序配置或動態建立其他 Match。

| HTTP | 行為 |
|---|---|
| `GET /rooms` | 回傳 `rooms` 陣列，含 id、players、capacity、status、arena identity（`match_id` 在 pv8 刪除，protocol-v8 §2a） |
| `POST /rooms` | 建立／回傳固定房間，Runtime 不可用時 503 |
| `POST /rooms/1/join` | 以 `request_id`、`protocol_version` 保留名額 |
| `POST /rooms/1/leave` | 以 `session_id`、`session_token` 驗證離開 |

Join request 範例：

```json
{"request_id":"a-client-generated-unique-id","protocol_version":7}
```

成功回覆包含 `player_id`、`session_id`、`session_token`、`udp_ip`、
`udp_port`、`protocol_version`、`arena_id`、`arena_version`。同一有效 request_id
重試回傳同一 reservation。保留與 joining 名額也計入最多四人限制（`MaxPlayers`＝4）。

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
Session 憑證隨機產生，UDP endpoint 不可用舊 session 靜默切換；唯一的例外是下面
「連線中斷與恢復」的重新綁定，它有條件、有紀錄，不是靜默的。

Reservation／有效 Session 的期限是 Gateway 單調 wall clock 五秒；等待 Runtime
JoinResult 的上限為三秒。Network worker 用既有 Hello／Welcome 保活（每 250 ms，見下面「連線中斷與恢復」的 Hello 間隔），即使畫面停止產生 Input 仍可存活；
60 Hz Input 也可保持連線存活。Hello 不延長遊戲移動期限。這些連線期限與
十五個缺包 Authority Tick 的替代輸入期限 是不同契約。

### 連線中斷與恢復（v7.1 `outage-resync`，D172、D173；條文 2026-10-23 使用者核准，D174）

斷線偵測與恢復只走 UDP：snapshot 串流本身就是心跳，不另開 TCP 心跳或控制通道，
wire 維持 pv7。恢復的意思是重新建立「時間上的對齊」：時鐘、相位、命令序列重新對齊，
身分（同一位玩家、同一個 session）接回來。各條的生效批次寫在括號裡；生效之前照上一段的現行行為。
條文的根據：[outage-resync 計畫](plans/v7.1/outage-resync.md) 第 3 節，
[特性報告](plans/v7.1/outage-characterize-report.md)。

**Client 的連線狀態（Playing 期間，`client-interrupted` 起）**

| 狀態 | 進入 | 期間的行為 | 離開 |
|---|---|---|---|
| 連線 | 進入 Playing；或恢復流程完成 | 現行行為 | 最後一份有效 snapshot 到達後 ≥ T_int → 中斷；非暫時性錯誤 → 斷線 |
| 中斷 | 連線狀態下，最後一份有效 snapshot 到達後 ≥ T_int | 見下方 | 收到有效 snapshot → 恢復流程 → 連線；最後一份 snapshot 後 ≥ T_max → 斷線；非暫時性錯誤 → 斷線 |
| 斷線 | 中斷狀態下 ≥ T_max；任何狀態下的非暫時性錯誤或 Gateway 的 Error | 既有的 Failure：關閉傳輸、回 Lobby、顯示原因 | 終點；重新加入走新的 join |

- Gateway 沒有送給 Client 的 UDP Leave 訊息。Gateway 讓 session 到期或移除玩家時，Client 看到的是自己從 snapshot 消失、收到 Error，或收不到封包（之後由 T_max 收斂）。
- **有效 snapshot**：含本機玩家、而且 tick 比上一份新的 snapshot，也就是網路 worker 同時放進兩份歷史（呈現用與交給模擬端）的那一份。
- **T_int＝0.5 s**（D173）。只看有效 snapshot 的到達時刻，用收包當下的 `steady_clock`。
  - 來源：v7.1 語料中，沒有注入停頓的跑次 snapshot 最大間隔 0.119 s；注入 250 ms 停頓時最大 0.283 s。
- **T_max＝5 s**（D172 的 D-11）。Playing 期間從最後一份有效 snapshot 起算；Welcome 與其他封包不延長它。
  - 所以 Gateway 活著、Match 卻不出 snapshot 時，「中斷」也有上限。
  - 收到 Welcome 之前（握手）照現行：從開始連線的時刻起算 5 s。收到 Welcome 之後、第一份 snapshot 之前，以**第一個** Welcome 的到達時刻當作「最後一份 snapshot」（之後每秒的 Welcome 不延長它）。
- **常數只維護一處**：T_int、T_max 在 C++（`Movement.hpp`）與 Go 各有一份，由一份共用的 fixture 測試綁定兩端的值（比照 `drop_reasons.json`）。
- **判定的輸入**：只有到達時刻與這兩個常數。ClockSync 的 offset、delay、時鐘回聲都不得進入判定（protocol-v7 §2）。
- **暫時性 socket 錯誤**（連線與中斷狀態都適用）：connection_reset、refused、network_unreachable、host_unreachable、network_down 只計數，當作這次沒有資料；其他錯誤照舊走 Failure。
  - **代價**：Gateway 程序死掉時，Client 以前收到 refused 立刻失敗，之後要等 T_max（5 s）才回 Lobby。
- **中斷期間**：
  - socket、session 憑證、傳輸序號、動作帳本都保留：不關閉傳輸、不重設動作、傳輸序號不歸零、連線 generation 不變。
  - Hello 照常每 250 ms（見下面的 Hello 間隔），兼作探測。
  - Input 與動作照現行規則產生、送出（D172 的 D-9 (a)）。未確認的窗口滿 12 個命令就停止產生，所以只有下行中斷時，Match 跑完窗口後就沒有輸入。中斷期間的操作不保證生效：開槍可能因參考過舊而 Expired，被擊中照算。
  - HUD 顯示「連線中斷」。Client 不推進 epoch。
- **Hello 間隔**（v7.1 `rebind-on-new-port` 起，D188；取代 D187 的條件式間隔）：所有階段一律每 250 ms（T_int／2）。
  - 只送 Hello 的期間，Gateway 的最後驗證時刻一直在 T_int 以內。
  - 上行從 Gateway 不認得的 endpoint 到達時（例如有線、無線同時連著，送出改走另一條，回程仍到舊位址；或 NAT 映射改變），原 endpoint 靜默滿 T_int 之後的第一個 Hello 就能改綁，不必等中斷，也不推論別的元件的狀態。
  - 速率：每秒 4 個 Hello，在預算 82＋4＋30＝116 以內；Gateway 每個 Hello 回一個 Welcome。v7.1 D188 之前的證據裡，Hello／Welcome 的計數是每 1 s（中斷期間每 250 ms）的規則，和之後的計數比較時要註明。
- **恢復流程**（收到第一份有效 snapshot 時，依序執行）：
  1. **時鐘**：提前結束目前的 ClockSync 視窗（該行 `reason=resume`、`final=0`），再開新視窗。漂移的回歸照現行（從上次跳變起算），不重新開始，`drift_ppm` 的意義不變。時鐘值只供診斷，不進第 2、3 步。
  2. **命令序列**：snapshot 的 movement epoch 和中斷前不同時，走既有的換 epoch 路徑（清掉送出窗口、Reset＋SeedLead），舊命令已被 Match 以 `EpochOld` 丟掉。
  3. **相位**：重新擷取，但不改寫還在途中的序號。依批次中最新的那份 snapshot（和 `ClientSimulation::Observe`、`Reseeds()` 一致）分三種：
     - epoch 不同（第 2 步），或權威的游標 > Client 的 tip：既有規則（換 epoch 的 Reset＋SeedLead，或 stall reseed）。
     - epoch 相同、游標 ＝ tip：沒有在途的序號，呼叫 SeedLead。
     - epoch 相同、游標 < tip（例如 Match 自己停頓、只追趕了 5 步）：不呼叫 SeedLead，不動未確認的命令與預測狀態，只重設相位的擷取：
       - 清掉相位樣本、命令年齡環、連續晚到計數；
       - `settledAfter_` 設成 Client 目前的 tip；
       - 還沒 slew 完的修正，剩下的部分取消（和 SeedLead 相同）；`settling_`、`reacquiring_` 清除；
       - 下一次相位決定照「第一次決定」處理；
       - 這個動作寫進移動紀錄（log-contract §1.4 的 `phase_path=resume_reset`）。
     - **為什麼不一律 SeedLead**：SeedLead 會清空未確認的命令，再從權威游標＋1 重新產生；權威還沒追上 tip 時呼叫它，會用不同內容改寫同一個序號，違反 §7「相同命令識別不可修改」（審查 C1）。
     - **代價**：游標 < tip 時，中斷期間累積的領先量會留到 Backlog 保險絲清掉（P2 的第二個已知例外，§7）。
     - **時點**：恢復事件在模擬端處理同一批 snapshot 的相位樣本（`ClientSimulation::Observe` 的 ObservePhaseSample）之前生效，所以中斷期間產生的序號不會成為樣本。
     - **介面**：網路 worker 在交給模擬端的那一批資料上帶一個一次性的「恢復」旗標，這是相位追蹤知道中斷結束的唯一管道。
       - 旗標和「把這份 snapshot 放進交給模擬端的批次」在同一個 mutex 區段內設定，所以模擬端不會拿到沒有旗標的恢復批次。
       - 連線換代的三個邊界（Close、Failure、Push）一併清除旗標；兩次恢復合併在同一批時只算一次。
       - 相位追蹤不讀中斷狀態與時長，也不自己再判斷一次間隔。
  4. HUD 的中斷提示消失，寫 A 級的 `connection resumed` 行（log-contract §1.4）。

**Match 的命令序列中斷（`match-resync` 起）**

- epoch 只由 PvpMatch 推進。換 epoch 的原因與中斷有關的有三個：
  - **Starvation** 保險絲：現行規則，30 個沒有領先量、有替代、佇列為空的 Tick；受 60 Tick 冷卻限制。
  - **Backlog** 保險絲：現行規則，30 個 Tick 的佇列量總和 ≥ 105；受同一個冷卻限制。
  - **Resync**（新增）：見下方。
- **晚到過久**：一個被替代的序號，**第一次**到達時，晚到的時間（到達時刻 − 替代時刻，host 的時鐘，和 slack 樣本同一個讀數）超過 N_o。
  - N_o＝20 Tick（333,333 µs，D173）。用時間而不是序號差判斷，所以 Match 停頓、追趕上限丟掉時間時，樣本的下限仍然成立（審查 M8）。
  - 這次到達**不產生 slack 樣本**，計入 `late_beyond_no`。同一序號之後的副本不再判斷。
  - 晚到判斷需要替代紀錄還在。紀錄的保留範圍（`retention-600` 起為 600 個序號，約 10 s）因此是遊戲規則的一部分：超出範圍的到達在帳本裡是 `resolved_untracked`，不產生樣本，也不觸發 Resync。從 `match-resync` 到 `retention-600` 之間，判斷看的是過渡規則保留的所有紀錄（600 序號內或屬於最新 64 筆），不只最新 64 筆。
- **Resync**：每個 epoch 的第一次「晚到過久」，排程在下一個 Tick 邊界換 epoch，和 Starvation／Backlog 的排程方式相同。
  - **合併**：
    - 已排定 Starvation 或 Backlog 時，不再排程：只換一次 epoch，原因與移動紀錄的 `reset_reason` 是先排定的那一個，resets 計 1，`resyncs` 不計。
    - 同一個 Tick 發生 LifeRespawn 時，LifeRespawn 優先（現行規則會清掉已排定的換 epoch）：resets 計 0，`resyncs` 不計，`reset_reason` 是 LifeRespawn。
    - 晚到過久的計數 `late_beyond_no` 在任何情況下都照計。
  - 不受 60 Tick 冷卻限制；換 epoch 時照現行設定冷卻起點。
  - 已排程到換 epoch 之前，同一個 epoch 的其他「晚到過久」只計 `late_beyond_no`，不再排程。
  - **判定的歸屬**：晚到時間由 MatchRuntimeHost 的 SlackTrack 量（它擁有替代紀錄與 host 的時鐘），Host 透過 PvpMatch 新增的公開介面要求在下一個 Tick 換 epoch；epoch 本身仍只由 PvpMatch 改變。這是新的依賴邊（Host → PvpMatch 的排程介面），列在計畫的 Delta-2。
- **品質**：Resync 和 Starvation 一樣計入品質視窗的 resets（D172 的 D-10：不為重連另開分支）。
- **一定換 epoch 的情況**（可檢查的定義）：
  - 冷卻期外，Starvation 的條件成立；
  - 任何時候，晚到過久的第一次到達。
- **不換 epoch 的情況**（已知，交給恢復流程第 3 步的相位重新擷取）：
  - 冷卻期內，中斷結束時 Starvation 還沒成立、也沒有晚到過久的到達（例如上行丟失 0.6 s：期間沒有命令到達，Client 只會 stall reseed）；
  - 短於保險絲的中斷（例如雙向 0.3 s）；
  - 只有下行、短於約 0.7 s 的中斷：Match 跑完窗口的 12 個命令後，沒有輸入的 30 個 Tick 還沒到。超過約 0.7 s（冷卻期外）時，Starvation 換 epoch，Client 走第 2 步。
- **pv7 的限制**：Match 看不到 Client 的下行中斷，所以只保證「Match 看得到的中斷」會換 epoch。Client 的相位一律在恢復時重新擷取，不受這個限制。

**Gateway（`client-interrupted`、`identity-rebind` 起）**

- **活性與上限**：照現行。只有更新的傳輸序號刷新活性（`lastSeen`）；session 期限 5 s 到期送 Leave（SESSION_TIMEOUT）給 Match。
  - T_max 和 Client 是同一個值，但起算點不同：Gateway 從最後一個刷新活性的上行封包起算，Client 從最後一份 snapshot 起算。
  - 在 macOS 上實測：機器睡眠時 Go 的單調時鐘停住，C++ 的 `steady_clock` 繼續走（v7.1 HANDOFF 的睡眠實測）。其他平台未實測。
- **最後驗證時刻**：session 另外記「原 endpoint 最後一個通過 session 驗證的資料包」的時刻。
  - 通過驗證：**已綁定的 endpoint** 送來、token 相符的 Hello，或 Input、Actions 的 endpoint 比對（`Admit`）。新 endpoint 送來的 Hello 不刷新它；改綁的判斷在刷新之前做。只有 Hello 帶 token，所以只看 token 的話，正常遊玩中每個 1 s 的 Hello 週期裡約有一半時間會被判成靜默（第二輪審查 C1）。
  - 不論序號新舊、payload 是否有效、是否被限速：endpoint 比對在限速之前。
  - 偽造原 endpoint 的封包只能讓這個時刻變新，也就是只能阻止改綁，不能促成改綁。
  - 這是通用層 session 的新欄位，屬於允許的活性類，但新增欄位本身是 Architecture Delta（計畫的 Delta-3）。
- **時鐘**：某位 Client 新到、而且通過上面的 session 驗證的資料包，和刷新之前的最後驗證時刻相隔 ≥ T_int 時，結束並重開該 Client 的時鐘視窗。沒有通過驗證的資料包（例如從其他位址送來、帶同一個 session id 的封包）不觸發重開，所以外部無法用偽造的封包灌出 clock sync 行。
  - 判斷只用上面那個時刻。正常遊玩時 60 Hz 的 Input 會一直刷新它，不會觸發。
  - 結果只進 diagnostic state（時鐘視窗），不進任何判定。
  - Match 的時鐘視窗量的是 Gateway↔Match，不受單一 Client 中斷影響。
- **身分的重新綁定**：Hello 從新的 endpoint 到達時，下列條件全部成立才把 session 改綁到新 endpoint：
  - token 正確；
  - 傳輸序號比 session 記錄的新；
  - 原 endpoint 的最後驗證時刻已過 ≥ T_int（原 endpoint 的任何資料包都會刷新它，所以單靠送一個跳號的封包製造不出「靜默」）；
  - session 還沒到期。

  T_int 由產品以參數傳給通用層（比照 `Expired(now, timeout)`），通用層不寫死產品常數。改綁之後，原 endpoint 的封包回 `ErrUnauthorized`；每次改綁寫一行 A 級的 `session rebound`。只用通用層 session 的軟狀態，不需要知道 Match 的任何狀態。Client 中斷期間不換 token、不重設傳輸序號，所以 Client 端不需要新邏輯。
  - **安全取捨**：token 在 Hello 裡以原文傳送。拿到 token、又能讓原 endpoint 的所有封包停止 T_int 以上的人（例如在同一網段上），可以搶走 session。只送 Hello 的期間，Hello 每 250 ms（上面的 Hello 間隔，D188），最後驗證時刻一直在 T_int 以內，所以搶 session 的人一定要先讓原 endpoint 靜默。紀錄讓事後查得到。防竄改的證明要改 wire，列入 v8 候選（D172 的 D-2 I-3）。

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

修改 `.proto` 後，用 CMake 建好的 v36.2 `protoc` 執行（PowerShell 範例；
其他 shell 寫法見 [protocol README](../../apps/object_fps_pvp/protocol/README.md)）：

```powershell
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11
protoc --version  # 必須為 libprotoc 36.2；PATH 指向本次 CMake 建置的 protoc
protoc --proto_path=apps/object_fps_pvp/protocol `
  --go_out=apps/object_fps_pvp --go_opt=module=gyo.local/object_fps_pvp `
  apps/object_fps_pvp/protocol/client_v9.proto apps/object_fps_pvp/protocol/runtime_v9.proto
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
以相同位移門檻比較兩個視窗；這個早期 smoke 門檻為 localhost 中位延遲不超過
150 ms，不能代替 v3 的三輪正式驗收（P50 ≤ 50 ms、P95 ≤ 80 ms）。Renderer capture
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

產品專屬 `run_timing.py` 支援雙 GUI 三輪 120 秒／200 個預先安排事件，以及
`timing_probe` 的 60／144 Hz 真實網路／預測 30 分鐘長測。命令延遲以相同識別
配對；呈現以位移交越配對。未配對／不明事件以無限延遲計入 nearest-rank 分位數，
不排除慢幀，所有呈現間隔均保留。GUI 計時期間不 readback 或存圖；GPU 正確性另測。
僅比較同機單調時鐘，不稱為 input-to-photon。固定門檻、執行命令、實測結果與限制見
[命令時序 v3 開發日誌](../dev_logs/2026_09_25_pvp_command_timing_v3.zh-Hant.md)。

## 7. 後續開發應保留的核心設計

### Design Invariants

| 原則 | 維持的性質 | 未來修改時要問的問題 |
|---|---|---|
| 封包、命令、Authority Tick、呈現 frame 分開 | 重送不能加速世界；FPS 不改正常移動速度 | 是否讓收包數、worker 或 Render 額外推動模擬？ |
| 相同命令識別不可修改 | 去重、ACK、重播及舊世代失效具有一致含義 | 是否用同一序號改寫操作，或復活已完成操作？ |
| 共用純規則，各端持有狀態 | 預測與權威可以逐命令比較 | 是否把鏡頭／socket／時鐘塞入 StepMovement？ |
| ACK 確認已完成的模擬 | Actual、Held、Neutral 都是不可撤回的權威歷史 | 是否又把 ACK 當作最後收到封包的序號？ |
| 過多積欠與耗盡提前量分別復原 | 阻塞與停頓不留下永久延遲或永久 Neutral | 是否只覆蓋一直晚到，漏掉交替 Actual／晚到？ |
| 控制事件有序，完整狀態可合併 | Join／Leave 不丟失，快照不形成無界 FIFO | 是否誤丟控制，或替換已部分寫出的 TCP frame？ |
| 連線 generation 與 movementEpoch 分工 | 換連線清整體歷史；單人 epoch 只切該玩家區段 | 是否為一人復原而重置另一人的時間線？ |
| 觀測與模擬契約分開 | 診斷時間戳不能成為 Client 指定的模擬參數 | 是否以診斷欄位驅動玩法，或把測試注入放入正式遊戲？ |

觀測也必須符合因果：Host 可能在 Client 的 send 呼叫返回前就接受命令，因此成功
send 保存開始／結束區間，不能用晚記錄的單點時間否定真實接收。首次送出延遲用
結束時間作保守上界。Presented 才能形成呈現樣本；缺未來快照不等於正在移動的
角色真的停住，靜止角色更不能算作時間線停頓。

#### Gateway：無狀態邊緣（v7.1 `edge-contract`；2026-10-18 使用者核准，D120）

Gateway 是無狀態的邊緣：內部訊息保留身分並且冪等，可靠送達只由端點（Match 與 Client）負責（protocol-v7 §2a，v7.1 D94、D-14、D105）。

| 原則 | 維持的性質 | 未來修改時要問的問題 |
|---|---|---|
| Gateway 只持有能重建、不參與遊戲判定的軟狀態 | Gateway 掛掉或換掉，不會改變任何遊戲結果；Match 的判定不依賴 Gateway 記得什麼 | 這個欄位或判斷，是否要知道「Match 已經有什麼」才能做對？是的話，它屬於 Match |
| 不持有 Match 決策狀態的副本 | 去重、合併、過時、衝突、重送只在端點發生，不會在 Gateway 悄悄改變權威的輸入（2026-10-10 事故） | 是否記下了游標、epoch、life、命令表、動作帳本、裁決或退役？ |
| 每則訊息獨立處理 | 依遊戲項目的身分或內容的判斷只在端點；Gateway 對每則訊息只翻譯，並檢查單則訊息內的版本、格式、範圍與授權 | 是否拿前一則訊息的內容來決定這一則？（依傳輸序號、速率與 session 生命週期的跨訊息判斷除外） |
| 丟棄要有界、要回報 | 每個丟棄出口都計數並回報給 Match（§4 的 DropReason）；Gateway 執行，政策與真相在 Match（D49） | 新的丟棄出口是否有 DropReason、計數與回報？ |

**允許的類別**（守衛 `gateway/state_whitelist_test.go` 的分類，名稱一致）：

- connection and session：socket、runtime link、session（token、endpoint、傳輸序號、速率、活性）、保留（玩家與 session 的對應、id 配發）。
- lifecycle：房間可用、建立、玩家階段、關閉與 once 旗標。
- configuration copy：`Config` 與 Match 的 `Ready`。
- bounded outbound or IPC queue：controls、snapshot 與 runtime link 的有界佇列（溢出要計數並回報）。
- results slot：每位玩家上次送出結果的時刻與至多 1 則待送的結果框架（latest-wins）。
- synchronization：mutex 與喚醒用的 channel。
- diagnostic state：時鐘同步、回聲、丟棄帳本、統計；判定路徑不得讀取（protocol-v7 §2「診斷不進判定」）。
- diagnostics carried with a queue item：佇列項目帶著的身分（收包時刻、傳輸序號、命令或動作 id 的範圍；snapshot 讀到 IPC 框架的時刻），在它被丟棄時拿來回報，或在寫出時算成 dwell、hold 填進同一則訊息（`cause-records`，D125）；都不被判定讀取。它隨項目進出佇列，項目離開佇列就消失；所以和 D105「單次呼叫的脈絡不得跨資料包保留」不衝突，是獨立的允許類別（N3，D120）。
- per-call context：處理一個 datagram、frame 或請求期間記下的值（例如 `Server.datagram`、讀取緩衝區）；不得被下一個 datagram 的判斷讀取。
- test seam：套件變數與注入的時鐘；正式路徑只用預設值。
- immutable table：鍵清單、原因表、sentinel error。

**禁止**（Gateway 不得做的事；括號內是負責的一方）：

- Gateway 不得持有 Match 決策狀態的副本，例如游標、epoch、life、命令表、動作帳本、裁決、退役（這些狀態的權威是 Match）。
- Gateway 不得依遊戲項目的身分或內容做跨訊息的判斷，例如去重、合併、判定過時、解決衝突（由 Match 判定）。
- Gateway 不得排程重送（可靠送達由端點負責：結果由 Match 重送到 Client 確認為止，輸入與動作由 Client 重送）。

**守衛與變更規則**：

- `gateway/state_whitelist_test.go` 遞迴走訪 Gateway 套件每個 struct 型別的欄位（含指標、map 的鍵與元素、slice、array、channel 與匿名 struct；介面欄位要明列它持有什麼），用 `go/ast` 掃描 `gateway` 與 `gateway/adapter` 的套件變數，以及以 `go` 啟動的函式與閉包裡的區域 map、slice，和分類表比對。新增未分類的欄位、變數或 goroutine 區域狀態，測試就失敗；分類表裡沒有「Match 決策狀態」這個類別。
- 新增任何 Gateway 狀態欄位（包括把它分進既有類別）都是 Architecture Delta，要經使用者同意（AGENTS.md §3 的 State Authority）。
- 依內容的判斷（某個允許的欄位被拿來做遊戲判定）測不到，靠審查守。
- 通用層 `services/gyo_gateway/session` 的狀態由它自己的 owner 管；產品層 Gateway 透過 `reservation.session` 持有它，歸在 connection and session。

**Replacement Criteria**（出現時不再繼續修補，改向使用者提出「重建或修改」）：

- 為了讓某個遊戲行為正確，需要 Gateway 記住 Match 已經知道的東西（判斷準則答「是」）。
- 新的外部 Client 介面或新的訊息種類，必須在 Gateway 加跨訊息、依內容的判斷才能運作。
- 同一類丟棄或延遲問題，需要第二次在 Gateway 加特例才能通過驗收（2026-10-10 事故與 v7.1 edge 系列之前的模式）。
- 分類表的允許類別本身需要擴充（而不只是新增欄位）。

**目前的已知項目**（只記錄）：

- K1（v7.1 D118、D119、D121）：通用層的固定窗口限速（120 包／秒）在 Gateway 自己停頓後，把緩衝中的合法封包算進同一窗口而丟掉輸入。`edge-rate-limit` 改為 token bucket（每秒補 120、容量 120，狀態留在通用層的 session，屬於允許的 session 類）；Gateway 停頓約 1.3 秒以內不再丟（開槍玩家約 91 包／秒）。
- K2（v7.1 `outage-resync`，D172）：session 的 endpoint 重新綁定與「最後驗證時刻」（§4「連線中斷與恢復」）屬於允許的 connection and session 類，判斷屬於 session 生命週期的跨訊息判斷，分類表的類別不變；新增的欄位本身照上面的變更規則提出。

#### 連線中斷與重新對時（v7.1 `outage-resync`；方向 D172、數值 D173；條文 2026-10-23 使用者核准，D174）

條文本體在 §4「連線中斷與恢復」。這裡寫的是不得改變的設計意圖，以及守住它們的測試。

| 原則 | 維持的性質 | 未來修改時要問的問題 |
|---|---|---|
| 存活判斷只走 UDP，snapshot 串流就是心跳 | 沒有第二條通道要同步、要判斷誰先失效 | 是否為了偵測斷線加了 TCP 心跳、控制通道或 wire 訊息？ |
| 中斷判定只讀到達時刻與常數 | 診斷值（時鐘回聲、offset、delay）不會讓玩家被判成中斷或斷線 | 中斷或斷線的判斷，是否讀了 ClockSync 推導出的任何時刻？ |
| epoch 只由 PvpMatch 推進；冷卻期外的 Starvation 與「晚到過久」的第一次到達一定換 epoch | 中斷期間的舊命令不會被補算；Client 不會自己開新的命令序列 | 是否讓 Client 推進 epoch，或為了不扣品質分而抑制 Starvation／Resync？ |
| 晚到超過 N_o（時間）的到達不產生樣本 | 發布的 slack 不小於 −N_o（−333,333 µs）；「晚到 1～10 秒」不會以大負值進入 Client 的相位追蹤 | 是否加大 N_o、改回用序號差判斷，或讓晚到過久的到達產生樣本？ |
| 恢復時 Client 一定重新擷取相位，而且不改寫在途的序號 | 中斷前與中斷期間的樣本不被沿用；同一序號的命令內容不變；領先量的沿用只限 §7 已知項目的兩種例外 | 恢復路徑是否又依賴「剛好發生 stall reseed」，或在權威還沒追上 tip 時呼叫 SeedLead？ |
| 相位追蹤只接收一次性的「恢復」旗標 | 中斷狀態歸 Client 的網路 worker，相位追蹤、Match、Gateway 都不讀它 | 是否讓 LocalPlayerPrediction 讀中斷狀態或時長，或在模擬端再判斷一次間隔？ |
| 身分的重新綁定只用 session 軟狀態 | Gateway 仍是無狀態邊緣；改綁有條件、有紀錄 | 改綁的條件是否需要知道 Match 的狀態，或在原 endpoint 沒有靜默時也允許？ |

**守衛**（各批實作時補上；第一行已存在）：

- **特性測試**（`outage-characterize`）：`PvpMatchTests.cpp` 的「PvP outage characterization: …」、`PredictionTests.cpp` 的 3 個。它們固定現狀；之後每一批改變其中的值時，都要寫明改了哪一個、為什麼。
- **`match-resync`**：
  - 「一定換 epoch」的兩種情況，由特性測試升格為守衛；
  - 在只有上行、暫存 0.6 s、冷卻期內的情境（現在有 34 個晚到、約一半超過 20 Tick），斷言發布的最小 slack ≥ −333,333 µs，並以 `late_beyond_no` 斷言守恆；
  - 突變「只壓掉觸發 Resync 的那一筆樣本」、「用序號差判斷」都要被殺掉；
  - 同一個 Tick 發生 LifeRespawn 與 Resync 時的計數（resets、`resyncs`、`reset_reason`）。
- **`client-interrupted`**：
  - `ConnectionLiveness` 列入 `test_diagnostic_isolation.py` 的 DECIDING 清單；
  - 突變「用時鐘回聲的時刻餵 Liveness」；
  - 差分 oracle（診斷值為極端值時，中斷判定逐位元相同）；
  - 新情境：Match 停頓超過追趕上限、epoch 相同、權威 < tip 時恢復，斷言未確認的命令與 worker 的窗口不變、沒有 `ConflictQueued`、沒有錯配的相位樣本；突變「恢復時一律 SeedLead」與「恢復時不重新擷取」都要被殺掉；
  - 邊界：游標剛好等於 tip 時恢復；
  - 競態：模擬端的 drain 發生在「放進 snapshot」與「設定旗標」之間的情境（同一個 mutex 區段的守衛）；
  - T_int、T_max 兩端相同的 fixture 測試。
- **`identity-rebind`**：
  - 通用層 session 的 Go 測試：靜默不足、序號不新、token 錯、已到期時都拒絕改綁；
  - 「以原 endpoint 送一個跳號的封包之後，T_int 內新 endpoint 仍不能改綁」；
  - 「原 endpoint 只送 Input、不送 Hello 0.6 s 時，新 endpoint 的 Hello 被拒絕」；
  - 「60 Hz Input 加上 1 s Hello 時，Gateway 的時鐘視窗不重開」；
  - 「從其他 endpoint 送來、帶同一個 session id 的資料包，不重開時鐘視窗，也不刷新最後驗證時刻」；
  - `state_whitelist_test.go` 不變。
- **`rebind-on-new-port`**（D188）：worker 驗收的 `CheckHelloCadence`：Welcome 之後、第一份 snapshot 之前，Input 流動、確認前進時，確認停住而 snapshot 仍到時，Hello 都是每 250 ms，而且不更頻繁。
- **變更規則**：特性測試升格為守衛之後，改變它的判定照 AGENTS.md §14.4（寫出 1～5，取得使用者核准）。

**已知項目**（只記錄）：

- **P2 的例外（D173 的 D-15 (a)）**：只有上行中斷時，Client 在新 epoch 裡把窗口填到 12。上行恢復後，這 12 個命令的領先量要等 Backlog 保險絲清掉，最長約 1 s（中斷結束後約 31 Tick，或冷卻結束時）。修剪多餘的領先量交給 v8 B6。
- **P2 的第二個例外**：Match 自己停頓、恢復時 epoch 相同而權威 < tip，Client 不改寫在途的序號，所以中斷期間累積的領先量（最多約 7 Tick）留到 Backlog 保險絲清掉（約 0.5～1 s，冷卻期內要等冷卻結束）。在那之前，新命令的樣本被這段領先量墊高，第一次相位決定可能據此把相位往後移（最多 2 Tick）。停頓超過約 100 ms 時（追趕上限 5 步開始丟時間），這段領先量會在約 0.5 秒內觸發 1 次 Backlog 重設，連線品質窗口因此判失敗，HUD 顯示 CONNECTION POOR 約 10 秒；單次停頓只讓連續失敗數到 1，不會移出（2026-10-30 在 master 上以停頓 140 ms 重現）。和第一個例外一樣交給 v8 B6。
- **只有上行中斷時 Client 不顯示中斷**：Client 照常收到 snapshot，看到的是角色停住再起步。
- **不換 epoch 的中斷**：§4 列出的三種，由相位重新擷取處理。
- **pv7 的限制**：只保證 Match 看得到的中斷換 epoch（§4）。

**Replacement Criteria**（出現時停止修補，向使用者提出修改或重建）：

- 需要 Gateway 記住中斷階段（重新綁定所需的 session 軟狀態除外）。
- 第二次需要在 Client 的連線狀態機加中斷特例，才能通過驗收。
- 需要第四種和中斷有關的換 epoch 原因（現有：Starvation、Backlog、Resync；LifeRespawn 與 SequenceExhausted 不屬於中斷）。
- 品質判定需要依「是否重連」分支。

#### 被替代序號的單一紀錄（v7.1 `outage-resync` C13 (b)；D171、D172；owner 名稱 D175；`single-store-refactor` 起生效）

- **權威**：「這個序號被替代過、第一次是否已到」只由 `MatchSubstitutionRecords` 記錄（`MatchRuntimeHost` 的 `substitutions_`，屬於 slack 樣本路徑的行為側；紀錄的開啟、保留與關閉原因都由它決定）。替代帳本（`MatchIngressLedger`）只讀每次到達的結果，並接收它關閉的紀錄；帳本自己只保留不參與判定的附欄：之後的副本數、第一份的參考年齡、看著解析的序號範圍、溢出的序號。
- **方向**：只允許「帳本 → `MatchSubstitutionRecords`」，不得反過來，因為診斷不進判定（protocol-v7 §2）；關閉原因的列舉也在紀錄側的標頭，診斷側引用它。守衛：突變 `v7-flow-ledger-drives-sample`，以及 `test_diagnostic_isolation.py` 的方向檢查。
- **保留範圍**：
  - 游標 − 序號 ≤ 600（`retention-600` 起生效）。超出時照既有的關閉規則以 aged 關閉並計數，不得無聲丟棄；關閉之後的副本不產生樣本，也不觸發 Resync。這個範圍也決定「晚到過久」能否被判斷（§4），所以是遊戲規則的一部分。
  - 之前（到 `retention-600` 為止）：樣本只取最新 64 個還沒到達的紀錄。`single-store-refactor` 的過渡規則是「600 序號內或屬於最新 64 個就保留，樣本只給最新 64 個」，行為與當時逐位元相同。
- **樣本與守恆（S3）**：每個序號只在第一次晚到時判斷一次：晚到 ≤ N_o 產生一個樣本（計 `slack_late`），晚到 > N_o 不產生樣本（計 `late_beyond_no`，`match-resync` 起）。
  - `retention-600` 之後，S3 是結構上的等式：`late_first == slack_late + late_beyond_no`；
  - 之前不保證：`match-resync` 之前，稠密替代時 64 筆上限讓 `slack_late` 偏少（`outage-characterize` 量到 187 對 64）；`match-resync` 到 `retention-600` 之間，只剩稀疏替代、超過 600 序號時，過渡規則保留的紀錄在帳本裡是 `resolved_untracked`（0 對 1）。所以 S3 只在 `retention-600` 之後檢查。
  - 夾限仍在發布點。

**Replacement Criteria**：

- 帳本為了分類，需要回寫紀錄。
- 第二次需要依診斷值改變保留規則。
- 保留範圍需要依情境設定，不再是單一常數。

#### 射擊判定的冷卻保留與命令步（v7.1 `a1-shot-step` 起生效；2026-10-19 使用者核准，D136）

A1 把射擊判定移到命令步 T(S)（protocol-v7 §6）。上一發走退回路徑、晚了 d 個 Tick 判定時，冷卻結束點跟著後移 d；下一發若照命令步的節奏壓著冷卻邊緣，會差 d。冷卻保留就是處理這個差距：

```text
K = min(K_max, cooldownTicks)        K_max = 2 Tick（契約常數，不上 wire）
                                     cooldownTicks = 射手當時武器的權威規則值（CombatRules）
判定 Tick D 時只差冷卻：
  gap = nextAllowedShotTick - D
  gap <= K  -> 保留到 nextAllowedShotTick，在那個 Tick 完整重判一次（不再保留第二次）
  gap >  K  -> REJECTION_COOLDOWN
Client：S = N（處理點擊那一幀所用預測的 epoch 裡已產生的最新命令）
```

- 受影響的只有「上一發的裁決回到 Client 之前就送出的射擊」：Client 收到裁決後，序號閘門改用它的 `resolved_tick＋cooldown` 重新對齊。連射武器（例如冷卻 4 Tick）在 RTT 50～100 ms 時約 1～2 發；d≤K 的延後 d 個 Tick 判定，d>K 的被拒（不扣彈、不造成傷害），之後恢復準時。
- 「取 min」不改變行為：gap＝nextAllowed−D，而 nextAllowed＝上一發的 J＋冷卻、D≥J，所以 gap 不會超過冷卻，K＝min(K_max, 冷卻) 與 K＝K_max 的判定結果永遠相同。min 的作用是讓契約條文 0≤K≤cooldownTicks 對任何武器資料都成立（實作 `Combat.hpp` 的 `ShotHoldLimit`；測試以冷卻 2、4 的規則端到端驗證保留與拒絕）。
- K_max＝2 的依據與盲區：`pv7-checkpoint` 的退回晚到量 52% 在 0～2 Tick；量測沒有「退回」與「壓著冷卻邊緣連射」同時出現的情境，全自動武器上線時重新評估 K_max（v8「多武器與射速」）。

| 原則 | 維持的性質 | 未來修改時要問的問題 |
|---|---|---|
| 射速相關的值只從權威規則推出 | 武器資料（射速、冷卻）改變時不必改程式或契約；Client 的序號閘門只用規則裡的冷卻值做加減 | 是否又寫死了由某個射速推導出來的常數（FireGate 的 guard 2 ms、spread 7 Tick 就是這樣退役的）？ |
| K＝min(K_max, cooldownTicks) | 保留上限永遠不超過冷卻；判定最多延後 K_max 個 Tick；J−A≤W＋K | 新武器需要更大的保留時，是重評 K_max（要量測與使用者同意），不是讓 K 脫離冷卻 |
| 射速的權威在 Match | Match 載入並驗證武器資料，經 `CombatRules` 告訴 Client；Client 的值只是副本 | 是否讓 Client 本機的資料決定射速或冷卻？ |
| 冷卻以 Tick 為單位 | 60 Hz 下射速只能是 3600÷cooldownTicks 發／分；Match 與 Client 不會各自取整出不同值 | 資料若改存發／分，換算與取整規則是否寫進契約？ |
| S＝N | 射線起點和玩家點擊時看到的自己一致 | 前搖或蓄力武器是否需要 S＝N＋k？k 的上限是 W－1＝12 |

**Replacement Criteria**（出現時不再調 K_max，改向使用者提出重新設計）：

- 為了讓某種武器在 clean 網路下權威 Cooldown＝0，需要依武器種類給不同的 K_max。
- 連射武器在常見的網路抖動下，被拒的射擊多到需要第二次保留或排隊才能接受。

### Gateway 的設計意圖與漂移（設計事故，2026-10-15）

- **原始意圖**：Gateway 對各種外部 Client 各自提供介面，內部只用一套抽象協議和 Match
  對接，Gateway 本身無狀態。這個意圖當時沒有寫進上面的 `Design Invariants`。
- **漂移**：v3 起陸續加入 Match 決策狀態的副本與遊戲語意的處理（`lastResolved` 副本、
  動作帳本與重送、30 Hz 節拍、結果退役、拒絕線與已寫出去重），都沒有被當成
  Architecture Delta 提出。2026-10-10 的事故（Gateway 悄悄丟掉已解析的命令）直接來自
  `lastResolved` 副本。
- **根因**：外部是有損、重複送的 UDP，內部協議是「每個命令恰好一次」的可靠串流；
  在兩種送達語意之間轉換必然要去重，去重就需要 Match 的狀態。
- **狀態**：v7.1 的 edge 系列已把 Gateway 改回無狀態邊緣（輸入、動作、結果原樣轉送，刪除 Match
  決策狀態的副本）；約束寫在上面的「Gateway：無狀態邊緣」（D120 核准），由白名單測試守住。完整紀錄見
  [設計事故紀錄](plans/v7.1/Incident/2026-10-15-gateway-state-drift.md)。

### 修改入口與 Ownership

下列路徑相對 repository root。它們是責任入口，不是新的抽象層：

| 責任 | 主要入口 |
|---|---|
| 時序常數與命令／玩家型別（含 `InputSendRate`、`InputSendDrainRate`、相位追蹤常數） | `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp` |
| 共用純移動 | `apps/object_fps_pvp/src/Pvp/Movement.cpp` |
| ACK、重播、窗口、相位追蹤與本機顯示校正 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp` |
| 命令產生路徑（產品與無頭 probe 共用）：elapsed 取樣、每份 snapshot 的樣本 | `apps/object_fps_pvp/src/Pvp/ClientSimulation.cpp` |
| Client 模擬角色：固定步期限、意圖信箱與過期（D30）、呈現副本 | `apps/object_fps_pvp/src/Pvp/ClientSimulationRole.cpp`、`include/RetroFPS/Pvp/ClientSimulationRole.hpp` |
| 本機射擊閘（序號閘門；v7.1 `a1-shot-step` 取代 FireGate） | `apps/object_fps_pvp/src/Pvp/ShotGate.cpp`、`include/RetroFPS/Pvp/ShotGate.hpp` |
| 玩家游標、替代輸入、epoch | `apps/object_fps_pvp/src/Pvp/PvpMatch.cpp` |
| Match 模擬角色、絕對 Tick 期限、owning 交接、ingress 統計 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp` |
| Match IPC（asio）：讀寫、latest-wins Snapshot、action lane（結果重送到 ACK）、心跳回覆 | `apps/object_fps_pvp/src/Pvp/IpcHost.cpp` |
| Client 網路 worker（asio）：token bucket、喚醒、保活、Drain／DrainSimulation | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp` |
| 網路路徑的 10 秒統計 | `apps/object_fps_pvp/src/Pvp/NetworkStatistics.cpp` |
| 遠端接收時間線 | `apps/object_fps_pvp/include/RetroFPS/Pvp/SnapshotTimeline.hpp` |
| 意圖發布、呈現與唯讀觀測整合 | `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` |
| Room／Session、Snapshot 與結果的轉送（`sendUDP`）、背壓 | `apps/object_fps_pvp/gateway/server.go`、`action_delivery.go`、`adapter/adapter.go` |
| runtime link（pv7）：原樣轉送、每位玩家有界 FIFO（溢出丟最舊）、心跳對時、`GatewayDropReport` | `apps/object_fps_pvp/gateway/runtime_link.go`、`action_delivery.go`、`clock_sync.go`、`drop_report.go` |
| Client↔Gateway 時間回聲（Hello／Welcome 的 `ClockSample`） | `apps/object_fps_pvp/gateway/echo.go` |
| 無狀態邊緣的守衛（狀態白名單） | `apps/object_fps_pvp/gateway/state_whitelist_test.go` |
| Gateway ingress 與丟棄計數；送出間隔統計 | `apps/object_fps_pvp/gateway/ingress_statistics.go`；`send_statistics.go` |
| 時間基準、Waiter、角色執行緒（Engine，不屬本產品） | `engine/time/include/engine/time/Waiter.hpp`、`engine/threads/include/engine/threads/RoleThread.hpp` |
| 線上資料契約 | `apps/object_fps_pvp/protocol/client_v9.proto`、`runtime_v9.proto`（規則見 [protocol-v7](protocol-v7.zh-Hant.md)，未改寫的條款沿用 [pv6 契約](protocol-v6.zh-Hant.md) 與 [v5 契約](protocol-v5.zh-Hant.md)；v5 契約第 9 節為 v4 歷史） |
| 日誌格式 | [日誌 Data Contract](log-contract.zh-Hant.md)；`drop_` 鍵的名稱表在 `apps/object_fps_pvp/gateway/ingress_statistics.go` |
| 產品驗收與證據分析 | `tests/object_fps_pvp/`、`build/acceptance/object_fps_pvp/` |

Architecture Delta 是 Client 增加預測／呈現責任、產品輸入從持續狀態變為可確認的
單步命令，再增加 epoch 復原及接收時間線；Protocol v3 是明確的產品契約變更。
Owner 仍為 `object_fps_pvp`，沒有新 Top-level Directory、產品間依賴或公共預測
框架。只調快照頻率無法定義命令身分、確認邊界與停頓復原，因此需要此產品內
垂直修改。現在有一個完成的產品案例，尚不足以把這套策略直接下沉為 Engine API。

可移除性已在副本實際刪除 192 項 owner 內容與 registration 後驗證：中立 Engine、
公共 Gateway 與獨立 clone fixture 仍成立。其他兩個大型 GUI 產品完成無 PvP 的
configure，未完整重建執行；完整範圍見 dev_log。未來若第二個產品出現相同責任，
應先比較實際共同契約，再決定可重用機制；lead、復原門檻及插值策略仍是產品政策。
以上是 v3 的記錄。v7 的產品移除檢查（2026-10-14）刪除 329 檔＋`projects.csv` 1 列，共通層修改 0，
見 [v7 穩定基線](plans/v7/STABLE_BASELINE.md)「L1 與產品移除」。

### 驗證順序與開發成本教訓

本次昂貴之處包含驗證器也在成熟：worker deadline、幀內重複計時、完整 ACK 顯示
端點、bootstrap 相位先後暴露；固定 RTT 代理讓 Hello 超車、hold 誤計、名義
144 FPS 被 VSync 限制，以及 Sent 單點時間無法表示送出區間，又使部分證據必須
重算或重跑。這些修正有實際價值，但過早啟動長測增加了返工成本。

後續同類修改採以下順序，避免以更多長測替代對契約的理解：

1. 先明確四種序列、ACK 三個分支、啟動與重設不變量，以短反例重現失敗。
2. 用確定性矩陣測 FPS／phase／停頓，再跑真 worker 與短 socket／GUI；角色有
   移動不足以證明原始命令被執行或鏡頭平滑。
3. 先驗證診斷語意與分析器。只有分析器變更且舊欄位充分時，先重算現有 trace。
4. 短測穩定後凍結 source／binary／分析器指紋，再跑完整 GUI 及長測；新缺陷先補
   最小回歸，確認受影響範圍後重跑，不重做無關環境盤點。
5. 用腳本聚合大量 trace，交給 Agent 的是摘要與異常片段；等待程序完成本身不需
   反覆讀大段日誌。子任務交付後結束，交接保留必要契約與證據索引。

最終長測仍需滿足已約定的完成條件，節省成本不能靠縮短必要測試、放寬門檻或
把舊版本跑次冒充新版本結果。token 成本回顧見下節。

## 8. Token 成本回顧：最重的是哪一段

### 統計方法與邊界

2026-09-26 依本機 session 用量紀錄核對，截止於當日 17:26:38 JST 的日文 commit
message 回覆；排除本次文件／成本分析。以同一 root session 的 metadata 關係選取
主代理及八個實作子代理，共九份 session、1,676 筆唯一 `response_id` 的
`token_usage_record.usage`。每份逐筆加總均等於最後 thread 累計；不重複加上
`event_msg.token_count` 或 compaction 內嵌紀錄。16 份自動審核 session 另排除，
因此這是可辨識的開發工作量分析，**不是全帳號用量或點數帳單**。

input 包含 cached input，output 包含 reasoning，兩者的子集不能再加一次。
總 input 203,138,250，其中快取 197,871,872、非快取 5,266,378；output 953,371，
其中 reasoning 367,907。這是多次推理反覆讀入上下文的累計，不是專案產生了兩億
token 新文字。快取／非快取／輸出的計費不同，也不能據此精確分攤使用者回報的
約 80% 額度及額外 400 點。[官方用量與定價說明](https://learn.chatgpt.com/docs/pricing)
亦區分這些類別，並說明訂閱額度不只由點數價格決定。

### 分段結果

以下合計主代理與實作子代理；M 為百萬 token，時間皆為 JST。

| 工作區間 | input（含快取） | 其中非快取 input | output | 原始 input+output 占比 |
|---|---:|---:|---:|---:|
| v3 之前：環境、v2、生命週期修正、研究與計畫 | 78.389 M | 2.136 M | 0.375 M | 38.6% |
| **9/25 21:38–9/26 00:14：v3 實作、邊界修正與最終驗收** | **124.220 M** | **2.685 M** | **0.573 M** | **61.1%** |
| 交付後：啟動服務問答與日文 commit message | 0.529 M | 0.445 M | 0.005 M | 0.3% |

**最密集的一小時是 9/25 22:00–23:00**：479 筆模型回應、59.251 M input
（其中非快取 1.303 M）、0.289 M output。當時集中處理永久晚到與 worker deadline、
提前量耗盡重設、frame delta 重複計時、完整 ACK 插值、代理測試問題及重驗。
其次為 23:00–24:00 的 bootstrap、觀測與交接修正及 release 驗收。這是依記錄時間
與工作主題的分段，不代表能把某一個函式或單一測試精確分配到多少 token。

三個長期實作子代理 `authority_commands`、`prediction`、`transport_v2` 從 v2
延續到 v3，累計原始 input+output 分別約 41.90 M、36.68 M、33.87 M；主代理約
87.90 M。它們在整個工作中的占比不能全部算成 v3 成本，也不能把多代理平行節省
的牆鐘時間當作 token 節省。主代理在 v3 的兩個工作 turn 就有 310 筆回應；加上
子代理後，這個區間共有 954 筆，反覆攜帶大型上下文是主要成本放大因素。

### 哪些是必要工作，哪些應該做得更省

必要工作是修正可重現缺陷、守住既定驗收門檻，以及在程式改動後取得有效的新版本
證據。單靠調整 Hz 不足以完成這次任務；原始命令沒有被執行時，Held 甚至會讓
畫面看起來仍能走。確定性矩陣、逐命令診斷及實際呈現量測因此有價值。

但以下成本不能全部合理化為必要測試：

- **長測啟動過早。** 啟動相位、完整 ACK 及觀測語意尚未穩定就跑昂貴驗收，後來
  每修一個邊界又需重驗。短反例、分析器自測與固定步進矩陣應更早成為前置關卡。
- **子代理延續過久、協調與讀取過密。** 多條工作線持續吸收新日誌和大上下文，
  有助平行定位，但放大累計輸入。應以明確交付物結束子任務，下一個問題只交接
  必要契約、異常片段與版本指紋，避免所有代理長期追蹤全部驗收。
- **簡單問答也付出過高上下文成本。** 9/26 01:23–01:26「是否仍需啟動服務」一段
  只有三筆用量回應，卻有 423,006 非快取 input、5,176 output，期間發生上下文
  壓縮；約占主代理全部非快取 input 的 19.5%。它不是編譯或網路測試成本。紀錄
  能證明兩者同時發生，不能將整筆精確歸因為壓縮費用。
- **等待時間不是 token 消耗的同義詞。** 30 分鐘外部程序自行跑測試不等於模型
  連續推理 30 分鐘；消耗來自喚回模型、讀結果、推理、輸出和重新帶入上下文。
  應使用完成通知、必要的進度摘要及批次統計，避免無新資訊的密集輪詢。

本次最重的技術工作確實是 v3 的反覆修正與驗收；可避免的放大因素則是長測排程、
持續擴大的上下文及代理協調方式。後續採第 7 節的驗證順序，保留品質要求，減少
尚未穩定就全面重跑及重複閱讀的成本。這次文件整理只核對既有證據，不再啟動整套測試。

本機彙總：`build/target/_build/test/logs/pvp-v3-cost-review/usage-summary.json`，
只保存分類數字、統計截止與一致性檢查，沒有複製原始對話。原始 session 僅作本次
本機核對，不是 PvP 執行或架構的必要依賴。

## 9. v4 分批升級入口

（歷史：本節記錄 v4 時期；目前 wire 為 v5，見文末 v5 一節。）

2026-09-28 已完成第01–05批與追加完整驗收，**v4已升格穩定基線**，
HTTP join、Ready／Welcome 同步升級並拒絕 v1–v3。前述 v3 移動修復量測與成本
回顧保留歷史證據；不能當成新增射擊或 v4 長測的驗收紀錄。
v4 的時間、ActionId、裁決、有效期、戰鬥狀態及 ownership 統一見
[v4 契約](protocol-v4.zh-Hant.md)；執行順序與各批進度見
[五份計畫](plans/v4/README.md)，新對話交接見 [HANDOFF](plans/v4/HANDOFF.md)。

第 02 批加入產品權威射擊及 Host 有界發布時間 metadata，第 03 批完成 v4 傳輸
及 headless 真網路交付／共存／恢復。第 04 批已接既有槍模與即時武器回饋，
真實雙 GUI 呈現短回歸通過；第 05 批交付整合測試與手動驗收指南。
**三輪GUI共存、60／144Hz各30分鐘及原生X11操作已通過。**
穩定範圍限Linux／X11／Vulkan、同機雙玩家與已驗證的受控網路；
指紋、完整資料、原始失敗及限制見 [穩定基線](plans/v4/STABLE_BASELINE.md)。
啟動、日誌與固定門檻見 [手動指南](plans/v4/MANUAL_ACCEPTANCE.md)，
實際通過／失敗及缺項見 [驗收狀態](plans/v4/ACCEPTANCE_STATUS.md)。
移動基線不調參，命中回溯與可重用同步機制留後續。
原分批交付後，使用者明確追加授權Agent接手完整驗收及通過後升格；現已完成。
手動指南與回報表保留作未來測例參考，不自動開始歷史命中或其他下一階段。

### 第 02 批：權威射擊的實際邊界

```text
Host SubmitActions / QueueActionAcknowledgement (mutex, bounded values)
    | Match ledger + staged window validation; no world mutation on I/O
    v
Authority Tick
    lifecycle -> ACK retirement -> transfer reserved actions / movement
    -> move ALL players -> resolve actions -> owning WorldSnapshot
                              |                  | movement + combat
                              v                  v
                       retained decisions    final publication per Advance
                       (max 32 / player)     -> latest snapshot mailbox
                              |              -> tick / steady-time index (64)
                       GetActionResults              |
                       copy, no eviction             +-> future ResolveActions
                              |                          (trusted age metadata)
                       contiguous ACK -> retire
```

`CombatRules`、`ShotRequest`、`ShotDecision`、`CombatState` 與帳本均屬
`object_fps_pvp`。射線使用 Engine 的 AABB／直立膠囊機制；地板、角色選取、傷害、
期限與冷卻是產品政策。Match 不讀時鐘、不依賴 SDL／Renderer／Campaign。
Host 的時間函式可替換以便確定性短測，正式預設為 `steady_clock::now`。

動作按首次交付 Tick、PlayerId、ActionId 排序；移動等待或重設不跳過射擊裁決。
接納失敗與終局拒絕分開：前者保留原 ID 重試，後者永久保留原答案直到連續 ACK。
帳本同時限制數量與退休下界距離；衝突整批無效果。已退休 ID 不復活，批內彼此
衝突的退休副本仍屬錯誤。HP／冷卻及裁決不受 movementEpoch 影響。

發布錨點只在 owning snapshot 進入 Host handoff 時建立。一次 catch-up 的中間
狀態沒有錨點；TakeSnapshot、重送與零步 Advance 不刷新時間。既有
`SnapshotProduced` trace 仍記錄各模擬 Tick 的產生事件，**不是發布／送達證據**。
單一玩家離開只清自己的帳本及交接，Host Reset 才清整個發布索引。

這次 Architecture Delta 是既有產品 domain／Host API 的局部擴充：沒有新公共
subsystem、依賴方向或 owner 轉移，只在既有 product source list／專屬測試增加
來源。第 02 批結束時 wire 未變，ActionResults 尚未接到 IPC／Gateway；
該批本身不能證明網路可靠交付。細節與短測見
[第 02 批 dev_log](../dev_logs/2026_09_28_pvp_v4_batch02.zh-Hant.md)。

### 第 03 批：v4 動作交付與移動共存

```text
Client game thread                          Client worker
  SubmitShot(tick, yaw, pitch) -- ID ------> immutable request/decision ledger
  Drain().decisions <---- transfer once --- count AND ID distance <= 32
          |                                 contiguous consumed ACK only
          +-- future batch 04 presentation             |
                                                       | UDP v4
                 movement 60 Hz + action/ACK <=30 Hz + Hello ~1 Hz
                                                       v
Product Gateway / Adapter
  authenticated Session -> PlayerId; token bucket 120 packets/s, 120 deep (v7.1 D121; was a fixed window)
  bounded request/result ledgers; rotate <=8 entries; never ACK for Client
                                                       | TCP runtime v4
                                                       v
Product Host: atomic shots + ACK handoff -> Authority Tick
  lifecycle -> ACK retirement -> movement -> shot decisions -> snapshot
                                             |                  |
                                retained Match ledger      latest full state
                                  <=32 / player             movement + combat
                                             |                  |
                        rotate <=8 results at <=30 Hz       latest-wins
                                             +--------+---------+
                                                      v
                                     Gateway -> Client worker -> Drain
```

Gateway 每個 Session 的封包上限是每秒 120 包（v7.1 D121 起改為 token bucket：每秒補 120、容量 120，任何長度 T 的區間最多 120T＋120 包；原為固定 1 秒窗口，Gateway 自己停頓後會把緩衝中的合法封包算進同一窗口而丟掉，K1）；第 08c 批起被限制的包依玩家與種類
（含 Hello）計數，各玩家的總和等於全 Gateway 的 `rate_limited_packets`。

Network Snapshot 是可替換狀態，ActionResults 是必須保存到連續消費確認的終局答案。
兩者各有來源；不把裁決僅存於快照或有丟棄策略的通知佇列。即使首結果或 ACK 遺失，
同 ID 重送仍取得原裁決且不重複傷害。Match 接納前保留裁決容量，Client 未消費
結果也計入有界窗口；movementEpoch 只改移動，不清 HP、冷卻或動作。

Host 同批 shots＋ACK 必須原子驗證；衝突不能先回收舊答案後才被發現。有效 ACK
可以預留新 ID 容量，但實際退休仍在 Tick 邊界。I/O 只交接 owning 值。已開始的
TCP frame 必須完整寫出；尚未開始的完整快照可替換。結果每批最多八筆循環覆蓋，
不只重送最後一筆。Gateway 排程以目前時間及實際寫完時刻重新錨定，避免阻塞解除
後用舊 ticker 時間或舊送出期限補發，侵占既有移動流量預算。

Architecture Delta 為產品 Client／Host／Gateway API、兩份 v4 schema／bindings
及產品專屬 action probe；建置引用由 v3 改為 v4。Ownership 與依賴方向不變，
Engine、公共 Gateway、公共 build／validation 無遊戲分支。只加 fire bool 不能
表達動作去重、期限、終局答案及消費確認，因此需要這個產品內的獨立資料流。
新增 owner 內容仍由產品 source／acceptance 登記；本批未重跑整份刪除副本建置。

同一組產物完成 16 案真 socket 六秒短測，4,724 筆動作均取得並消費裁決，
23,040 筆快照 HP 符合唯一效果。正常共存移動 Actual 100%、執行 P95 45.86 ms；
動作／ACK 最高 30 包／秒，正常所有認證流量最高 91 包／秒。故障解除後最慢新
裁決 385.90 ms；最慢連續 Actual 起點 635.94 ms，至 902.58 ms 已持續至少 250 ms。
Gateway 停頓一秒觸發 118 包限流拒絕，但接受窗口仍 ≤120 且恢復通過；原始失敗
報告與明列拒絕後的重核結果皆保留。

這些是同機 headless 共存／恢復證據，不是 GUI 可見延遲、input-to-photon、實體
LAN 或長測結論。第 03 批當時未接 GUI，也沒有新增正式測試操作入口。重跑方法、故障矩陣
及限制見 [第 03 批 dev_log](../dev_logs/2026_09_28_pvp_v4_batch03.zh-Hant.md)。

### 第 04 批：本機武器呈現與權威裁決分離

```text
SDL native events -> capture/focus/window gate -> one left-button rising edge
                                                      |
Update frame: mouse delta ONCE -> absolute yaw/pitch --+
  |                                                   |
  +-> existing fixed movement prediction/replay        v
  |                                           ClientConnection.SubmitShot
  |                                                   | optional ActionId
  |                                       one local Shoot / visual recoil
  v                                                   v
world camera                                     WeaponViewModel
(predicted/interpolated position)                 own camera + depth layer
                                                      |
Match -> latest snapshot.combat -> HP/cooldown         |
      -> retained ShotDecision -> Drain once          |
                     |                                |
                 hit/reject UI                        |
                     +---------- Render --------------+
                                   |
                         PresentStatus::Presented
                                   |
              movement + read-only weapon observation, same frame ID
```

`ActionId` 配置成功才開始本機動畫；傳輸重送、裁決與移動 replay 不觸發第二次動畫。
本機冷卻讀 Match rules，瞄準角在提交時固定；後座只改槍模 placement，權威射線
仍來自 Match 眼點。HP 只取最新 combat 快照，遲到裁決不倒退 HUD。

產品保留 `WeaponViewModel`，增加不依賴 Campaign 的 presentation frame overload，
選擇 Idle／Draw／Shoot。FBX、三張材質、GPU 資源及一次成功 Idle Render 在 Join
前完成，第一槍無資產載入。建置只納入既有三個呈現來源與 Model／ModelRenderer／
SDL_IMAGE／UFBX；不加入 WeaponController、彈匣或換彈。Match／Gateway 指紋與
第 03 批相同，Engine／公共 Gateway 無變更。

輸入捕捉所在幀不開槍，按住不連發；失焦、Tab 釋放、移動／縮放／最小化視窗清
當幀 shot edge。Tab 釋放所在幀亦不允許後續 click 重新捕捉。動作已提交後的保存
與重送仍由 worker 負責。換身分／離開清本機呈現，movementEpoch 不清。

五項真 GUI 短測通過：30／60／144 FPS、獨立 GPU 圖像、16 秒跨視窗延遲。
實際呈現約 29.9／59.6／142.1 FPS；21 個 SDL 射擊在下一個成功呈現幀出現，
提交至 Render 返回為 1.59–2.62 ms。跨視窗 20／20 事件配對，P50 48.43 ms、
P95 48.84 ms，原 50／80 ms 門檻不變。全部慢幀與生命週期資料保留；不是
input-to-photon，也不是 120 秒／200 事件或長測認證。

Architecture Delta 限於產品 Client 的呈現責任、既有能力依賴與只讀觀測；
核心、Host 和 Gateway 不識別槍模／動畫。沒有新 subsystem 或跨 owner 依賴。
詳細輸入／GPU 證據、初次 sandbox 失敗及限制見
[第 04 批 dev_log](../dev_logs/2026_09_28_pvp_v4_batch04.zh-Hant.md)。

## v5 規劃入口：人物、跳躍與生命循環

2026-09-28：已保存 [五批計畫與進度](plans/v5/README.md)、
[v5 契約](protocol-v5.zh-Hant.md) 及 [交接](plans/v5/HANDOFF.md)。
第 01 批文件／基線及第 02 批人物呈現／短測已完成；第03批建成三角色v5，
2026-10-02以持續相位追蹤＋輸入worker token bucket取代啟動相位對齊（A1）與低幀率守門、
加入連線品質移出後結案（PR #11）。第04批2026-10-02完成完整動作呈現（PR #14）；
第05批2026-10-03完成整合短測與完整驗收（macOS），**v5升格穩定基線**（PR #15、#16）。
進度見 [v5 計畫](plans/v5/README.md)，指紋與範圍見 [v5 穩定基線](plans/v5/STABLE_BASELINE.md)。
以下第02–03批段落與圖示是當時切片的歷史說明（第02批圖示仍為v4玩法）。
原 [v4 穩定基線](plans/v4/STABLE_BASELINE.md) 與驗收證據保持原範圍。

新增責任是產品內生命世代、跳躍／彈匣／換彈／重生狀態及 Client 人物呈現。
lifeGeneration、movementEpoch 與 Session ActionId 分離；重生不清未 ACK 的動作帳本。
Match 保持膠囊權威，Client 使用 PvP 自有模型／骨骼資料；不依賴 v2、Enemy／Campaign
生命週期，也不向 Engine 或公共 Gateway 下沉 FPS 政策。完整 v5 Architecture Delta
見 v5 契約第 7 節；第 02 批實際變化限於下述 Client 呈現切片，owner 不變。

第 02 批先在 v4 玩法下驗證人物 Idle／Jog；第 03 批才同時升三角色 v5；第 04 批
完成動作呈現，第 05 批短整合、交付驗收指南及經授權的完整驗收。
第 01 批實際結果見 [dev_log](../dev_logs/2026_09_28_pvp_v5_batch01.zh-Hant.md)。

### 第 02 批已實作：玩家呈現與位移相位

```text
Match v4 (capsule / HP / movement / shot decisions)
    -> Gateway v4 -> Client worker -> SnapshotTimeline
                                        |
                         sampled position / yaw / tick / epoch
                                        |
             +---------------- PlayerPresentation ----------------+
             | per-player displacement -> signed Jog phase        |
             | Idle/Jog legs + spine_01 pistol upper-body mask    |
             | blended local TRS -> global pose -> body / hair / gun
             +-----------------------+----------------------------+
                                     |
                   existing Engine ModelRenderer / Render
                                     |
                 successful Presented + read-only observations

PvP player JSON / existing female + UAL + pistol assets
    -> product character loader -> pre-Join load / GPU warmup
```

人物不影響權威移動／命中。每位遠端人物依相鄰有效呈現樣本的水平距離推進相位，
而非按鍵或render dt；轉向、貼牆、hold不原地跑。長幀、Skipped、身分／epoch與
時間線重定位建立新步程段，避免恢復時補走未呈現距離。角色／髮髻／槍共用組合姿勢。
固定reference腳底與1.8身高校準後，Jog以3單位／秒參考、stride scale 2播放；
後退反向、側移近似，沒有IK或骨骼命中宣稱。

Client在Join前預備當前兩人產品所需的一個遠端GPU instance；退出重用資源但清身分。
Build只啟用產品既有CharacterPresentationDefinition與新Presenter，沒有把玩家接到
Enemy／Campaign；Match domain未增加Model／SDL，Engine與公共Gateway未改。
專用CPU與GUI驗收仍由產品owner選擇。這是產品Client新增呈現責任及既有介面延伸，
沒有新跨owner依賴、公共subsystem或FPS通用框架。

30／60／144 FPS雙GUI、獨立GPU圖像、原生操作與16秒移動＋射擊短測通過。
同路程相位一致，人物Submit CPU P95約2.68–3.10ms；20／20跨視窗位移事件
P50／P95為44.58／45.50ms，1920／1920 Actual，原門檻不變。
只是本次候選短驗證，沒有覆蓋v4完整性能認證或升格v5。完整校準、架構Delta、
失敗與修正、圖像及成本見 [第02批dev_log](../dev_logs/2026_09_28_pvp_v5_batch02.zh-Hant.md)。

### 第03批：v5命令、生命與動作的交接

```text
Client (Frame)
  Space edge -> fixed 60 Hz command -> shared 3D movement -> prediction
  R / click  -> Session ActionId + lifeGeneration -> local HUD / feedback
        |                         |
        +---- Client worker ------+  input 60 Hz / action+ACK 30 Hz
                  |
        v5 Client protobuf / UDP (whole datagram <= 1200 bytes)
                  |
       Go product Gateway / Adapter
       session validation / bounded delivery / explicit enum conversion
                  |
        v5 Runtime protobuf / framed TCP
                  |
       Runtime Host: bounded handoff / scheduling / backpressure
                  |
       Match (Authority 60 Hz)
       +---------------------------------------------------------+
       | 1. due respawn / reload completion                       |
       | 2. all players: shared 3D fixed movement                 |
       | 3. stable action order: life -> age -> weapon -> hit      |
       |    lethal damage immediately changes target to Dead     |
       +---------------------------------------------------------+
          |                          |
          | latest-wins Snapshot     | retained terminal decisions
          | life / pose / HP / ammo  | Session ActionId / request life
          +-------------+------------+
                        v
                  Client Drain
          +-------------+---------------------+
          |                                   |
 latest own state                       remote same-time segment
 prediction restore/replay              position + life + combat
 latest authority HUD                   timeline -> PlayerPresentation
          |                                   |
  rendered 3D camera                    Idle/Jog now; full actions in 04
```

世界Tick、移動epoch內sequence、Session ActionId與lifeGeneration不是同一識別。
死亡保留連線與中立命令；重生保留PlayerId／Session／動作帳本，只提升生命及移動世代。
因此舊生命射擊／換彈仍可可靠完成裁決與ACK，而新生命的移動、HP、彈匣和HUD不被污染。
動作結果不可套用Snapshot的latest-wins；純移動歷史在重生時失效，另列取消證據。

跳躍位置、垂直速度、接地狀態由同一產品純步驟模擬與重播。Held輸入移除跳躍邊沿；
Client窗口滿仍更新允許的視角／網路，丟棄未分配跳躍沿。生命切換清預測與呈現段，
不清其他玩家時間線或未ACK動作。遠端combat取位置所在區間，本機HUD取最新權威。

新增依賴限於產品既有Collision／Client呈現能力及v5生成型別；Match無模型／SDL／
Renderer，公共Gateway及Engine無FPS分支。第04批才處理完整骨骼動作與素材重定時。
三角色版本、短驗證結果與原始證據入口見
[第03批dev_log](../dev_logs/2026_09_28_pvp_v5_batch03.zh-Hant.md)及[v5交接](plans/v5/HANDOFF.md)。

## v6、v7、v7.1 的聯網架構變更

版本號：vN＝遊戲版本，pvN＝網路協議版本。只列與聯網架構有關的批次；完整進度見各版計畫。

### v6（2026-10-08 升格，pv6）

- 第 09 批（PR #57）：**唯一的 wire 變更**，Client／Runtime 協議升為 pv6（受擊欄位含攻擊者 id、arena 內容 digest），
  見 [pv6 契約](protocol-v6.zh-Hant.md)。
- 第 12 批（PR #60）：Client 的本機射擊閘 `LocalFireGate`，估計最早可裁決的 Tick，避免送出權威會拒絕的射擊。
- 第 16 批（PR #63）：房間上限 2→4 人，維持 pv6。第 17 批（PR #65）：Client 依 Match 選地圖、三個角色的日誌。
- 程序與執行緒結構沿用 v5：主執行緒產生命令、worker 與 IPC 以短休眠輪詢。
- 入口：[v6 計畫](plans/v6/README.md)、[v6 穩定基線](plans/v6/STABLE_BASELINE.md)（2026-10-14 標為缺陷版：
  Gateway 悄悄丟掉已解析的命令，修正在 v7 第 08c 批）。

### v7（2026-10-15 升格，仍是 pv6）

主題是「30 FPS 與網路時序」，交付任務 1～3（D76），不改 wire（D73）。

- 第 02 批（PR #70）：產品庫 `client_simulation`，產品與無頭 probe 改走同一條命令產生路徑（行為不變）。
- 第 03、04 批與 TT-1（PR #71）：每份 snapshot 都進相位追蹤；Client 分成主執行緒、`gyo-client-sim`、
  `gyo-client-net` 三個角色，命令在固定步期限產生；Engine 新增 `GYO::Time`（Waiter、晚醒統計）與 `GYO::Threads`。
- PR #73（P2）：
  - P2-log：網路路徑的 10 秒統計。
  - 第 06 批：Gateway→Client 的結果通道約 15 Hz→30 Hz。
  - 第 07 批：Match Tick 改為絕對期限的 Waiter＋發布通知，`IpcHost` 改用 asio（`gyo-match-ipc`）。
  - 第 08 批：`ClientConnection` 改用 asio，`SendInput`／`SubmitAction` 喚醒 worker。
  - 第 08b 批：Match 的 action lane 與 Gateway 的結果改為事件驅動：距上次送出 ≥1/30 秒立即送，否則設計時器。
  - 第 08c 批：Gateway 不再丟掉已解析的命令（Match 是唯一的判斷者）、拒絕線、依原因計數的丟棄（死信原則 D57）。
  - 第 09a 批：停頓後送輸入不再被卡住（`InputSendDrainRate`）。第 09b 批：延遲消失時撤回多修的相位修正。
  - 第 09 批：FireGate 兩個常數維持 v6 的值（2 ms、7 Tick）並在測試釘住（縮小版凍結，D75）。
- 第 16 批（PR #74）：整合驗收與升格。
- 移到 v7.1：pv7（runtime link 心跳對時、`GatewayDropReport`）、日誌 Data Contract、產品去 SDL、Windows LAN 場次。
- 入口：[v7 計畫](plans/v7/README.md)、[v7 交接](plans/v7/HANDOFF.md)、[v7 穩定基線](plans/v7/STABLE_BASELINE.md)、
  [v7.1](plans/v7.1/README.md)。

### v7.1（pv7）

hotfix：先檢測、再評估；wire 一次設計、一次合併（v7.1 D90）。**2026-10-21 升格為穩定基線**（[v7.1 穩定基線](plans/v7.1/STABLE_BASELINE.md)，D157）；現行 wire 是 pv7。

- PR #78（pv7-line，`pv7-merge`）：Client／Runtime 協議升為 pv7，見 [protocol-v7](protocol-v7.zh-Hant.md)。
  - Gateway 回到無狀態邊緣：輸入、動作、結果原樣轉送，刪除游標副本、拒絕線、已寫出去重、動作帳本與 30 Hz 持有；
    去重與結果重送只由 Match 負責（§2a；約束見第 7 節「Gateway：無狀態邊緣」）。
  - 丟棄回報：Gateway 執行的丟棄依原因以 `GatewayDropReport`（oneof 21）回報 Match（§4）。
  - 對時與回聲：runtime link 的 `ClockSample` 心跳（oneof 20）、Client↔Gateway 的 Hello／Welcome 時間回聲（§3）。
  - 限速：通用層 session 的固定窗口改為 token bucket（每秒 120、容量 120，K1，D121）。
  - 射擊照命令步判定（A1，`a1-shot-step`），FireGate 退役（§6）；骨骼 hitbox 只有欄位（§7）。
- PR #82（after-pv7）：[日誌 Data Contract](log-contract.zh-Hant.md)（與 `GatewayDropReport` 共用名稱表）、
  Engine Trace、死信原則確認、事故紀錄結案、觀測工具、重新評估。
- 入口：[v7.1 計畫](plans/v7.1/README.md)。
