# 第 04 批：Client 三角色（任務 1、任務 2 的另一半）

狀態：**未開始**。PR 線 P1b（TT-1、04、05，跨層同一 PR）。依賴 P1a 與 TT-1。
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
