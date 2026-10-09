# PvP v7 分批計畫與進度

更新：2026-10-09。Owner：`object_fps_pvp`。
**狀態：P1a、P1b 完成**（PR [#70](https://github.com/yojinn-io/GYO-Engine/pull/70) `f3d176d`、PR [#71](https://github.com/yojinn-io/GYO-Engine/pull/71) `660310e` 已合併；第 01～05 批與 TT-1 完成）；**P2 進行中**（PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)：P2-log 完成；第 06 批等使用者決定）。

v7 處理 v6 留下的核心問題：單執行緒主迴圈與以畫面幀為節拍的時序（v6 D19、D21，以及 LAN 聯機測試）。另外加入音效、解析度與 FPS 的選擇、產品與 SDL 的隔離、日誌補強。

規劃方法（2026-10-08，ultracode）：
1. 唯讀盤點：9 個區域 agent，加 1 次 xhigh 對抗式完整性檢查，結果是 [盤點](INVENTORY.md)。
2. 研究 2 個：SDL 3.4 的執行緒與計時器；各標準庫、asio 與 Go 的等待原語。
3. 分批方案 3 個，由 1 位評審合成，再做 1 次對抗式檢查。
4. 使用者決定 D27～D34，見[交接](HANDOFF.md)。

先讀[交接](HANDOFF.md)、[盤點](INVENTORY.md)與指定批次。
版本號：v7＝遊戲版本，pvN＝網路協議版本（v6 D20）。

Engine 的工作放在 Engine 計畫夾（v6 D1、D12），文件匿名。刪除本產品後，那些紀錄仍然完整：

- [時間、執行緒與 Trace](../../../architecture/plans/time-threads-trace/README.md)：TT-1、TT-2。
- [輸入與呈現](../../../architecture/plans/input-and-present/README.md)：IP-3～IP-5（SDL 隔離、顯示與設定的機制）。
- 音效：Engine 計畫夾在 P6 開始時建立（AU-1）。

## 已決定的任務

| # | 任務 | 來源 |
|---|---|---|
| 1 | 主執行緒（事件＋畫面）、模擬、網路三個角色分離。依 SDL 3.4 的契約，swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒（`SDL_gpu.h:4066-4094`、`:4202-4218`；`SDL_video.h:1184`），所以正好三個角色，不另開 render 執行緒 | v6 D19（IP-2 的縮放停頓、`nextDrawable` 停頓） |
| 2 | 移動命令在固定步邊界產生，不再等畫面幀；相位追蹤處理每一份 snapshot 的樣本（目前每幀只用最新一份，30 FPS 約一半樣本到不了）。無頭 probe（矩陣、timing、quad、network）也自己重做了同樣的「命令跟著幀」（`gameplay_action.hpp:106-161`），所以要經產品的接縫一起處理 | v6 D21、第 07 批後續的 A/B 調查 |
| 3 | 網路路徑的短休眠輪詢改為事件驅動：Match IPC 每輪 1 ms（`IpcHost.cpp:327`；另有 `:336` 的 5 ms accept、`:339`／`:344` 的 10 ms future 輪詢）、Client 網路 worker 每輪 min(2 ms, 到下一次輸入期限)（`ClientConnection.cpp:594-597`）。實測成本在現有主機上只有 1～3 ms，屬推測的壓力；觀測到的是 SendInput／SubmitAction 不會喚醒 worker。v6 第 16 批的「4 人 IPC 寫出負載」沒有觀測到（合併 0 次），不作為證據 | v6 D21；[盤點](INVENTORY.md) |
| 4 | runtime link（Gateway↔Match）的心跳對時：偏移、RTT、漂移，只作量測與診斷。移動以 Tick 與序號裁決；射擊的 Expired／InvalidReference（250 ms，`PvpMatch.cpp:281-286`）與逐出（參考年齡中位數 160 ms，`MatchRuntimeHost.cpp:86-101`、`:375-389`）用本機單調年齡，**對時結果不得用在這兩處**。屬 wire 變更：目前收到未知訊息會斷線、版本要求完全相等（`IpcHost.cpp:210`、`:246`；`runtime_link.go:48-51`、`:284-291`），所以必須升 pv7 | 2026-10-06 使用者決定；D32 |
| 5 | 音效：Engine 新增 audio 模組，本產品加入射擊、受擊等聲音 | 2026-10-06 使用者決定（「有聲音遊戲才算完整」） |
| 6 | 解析度、視窗模式與 FPS 上限由玩家選擇。D41（2026-10-09）：加入 FPS 上限；v7 以命令列選項提供，遊戲內的設定 UI 與設定的保存移到 v8。Engine 提供機制，本產品決定政策；目前寫死 1280×720（`PvpApplication.cpp:127`、`:895-896`、`:1029` 的 HUD 參考值、`PvpApplication.hpp:96-97`） | 2026-10-06 使用者決定 |
| 7 | 產品不再直接依賴 SDL。完成條件以 SDL 符號判定（D33）：產品、驗收 probe、產品測試中 `SDL_*`、`SDLK_*`、SDL 的型別與 include、Engine 的 `*Native.hpp` 都是 0 件（CMake 的後端元件選擇除外）。只看 include 不夠，因為 `SdlPlatform.hpp`、`SdlInput.hpp` 本身 include SDL，`PvpApplication.hpp` 也暴露 `SdlPlatform&`、`SdlGpuRenderDevice&`。現存使用的位置見[盤點](INVENTORY.md) | 2026-10-06 使用者決定；D33 |
| 8 | 日誌與診斷補強：v6 LAN 聯機測試發現的缺口，建在 Trace 之上（各缺口的 owner 見「v6 LAN 聯機測試的觀察」） | 2026-10-08 使用者決定（「下沉到 v7 的子系統中」） |

## Engine 子系統的處理（D27）

依觀測到的壓力分配（[盤點](INVENTORY.md)），都屬 Architecture Delta，各批說明 AGENTS.md §3 的七點。依賴由下而上；每個子系統與它的第一個消費端放在同一個 PR。

| 子系統 | v7 的處理 | 第一個消費端（PR） | 壓力 |
|---|---|---|---|
| Time | `GYO::Time`（Base 層），**Engine 的時間基準**（D29）：單調時鐘宣告、Waiter（等到期限或被通知）、晚醒統計、各平台後端。`RuntimeLoop` 改用它，只換時鐘來源。固定步沿用 `FixedTickRuntime`（累加器的格點已是絕對的；LAN 量到 60.00002 Hz），缺的是等待與晚醒量測 | Client 模擬角色（P1b） | 強：主要晚醒在 Match Tick 的相對 cv 等待（`MatchRuntimeHost.cpp:288-296`）。macOS 的 std 與 SDL 等待都有 25～50% 的計時器合併 leeway |
| Threads | `GYO::Threads`（Base 層，依賴 Time）：最小的角色執行緒，提供名稱、協作停止、優先級提示。不做執行緒池與排程器 | Client 模擬角色（P1b）；P3 時加入 Engine 自己的 Trace 寫檔執行緒 | 中：執行緒的 owner 各自開執行緒，停止方式有三種；產品為 `std::jthread` 加了 AppleClang 的 `-fexperimental-library` |
| Channels | 不先統一，各自實作（語意不同：Trace 多生產者有上限、Audio 即時 SPSC、意圖信箱只留最新值） | — | 弱 |
| Net transport | 留在產品內：任務 3 以產品內的 asio 完成。C++ 消費端只有本產品，Gateway 是 Go | — | 弱：Go 端已是事件驅動；asio 由產品取得（`apps/object_fps_pvp/CMakeLists.txt:8-10`） |
| Trace | `GYO::Trace`（精簡）：有界紀錄 sink、以通知喚醒的背景寫檔、Base 的日誌 facade。不做錄製重播 | 任務 8 的紀錄（P3） | 強：v6 LAN 的關鍵數字都要從 movement trace 離線重建 |
| SDL 隔離 | 擴充 platform／input（IP-3、IP-4） | 產品與 probe 的遷移（P4） | 強：產品、7 個 probe 檔、2 個產品測試、Engine 測試 |
| Display | 擴充 platform（IP-5）。工具不是第二個使用者：ui_editor 的大小與像素密度由 ImGui SDL3 後端吸收，preview 已停用；第二個消費端是驗收 probe 與產品測試的唯讀查詢 | 解析度與 FPS 的選擇（P5） | 弱（主要來自任務 6） |
| 使用者設定的保存 | `engine/io` 的原子寫入＋每位使用者的目錄（IP-5）。已有兩份 temp＋rename 的原子寫入（`tools/ui_editor/src/FileService.cpp:54-94`、`engine/render/shaders/pipeline/src/main.cpp:64-72`）。鍵與值的意義、版本與驗證規則屬各產品的 Data Contract。D41：設定的保存隨設定 UI 移到 v8，v7 沒有消費端，所以不做 | v8 的設定 UI | 弱 |
| Audio | `GYO::Audio`（中立混音器，自己維護樣本計數並依時間戳排程）＋SDL 後端。SDL 3.4 沒有時間戳排程的 API | 音效（P6） | 無（使用者決定的功能） |

- **framework 層的現況**：
  - 現在沒有 framework 層。頂層 `framework/` 只是空的佔位，2026-09-01 在 `b63d153` 刪除；`docs/architecture.md` 也寫明不引入 umbrella framework。
  - framework 的定義：Engine 與 Game 之間、支持多個遊戲的公共層，承載遊戲的組合方式（骨架、角色編排、Client 預測迴圈、權威主機），由它呼叫遊戲。Engine 則提供不規定遊戲怎麼組合的機制。
  - 啟動條件：第二個遊戲以相同方式組合 Engine 機制時（例如複製 `ClientSimulation`、`MatchRuntimeHost`、三角色編排），從兩個實例中抽出。不用它的遊戲必須照樣成立。
  - v7 的角色編排集中在 `client_simulation` 與 `MatchRuntimeHost`，保留日後抽出的路。
- **Job（工作分割、平行運算）**：目前沒有壓力，v7 不建。方向：建在 Threads 之上，worker 數＝核心數扣掉角色執行緒，第一個消費端出現時再做（可能是非同步資產載入）。
- 錄製與重播成立的前提：時間、輸入、網路都經過 Engine（Time、Input、產品內的傳輸），由 Trace 記錄。v7 不做。

## 進度

### 本產品的批次

| 批次 | 文件 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|---|
| 01 | [盤點](INVENTORY.md)、本文件、[交接](HANDOFF.md) | ultracode（規劃）→ medium（文件） | 完成，PR [#69](https://github.com/yojinn-io/GYO-Engine/pull/69) 已合併（`a7cba38`） | 計畫、盤點、README 更正、v7 交接（D27～D34）、v6 的 v1.1.0 紀錄、Engine 計畫夾 |
| 02 | [ClientSimulation 接縫](02-client-simulation-seam.md) | high（等價性局部 xhigh） | 完成，隨 P1a 的 PR [#70](https://github.com/yojinn-io/GYO-Engine/pull/70) 合併（`f3d176d`） | 產品庫 `client_simulation`，行為不變（golden）；產品與 4 個無頭 probe 改走同一條命令產生路徑 |
| 03 | [每份 snapshot 進相位追蹤](03-per-snapshot-phase.md) | high（樣本順序與死區局部 xhigh；xhigh 審查 1 次） | 完成（D40，與第 04 批一起在 PR #71 合併，`660310e`） | 同一批中每份含自己的 snapshot 都交一個樣本；一幀一份時與 v6 逐位元組相同 |
| 03a | [相位追蹤的小量測](03a-phase-only-measurement.md) | medium | 完成（只記錄；4 ms 狀態 11／12 輪） | 只記錄：在含 03 的 P1b 分支頭上量 03 單獨的效果（不合併那個頭） |
| 04 | [Client 三角色](04-client-roles.md) | high（交接、過期、生命週期、GUI 斷言局部 xhigh） | 完成（PR [#71](https://github.com/yojinn-io/GYO-Engine/pull/71) 合併，`660310e`；D30 門檻的放大由使用者決定 C1 之後再看） | 模擬角色在固定步期限產生命令；主執行緒只發布意圖並讀呈現副本 |
| 05 | [C1：30 FPS 修正前後對比](05-30fps-comparison.md) | medium（宣告草案用 high） | 完成（2026-10-09）：C1 通過（4 ms 狀態；8 ms 未驗證），使用者操作的 L2、L3 通過 | C1（任務 1、2）；拖動與縮放的 L2；Spaces、縮小、遮住的 L3 |
| P2-log | [網路路徑的 10 秒統計](p2-log-network-statistics.md) | medium | 完成（2026-10-09；`983e091`，PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)） | 只加記錄的 commit，作為 P2 的 before |
| 06 | [Gateway 結果通道改為 30 Hz](06-gateway-results-30hz.md) | medium（資格容許局部 xhigh） | 實作、L1、開發跑次完成；碰到「結果每秒 ≤31」的停止條件，等使用者決定（2026-10-09） | Gateway→Client 結果通道約 15 Hz→30 Hz（P2-log 實測 15 Hz） |
| 07 | （P2 開始時撰寫） | high（局部 xhigh） | 未開始 | Match Tick 改為絕對期限的 Waiter＋發布通知；IpcHost 改用 asio |
| 08 | （P2 開始時撰寫） | high（局部 xhigh） | 未開始 | ClientConnection 改用 asio；SendInput／SubmitAction 喚醒 worker |
| 09 | （P2 開始時撰寫） | high（常數推導局部 xhigh）；量測 medium | 未開始 | FireGate 常數先推導並凍結，再跑 C2 與 25 案回歸 |
| 10 | （P3 開始時撰寫） | high | 未開始 | 任務 8 的紀錄；日誌格式定為產品 Data Contract |
| 11 | （P3 開始時撰寫） | high（局部 xhigh；契約建議 ultracode 審查） | 未開始 | **pv7**（唯一的 wire 變更）：runtime link 心跳對時＋Client↔Gateway 時間回聲 |
| 12 | （P4 開始時撰寫） | high | 未開始 | 產品、probe、產品測試不再直接使用 SDL；符號守衛 CTest |
| 13 | （P5 開始時撰寫） | high（視窗模式的切換順序局部 xhigh） | 未開始 | 玩家以命令列選項選擇解析度、視窗模式與 FPS 上限（D41）；HUD 依解析度縮放；30 FPS 手感的 L3（D41）。設定 UI 與 `settings.json` 在 v8 |
| 14 | （P6 開始時撰寫） | high | 未開始 | 射擊、命中、受擊、換彈的音效 |
| 15 | （P7 開始時撰寫） | medium | 未開始 | v7 LAN 場次：Windows 實機、兩機漂移、Windows 的視窗切換 |
| 16 | （P7 開始時撰寫） | medium | 未開始 | 整合驗收、STABLE_BASELINE v7、是否發行 |

P2 之後的批次文件在該線開始時寫進該線的 PR，行號才不會過時。在那之前，範圍、驗收與停止條件以[交接](HANDOFF.md)的「P2 以後各批的範圍」為準。

### Engine 計畫的批次（狀態以各計畫夾為準）

| 批次 | 內容 | 建議檔位 | 本產品的關係 |
|---|---|---|---|
| TT-1 | `GYO::Time`（時間基準、Waiter、晚醒統計、kqueue／高解析度 waitable timer／cv 後端）、`GYO::Threads`（角色執行緒）、`RuntimeLoop` 改用時間基準 | high（後端、Notify 與期限的競爭、不早醒、停止與 join 順序局部 xhigh） | 完成：和第 04 批同一個 PR（P1b，#71 合併，`660310e`）。2026-10-08 實作與 L1 完成、xhigh 審查完成；2026-10-09 L2（只記錄）完成 |
| TT-2 | `GYO::Trace`：有界 sink、通知喚醒的寫檔、Base 日誌 facade | high（寫檔關閉順序局部 xhigh） | 第 10 批依賴 |
| IP-3 | Engine 後端公開標頭去掉 SDL 型別（`*Native.hpp`）；日誌轉送、執行檔目錄、進入點、文字輸入與剪貼簿、事件時間戳換算 | high（時間戳換算局部 xhigh） | 第 12 批依賴 |
| IP-4 | 測試用事件注入與觀測、視窗查詢與擺放；SDL3 改為 PRIVATE 連結 | high | 第 12 批依賴 |
| IP-5 | 顯示器與模式列舉、視窗模式、像素密度（使用者目錄與 `engine/io` 的原子寫入依 D41 移到 v8） | high（模式切換順序局部 xhigh） | 第 13 批依賴 |
| AU-1 | `GYO::Audio`＋SDL 後端 | high（callback 即時安全、時間戳→樣本位置局部 xhigh） | 第 14 批依賴 |

### PR 線

| PR | 批次 | 理由 |
|---|---|---|
| P0 | 01 | 規劃本身的交付（比照 v6 #39） |
| P1a | 02、03a | 只改產品，行為不變（逐位元組相同的證明）。P0 的狀態同步併入。03a 的結果以文件併入 |
| P1b | 03、TT-1、04、05 | 第 03 批在命令跟著畫面幀產生的架構下會讓 30 FPS 變差（D40），所以和讓命令改在固定步產生的第 04 批一起合併。跨層必須一起合併：Waiter 與 Threads 的第一個消費端是第 04 批（Match Tick 先用的話，C1 的 after 就混入 Match 的改動）。P1a 的同步併入 |
| P2 | P2-log、06、07、08、09 | 網路路徑一條功能線；全部在 wire 變更之前 |
| P3 | TT-2、10、11 | 診斷：Trace、紀錄、跨機對齊用的對時；v7 唯一的 wire 變更，三個角色同一個 PR |
| P4 | IP-3、IP-4、12 | 標頭去 SDL、PRIVATE 連結與產品遷移必須一起合併 |
| P5 | IP-5、13 | 機制與唯一消費端的政策一起審查 |
| P6 | AU-1、14 | Engine 模組與第一個消費端一起合併 |
| P7 | 15、16 | 第 16 批若在 P6 的頭執行，可以併入 P6 |

### 依賴

```text
P0   [01 計畫]
       │
P1a  [02 接縫] ─────────────→ [03a 小量測（量 P1b 分支上的 03）]
       │
P1b  [03 每份 snapshot] → [TT-1 Time＋Threads] → [04 三角色] → [05 C1]
       │
P2   [P2-log] → [06 Gateway 30 Hz] ┐
                [07 Match asio]    ├→ [09 FireGate 常數 → C2]
                [08 Client asio]   ┘
       │            （↑ 所有新舊對比到這裡結束）
P3   [TT-2 Trace] → [10 診斷紀錄] → [11 pv7]   ← 唯一的 wire 變更
       │
P4   [IP-3] → [IP-4] → [12 產品去 SDL]
                 │
P5            [IP-5] → [13 解析度與 FPS 的選擇（命令列選項、30 FPS 的 L3）]
P6   [AU-1]（需 TT-1、IP-3）→ [14 音效]
P7   [15 LAN 場次]（需 11）→ [16 整合與升格] ← 全部
```

- 所有新舊對比（C1、C2）都在 pv7 之前完成，凍結的分析器一直可用。
- C1 的 after 只含任務 1、2：Gateway、Match、網路 worker 的輪詢與送出節拍都沒有改。ClientConnection 只多一個給模擬角色的佇列。

## 執行規則

- 每次只執行使用者指定的批次；一條功能線一個 PR，commit 與 PR 用日語。依賴未完成時，不得以部分成果頂替。
- 每批開始、里程碑、停止時更新本表、[交接](HANDOFF.md) 與 dev_log，然後停止，不自動開始下一批。合併後的狀態同步放進同一條線的下一個 PR。
- 主對話的檔位由使用者決定；表中是建議值。ultracode 與高於主對話的檔位，在批次開始時說明理由並徵求同意（v6 D8）。xhigh 只用在批次內的局部。
- **wire 只在第 11 批升一次 pv7**，嚴格版本相等，三個角色同一個 PR，舊程序重新啟動（D32，沿用 v6 D11①）。之後若又需要改 wire，先徵求使用者同意。
- **v7 不改權威結果**。每個實作批次以同機兩樹 digest（35／35）證明權威不變。
- **分析器凍結並版本化**：凍結的檔案不改；v7 的新語意另立新檔（分析器 v7）。新紀錄不在 `commands.jsonl` 新增事件種類（`command_evidence.py:137-138` 遇到未知種類會拋出），一律寫到另外的檔案。
- L2 需要使用者核准的事前宣告；先凍結來源、產物與分析器，量測只在 CI 綠燈並凍結的 head 上執行；開發與乾淨量測不同時進行；失敗的跑次保留，先有限定位，不默默重跑。L1／L2／L3 沒有固定順序。
- 驗證與突變腳本跨平台（Python subprocess 的逾時或 CTest TIMEOUT）；突變只在預期的斷言觸發時才算 killed。
- 對玩家的門檻跨平台相同，不因平台放寬。
- 範圍、門檻或 ownership 需要改變時，先報告具體證據。
- 決策從 D27 接續編號（v6 交接與各 Engine 計畫夾共用同一套 D 編號）。

### 平台表（D34）

| 平台 | 驗收方式 |
|---|---|
| macOS Intel x86_64（Metal、120 Hz） | 實機：L2、L3 |
| Windows x64（D3D12） | 只在使用者與朋友的 LAN 場次（第 15 批）：Client 的模擬步晚醒（含縮小與遮住）、視窗切換、兩機偏移與漂移；朋友能主持時加測 Windows Match，否則 Windows Match 標「未驗證」 |
| Linux x64 | 實機未驗證；CI 跑 L1 |
| macOS arm64 | 實機未驗證；CI 跑 L1 |
| 兩機時鐘漂移 | 經 pv7 的 Client↔Gateway 時間回聲在第 15 批量（runtime link 永遠同機） |

LAN 場次排不出來時，第 15 批各項標「未驗證」，是否升格由使用者在第 16 批開始時決定（比照 v6 D26）。缺口照實寫進 v7 的 STABLE_BASELINE。不做 CI 只記錄的計時測試：現行 workflow 拿不到通過測試的輸出。

## 驗收與方法

- 跨平台實機驗證依上方平台表。
- 30 FPS 與主機約 8 ms 晚醒的情況要納入驗收（v6 D21）；30 FPS 長局實驗需要另外授權。
- 可重複使用的效能比較方法：多次跑次、交錯順序、依主機狀態分層、事先宣告判定規則（v6 第 07 批的教訓：單次比較會誤判）。

### v6 的 30 FPS 失敗案例與各幀率的對照（任務 1、2 的修正對比基準）

2026-10-07 使用者指示：保存 v6 的失敗案例，v7 分執行緒完成後再跑一次，作為修正前後的對比。

- 基準包：`build/target/_build/test/logs/pvp-v6-30fps-reference-20261007/`。
  - 內容：6 個失敗跑次的逐位元組複本、各跑次的指標、v6 全部 clean-30／60／144 跑次的指標，以及說明檔。
  - 雜湊清單 `files.sha256`（清單本身 SHA-256 `f43769b6…`）。另有 `sources.sha256`，記錄各跑次原始 trace 的雜湊；這些 trace 沒有複製，仍留在各批的證據夾。
- v6 正式跑次依幀率與主機狀態分組（不含開發跑次）。主機狀態的定義：Match 的 snapshot 產生時間對擬合 60 Hz 格點的晚醒 P99，≥6.0 ms 算 8 ms 狀態（參考包的定義；STABLE_BASELINE 14b 用的是 probe 的計時器晚醒，兩者不同）。參考包的「07a 判定」是各跑次當時工具記錄的判定，加上 07a 豁免的一筆；延遲是移動 Actual 的中位數，單位 ms：

| 案例 | 主機狀態 | 通過／跑次 | Held 比率 | 單次 Held 最多 | P50 | P95（範圍） |
|---|---|---|---|---|---|---|
| clean-30 | 4 ms | 13／15 | 0.43% | 38 | 21.6 | 27.2（24.9～41.0） |
| clean-30 | 8 ms | 1／5 | 1.82% | 48 | 26.9 | 40.3（37.0～44.6） |
| clean-60 | 4 ms | 15／15 | 0.03% | 8 | 24.4 | 27.8（25.2～39.1） |
| clean-60 | 8 ms | 6／6 | 0.02% | 2 | 25.3 | 37.5（37.2～40.3） |
| clean-144 | 4 ms | 5／5 | 0% | 0 | 34.8 | 38.5（37.6～39.2） |
| clean-144 | 8 ms | 正式集合沒有樣本（14b 最終來源的矩陣有 1 個樣本：通過，P50／P95 34.5／40.3） | — | — | — | — |

  - clean-60 沒有產品上的失敗：唯一一次失敗（第 07 批 4b）是連線時初始 seed 的夾住，第 07a 批的分析器已豁免。
  - 通過率與 Held 隨幀率改變：30 FPS 的 Held 是 60 FPS 的十倍以上，8 ms 狀態更明顯。原因是命令跟著畫面幀產生（D21）。
  - 延遲也隨幀率改變：clean-144 的 P50 比 clean-60 高約 10 ms。原因未調查，只記錄現象。
  - 延遲同時隨主機狀態改變：60 FPS 的 P95 在 4 ms 狀態約 28 ms，在 8 ms 狀態約 37 ms。網路各段（IPC 1 ms、relay 2 ms、worker 2 ms 的輪詢）只解釋約 9 ms 中的約 1.6 ms，其餘來自 Match Tick 等待的晚醒（Time 子系統，第 07 批），不是任務 3 的輪詢。
  - 現在 8 ms 狀態才是常態（14b 矩陣 92%、LAN 75%），4 ms 狀態也可能缺樣本。

| # | 來源 | tree | 主機延遲 P99 | Actual | Held（玩家 1／2，發生時段） | 失敗原因 |
|---|---|---|---|---|---|---|
| F1 | 第 04 批 B0 跑次 4 | `6381e9d` | 3.0 ms | 99.94% | 1／1（1.2 秒） | 57.7 ms 的真正卡頓後停頓重設 |
| F2 | 第 07 批 B1 跑次 4 | `5b0553a` | 7.3 ms | 97.19% | 37／11（8.4～14.9 秒） | Held 替代，加上停頓重設 |
| F3 | 交錯 A/B run-5 B | `5b0553a` | 7.9 ms | 97.79% | 23／15（8.5～14.4 秒） | Held 替代，加上停頓重設 |
| F4 | 交錯 A/B run-6 A | `6381e9d` | 6.7 ms | 97.26% | 25／21（8.5～14.9 秒） | Held 替代 |
| F5 | 交錯 A/B run-6 B | `5b0553a` | 7.1 ms | 99.16% | 7／8（8.8～14.0 秒） | 停頓重設 |
| F6 | 第 10 批 L2 before | `5da939f` | 3.6 ms | 97.73% | 22／16（1.0～14.8 秒） | Held 替代（4 ms 狀態，從開局就開始） |

- Held 替代＝3 步幀最舊的一步錯過自己的 Tick，Match 以上一筆命令頂替。停頓重設＝Client 落到權威後面，以中立命令重新播種（機制見 v6 交接「第 07 批後續：交錯 A/B 調查」）。
- 對比的做法（D31；判定規則在第 05 批開始時事前宣告）：
  - **前提**：矩陣用的無頭 action probe 自己有幀迴圈，不經過 `RuntimeLoop`／`PvpApplication`，兩個 Client 共用同一個幀（`gameplay_action.hpp:106-161`；timing_main、quad_main、network_main 同樣）。GUI probe 經過 `PvpApplication`，但不經過 `RuntimeLoop`。第 02 批讓產品與 probe 走同一個 `ClientSimulation`，第 04 批把模擬角色的執行體也放進同一個型別。
  - **C1**（第 05 批）：before＝v6 的最終量測來源 `fee92ff`（產品程式＝`c7d6dd3`＝tag `object_fps_pvp-v1.1.0` 的產品程式，產物雜湊見 STABLE_BASELINE），after＝P1b 的頭（只有任務 1、2，仍是 pv6）。Gateway 結果通道的缺陷兩棵 tree 都保留。兩者在同一台機器上從零建置。
  - **C2**（第 09 批）：任務 3 之後、pv7 之前，用同一份 before 建置。
  - 跑次：clean-30、clean-60 每棵 tree 至少 6 輪，交錯先後，機器閒置；clean-144 作為對照組，每棵 tree 至少 3 輪（使用者 2026-10-07 確認保留）。
  - 主機狀態：主分類用參考包的定義，同時記錄 probe TimerBaseline 與兩者的一致率。報告各棵 tree 的狀態頻率，只在同一狀態內比較；每輪前後量一段沒有 Client 的 Match 空轉晚醒，證明 after 沒有把主機推向另一個狀態，頻率明顯偏移就停下。兩種狀態都設輪數上限，上限內沒有樣本就標「未驗證」。第 07 批讓 Match Tick 變精確之後，C2 改以 probe TimerBaseline 分層。
  - 指標：各狀態的通過率、Held 筆數與發生時段、停頓重設次數、Actual 比率；移動延遲 P50／P95 只記錄、不判定。
  - 各指標的權威來源事先寫明：Held、Actual、停頓重設由兩棵 tree 共用的同一個抽取器計算；通過與失敗以凍結分析器判定；分執行緒後語意改變的規則（STALL_RULE、seed 夾住豁免、每幀的 runtime_gap、60 Hz ±2 步）只以分析器 v7 判定，清單事先列出。
  - 兩種比較：同一幀率的 v6 對 v7；同一棵 tree、同一主機狀態下，各幀率之間的差距。
  - v7 預期（使用者 2026-10-07）：命令不再跟著畫面幀產生，所以 clean-30 與 clean-60 的結果應該相近，各幀率之間的差距要明顯小於上表的 v6 差距。延遲隨主機狀態的差異不在這項預期內。
  - 分析器：第 07a 批清單（`9e086f1d…`）中的 `acceptance_util.py` 是 `PROTOCOL_VERSION = 5`，逐字使用時 `gameplay_evidence.py:140-141` 與 `gameplay_soak_evidence.py:178-181` 會拒絕所有 pv6 trace，連 before 也一樣。F1～F5 是 pv5 的 tree，07a 原樣適用；F6 與之後的 pv6 跑次，是以 tree 自己的常數 6 分析的。所以對比改用 v6 最終工具清單（`0d2ffcfb…`）的 Python 分析器，也就是 07a 的規則加 `PROTOCOL_VERSION 6`（另加 `action_probe.py` 的一處註解）；C1 用的 Python 子集另立 sha256 清單並凍結。v7 若修改分析器，after 的 trace 用新舊兩版各分析一次。
  - v6 第 16 批之後的工具清單是 `b8eabc7c…`（61 檔，`logs/pvp-v6-batch16-dev-20261007/frozen-tools.sha256`）。其中 Python 分析器除了 `acceptance_util.py` 與 `action_probe.py` 以外與 07a 逐位元組相同；GUI probe 與 GUI runner 支援有 bot 的房間，2 人房的判定不變；4 人另有 `quad_evidence`（與 `command_evidence` 的重複，見「候選」的收斂項目）。

### v6 L3 的觀察：切換工作區後斷線（v6 第 14a 批；v7 完成後重新確認）

2026-10-08 使用者在 v6 第 14a 批的 L3 中，一邊連續射擊一邊切換 macOS 工作區（Spaces）去看其他畫面，回來後看到斷線訊息。使用者決定不判 v6 失敗，交給 v7 一起處理。

- 當時的 Gateway／Match 輸出沒有保留，原因未查明；使用者不確定是切換工作區造成，還是程式的問題。
- 盤點的更正：主執行緒停頓不會讓 Gateway 的 5 秒 session 逾時，因為網路 worker 每 1 秒送一次 Hello（`ClientConnection.cpp:392-398`、`session.go:53-67`）。程式中可能的路徑有兩條：
  - Match 在連續 3 個失敗的 10 秒品質窗口後逐出（`MatchRuntimeHost.cpp:372-392`）。
  - 整個程序被節流（App Nap，未驗證）；這條任務 1 解決不了。
- v7 的確認在第 05 批的 L3：在連續射擊中切換工作區、最小化與遮住視窗，確認不再斷線；保留 Client、Gateway、Match 的日誌與 movement trace。重現時停下，提出 macOS 程序活動宣告作為新的 Architecture Delta。
- 2026-10-08 LAN 聯機測試的 Windows 切換視窗：見下一節（屬任務 2 的證據）。

### v6 LAN 聯機測試的觀察（v6 第 17 批；v7 的真實網路基準）

2026-10-08 使用者與朋友做了 LAN 聯機測試：Mac（Intel）上跑 Match＋Gateway，Client 是 2 台 Windows（D3D12，各 60 FPS 與 165 FPS），約 14.5 分鐘，2 人。功能全部正常、沒有斷線。結果見 v6 [驗收狀態](../v6/ACCEPTANCE_STATUS.md)的「LAN 聯機測試」。使用者的評語：這是真實網路下的樣態，強化了 v7 的重要性。

證據：`build/target/_build/test/logs/pvp-v6-lan-20261008/`（git 忽略，只在本機；`artifacts.sha256`）。

真實網路下看到三種中斷（Held＝Match 在該 Tick 沒有這位玩家的命令，以上一筆頂替）：

| 樣態 | 時間與規模 | 推測原因 | 對應的 v7 任務 |
|---|---|---|---|
| 個別 Client 的網路抖動 | Held 連續 5～8 Tick（約 80～130 ms），同時那台 Client 的 Snapshot 也延遲（最大年齡 102 ms）；`CONNECTION POOR` 失敗窗口最多 2／3 | Client 端的 Wi-Fi | 任務 2（相位追蹤處理每一份樣本）與連線品質判定要以真實網路數據調整 |
| 兩位玩家同時中斷 | 2 次，兩人同一時刻 Held 7～8 Tick。兩次 Match 的 Tick 間隔都正常（最大 24.8 ms、20.4 ms），兩台 Client 在同一批 Tick 都有 126～171 ms 的 snapshot 空窗。47.5 ms 的 Tick 間隔（tick 56834）出現在第二次之前 3.2 秒，只造成 1 Tick 的同時 Held | Mac 端的網路路徑（Wi-Fi 收包叢集等） | 任務 3、任務 8（Gateway 逐包的間隔紀錄） |
| 切換視窗時中斷 | 3 個 Held Tick（28486／28488／28490）發生在 267.5 ms 的事件處理之前約 1.5 秒，時間點是失焦後改以 33 ms 呈現、每幀 2 筆命令的時候；267.5 ms 的 pump 期間 live frame 仍產生 15 筆命令、10 次呈現 | 命令跟著畫面幀的節拍，而 OS 在失焦時改變了節拍 | 任務 2（任務 1 是背景） |

- 整體：Match 收到實際命令的比例 99.46%／99.74%；Snapshot 每秒 60、年齡 P50／P95 約 8／19 ms；兩人各擊殺 19 次，38 次重生全部在四角。
- Windows 上的切換視窗：日誌只有 9 行「releasing pointer」（A 2 行、B 7 行），沒有記錄原因，所以次數無法從日誌確認。沒有斷線，只有上面的短暫中斷。
- 任務 1 最強的觀測證據在 macOS：IP-2 L2-after 的 pump 3056.8／4322.9 ms（`live_frames=0`）、兩台同時 `nextDrawable` 約 1016 ms、Match 連續頂替最多 29 Tick 與 5 次停頓重設；第 06／09 批 L3 的 drawable 停頓最長 1181 ms（見[盤點](INVENTORY.md)）。

日誌缺口（任務 8）。依 owner 分，Engine 的 Trace 提供結構化事件與背景寫檔，各產品角色決定記什麼：

| 缺口 | 這次的影響 | owner |
|---|---|---|
| 動作被拒絕的原因沒有寫進日誌：Client 只記次數；Match 不記動作裁決 | 一台 Client 有 9 次被拒，原因（冷卻、換彈、彈匣已滿等）查不出來 | 產品 Client、Match（以 Trace 記錄） |
| Match 結束時沒有記錄；runtime link 斷掉時 IpcHost 的 Connection 靜默退出（`IpcHost.cpp:202-246`，`:341` 吞掉例外） | 分不出正常結束或異常終止 | 產品 Match；Trace 的生命週期事件 |
| 沒有指定 Gateway 時，錯誤只顯示逾時；Gateway 也不警告「advertise-ip 是 loopback 卻有遠端加入」 | 一台 Client 未加 `--gateway`，連到預設的 127.0.0.1，訊息沒有提示 | 產品 Client、Gateway |
| 每 10 秒統計缺每位玩家的 Held 次數（Match）、封包間隔分布與 control lane 的丟棄計數（Gateway）；Client 的 snapshot 年齡只有每秒一次的點取樣 | 這次的 Held 分析是從 movement trace 另外計算 | 產品 Match（Trace）、Client；Gateway 是 Go，屬產品自有 |
| 跨機器的時間無法對齊（Client B 的牆鐘比 Mac 快約 0.45 秒） | 跨機器事件靠牆鐘推估 | 任務 4，以及 pv7 的 Client↔Gateway 時間回聲（D35，P3 開始前確認） |

### 本機射擊閘的兩個常數（v6 第 12 批；v7 的第 09 批重新評估）

2026-10-07 使用者指示：v7 完成後重新評估下列兩個常數（`apps/object_fps_pvp/include/RetroFPS/Pvp/FireGate.hpp`）。它們都由 v6 的時序推導而來，任務 1～3 會改變那些時序。

| 常數 | v6 的值 | 推導的依據 | v7 會改變的地方 |
|---|---|---|---|
| `FireGateGuardSeconds` | 2 ms | 相位追蹤的死區（`MovementPhaseDeadbandSeconds`），也就是已決定的相位誤差上界。追蹤控制的是到達餘裕的 P90，所以最壞時是「死區＋高於 P90 的主機晚醒」；v6 以合成時間線確認 2 ms 時下界成立，殘餘風險由第 12 批 L2 實測確認 | 任務 2：命令改在固定步邊界產生、相位追蹤處理每一份 Snapshot 的樣本，追蹤誤差的分布會不同；任務 3 與 Time 子系統可能改變主機晚醒的尾巴 |
| `FireGatePendingSpreadTicks` | 7 Tick | 尚未判定的射擊最晚在最早判定 Tick 之後幾個 Tick 被判定：Client 與 Gateway 各一個 30 Hz 送出間隔、Gateway 寫出迴圈的喚醒、保護與 Tick 邊界各 1。盤點更正：Gateway→Client 的結果通道實測約 18 Hz（P50 66 ms），原因是寫出後的重新錨定（`server.go:589-592`、`action_delivery.go:205`、`:329`），第 06 批修正；Gateway→Match 通道由輸入的 notify 喚醒，移動時約 ≤50 ms，閒置時可能拉到約 2 個間隔，從未量過 | 任務 3：網路路徑改成事件驅動後，30 Hz 節拍與輪詢休眠可能取消或改變，延遲範圍會縮小 |

- 做法（第 09 批）：
  1. 用第 06～08 批的 L2 數據推導兩個值（必要時在常數變更前的 P2 頭另做一次宣告量測）；兩條通道都先量再推導。
  2. 更新合成時間線（`tests/object_fps_pvp/FireGateTests.cpp`）的建模，commit 並凍結常數。
  3. 在 v6 最終 tree 與 v7 tree 上，以同一份事前宣告比較動作短測（C2），加上 12 Tick 連點的情境。原本的 12 Tick 原生點擊 runner 只支援 X11，所以改為 SDL 注入的 action short 新模式，並以宣告過的 probe-only 修補移植到 v6 worktree。
- 判定沿用 v6 第 12 批：乾淨跑次的權威 Cooldown 拒絕為 0，本機擋下與權威拒絕分開列出。
- v6 第 12 批的 L3（使用者，2026-10-07）：連點仍有輕微差異，判斷與本節的時序問題相同，v7 之後再確認手感。
- 參考：
  - 模擬：`build/target/_build/test/logs/pvp-v6-batch12-sim-20261007/`；
  - L1 突變：`build/target/_build/test/logs/pvp-v6-batch12-l1-20261007/`；
  - 紀錄：v6 [第 12 批 dev_log](../../../dev_logs/2026_10_07_pvp_v6_batch12.zh-Hant.md)。

## 候選（未定案，有需求時再開）

- 非同步資產載入（Job 的第一個實際需求）。
- 當機回報：當機時收集最後的 trace、版本、平台。
- 單機劇情＋聯機選項：本機執行同一套權威模擬（見 v6 交接）。也可能是 framework 層的第二個使用者。
- PvP 骨骼 hitbox（2026-10-07 使用者提出）：部位判定、爆頭倍率。
  - 前置條件：Match 端的權威姿勢；跨平台決定性的姿勢取樣；射擊回溯；wire 新增命中部位欄位（屬於協議變更，v7 的 wire 只改一次，所以不在 v7）；v6 第 11 批的俯仰瞄準；修改契約 §6 的 Hit_Head 規則。
  - 不在 v6 第 10 批做的原因：會違反 D11①（不改 wire）；Match 目前沒有模型與動畫的依賴；會破壞第 10 批「差異只來自數值統一」的歸因證明。
  - Engine 端已鋪路：FF-9 之後，球形 `VerticalCapsule` 與長度 0 的 `Math::Capsule` 都合法，射線與球掃掠的 `Math::Capsule` 版不變。
- 驗收分析器的收斂（v6 第 16 批留下的 Code Smell，AGENTS §12）：`quad_evidence.py` 的指令階段是 `command_evidence.py` 的 N 人版，兩者職責重複；目前以 CTest 的逐值交叉驗證維持一致。v7 若修改分析器，可以一併收斂成一份 N 人版，並照上面的規則以新舊兩版各分析一次。
- `AssetWatcher` 的偵測時刻 `detectedNs`（`AssetWatcher.cpp:171`）改用 Engine 的時間基準（檔案時間本身維持牆鐘語意）。
- `LogFile` 非同步化、把 `ClientConnection` 的 HTTP 移到獨立執行緒：已知限制是關閉時最多約 3 秒；遊戲進行中 HTTP 不會阻塞 UDP。
