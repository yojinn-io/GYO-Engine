# PvP v7 盤點：時間、執行緒、sleep、socket 輪詢

日期：2026-10-08。Owner：`object_fps_pvp`。來源 master `9a6fa8e`；行號以這個 commit 為準。
v7 README 要求：「v7 開始時，先做一份唯讀盤點（所有用到時間、執行緒、sleep、socket 輪詢的地方，含 Engine、產品、工具），作為各子系統『觀測到的壓力』的證據」。本文件就是這份盤點。分批與決定見 [README](README.md) 與[交接](HANDOFF.md)。

## 1. 範圍與方法

- 唯讀：沒有修改任何檔案，也沒有建置或執行 CTest。本機證據日誌只讀取，分析腳本放在 scratchpad。
- 9 個區域 agent：
  - Engine 核心（runtime、platform、render、input、base、config）
  - Engine 其他模組（net、asset、io、model、ui、text、collision、math）與 `services/gyo_gateway`
  - Client、Match、Gateway
  - 驗收 C++ probe
  - 驗收 Python、測試、CI
  - 其他產品與工具
  - 本機證據日誌
- 1 個對抗式完整性檢查（xhigh）：
  - 自己以 grep 重新掃描，找出漏掉的地點。
  - 實際讀程式，抽查 23 個關鍵地點。
  - 嘗試推翻任務 1～4、7、8 與 README「壓力」欄的宣稱。
- 之後的規劃另做了 2 個研究，主題是 SDL 3.4 的原始碼與各標準庫、asio、Go 的等待原語，摘要見第 6 節。
- 共 303 個地點，完整資料在本機證據（第 9 節）。本文件只寫結論與會影響規劃的地點。

## 2. 執行緒與等待的現況

### C++（Engine、產品、驗收）

| 角色 | 建立的地方 | 節拍與等待 | 停止方式 |
|---|---|---|---|
| Client 主執行緒（事件、Update＝命令產生、Render） | `main.cpp` → `RuntimeLoop::Run`（`RuntimeLoop.cpp:27-57`） | 沒有 frame cap、沒有等待；節拍由 VSYNC 下的 swapchain 等待決定；present 為 Skipped 時 `SDL_Delay(10)`（`PvpApplication.cpp:1162`） | `RuntimeControl::Stop` |
| Client 網路 worker | `ClientConnection.cpp:124-125`（`std::jthread`） | `sleep_for(min(2 ms, 到下一次輸入期限))`（`:594-597`），不能被喚醒 | stop_token；解構時 `DisconnectRemote`，httplib 最多約 3 秒 |
| Match 模擬（Tick） | `match_main.cpp:52`（`std::jthread`，執行 `MatchRuntimeHost::Run`） | `FixedTickRuntime`（60 Hz，最多補 5 步）＋相對的 `condition_variable_any::wait_for(secondsUntilNextTick)`（`MatchRuntimeHost.cpp:288-296`） | stop_token 傳進 wait_for |
| Match IPC | `IpcHost.cpp:366`（`std::jthread`） | 每輪 `sleep_for(1 ms)`（`:327`）；accept 每 5 ms 重試（`:336`）；future 每 10 ms 輪詢（`:339`、`:344`）；每輪最多寫一個 frame | stop_token |
| Match 程序主執行緒 | `match_main.cpp:21` | 每 50 ms 輪詢訊號旗標（`:57`） | SIGINT／SIGTERM |
| MovementTraceWriter（Client 與 Match） | `MovementTraceWriter.hpp:24` | 每 20 ms 醒來寫檔（`:27`） | `Finish()`：request_stop＋join |
| 驗收 probe 的幀迴圈（action、timing、quad、network） | 各 probe 的 main | `sleep_until` 絕對期限（`gameplay_action.hpp:161` 等）；network_main 不設節拍，每 2 ms 一輪 | 時間到，或 stop 檔 |
| GUI probe 主執行緒 | `gui_main.cpp:1090`、`gui_quad_main.cpp:238` | 自己呼叫 ProcessEvents／Update／Render，再以 `SDL_DelayNS` 睡完 1/fps 的剩餘時間；`gui_quad` 用 `sleep_until` | 排程結束 |
| 驗收 ActionEvidenceWriter | `action_main.cpp:35` | condvar，佇列上限 2048 | 旗標＋notify＋join |
| 驗收 mock 與 relay | `worker_main.cpp:57`、`:221`、`:315`；`network_main.cpp:57`；Python 的 relay | mock UDP 每 100 µs 輪詢；Python relay `select` 2 ms 或 10 ms | 各自 |

- Engine 的模組都不開執行緒，也都不 sleep。
- `SdlGpuRenderDevice` 只記錄擁有者執行緒，並以 `SDL_GetTicksNS` 計時慢 acquire。
- 沒有任何地方設定執行緒名稱或優先級。

### Go（Gateway，產品自有）

- runtime link 的寫出由 wake channel 喚醒（每筆輸入、動作、結果、控制都會通知），另有 30 Hz 的 ticker 作為後備（`runtime_link.go:234`）。
- 讀端是阻塞讀取，期限 5 秒（`:268`）。
- UDP 接收是阻塞讀取（`server.go:136`）。
- UDP 送出由 select 處理（`:137`）：動作結果走 30 Hz ticker，control 與 snapshot 由事件觸發。
- 另有 100 ms 的過期檢查（`:138`）與 10 秒的統計（`:139`）。
- 結論：Go 端已經是事件驅動，只有 C++ 端在輪詢。

### 時鐘

- **`std::chrono::steady_clock`**：`RuntimeLoop`、產品（Client 自己重取，`PredictionElapsedTime`；Match；SnapshotTimeline）、probe。
  - macOS 的 libc++ 讀的是 `CLOCK_MONOTONIC_RAW`，會計入系統睡眠。
  - Python 的驗收工具依賴這一點（`run_network.py:68-82`、`test_steady_clock.py`）。
- **`SDL_GetTicksNS`**：`SdlGpuRenderDevice`（慢 acquire 的日誌）、產品日誌行內的 `SDL_GetTicks`（`PvpApplication.cpp:211-212`）。
  - 本 repo 的 SDL 建置在 macOS 上用 `mach_absolute_time`，**不計入**睡眠。
- **`system_clock`**：`AssetWatcher`，把檔案時間換算成牆鐘（`AssetWatcher.cpp:13-27`）。
- 結論：Engine 內同時用了三種時鐘，Engine 沒有自己宣告的時鐘。

## 3. 各子系統的壓力（依 owner）

「不同 owner」指 Engine 模組、產品角色、驗收、產品測試、工具、其他產品；同一個產品的不同角色只算同一個 owner。

| 子系統 | 壓力 | 消費端 | 結論（D27） |
|---|---|---|---|
| Time | 強 | 產品 Match：相對 cv 等待，LAN 的 Tick 間隔 P99 23.5 ms、最大 47.5 ms；`RuntimeGap` 只在 ≥100 ms 時記錄。產品 Client：繞過 `FrameContext.deltaSeconds`，自己重取時鐘。驗收：9 個 probe 迴圈與 TimerBaseline 各自計時。Engine：三種時鐘 | Engine `GYO::Time`，作為 Engine 的時間基準（D29） |
| Threads | 中 | 產品的 4 個執行緒 owner 與驗收的寫出器；v7 會加入 Engine 自己的 Trace 寫檔執行緒；AppleClang 為 `std::jthread` 需要 `-fexperimental-library`（`apps/object_fps_pvp/CMakeLists.txt:30-32`）；停止方式有 stop_token、旗標、20 ms 輪詢三種 | Engine `GYO::Threads`（最小，角色執行緒） |
| Channels | 弱 | 交接的形狀至少有 4 種（mutex 信箱、deque、有上限的 sink、condvar 佇列），語意都不同 | 不先統一 |
| Net transport | 弱 | C++ 兩端都屬於本產品（`ClientConnection`：asio UDP 非阻塞＋httplib；`IpcHost`：asio TCP 非阻塞＋產品 framing）；Go 用不到 C++；asio 由產品取得（`apps/object_fps_pvp/CMakeLists.txt:8-10`） | 留在產品內 |
| Trace | 強 | 產品 Client（`LogFile` 每行在呼叫端執行緒 flush、SDL_Log 轉送、慢事件日誌、MovementTrace）；產品 Match（LogFile、MatchEventLog、MovementTrace）；Engine（SdlGpu 的日誌用另一個時鐘）；驗收（ActionEvidenceWriter、Python 分析器）；Gateway（10 秒統計的紀錄格式） | Engine `GYO::Trace`（精簡） |
| SDL 隔離 | 強 | 產品（`main.cpp`、`PvpApplication.cpp/.hpp`）、7 個 probe 檔、2 個產品測試、Engine 測試、ui_editor 的 `SDL_Log*`；未啟用產品 | Engine（IP-3、IP-4） |
| Display | 弱 | 模式切換只有任務 6 需要；probe（platform_fingerprint、LatencyWindow 的擺放、action_short 的 `SDL_SetWindowSize`）與產品測試只需要唯讀查詢與擺放；ui_editor 由 ImGui SDL3 後端處理 | Engine（IP-5） |
| Settings | 弱 | 原子寫入已有兩份（ui_editor、shader pipeline）；使用者目錄沒有消費端（沒有任何地方呼叫 `SDL_GetPrefPath`） | `engine/io`（IP-5） |
| Audio | 無 | `engine/asset` 的 SoundLoader 沒有使用者；SDL_AUDIO 有編進來，但 `SdlPlatform.cpp:44` 只初始化 VIDEO | Engine（AU-1，使用者決定的功能） |

其他產品與工具：
- `object_fps`、`object_fps_v2`：在 `engine/config/projects.csv` 為 `enabled=0`。兩者都用 `RuntimeLoop`、可變 dt（夾在 0～0.05 s），沒有執行緒、sleep、網路、音效。
- `ui_editor`：用 `RuntimeLoop`，但忽略 `FrameContext`。
- `object_fps_preview`：已停用，用自己的迴圈加 `SDL_Delay(16)`。
- 它們依賴 `RuntimeLoop`「單執行緒、可變 dt」的契約，所以 v7 不改這個契約。

## 4. 被推翻或需要修正的宣稱

| 原本的宣稱 | 判定 | 證據與更正 |
|---|---|---|
| 任務 1：單執行緒主迴圈讓事件或呈現的停頓停止命令產生（README 引用 Windows 267 ms） | 成立，但要更正證據 | 機制成立：命令只在 Update 產生（`PvpApplication.cpp:969-976`）。最強的證據在 macOS，README 沒有引用：IP-2 L2-after 的 pump 3056.8／4322.9 ms（`live_frames=0`），兩台 Client 同時 `nextDrawable` 1016 ms，Match 連續頂替最多 29 Tick、停頓重設 5 次（`engine-ip2-20261005/l2-after/client-b.log:2,5,13`）；第 06／09 批 L3 的 drawable 停頓最長 1181 ms，證據日誌中共 28 行 slow acquire。Windows 267 ms 的 pump 期間，live frame 仍產生 15 筆命令；那 3 次 Held 發生在約 1.5 秒前，是失焦後 33 ms 節拍造成的（屬任務 2） |
| macOS Spaces 切換造成斷線，原因是主迴圈停止產生命令 | 無法驗證 | 主執行緒停頓不會讓 Gateway 的 5 秒 session 逾時，因為 worker 每 1 秒送 Hello（`ClientConnection.cpp:392-398`、`session.go:53-67`）。程式中的路徑是 Match 在連續 3 個失敗窗口後逐出（`MatchRuntimeHost.cpp:372-392`），或整個程序被節流（App Nap） |
| 任務 2：命令依畫面幀產生 | 成立 | `LocalPlayerPrediction.cpp:286-305`：同一幀的所有步共用一個輸入樣本。LAN 上 33 ms 的幀，兩筆命令只隔約 1 µs 送出。probe 重做了同樣的結構（`gameplay_action.hpp:133`） |
| 任務 2：相位追蹤每幀只用最新一份 snapshot | 成立 | `PvpApplication.cpp:416-424`、`LocalPlayerPrediction.cpp:106-126`、`:155-158`；F2 參考跑次約 961 份 snapshot 對 480 幀。LAN 上 60 FPS 有 20.2% 的自身 snapshot 落在一幀多份，165 FPS 為 1.8% |
| 任務 3：IPC 1 ms 與 worker 2 ms 的輪詢讓網路路徑依賴 OS 計時粒度 | 結構上成立，但實測影響小 | 行號已漂移（見第 8 節）。實測：矩陣的上行 P99 1.6～2.2 ms、下行 P99 4.9～6.3 ms（含 relay 的輪詢）；LAN Windows worker 的 generated→send P99 2.97／6.45 ms（n≈52k）。主要的晚醒在約 16 ms 的長等待（Match Tick、probe 迴圈）。Windows Match、Linux 與 CPU 成本沒有量過 |
| 任務 3：v6 第 16 批的 4 人 IPC 寫出負載是壓力證據 | 推翻 | 所有 batch16-dev 與 14b 的 quad 跑次，transport 事件都是 0；唯一例外是 `7-quad-clean-30/clean-30-2` 一次 1.38 ms 的合併。需求的算術（每秒 120 → 180 frame）是推測的壓力。注意：host 端 `snapshot_` 槽的覆蓋與補步時只發布最後狀態沒有計數（`MatchRuntimeHost.cpp:264-275`） |
| 任務 4：只用於診斷，權威以 Tick 與序號運作，屬 wire 變更 | 成立，但要更正 | 屬 wire 變更：`runtime_v6.proto:105-119` 沒有心跳；收到未知訊息會斷線（`IpcHost.cpp:246`、`runtime_link.go:284-291`）；版本要求完全相等（`IpcHost.cpp:210`、`runtime_link.go:48-51`）。權威並非完全以 Tick 為準：Expired／InvalidReference 用主機的 snapshot 年齡 250 ms（`PvpMatch.cpp:281-286`），逐出用參考年齡中位數 160 ms（`MatchRuntimeHost.cpp:86-101`、`:375-389`）。實際量到的跨機偏移（約 0.45 s）在 Client↔Gateway，LAN 測試時 runtime link 是同機 |
| 任務 7：產品使用 SDL 的清單與「不 include SDL 標頭」的完成條件 | 成立，但要更正 | 行號已漂移（第 8 節）。7 個 probe 檔中只有 gui_main 與 gui_quad_main 直接 include SDL；其餘經 Engine 的公開標頭取得 SDL（`SdlPlatform.hpp`、`SdlInput.hpp` 都 include `<SDL3/SDL.h>`）。產品測試 `PointerCaptureCharacterizationTests`、`PlayerPresentationTests:44` 沒有列入 |
| 任務 8：拒絕原因、Match 結束、`--gateway` 提示、Held 與封包間隔、跨機時間 | 成立 | `PvpMatch.cpp:259-335`（裁決只在 wire 上）、`IpcHost.cpp:245`、`:53-58`、`match_main.cpp:57-61`、`PvpApplication.cpp:380-410`、`:729-740`、`server.go:685-717`。另一個缺口：IpcHost 的 Connection 會靜默退出（`:202-246`，`:341` 吞掉例外） |
| README 的 LAN 表：兩次雙人同時中斷中，有一次 Match 有 47 ms 的 Tick 間隔 | 推翻 | `pvp-v6-lan-20261008/mac/match-commands.jsonl`：兩次（tick 40264-40293、57024-57051）的最大 Tick 間隔是 24.8／20.4 ms；47.5 ms 的間隔在 tick 56835 結束，比第二次早 189 Tick，只造成 1 Tick 的同時 Held。兩台 Client 在同一批 Tick 都有 126～171 ms 的 snapshot 空窗，指向 Mac 端的網路路徑 |
| README Time 列：需要絕對格點的固定步時鐘 | 部分成立 | 累加器保留餘數，格點實際上已經是絕對的（`FixedTickRuntime.hpp:44-65`；LAN 長時間 60.00002 Hz）。缺的是絕對期限的等待與晚醒量測 |
| README Threads 列：ClientConnection、IpcHost、MatchRuntimeHost、MovementTraceWriter 各自開執行緒 | 部分成立 | MatchRuntimeHost 本身不開執行緒；模擬執行緒由 `match_main.cpp:52` 建立。驗收的 ActionEvidenceWriter 是另一個 owner |
| README Net transport 列：兩條傳輸都在產品內，而且都在輪詢 | 部分成立 | 只有 C++ 端輪詢；Go 端是事件驅動（`runtime_link.go:232-293`、`server.go:338-356`） |
| README Display 列：工具也是第二個使用者 | 部分成立 | 見第 3 節 |
| README Settings 列：目前沒有任何保存機制 | 部分成立 | 已有兩份原子寫入；使用者目錄沒有消費端 |
| `FireGatePendingSpreadTicks` 的依據：Client 與 Gateway 各一個 30 Hz 間隔 | 成立，但要更正 | 請求方向大致相符（`runtime_link.go:234-259`、`action_delivery.go:189`、`:322`）。結果方向實測約 18 Hz：矩陣 relay 的 ActionResults 每秒 17.8～18.4 包，送出間隔 P50 65.8～66.2 ms，64～69% 的間隔超過 40 ms，clean-30／60／144 都一樣。原因：ticker case 以 `time.Now()` 判定（`server.go:589-592`），寫出後又以寫出結束時間重新錨定 `nextSend`（`action_delivery.go:237`、`:329`），下一次 tick 通常剛好早於 `nextSend` 而被跳過 |

其他更正：
- 「Windows 上 2 ms 的 sleep 會變成約 15.6 ms」：在 LAN 的兩台 Windows Client 上不成立（見任務 3 列）。原因見第 6 節。
- 「Client 的 HTTP 阻塞會讓 UDP 輸入停 3 秒」：遊戲進行中不成立。`PollLobby` 在 session 存在時立即返回（`ClientConnection.cpp:243-268`）；只有關閉時會讓主執行緒的解構等待最多約 3 秒。
- `localCooldownUntil`：已在 v6 第 12 批（`c5bb96a`）刪除，改為以 Tick 為準的 LocalFireGate。
- 「Match Tick 用 sleep_until」：不成立。用的是相對的 `condition_variable_any::wait_for`（`MatchRuntimeHost.cpp:288-296`）。
- 「8 ms 狀態下延遲 P95 上升屬任務 3」：網路各段只解釋約 9 ms 中的約 1.6 ms，其餘來自 Match Tick 等待的晚醒。

## 5. 盤點補上的地點（README 原本沒有列）

- `tests/object_fps_pvp/PointerCaptureCharacterizationTests.cpp:72-104`、`:127-200`、`:267-269`：建立真正的 `SdlPlatform`／`SdlInput`，組出原生 `SDL_Event` 直接交給 `HandleEvent`，並呼叫 `SDL_SetWindowSize`。檔案本身沒有 SDL 的 include，SDL 經由 Engine 標頭取得。
- `tests/object_fps_pvp/PlayerPresentationTests.cpp:44`：`SDL_GetBasePath`。
- `tests/common/input/SdlInputTests.cpp:32-36`：Engine 測試以原生事件注入。
- `PvpApplication.cpp:946-948`、`:977`、`:990`、`:996`：Update 裡除了模擬，還有資產的 `BeginFrame`／`Update`、大廳 UI、`SubmitAction`、`SetRelativeMouseMode`（`SdlInput.cpp:298` 會呼叫 SDL 的視窗操作）。所以任務 1 不能把整個 Update 搬到模擬執行緒，必須拆開。
- Gateway 結果通道約 18 Hz（第 4 節）。
- 本機證據中，任務 1 最強的 macOS 證據（第 4 節）。

## 6. 研究摘要（規劃時）

- **SDL 3.4.0**（`build/target/_build/test/_deps/sdl3-src`）：
  - 只能在建立視窗的執行緒呼叫：swapchain 的 claim、acquire、wait，以及 present（提交取得 swapchain 的 command buffer 時隱含 present）（`SDL_gpu.h:4066-4094`、`:4202-4218`、`:4246-4247`、`:4274-4293`）。
  - `SDL_CreateWindow` 只能在主執行緒（`SDL_video.h:1184`）。
  - 視窗大小改變時，各後端由事件 watch 寫入 `drawableSize`／`needsSwapchainRecreate`，沒有上鎖。
  - 結論：任務 1 正好三個角色，不另開 render 執行緒。
- **Windows 計時**：任何 `SDL_InitSubSystem` 都會調高程序的計時器解析度；`SDL_DelayNS` 用高解析度 waitable timer。這解釋了 Windows worker 的 2 ms 為什麼準。Match 不連結 SDL，這兩點對 Match 不適用。
- **macOS 的晚醒**：`SDL_DelayNS` 與 libc++ 的等待最後都是 nanosleep，API 本身沒有差別。macOS 依等待長度的比例給計時器合併的 leeway，所以約 16 ms 的等待會晚 4～8 ms。單靠執行緒的 QoS 降不了 leeway；kqueue 的 `EVFILT_TIMER` 加 `NOTE_CRITICAL` 實測 ≤0.11 ms。
- **libc++ 的 `steady_clock`** 是 `CLOCK_MONOTONIC_RAW`；本 repo 的 SDL 在 macOS 用 `mach_absolute_time`。兩者速率相同，但起點不同，每次系統睡眠後差值會跳動。Linux 上 libstdc++ 用 `MONOTONIC`，SDL 用 `MONOTONIC_RAW`。
- **asio 1.38**：`post` 可以從其他執行緒安全呼叫；一條 io 執行緒就能同時處理 socket、steady_timer 與跨執行緒喚醒，可以在產品內取代 1／2 ms 的輪詢。
- **Go**：`time.Ticker` 走絕對格點，延遲時跳過錯過的 tick；結果通道的缺陷只影響節拍，不影響 wire 語意。
- **SDL 3.4 的音效**：callback 在裝置的音訊執行緒上執行（`SDL_THREAD_PRIORITY_TIME_CRITICAL`）；沒有時間戳或樣本位置排程的 API，也沒有裝置播放時鐘，必須由自己的混音器維護樣本計數。
- **SDL_GetPrefPath**：每位使用者、每個 app 一個可寫目錄，不存在時自動建立，可以從任何執行緒呼叫。
- **SDL_PushEvent**：可以從任何執行緒呼叫；event watch 在 push 的那條執行緒上同步執行。dummy 驅動下，push 進去的視窗事件不會更新視窗狀態，也不會觸發 swapchain 重建。

## 7. 覆蓋缺口

- Windows 的 Match、Gateway 與所有 Linux 主機的計時從未量過；Windows 只量了 Client 的網路 worker。
- macOS 的 4／8 ms 雙峰狀態，原因推測是依等待長度給的計時器合併 leeway，沒有直接測試。
- 1／2／20 ms 輪詢迴圈的 CPU 與耗電成本從未量過（P2-log 補量）。
- host 端靜默的 snapshot 合併沒有計數。
- Gateway→Match 的 action batch 通道從未量過；結果通道約 18 Hz 只在矩陣的 relay 後量到，沒有 Gateway 端的紀錄。
- Spaces 斷線時沒有保留 Gateway／Match 的日誌；App Nap 是否也節流 worker 執行緒不明。
- Windows 失焦時為什麼改成 33 ms 呈現（DWM 或驅動）沒有調查。
- 盤點範圍不含 `docs/`、`third_party/`、`assets/`；Python runner 只掃了計時構造，沒有逐一評估 v7 的語意影響。

## 8. v6 文件中只記錄、不修改的漂移

以下是 v6 文件中已過時的行號或說法。屬範圍外的整理（AGENTS §11），只在這裡記錄，正確位置以本文件為準：

- v6 交接 D20：「`runtime_v5.proto`」「pv5」→ 現在是 `runtime_v6.proto`，`adapter.go:17-19` 由 `ProtocolVersion = 6` 導出 ClientVersion 與 RuntimeVersion。
- v6 交接 D21：`IpcHost.cpp:267`（2026-10-07 時 `:272`）→ `:327`；`ClientConnection.cpp:567-570`（`:589-592`）→ `:594-597`。
- v6 交接延後項目 8：
  - `backpressure_test.go:102-158` 的 60 Hz ticker 已在第 02c 批（`2fdaa3d`）刪除。
  - `worker_main.cpp` 的行號：`:336-352` → `:362-364`、`:389-390`；`:219,340,366` → `:242`、`:352`、`:366`、`:404`。
  - `gui_main.cpp` 與 `action_short.hpp` 的行號已移動，部分時間常數已改為結構條件。
- v6 交接延後項目 4：`localCooldownUntil` 已刪除。
- v6 STABLE_BASELINE「Python 分析器與第 07a 批相同」：`acceptance_util.py`（`PROTOCOL_VERSION` 5→6，`f255bf1`）與 `action_probe.py` 例外。
- v7 README 原本的行號（任務 3、6、7）：已在 v7 README 更正。

## 9. 證據

本機（git 忽略）：

- `build/target/_build/test/logs/pvp-v7-inventory-20261008/`：
  - `inventory.json`（303 個地點）、`inventory-digest.md`、規劃的 JSON（`planning.json`、`synth-batches.txt`）、確認後的計畫草稿、分析輔助腳本。
  - `files.sha256`，清單本身 SHA-256 `ddc23da08fa9fa70e89b663293d8f0bd01ceb5ca55d2e5d00617aed3fa045901`。
- `build/target/_build/test/logs/engine-time-platform-inventory-20261008/`：
  - Engine 範圍的研究 JSON 與計時探針原始碼，給 Engine 計畫引用，命名中性。
  - `files.sha256`，清單本身 SHA-256 `3f2822ba4368242ef486dccd5ad9bbde5d0e01dca07946a9a21882ee45541144`。
- 盤點讀取的既有證據：
  - `pvp-v6-lan-20261008/`
  - `pvp-v6-batch14b-20261008/`
  - `pvp-v6-30fps-reference-20261007/`
  - `pvp-v6-batch16-dev-20261007/`
  - `engine-ip2-20261005/`
  - `pvp-v6-batch06-20261005/`
  - `pvp-v6-batch09-l3-20261006/`
