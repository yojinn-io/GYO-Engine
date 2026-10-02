# Object_FPS_PVP 聯網架構與操作

本文件描述 Object_FPS_PVP 的 LAN 聯網與本機移動預測。Linux／Windows Client 經 LAN
加入同一場 Match，Client 即時預測自己的移動，C++ Match 統一裁定移動與地圖碰撞。
這是 GYO 固定 Tick 與外部驅動邊界的實際案例，不是通用 Multiplayer Protocol。

2026-09-28：目前 wire 已升 v4，動作交付與批次狀態見第 9 節及
[v4 契約](protocol-v4.zh-Hant.md)。下列 v3 量測／長測與成本回顧保留為歷史證據；
正式 GUI 射擊與 Mark23 已於第 04 批接入，不將舊驗收視為 v4 的新驗收。

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

以下字符圖中的 Client A、B 各有自己的 worker；兩者共用同一 Gateway／Match。
箭頭表示資料流，不表示底層程式碼依賴上層。

```text
Client A / Client B (each process)
  Main / render thread
    SDL input -> fixed-step accumulator (60 Hz) -> LocalPlayerPrediction
                          |                           |
                          | immutable command window | predicted pose
                          v                           v
    ClientConnection worker (independent 60 Hz)     local camera
      HTTP lifecycle + Hello keepalive                ^
      resend full unacked window <= 12                | reconcile latest ACK
      received snapshot history <= 64 ----------------+
                          |                           |
                          |                           +-> SnapshotTimeline
                          |                               remote render pose
                          v
  HTTP control / UDP Client Protocol v4
                          |
                          v
  Product Go Gateway
    Room / Session -> PlayerId; version / schema adapter
    input windows downstream; latest snapshot upstream to each peer
                          |
                          v
  loopback TCP / Runtime Protocol v4
                          |
                          v
  C++ IPC Host I/O <---- bounded owning handoff ----> MatchRuntimeHost
    frame read/write                                  authority clock 60 Hz
    socket waits stay here                            PvpMatch::Tick
                          ^                           command cursor / epoch
                          |                           StepMovement
                          +---- full Snapshot <-------+

  Return path: Match -> IPC -> Gateway -> Client worker
               -> atomic Drain(state, generation, history)
               -> local reconciliation + remote timeline -> Presented
```

### Ownership 與程式碼依賴

```text
Client（SDL 輸入 / Lobby / GYO Render）
    │ HTTP JSON 控制 / UDP Client Protocol v4
    ▼
Object_FPS_PVP Go 組合層
    ├─ Room、容量、加入資格、Session → PlayerId
    ├─ ObjectFPS Adapter：schema / version / 欄位轉換
    └─ 使用 services/gyo_gateway 的 HTTP / Session / framing
    │ Runtime Protocol v4 / loopback TCP
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

```text
  object_fps_pvp owner
    Client prediction ----+
                          +--> StepMovement(Arena, State, Command)
    Match authority ------+       pure product movement / collision policy
                                  |
                                  v
                            reusable Engine geometry / fixed-tick / rendering

    Product Gateway adapter ----> services/gyo_gateway transport / Session tools
    Product probes -------------> product libraries + read-only observations

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
Network worker 以每秒 60 個、容量 2 的 token bucket 送出完整未確認窗口：含從未送出命令的窗口在下一次輪詢
（≤2 ms）有 token 即送，沒有新命令的窗口在 60 Hz 期限（上次送出＋1/60 秒）重送並保留一個 token 給下一個新命令
（2026-10-02，見 [v5 fix/03](plans/v5/fix/03-a1-missed-frame-starvation.md)）；
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
計時餘數，不補中立命令。新播種的首次 Advance 最多計入一步；pending 為空且
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

來源為產品的 `protocol/client_v4.proto` 與 `protocol/runtime_v4.proto`，彼此不 import。
目前兩者版本都為 4，Client／Gateway／Match 必須一起升級，明確拒絕 v1–v3。Adapter 明確映射兩份生成型別，
Match 核心只接收普通 C++ domain 值。

Client Protocol 包含 Hello、Welcome、PlayerInput、WorldSnapshot、Error、ActionBatch、ActionResults。
Hello 提交 session token；Welcome 回覆 PlayerId、MatchId、tick/snapshot rate 與
arena identity 及 Match CombatRules。PlayerInput 包含 movement_epoch 及 commands，每個命令只有 sequence、移動軸、yaw／pitch，
沒有 Client 指定時間、位置、傷害或擊殺結果。Snapshot 包含 Authority Tick、完整玩家集合
與每位玩家的 last_resolved_command、movement_epoch、contiguous_pending_commands，
另有獨立 combat 集合。動作身分、裁決及消費確認見第 9 節。

RuntimeEnvelope 包含獨立 `protocol_version` 和 oneof：Ready、PlayerJoin、
PlayerLeave、PlayerInput、JoinResult、RuntimeError、WorldSnapshot、ActionBatch、ActionResults。
Ready 宣告 arena identity、60 Hz、snapshot interval 1、容量 2 及 Match CombatRules。
Runtime PlayerInput 的 PlayerId 來自已驗證的 Session 映射，不信任 Client 任選 ID。

Adapter 驗證 Protobuf、版本、finite 數值、移動軸範圍與線上欄位範圍；Match 自行
裁定出生位置、速度、合法視角與碰撞。Arena identity 不符時 Client 拒絕加入。

### UDP v4

最大 datagram 為 1,200 bytes，包括以下 24-byte big-endian header：

| Offset | Bytes | 欄位 |
|---|---:|---|
| 0 | 4 | ASCII `GYOP` |
| 4 | 2 | Client protocol version，現為 4 |
| 6 | 2 | message type：Hello 1、Welcome 2、Input 3、Snapshot 4、Error 5、Actions 6、ActionResults 7 |
| 8 | 8 | Session ID |
| 16 | 4 | UDP sequence |
| 20 | 2 | Protobuf payload length |
| 22 | 2 | Channel，v4 只允許 0：unreliable sequenced |

Header 後面是對應 message 的 Protobuf payload。公共 framing 驗證長度與 Channel，
不解釋產品 message type；PvP 邊界驗證版本與類型。本輪只有 Channel 0；尚無
多 channel、傳輸層 ACK／ack_bits 或 Reliable Ordered；動作層另有消費 ACK。UDP sequence 用半範圍比較處理 32-bit wrap。
Hello 可重送，Welcome 可重發；完整 Snapshot 修復漏掉的加入／離開資訊。

### IPC v4

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
{"request_id":"a-client-generated-unique-id","protocol_version":4}
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
  apps/object_fps_pvp/protocol/client_v4.proto apps/object_fps_pvp/protocol/runtime_v4.proto
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

### 不可破壞的契約

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

### 修改入口與 Ownership

下列路徑相對 repository root。它們是責任入口，不是新的抽象層：

| 責任 | 主要入口 |
|---|---|
| 時序常數與命令／玩家型別 | `apps/object_fps_pvp/include/RetroFPS/Pvp/Movement.hpp` |
| 共用純移動 | `apps/object_fps_pvp/src/Pvp/Movement.cpp` |
| ACK、重播、窗口與本機顯示校正 | `apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp` |
| 玩家游標、替代輸入、epoch | `apps/object_fps_pvp/src/Pvp/PvpMatch.cpp` |
| 權威排程與 owning 交接 | `apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp`、`IpcHost.cpp` |
| 網路 worker、保活、原子 Drain | `apps/object_fps_pvp/src/Pvp/ClientConnection.cpp` |
| 遠端接收時間線 | `apps/object_fps_pvp/include/RetroFPS/Pvp/SnapshotTimeline.hpp` |
| 幀內採樣、呈現與唯讀觀測整合 | `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` |
| Room／Session、轉接與背壓 | `apps/object_fps_pvp/gateway/server.go`、`runtime_link.go`、`adapter/adapter.go` |
| 線上資料契約 | `apps/object_fps_pvp/protocol/client_v4.proto`、`runtime_v4.proto`（第 9 節記錄 v4 擴充） |
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
指紋、完整數據、原始失敗及限制見 [穩定基線](plans/v4/STABLE_BASELINE.md)。
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
  authenticated Session -> PlayerId; fixed-window cap 120 packets/s
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
第 01 批文件／基線及第 02 批人物呈現／短測已完成；第03批已建成三角色v5候選，
功能與網路短測完成，但可見延遲守門尚有啟動相位問題，整批驗收未結案。
第02批下述v4圖示是當時切片的歷史說明。
原 [v4 穩定基線](plans/v4/STABLE_BASELINE.md) 與驗收證據保持原範圍。

新增責任是產品內生命世代、跳躍／彈匣／換彈／重生狀態及 Client 人物呈現。
lifeGeneration、movementEpoch 與 Session ActionId 分離；重生不清未 ACK 的動作帳本。
Match 保持膠囊權威，Client 使用 PvP 自有模型／骨骼資料；不依賴 v2、Enemy／Campaign
生命周期，也不向 Engine 或公共 Gateway 下沉 FPS 政策。完整 v5 Architecture Delta
見 v5 契約第 7 節；第 02 批實際變化限於下述 Client 呈現切片，owner 不變。

第 02 批先在 v4 玩法下驗證人物 Idle／Jog；第 03 批才同時升三角色 v5；第 04 批
完成動作呈現，第 05 批短整合與交付驗收指南。每批完成後停止，完整驗收另行授權。
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
