# 時間、執行緒與 Trace：交接

更新：2026-10-08。**狀態：已規劃，TT-1、TT-2 未開始。** 2026-10-08 由提出需求的消費端在其規劃批次中建立。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。依 D12 匿名。

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [批次計畫](PLAN.md)：TT-1、TT-2 的範圍、驗收、Architecture Delta、停止條件。
3. 本文件：決策、消費端的壓力摘要、研究事實、未結事項。
4. 相關 Engine 計畫：[輸入與呈現](../input-and-present/README.md)（IP-3 的 SDL 事件時間戳換算依賴本計畫的時間基準）。

## 決策紀錄

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-08 | D27：Engine 建最小的角色執行緒（`GYO::Threads`：名稱、協作停止、優先級提示），不做執行緒池與排程器；Channels 不先統一；Job 系統等第一個平行運算的消費端出現時，建在 Threads 之上（worker 數＝核心數扣掉角色執行緒）。修訂輸入與呈現計畫的 D19：多角色的執行緒由本計畫提供，角色本身由消費端做 | 使用者 |
| 2026-10-08 | D28：本夾負責 Time、Threads、Trace（TT-1、TT-2）；SDL 隔離與顯示接續在輸入與呈現計畫（IP-3～IP-5） | 使用者 |
| 2026-10-08 | D29：`GYO::Time` 是獨立的 Base 層 target，並且是 Engine 的時間基準；`RuntimeLoop` 在 TT-1 改用它，只換時鐘來源，不改 loop 的語意。Waiter 採 latch，不收 `std::stop_token`；後端以相對期限加醒後重新檢查 | 使用者 |
| 2026-10-08 | 每個 Engine 子系統與它的第一個消費端放在同一個 PR；TT-1 的第一個使用者是消費端的模擬角色 | 使用者 |

## 消費端的壓力摘要

提出需求的消費端是一個線上遊戲，有三個程序角色：繪製畫面的 Client、權威模擬的伺服器端、Go 的轉送程序。以下是它的盤點中與 Engine 有關的事實。數字未標「實機」的，是靜態分析。

- **時鐘**：
  - `engine/runtime/src/RuntimeLoop.cpp:34`、`:67`、`:74-94` 用 `steady_clock`。
  - `engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp:936-951` 用 `SDL_GetTicksNS`。
  - `engine/asset/src/AssetWatcher.cpp:13-27`、`:171` 用 `system_clock`。
  - Engine 沒有公開的時鐘型別（`RuntimeLoop.hpp:39` 的 `Clock` 是 private）。
- **等待**：
  - Engine 內沒有任何 sleep、等待原語或執行緒（只有 `SdlGpuRenderDevice` 記錄擁有者執行緒）。
  - 消費端的等待：
    - 伺服器端的 Tick：相對的 `condition_variable_any::wait_for`（`apps/object_fps_pvp/src/Pvp/MatchRuntimeHost.cpp:288-296`）。
    - IPC：每輪 1 ms 的 `sleep_for`（`IpcHost.cpp:327`）。
    - Client 網路：每輪 2 ms（`ClientConnection.cpp:594-597`）。
    - 紀錄寫出：每 20 ms（`MovementTraceWriter.hpp:27`）。
    - Client 主迴圈：`SDL_Delay(10)`（`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:1162`）。
  - 9 個驗收迴圈各自以 `sleep_until` 或 `SDL_DelayNS` 計時。
- **晚醒（實機，macOS Intel）**：
  - 伺服器端 Tick 間隔 P99 23.5 ms、最大 47.5 ms（LAN 場次，14.5 分鐘）。
  - 主機有 4 ms／8 ms 兩種狀態，8 ms 狀態才是常態（量測場次的 75～92%）。
  - 同時刻 `sleep_until` 晚 4～8 ms、`SDL_DelayNS` 晚約 1～2 ms。
  - 1～2 ms 的短輪詢本身準確（P99 ≤2.2 ms）；Windows Client 的 2 ms 輪詢 P99 為 2.97／6.45 ms（實機）。
- **固定步**：`FixedTickRuntime` 的累加器保留餘數，格點實際上是絕對的（伺服器端長時間量到 60.00002 Hz）；但伺服器端以相對等待接在 Advance 後面，晚醒被當成補步吸收，沒有紀錄。
- **執行緒**：
  - 消費端有 4 個執行緒 owner（Client 網路、伺服器端 Tick、IPC、紀錄寫出），驗收另有寫出器。
  - 停止方式有 stop_token、旗標、20 ms 輪詢三種；沒有名稱與優先級。
  - `std::jthread` 讓消費端的 CMake 在 AppleClang 加了 `-fexperimental-library`（`apps/object_fps_pvp/CMakeLists.txt:30-32`）。
- **紀錄**：消費端的關鍵量測數字都要從逐筆的移動紀錄離線重建；Engine 的慢 acquire 日誌用另一個時鐘，與其他紀錄對不上時間。
- 證據：`build/target/_build/test/logs/engine-time-platform-inventory-20261008/`（`files.sha256` 的 SHA-256 `3f2822ba4368242ef486dccd5ad9bbde5d0e01dca07946a9a21882ee45541144`），以及消費端的盤點證據 `build/target/_build/test/logs/pvp-v7-inventory-20261008/`。

## 研究事實（規劃時，2026-10-08）

- libc++（macOS）的 `steady_clock::now()` 呼叫 `clock_gettime(CLOCK_MONOTONIC_RAW)`（在本機以 dylib 反組譯確認），會計入系統睡眠；本機量到它與 `CLOCK_UPTIME_RAW` 相差約 52 秒。
- 本 repo 的 SDL 3.4.0 建置在 macOS 上沒有定義 `HAVE_CLOCK_GETTIME`，`SDL_GetTicksNS` 用 `mach_absolute_time`（`sdl3-src/src/timer/unix/SDL_systimer.c:66`、`:104`），不計入睡眠。
- `SDL_DelayNS` 在 macOS 是 nanosleep 加剩餘時間迴圈；libc++ 的 `sleep_until(steady)` 換成相對的 `sleep_for`，也是 nanosleep。核心看到的是同一種相對 nanosleep，所以 4 倍的差異不是 API 造成的。macOS 依等待長度的比例給計時器合併 leeway；單靠執行緒的 QoS 降不了 leeway；kqueue 的 `EVFILT_TIMER` 加 `NOTE_CRITICAL` 實測 ≤0.11 ms。
- Windows：任何 `SDL_InitSubSystem` 都會經 `SDL_InitTicks` 調高程序的計時器解析度；`SDL_DelayNS` 用 `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`。不初始化 SDL 的程序沒有這兩點。MSVC 的 `steady_clock` 是 QPC；`sleep_until` 以 `Sleep(ms)` 迴圈實作。
- Linux：libstdc++ 的 `steady_clock` 是 `CLOCK_MONOTONIC`；cv 的 `wait_until(steady)` 用 `pthread_cond_clockwait`。SDL 用 `CLOCK_MONOTONIC_RAW`。
- 詳細的 file:line 見證據夾的 `research.json`。

## 未結事項

- TT-1 開始時：依本機實測，在 `NOTE_CRITICAL` 與明確的小 `NOTE_LEEWAY` 之間二選一。
- Windows 不初始化 SDL 的程序（消費端的伺服器端）的計時精度，從未量過；只能在消費端主持的 LAN 場次量，否則標「未驗證」。
- `AssetWatcher` 的偵測時刻 `detectedNs` 改用時間基準：候選，本計畫不做。
