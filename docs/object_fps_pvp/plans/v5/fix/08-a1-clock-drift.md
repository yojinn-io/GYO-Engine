# 08 Client／Host 時鐘漂移使 A1 對齊隨時間失準（長局）

狀態：**已解決（2026-10-02實作與CPU驗證完成，PR #11送審中；實機report-only GUI冒煙1輪PASS（不計次）；第03批結案驗收未執行）**。2026-10-02發現。Owner：`object_fps_pvp`。
相關：A1本體PR #2（`ff11ee3`）、低幀率守門PR #3（`9cd7f26`）；與[02](./02-a1-cancelled-by-stall-reseed.md)同根：
A1是開環量測，每個epoch只量一次。第03批未結案，第04批暫停。
索引見[README](./README.md)；偶發掉幀重設見[03](./03-a1-missed-frame-starvation.md)。

## 問題成因

**兩端時鐘頻率不同。**Client的固定步由自己的`steady_clock`量幀時間驅動，Host的60Hz由Match機器的
`steady_clock`驅動。兩台機器的晶振有ppm級頻率差，Client與Host的命令節奏因此有固定的速率差。

**A1量一次就不再更新。**`LocalPlayerPrediction::Reconcile`在epoch開始取一次Host的w，決定調整量後只slew一次；
之後沒有任何回饋。速率差讓實際相位以`ppm × 經過時間`線性偏離當初對齊的位置：
- Client較快：命令越來越早到，Host佇列慢慢變長，延遲逐步增加。直到佇列平均多約2格，
  Backlog重設（30Tick合計≥105）才會開新epoch重新量測。
- Client較慢：命令越來越晚到，延遲先變小，lead耗盡後出現Held，最後Starvation重設才重新量測。

**診斷看不到。**觀測只記得當初的決定：相位已偏離時，`startPhaseShiftSeconds`仍顯示已套用，
驗收紀錄也仍是`shift_armed`。

**本機測試結構上測不到。**Client、Gateway、Match同在一台機器、共用一個時鐘，速率差恰為0。
fix/01評估的「幀時鐘漂移」是顯示時鐘對`steady_clock`，不是Client對Host。

## 影響

CPU模擬（本文件所有數字皆為模擬，非實機）：現行master產品碼，60 FPS、±0.5ms抖動，無卡頓，
15分鐘，每設定18種worker／Host相位組合合併；延遲為Actual（產生→執行），契約門檻P50≤50ms。

| Client時鐘 | RTT | A1 延遲P50（第1→5→10→15分） | 15分內重設 | 備註 |
|---|---|---|---|---|
| 0ppm | 20 | 40.3 → 40.3 → 40.3 → 40.3 | 0 | 等同本機測試 |
| 快10ppm | 20 | 40.6 → 43.5 → 46.3 → 49.4 | 0 | 無任何修正 |
| 快20ppm | 20 | 40.8 → 46.2 → 51.8 → 57.5 | 0 | 約第10分鐘超過50ms |
| 快50ppm | 20 | 41.2 → 54.6 → 68.8 → 49.6 | Backlog 13 | 鋸齒；P95 80.0ms |
| 慢20ppm | 20 | 40.3 → 35.7 → 35.8 → 35.8 | Starvation 25 | Held 0.232% |
| 快20ppm | 0 | 30.4 → 35.7 → 41.2 → 47.2 | 0 | |
| 快20ppm | 40 | 51.9 → 55.3 → 62.9 → 68.6 | 0 | |

- 每10ppm約使15分鐘後的延遲多8ms；20ppm以內15分鐘不觸發任何重設。
- 加上每60秒一次75ms卡頓：RTT 0時每次卡頓都stall reseed，A1被取消後不再對齊（第15分鐘對齊0%）；
  RTT 20／40時每次卡頓都引發Starvation重設、A1重新量測，漂移被順帶清掉，代價是Held約0.5–0.6%、
  每分鐘一次重設。長局中A1的效果取決於是否碰巧發生重設。
- 實際速率差未實測：一般硬體在數十ppm以內；兩端都有NTP校頻時可能很小，但macOS的`steady_clock`
  不受NTP校頻。使用者決定不做實測：邏輯缺陷不因實測數值而消失，實測只能說明嚴重程度。
- 不受影響：正確性（Actual／Held／HP／彈藥／生命週期）；wire、門檻、lead、60Hz、插值。

## 如何復現

CPU模擬，不需GUI或閒置機器（虛擬時鐘，CPU負載不影響結果）。模擬器是評估用暫存碼，未進產品／CTest，
原始碼與輸出保存在git忽略的`build/target/_build/test/logs/pvp-v5-drift-sim-20261002/`。

```bash
cmake --preset test -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build build/target/_build/test --target engine gyo_collision
E=build/target/_build/test/logs/pvp-v5-drift-sim-20261002
c++ -O2 -DNDEBUG -std=c++20 -fexperimental-library \
  -Iapps/object_fps_pvp/include -Iengine/base/include -Iengine/io/include -Iengine/asset/include \
  -Iengine/runtime/include -Iengine/collision/include -Ibuild/target/_build/test/_deps/nlohmann_json-src/include \
  $E/Sim.cpp apps/object_fps_pvp/src/Pvp/{Arena,PvpMatch,ShotQuery,Movement,LocalPlayerPrediction,MatchRuntimeHost}.cpp \
  apps/object_fps_pvp/src/Gameplay/Player/PlanarMovement.cpp apps/object_fps_pvp/src/Collision/CharacterCollision.cpp \
  build/target/_build/test/engine/libengine.a build/target/_build/test/engine/collision/libgyo_collision.a -o $E/sim_drift
$E/sim_drift A1 20 20          # A1、Client快20ppm、RTT 20：每分鐘一列
$E/sim_drift U 20 20           # 同條件但不對齊（剝除Host等待）
python3 $E/runall.py $E/out_steady.txt && python3 $E/summary.py $E/out_steady.txt 20
```

- 用法：`sim_drift <A1|U> <ppm> <rttMs> [卡頓間隔秒 卡頓毫秒] [Host相位數]`，ppm為正表示Client較快。
- 每列：`lat50／lat95`延遲、`slack50／slack5`Host收到→執行、`held`、`rS／rB`Starvation／Backlog重設、
  `reseed`、`applied`觀測顯示已套用的幀比例、`queued`Host佇列平均。
- 原始輸出：`out_steady.txt`（無卡頓）、`out_hitch75_60s.txt`（每60秒75ms卡頓）。

## 解決方案

### 提案（2026-10-02，**使用者已核准**；已實作，見文末「實作」）

同時處理[02](./02-a1-cancelled-by-stall-reseed.md)、[03](./03-a1-missed-frame-starvation.md)、本文與
[09](./09-covered-gap-stuck-late.md)：**輸入worker改為token bucket＋閉環的持續相位追蹤（CT）**。
兩者缺一不可：只做CT時10%掉包的重設是A1的14倍（見「已評估、未採用」）。原型只存在於模擬器，產品碼未改。

**1. Host：移動餘裕樣本（取代epoch首窗口等待）**
- `MatchRuntimeHost`逐玩家、逐epoch／life記錄每個序號第一次收到的時刻。每個Tick對新裁決的序號：
  已收到者（Actual）樣本＝裁決時刻－首次收到；未收到者（被替代）記下裁決時刻（最多64個），
  之後才到的命令產生負值樣本＝－（收到時刻－裁決時刻）。
- 每份發布的Snapshot帶「上次發布以來最小的樣本」及其序號。Match不讀取；Leave、換epoch／life時清除。

**2. Client：持續相位追蹤（取代A1、HostLate規則與低幀率守門）**
- 記錄每個真實命令發布時的年齡（lag，同A1的首窗口已過時間）。對「目前相位穩定後才產生」的命令，
  誤差e＝樣本＋lag－2 Tick－4ms：與A1相同的量（在固定步邊界送出的命令，lead之外餘裕4ms），目標不變。
- 播種（epoch開始或stall reseed）後：滿8個幀間隔且有8個樣本即首次決定，取P90；|e|＞0.5ms才修正。
- 追蹤：最近240個樣本（約4秒）的P90，|e|＞2ms才修正。P90對準「立即送出」的命令，等同A1的語意；
  現行worker下用最小值會追著到達時間的散布跑、延遲多一幀，用中位數延遲多約7ms（模擬）。
- 遲到：連續2個負值樣本立即以其中最晚者修正，不等完整窗口（處理09）。
- 每次修正上限±2 Tick，沿用A1的slew（每幀最多該幀時間的25%）與追趕容量夾限；
  slew完成前產生的命令，其樣本不再計入。Host沒有回報樣本時相位維持播種時的狀態。

**3. 輸入worker：60/s token bucket（容量2）**
- 窗口含從未送出的命令時，下一次輪詢（≤2ms）只要有token就送出；沒有新命令的窗口在既有60Hz期限
  （上次送出＋1/60秒）重送，但須保留一個token給下一個新命令。平均送出率不超過每秒60包，
  最多連送2包，不補送過期批次，worker不產生命令。
- 只放寬期限（不加token bucket）會在40／52 FPS、144Hz達每秒80／98.6／84包，加上動作30Hz會超過
  `services/gyo_gateway/session`的每秒120包上限，故不採用。容量1.5在30 FPS與vsync掉幀時退步。

**4. Wire（v5候選，三角色同批）**
- `client_v5.proto`與`runtime_v5.proto`的`PlayerState`：刪除`epoch_start_wait_us = 16`（保留編號），
  新增`optional uint64 movement_slack_sequence = 17`、`optional sint32 movement_slack_us = 18`。
  兩者同時存在或同時不存在；|us|≤1,000,000；1≤序號≤`last_resolved_command`；不合規整份Snapshot無效。
- Input方向不變。產品Gateway（`adapter.go`）照現行方式驗證並逐欄複製；`IpcHost`編碼、`ClientConnection`解碼。

**5. 診斷與驗收器**
- `LocalMovementObservation`的`epochStartWaitSeconds`／`startPhaseShiftSeconds`／`startPhaseSkip`改為追蹤診斷：
  狀態（取得中／穩定中／追蹤中）、最近一次決定的誤差、修正次數、遲到修正次數、最近一次修正量。
- `start_phase_record.hpp`、`start_phase_evidence.py`及其測例改為記錄每epoch／life的首次決定時間與誤差、
  修正與遲到修正次數、量測窗口內各狀態的時間；不再有「一次決定」與`cancelled_by_reseed`。

**6. 須核准的契約變更**（其餘不變：lead 2、4ms目標語意、slew、60Hz、插值、門檻、Starvation／Backlog、每秒120包）

| 項目 | 現行 | 提案 |
|---|---|---|
| 啟動相位對齊（§1） | A1：每epoch量一次、HostLate、低幀率守門（1.1／1.06 Tick） | CT：持續追蹤，同一目標語意；不需低幀率守門 |
| 移動worker（§1、network-architecture） | 獨立60Hz期限送出最新窗口，首窗口立即 | 60/s token bucket；新命令立即，重送保留一個token |
| PlayerState（§2） | 可選`epochStartWaitMicros` | 可選`movementSlackSequence`＋`movementSlackMicros` |
| StartPhaseSkip（§1） | HostLate／FrameRateBelowTick／CancelledByReseed | 刪除，改為追蹤診斷 |

**7. Architecture Delta**
1. 需求：02、03、08、09的觀測問題。現行結構的問題是開環量測與送出相位鋸齒，不是缺少新層。
2. 邊界：產品Client／Runtime契約的一個Host時序觀測欄位替換；ClientConnection的送出規則。
3. 影響：只有`object_fps_pvp`的Client、Match、產品Go Gateway與專用驗收；Engine與`services/gyo_gateway`不改。
4. 依賴方向不變，沒有新的跨owner依賴。
5. Ownership不變；Host多一份產品內的逐序號收到時刻帳本（取代epoch首窗口帳本）。
6. 較小替代不足：見下表。CT同時刪除A1的首窗口量測、HostLate、低幀率守門與撤回／恢復狀態機。

**8. 實作分批（檔位依「變速箱」）**
1. Host餘裕樣本＋wire＋Gateway／IpcHost／ClientConnection解碼與驗證及測試（high）。
2. Client的CT取代A1與守門，改寫`PredictionTests`的A1段與`MovementRecoveryTests`（控制器核心xhigh，其餘high）。
3. ClientConnection token bucket，同步更新測試中的worker模型與驗收mock（high）。
4. 診斷、驗收紀錄器與分析器及測例；契約與網路架構文件（medium）。
5. 全套CPU驗證（CTest `-L pvp`、Go unit／race、Python）；以產品碼重跑本模擬器全設定；
   report-only GUI冒煙由使用者另行安排（medium）。計次GUI與25案矩陣仍屬第03批結案。

### 模擬結果（提案 vs 現行）

15分鐘、18組相位；延遲為RTT 20下0ppm／Client快20ppm的P50（ms）；Held為該情境全部ppm×RTT的平均；
重設為全部ppm（0、±20）×RTT（0／20／40）的總和。A1／U＝現行產品（現行worker），P＝提案。

| 情境 | A1 延遲 | A1 Held% | A1 重設 | U 延遲 | U 重設 | P 延遲 | P Held% | P 重設 |
|---|---|---|---|---|---|---|---|---|
| 穩定60 FPS | 40.3／49.4 | 0.05 | 55 | 52.1／63.8 | 44 | 40.3／40.8 | 0.00 | 0 |
| 每60秒53ms卡頓 | 40.7／41.1 | 0.42 | 992 | 60.2／65.7 | 448 | 39.6／40.3 | 0.12 | 76 |
| 每60秒75ms卡頓 | 40.8／41.0 | 0.42 | 1559 | 49.2／57.4 | 1332 | 40.4／40.5 | 0.18 | 71 |
| 卡頓風暴（每400ms卡45ms，60秒） | 40.8／46.5 | 1.35 | 1322 | 65.7／71.3 | 145 | 40.6／40.9 | 0.43 | 18 |
| 開局卡頓（smoke-2重建） | 40.0／49.9 | 0.11 | 170 | 44.8／55.7 | 150 | 39.8／40.2 | 0.01 | 0 |
| 30 FPS | 38.5／46.5 | 5.22 | 703 | 同A1 | 同A1 | 32.0／32.5 | 0.01 | 0 |
| vsync 50 FPS | 51.9／58.2 | 0.63 | 105 | 同A1 | 同A1 | 38.1／38.4 | 0.00 | 0 |
| vsync 55–58 FPS | 41.2／46.6 | 0.70 | 3336 | 52.0／63.2 | 103 | 38.6／40.6 | 0.00 | 0 |
| 144Hz | 45.7／54.8 | 0.00 | 54 | 55.9／64.7 | 1 | 44.5／45.5 | 0.00 | 0 |
| 10%輸入掉包 | 44.3／54.8 | 1.73 | 623 | 54.9／66.2 | 107 | 40.4／40.8 | 0.86 | 13 |
| 5%雙向掉包 | 43.9／49.5 | 0.89 | 890 | 52.2／62.8 | 109 | 40.3／40.8 | 0.22 | 3 |
| Host晚醒0.1–4ms | 40.5／49.9 | 0.06 | 72 | 52.6／63.6 | 47 | 38.5／39.0 | 0.00 | 0 |
| 網路抖動0–5ms | 43.2／51.9 | 0.08 | 94 | 54.4／65.4 | 59 | 40.5／41.0 | 0.00 | 0 |

- 每秒輸入封包：P在所有情境≤60（現行約59）。修正次數：0ppm約每局1次，±20ppm約9次，±100ppm每20秒1次。
- 30分鐘±100ppm：延遲40.3–40.8ms不變，零Held、零重設。
- 對A1沒有任何一格Held多於0.05個百分點或重設多於2次。唯一較差的是Client時鐘慢20ppm時的延遲（例如穩定60 FPS、RTT 0：
  A1 23.9 vs P 27.8）：那是A1隨漂移吃掉自己的安全餘裕、接著Starvation重設的過渡，P維持同一目標。
- 對未對齊：P的延遲低10–25ms；10%掉包與卡頓風暴的Held與未對齊相近（0.86 vs 0.88、0.43 vs 0.41），
  這是對齊本身（較小餘裕）的取捨，與已核准的A1相同。

### 已評估、未採用

| 方案 | 結果 |
|---|---|
| O2W（reseed後重量，見02） | 無卡頓的漂移情境reseed為0次，修不到08 |
| 只改worker（token bucket，不做CT） | 修好03（vsync 55–58 FPS重設3336→40）與低幀率Held，但漂移、卡頓重設（53ms 579、風暴1093）仍在 |
| 只做CT（現行worker，含低幀率守門） | 修好漂移，但10%掉包重設592（A1 42，RTT 0）、vsync 55–58 FPS與A1相近（630 vs 663，RTT 20） |
| CT＋晚端下限（P5餘裕≥8／16.7／20.7ms） | 乾淨情境延遲多約5ms，掉包重設仍高於A1；改worker後不需要 |
| worker只放寬期限（無token bucket） | 每秒達98.6包，會撞上Gateway每秒120包上限 |
| 3c驗收器預熱、3b出生暫停、O2H（見02） | 只修測試情境或已否決 |

原型與輸出：`build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/`（git忽略；`ct/`為產品碼覆蓋層，
`drift/`為模擬器、`build.py`、`sweep.py`、`final_compare2.py`與`out_final_bucket.txt`等輸出）。

### 實作（2026-10-02）

- Host：`MatchRuntimeHost`以逐序號的slack帳本取代epoch首窗口帳本（`TrackSlack`），每次Advance只讀一次時鐘並與發布參考共用；
  `JudgeConnectionQuality`與`PvpMatch::GetMovementQuality`實作[10](./10-connection-quality-eviction.md)。
- Client：`LocalPlayerPrediction`以`TrackPhase`／`Correct`取代A1、HostLate與低幀率守門；`LocalMovementObservation`改為
  `phaseTracking`／`phaseErrorSeconds`／`phaseCorrectionSeconds`／`phaseCorrections`／`phaseLateCorrections`；
  `PendingInput()`帶`observedAuthorityTick`。常數在`Movement.hpp`（`MovementPhase*`、`InputSendBurst`、`ConnectionQuality*`）。
- 輸入worker：`ClientConnection`的token bucket；HUD在連續不合格時顯示剩餘秒數。
- Wire：兩份proto、Go bindings以pinned protoc 36.2＋protoc-gen-go v1.36.11重新產生；產品Gateway的`adapter.go`、`server.go`、
  `runtime_link.go`（合併取最大observed tick、白名單加`PlayerEvicted`）；`IpcHost`編碼／解碼與移出通知。
- 驗收：`start_phase_record.hpp`與`start_phase_evidence.py`改為相位追蹤紀錄（檔名沿用）；`run_timing.py`摘要改為
  「Round N phase tracking」；worker／network probe的最大封包與送出規則檢查改為token bucket。
- 與原型的唯一偏差曾在寫測例時出現：一度改成「slew期間不收樣本」，以產品碼重跑模擬發現53／75ms卡頓重設由約2次增為52–101次
  （落後超過一次修正上限2Tick時，必須等新命令累積遲到樣本），已改回原型語意。
- 以產品碼重跑本模擬器全部171組設定（token bucket worker）：除卡頓風暴外162組與核准的原型逐位元相同；風暴在產品中約90秒
  因被替代比例超標而被移出（fix/10的預期行為），之後不再裁決，故比例不同。重設總數173（原型181）。

## 驗證

- 現象：上表與`out_*.txt`；0ppm各RTT 15分鐘延遲不變、零重設，與本機實機結果一致。
- 修正後須在同一模擬器重跑全部設定，並補產品單元測試；計次GUI與25案矩陣仍屬第03批結案範圍。

## 殘留風險與後續

- 模擬的顯示嚴格60Hz、抖動為合成；單一局的延遲以16.7ms階梯跳動（60 FPS的幀量化），表中平滑是18組合併取中位數。
- 在修正前，任何長時間對局（含第05批30分鐘長測）在兩台實體機器上都可能呈現延遲漂移或週期性重設。
