# 第 04 批：Client 三角色（任務 1、任務 2 的另一半）

狀態：**進行中**（2026-10-08 開始）。PR 線 P1b（03、TT-1、04、05，跨層同一 PR；第 03 批依 D40 移入）。依賴 P1a、第 03 批與 TT-1。
完成條件之一：第 03 批留下的已知失敗（產品路徑的回復測試，fps 30 的 1 個組合）在本批之後通過。
起因：
- 命令在主執行緒的 Update 內產生。以下三種情況會讓命令停止，或成批產生：
  - OS 的 modal loop：pump 3.0～4.3 s，`live_frames=0`。
  - `nextDrawable` 約 1 s。
  - 失焦時改成 33 ms 節拍。
- 依 SDL 3.4 的契約，swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒，所以正好三個角色（[盤點](INVENTORY.md)第 4、6 節）。

## 目標與範圍

做：

- **模擬角色的執行體整個放進 `client_simulation`**：
  - 內容：執行緒（Engine 的 `GYO::Threads`）、Waiter 的期限迴圈（`GYO::Time`）、意圖信箱、過期判定、呈現副本的發布、樣本佇列的消費。
  - PvpApplication 與所有無頭 probe 都實例化**同一個型別**，C1 因此量到產品路徑。
  - 第 02 批的守衛擴充為也禁止 probe 自己步進 ClientSimulation（不論以 Waiter 或 sleep 驅動）。
- **模擬角色**：
  - 以 Waiter 等絕對期限：取樣時刻加 `secondsUntilNextTick`，含相位 slew。
  - 主執行緒的重設、epoch、生命邊界命令，以 Notify 送達。
  - 停止與 join 必須在 `ClientConnection` 解構之前完成。
  - `RuntimeGap` 改由模擬角色依自己的喚醒間隔（≥0.1 s）發出。
- **交接**：
  - 輸入意圖信箱：軸、yaw／pitch 的增量累加（恰好消費一次）、跳躍上升沿的計數、controls 旗標、sampledAt。
  - 過期的政策依 **D30**：沿用最後的意圖，直到年齡超過 max(100 ms, 3×近期發布間隔)；之後軸回中立、瞄準維持、丟棄跳躍上升沿。宣告最低支援 20 FPS。
  - `ClientConnection` 新增一個給模擬角色的自身樣本佇列，有上限，丟棄會計數。
  - 主執行緒的 Drain 留給呈現、裁決與 FireGate；ACK 判定在本批不動，第 08 批再評估。
  - 主執行緒只發布意圖，並讀取模擬角色發布的呈現副本：Observation、前後狀態與步時間（供插值）、FireGate 用的 ShotTiming。
  - live frame 不再推進預測。
- **只能在主執行緒做的工作，留在主執行緒**：
  - `assets.BeginFrame`／`Update`、UI、`SetRelativeMouseMode`、文字輸入（`PvpApplication.cpp:946-948`、`:990`、`:996`）、render／present。
  - 射擊仍由主執行緒呼叫 `SubmitAction`（已經是執行緒安全）。
- **probe**：
  - 無頭 probe：每個 Client 一個模擬角色；probe 的幀迴圈就是主角色，以宣告的 fps 發布意圖，節拍（`sleep_until`）與 TimerBaseline 不變。
  - GUI probe 的斷言改為觀察步序號，或等待觀察結果（`action_short.hpp:436-481`、`gui_main.cpp:692-699`、`:912-946`）。
  - 主執行緒的停頓故障（`gui_main.cpp:601-602`、`timing_main.cpp:77-80`）改為斷言「不造成 Held，也不造成停頓重設」。
  - 模擬角色的停頓由 L1 的注入時鐘測試涵蓋，不加正式的測試旗標。
  - 重新定義分執行緒後 drain-stall 案例注入的是什麼（只停主執行緒的 Drain，或兩邊都停），逐條核對分析器對這個案例的預期；語意改變的部分列入分析器 v7。
- **紀錄與分析器**：
  - `commands.jsonl` 不新增事件種類。模擬步晚醒的摘要寫到另外的檔案，或寫進 result 欄位。
  - 分析器 v7：只針對編碼了幀語意的規則另立新檔，並帶分析器 id。規則有：seed 夾住豁免（`dropped == frame − 1 tick`）、每幀的 runtime_gap、60 Hz ±2 步的產生檢查、STALL_RULE。新檔含 self-test 與突變；凍結的檔案不動。

不做：
- 不改 `RuntimeLoop`。
- 不開 render 執行緒。
- 網路 worker 的 2 ms 輪詢不動，以保持歸因。
- Match、Gateway、wire 不改。

## 驗收點

- L1（CTest，CI 四平台；使用注入時鐘）：
  - (a) 意圖以 30、60、144 Hz 到達時，產生的命令序號與步時間都相同。
  - (b) 主執行緒停 250 ms 時，模擬仍每步產生命令。
  - (c) 意圖過期時：軸回中立、瞄準維持、跳躍最多一次。
  - (d) 20、25 FPS 與單一 60～120 ms 的長幀時，角色不會中途停步（D30）。
  - (e) seed、epoch、生命邊界的重設。
  - (f) 等待中關閉，在 CTest TIMEOUT 內完成。
  - (g) 多份 snapshot 的批次中，樣本數＝自身 snapshot 數。
  - (h) probe 與產品建構的是同一個模擬角色型別。
- 其他 L1：
  - 既有的 FireGate、預測、時間線測試通過。
  - GUI probe 的 CTest 以新斷言通過；worker_main 不改，照常通過。
  - 分析器 v7 的 self-test 通過。
  - 權威 digest 35／35。
  - Match 的連結閉包不含 SDL（產品的檢查）。
  - 本機 macOS 以 `-fsanitize=thread` 各跑一次新測試與矩陣短測，並記錄（不改 workflow）。
- 突變：
  - 改回逐幀 Advance 時，(a) 觸發。
  - 模擬改為等主執行緒發布才步進時，(b) 觸發。
  - 拿掉過期判定時，(c) 觸發。
  - 漏掉一步時，GUI 斷言觸發。
- 開發跑次（不計次，但保留）：確認凍結分析器在結構上接受 after 的 trace。

## 建議檔位

high；局部 xhigh：意圖與過期的交接、期限與 `phaseShiftSeconds_` 的交互、樣本佇列的順序、模擬角色的生命週期與關閉順序、GUI 斷言的語意。

## Architecture Delta（產品內）

1. 需求：任務 1（v6 D19，經 D27 修訂）與任務 2 的固定步邊界。
2. 問題：命令在主執行緒的 Update 內產生；modal loop、`nextDrawable`、失焦的節拍都會停住命令，或讓命令成批產生。
3. 邊界：Client 內新增模擬角色，以及兩個交接（意圖信箱、呈現副本）；ClientConnection 多一個消費者佇列。
4. 影響：PvpApplication、ClientSimulation、ClientConnection、GUI 與無頭 probe、產品測試、新版本的分析器檔。
5. 依賴方向：`client_simulation → GYO::Time、GYO::Threads`（TT-1）；沒有其他新的 Engine 邊。
6. Ownership：
   - 模擬角色：預測、相位追蹤、命令時點。
   - 主執行緒：事件、輸入取樣、UI、資產、FireGate 與射擊送出、呈現。
   - 網路 worker：socket。
7. 更小的變更不可行：
   - live frame 涵蓋不了沒有 EXPOSED 的 modal loop，也涵蓋不了 `nextDrawable` 阻塞。
   - 另開 render 執行緒違反 SDL 3.4 的契約（`SDL_gpu.h:4202-4218`）。
   - 整個 Update 搬走的話，SDL 視窗操作與沒有同步保護的 AssetManager 會離開主執行緒。

## 停止條件

- 必須改 MovementPhase 常數、FireGate 常數或相位追蹤的定義，測試才能通過。
- LocalPlayerPrediction 為顯示幀寫的啟發式（`frameEvidence_`、`coveredGap`、`PhaseSlewFraction`）需要調整常數。
- 權威 digest 改變。
- 模擬角色需要呼叫只能在主執行緒呼叫的 SDL。
- 凍結分析器在結構上拒絕 after 的 trace（未知種類、協議、schema）。
- GUI 斷言只能靠正式的測試旗標維持。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal） | L1、TSan、開發跑次；L2 在第 05 批 |
| Windows（D3D12） | CI 的 L1；實機在第 15 批的 LAN 場次 |
| Linux、macOS arm64 | CI 的 L1；實機未驗證 |

## 實作（2026-10-08，進行中）

- **模擬角色**（`client_simulation`）：
  - `ClientSimulationLoop`：一步的處理，沒有執行緒、時間由呼叫端給，L1 以注入時鐘驅動。順序：世代或玩家改變時從頭開始 → arena 改變時重選 → 每份 snapshot 的相位樣本 → 最新一份 Reconcile → 依意圖決定輸入 → `Frame`。
  - `ClientSimulationRole`：`GYO::Threads` 的角色執行緒（Interactive）。每一步 `DrainSimulation` → `Run` → 有變化的視窗交給連線 → 發布呈現副本 → Waiter 等下一個期限。執行緒入口自己處理 Runtime Error，主執行緒讀 `Error()` 後明確失敗。
  - 期限＝取樣時刻＋`SecondsUntilNextStep()`。這個值含相位 slew：解「經過時間 − slew＝剩餘時間」，延後時最多拉長到 1／(1−¼)，提前時最多縮短到 1／(1+¼)；換算成期限時無條件進位到 ns。不活動時每 1 Tick 跑一次。
- **交接**：
  - 意圖信箱：軸、**絕對**瞄準、controls、跳躍按下的累計次數、sampledAt。計畫寫的是「yaw／pitch 增量累加」，改為絕對值：主執行緒本來就擁有視角並整合滑鼠，絕對值不會遺失也不會重複套用，畫面的相機與命令也用同一個值。
  - 過期依 D30（常數在 `ClientSimulationRole.hpp`）：近期發布間隔取最近 4 個間隔的最大值。超過當時門檻的間隔是停頓，不算進近期發布間隔（自我檢查時發現：算進去的話，6 秒的 modal loop 之後幾幀內再停一次，舊意圖會撐到 18 秒；已加測試與突變）。
  - 呈現副本：Observation、插值用的前後位置與修正、ShotTiming、步的時刻、步所用意圖的 sampledAt、所見 snapshot 的連線世代。主執行緒在世代不一致時（角色還沒在這個 session 走過一步）不顯示本地預測，首幀的視角從權威位置開始。主執行緒以 `PresentAt(now)` 放到 Update 的取樣時刻：插值與修正隨時間前進，ShotTiming 的 `secondsSinceStep` 加上經過時間（FireGate 依整步計算，所以和下一步發布的值算出同一個 Tick；L1 有核對）。
  - 重設：主執行緒**不送**重設命令（計畫寫「以 Notify 送達」）。世代、玩家、arena 由角色從自己的 drain 判斷，和快照在同一把鎖內取得，沒有跨執行緒的先後問題；epoch 與生命邊界本來就由權威狀態的 reseed 處理；跳躍的清除由 controls＝false 處理。
  - `ClientConnection::SendInput(input, generation)`：角色在 drain 與送出之間若跨過 session 邊界，舊視窗會被丟棄（世代的改變都在同一把鎖內）。
- **PvpApplication**：每個 Update 只發布意圖；Render、HUD、FireGate 讀 `PresentAt` 的結果；主執行緒的 `RuntimeGap` 移除（改由角色的 Advance 依自己的喚醒間隔發出）；live frame 不發布意圖、不推進預測。新增唯讀診斷 `LocalMovementIntentSampledAt()`。
- **probe**：
  - 5 個無頭 probe（timing、action、gameplay、network、quad）每個 Client 一個角色，幀迴圈只發布意圖；結束時先停角色再結束 trace。
  - timing probe 的主執行緒停頓：加上「epoch 不得改變」。
  - network probe 的 6 秒停頓：原本斷言「lead 重建」（只在模擬停住時才會發生），改為斷言 epoch 不變。伺服器端的輸入逾時不再由這個 probe 涵蓋：意圖過期後，角色會送中立命令。
  - drain-stall（action probe）：定義為只停主執行緒的 Drain。角色有自己的佇列，不受影響；這個案例的動作端預期（32 格、未消費的裁決）不變。
  - GUI probe：phase stall 改為「所有停頓（含 250 ms）都不得重設 epoch，也不得出現晚到修正」，並加上兩次觀測之間步數的上下界（取代「重複計入停頓」的檢查）；視窗移動後的停止與死亡位置，改為等待反映該變化的呈現。
  - 守衛（`test_probe_command_path.py`）：probe 不得寫出 ClientSimulation、ClientSimulationLoop、LocalPlayerPrediction、PredictionElapsedTime，也不得自己送命令視窗（傳輸用的 worker probe 除外）；發布意圖的 probe 必須建構 `ClientSimulationRole`，產品也是。
- **依賴**：`client_simulation → client_network`（產品內的新邊；計畫第 5 點只列了 Engine 的邊）、`→ GYO::Time、GYO::Threads`。
- **測試**：
  - `ClientSimulationRoleTests.cpp`：(a)～(g) 與 PresentAt。
  - `MovementRecoveryTests` 的產品路徑改由角色驅動（幀只發布意圖，模擬在自己的期限上走、和 worker 一樣晚醒）。**第 03 批留下的已知失敗通過**。
  - 同一檔中「追蹤 vs 未追蹤」的比較，未追蹤的基準改為和追蹤同一路徑。原本基準一律是 v6 路徑；產品路徑換成角色後，跨架構比較的是不同的量（見下方「延遲」），產品的 144 FPS 起始相位與漂移案例有 7 個斷言因此失敗。測試的意圖是「同一路徑上，追蹤有沒有改善」。
  - 突變目錄：`v7-02-probe-direct-prediction` 改寫為新守衛；`v7-03-probe-no-sample` 的規則被本批取代，換成 `v7-04-probe-steps-simulation`；新增 `v7-04-per-frame-advance`、`v7-04-wait-for-main-thread`、`v7-04-no-stale-intent`。
- **分析器 v7**（`build/acceptance/object_fps_pvp/command_evidence_v7.py`，ID `pvp-v7-commands-1`）：凍結的檔案不動，只重新判定意義改變的規則，結果另存 `command-evidence-v7.json`。
  - runtime_gap：Client 端只來自模擬角色（自己的喚醒間隔 ≥100 ms，或丟掉、擋下的時間）；主執行緒的停頓只出現在呈現間隔或 probe 的幀間隔。Host 的 runtime_gap 另列。
  - seed 夾住豁免：公式不變（`dropped == frame − 1 tick`），但 frame 是模擬自己的間隔，只在 2 tick 以下豁免；更長的表示模擬自己晚醒，算模擬的 gap。
  - 60 Hz ±2 步：只有模擬的 gap 會跳過檢查；主執行緒的停頓（注入或觀測到的）不跳過。
  - STALL_RULE 的 v7 版（文字在檔內）。
  - 新規則：注入主執行緒停頓的跑次，從停頓開始到放開後 1.5 秒內不得有 Held／Neutral，量測窗內不得有 LifeRespawn 以外的 epoch 重設。
  - 回合的種類：timing 與 GUI 用各自的量測窗，有產量檢查；遊戲矩陣的回合（`action-client.json`）用整段執行、沒有產量檢查，乾淨回合（`mode` 為 baseline）中模擬的 gap 是錯誤（和凍結的遊戲分析器同一種處理）。
  - self-test（CTest `object_fps_pvp.command_evidence_v7`）與突變 3 件。執行器（`run_timing.py` 等）不改；第 05 批的宣告寫明兩版都分析。
- **驗收（截至目前，`aa9a342`）**：
  - 全量 CTest 67／67（`-j6`）。
  - 突變 9 件全部 killed：`v7-02-probe-direct-prediction`（改寫為新守衛）與 `v7-04-*` 8 件。`v7-04-per-frame-advance` 第一版讓 CTest 逾時（期限沒有前進，喚醒變得極密，不是預期的失敗方式），改成「每 30 FPS 幀前進一次」後 killed。
  - 權威 digest：v6 最終 tree（`fee92ff`）對本分支，35／35 相同。
  - Match 的連結閉包：ipc、runtime_host、match_domain、engine、gyo_time、collision、net、runtime_v6、protobuf、absl；動態庫只有系統的三個，沒有 SDL。
  - TSan（本機 macOS，獨立的 build／輸出目錄，workflow 不改）：`gyo_time_tests`、`gyo_threads_tests`、角色與 ClientSimulation 的 14 個測試案例，警告 0；矩陣短測 clean-60（TSan 的 Match 與 probe、一般的 Gateway）通過，TSan 報告 0。
  - 開發跑次（一般建置，不計次、保留）：矩陣 clean-30、clean-60 都通過（凍結分析器）；Actual 1677／1680、P50 約 37 ms、P95 約 38 ms、first send P95 約 1.3 ms、30 Tick 佇列和最大 60；分析器 v7 也通過（模擬的 gap 0）。第 03 批的 clean-30 是 Actual 97.6% 的失敗。timing 的 250 ms 主執行緒停頓（60 FPS、30 秒）：通過，Actual 100%，每位玩家正好 1800 個命令，停頓到放開後 1.5 秒內沒有 Held／Neutral，沒有 epoch 重設。
- **觀察**：全量 CTest（`-j6`）中 `object_fps_pvp.worker` 失敗 1 次（「fully acknowledged 60 FPS publication caused excessive sends」）；單獨跑 5／5 通過，再跑一次全量也通過。worker 與連線的傳輸程式沒有改；判斷為既有的負載敏感，記錄於此。

## 待使用者決定

1. **D30 與 (d) 的範圍不一致**：D30 的門檻是 max(100 ms, 3×近期發布間隔)，60 FPS 時就是 100 ms；(d) 寫的是「單一 60～120 ms 的長幀不中途停步」。實測：110 ms 的長幀產生 1 個中立命令，120 ms 產生 2 個。現在的 (d) 只測到 100 ms。
   - (i) 維持 D30，把 (d) 的範圍改為 60～100 ms。
   - (ii) 最小門檻改為 3×(1／最低支援 FPS)＝150 ms，(d) 照原文。和「最低支援 20 FPS」是同一個數字；長停頓時多走約 50 ms。
   - 建議 (ii)：100～150 ms 的偶發長幀（shader 編譯、視窗事件）常見，中途停 1～2 步會被權威執行。
2. **輸入延遲**（記錄，C1 之後與相位追蹤一起決定）：常數不變時，產品路徑「產生→執行」的中位數固定約 37 ms（RTT 0）／47 ms（RTT 20），和幀率無關；v6 路徑是 24～35 ms／31～44 ms。
   - 原因：v6 的相位樣本含「命令的 age」（步邊界到發布），幀相位被 age 吸收，命令實際的 slack 比目標少一個 age；角色在步邊界上產生命令，age 約為 0，同樣的目標下每個命令多出約 v6 的平均 age 的 slack。
   - 從輸入的角度：步使用的是邊界之前最後一個意圖，所以「輸入取樣→執行」比 v6 多出約 1 幀（144 FPS 約 7 ms、60 FPS 約 17 ms）。
   - 要縮短，需要改 MovementPhase 常數或相位追蹤的定義，屬於停止條件；本批不改。

