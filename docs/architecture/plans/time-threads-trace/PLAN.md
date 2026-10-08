# 時間、執行緒與 Trace：批次計畫

更新：2026-10-08。狀態：TT-1、TT-2 未開始。先讀 [進度與執行規則](README.md)、[交接](HANDOFF.md)。
file:line 以 master `9a6fa8e` 為準，批次開始時重新核對。本文件依 D12 匿名。

---

## TT-1：Time 與 Threads

### 目標與範圍

做：

**`GYO::Time`（`engine/time`）**
- 獨立的 Base 層 target：PUBLIC 依賴 `GYO::Base`，加上 OS 函式庫；不含 SDL。
- **Engine 的時間基準（D29）**：Engine 內的經過時間、等待與時間戳都用它。
- 時鐘：`Engine::Time::MonotonicClock`＝`std::chrono::steady_clock`，另提供 `NowNs()`。文件照實寫各平台的來源：
  - macOS：libc++ 為 `CLOCK_MONOTONIC_RAW`，會計入系統睡眠。本 repo 建置的 SDL 用 `mach_absolute_time`，不計入睡眠：速率相同，起點不同，每次系統睡眠後差值會跳動。
  - Windows：QPC。
  - Linux：libstdc++ 為 `CLOCK_MONOTONIC`；SDL 用 `MONOTONIC_RAW`。
  - SDL 事件時間戳的換算規則寫在這裡；實作在輸入與呈現計畫的 IP-3，每次 pump 重新取樣一次配對。
- **Waiter**：
  - 只有一個等待者；任何執行緒都能 `Notify`；通知會 latch，多次通知合併成一次喚醒。
  - `WaitUntil(deadline)` 回傳 `{reason: Deadline|Notified, planned, woke, late}`；因 Deadline 醒來時保證 `woke ≥ deadline`。
  - API 不收 `std::stop_token`（避免 AppleClang 的實驗性旗標）；停止由呼叫端處理：Notify 加自己的旗標。
- **後端**（全部用相對期限，醒來後依 steady 重新檢查，不使用換算出來的絕對時間）：
  - macOS：kqueue 的 `EVFILT_TIMER`，加 `NOTE_CRITICAL` 或明確的小 `NOTE_LEEWAY`（批次開始時依實測二選一）；Notify 用 `EVFILT_USER`。
  - Windows：`CreateWaitableTimerExW(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION)` 加 auto-reset event，以 `WaitForMultipleObjects` 等待；系統不支援時退回一般的 waitable timer，並由能力查詢回報。
  - 其他平台：`std::condition_variable` 的 `wait_until(steady)`。
- **LateWakeStats**：固定 bin 的直方圖，加上 max 與 count；不配置記憶體，沒有自己的執行緒。
- 例外：`AssetWatcher` 把檔案時間換算成 `system_clock`（`engine/asset/src/AssetWatcher.cpp:13-27`），屬於牆鐘語意的檔案時間，本批不改；在 `GYO::Time` 的文件寫明。

**`GYO::Threads`（`engine/threads`，D27）**
- 獨立的 Base 層 target，依賴 `GYO::Time`；不含 SDL。
- 具名的角色執行緒，基於 `std::thread`（不使用 `std::jthread`／`std::stop_token`）。
- 協作停止：要求停止時 Notify 這個角色的 Waiter，迴圈自己檢查停止旗標；解構時 join。join 的順序由擁有者負責。
- 執行緒名稱：給除錯器與 Trace 使用。
- 優先級提示（例如 Interactive／Normal／Background），由各平台自己對應；設定失敗只回報，不中止。研究實測過，macOS 上單靠 QoS 降不了計時器晚醒，所以它只是提示；晚醒由 Waiter 的後端處理。
- 不做：執行緒池、排程器、工作佇列、Channels 的共通抽象。

**Engine 內部改用時間基準**
- `RuntimeLoop` 改用 `MonotonicClock`：**只換時鐘來源，不改 loop 的語意**。
  - 不變的部分：`ProcessEvents`→`Update`→`Render` 的順序、`FrameContext.deltaSeconds` 的定義（相鄰兩個 pair 起點的差、第一個 pair 為 0、不夾住）、live frame 的規則。
  - `MonotonicClock`＝`steady_clock`，數值與現在相同。
  - `RuntimeLoop.hpp` 的成員簽名用到時鐘型別，所以新增 PUBLIC 依賴 `GYO::Engine → GYO::Time`。
- `SdlGpuRenderDevice` 的慢 acquire 計時由 `SDL_GetTicksNS` 改為時間基準，不加新的依賴邊。

不做：
- 不改 `FixedTickRuntime` 的語意：累加器保留餘數，格點已經是絕對的；缺的是等待，不是時鐘。
- 不加 frame cap。
- 不做 render 執行緒。

### 驗收點

L1（CTest，CI 四平台；測試放在 `tests/common/time`、`tests/common/threads`）：
- Waiter：
  - 期限前收到 Notify，以 Notified 醒來。
  - 等待前送出的 Notify 不會遺失（latch）。
  - 多次 Notify 合併成一次喚醒。
  - 因 Deadline 醒來時 `woke ≥ deadline`。
  - 假喚醒會重新檢查。
  - `late` 以時間基準計算。
- Threads：停止時在 CTest TIMEOUT 內 join；等待中收到停止會醒來；名稱與優先級的設定在各平台不報錯。
- `RuntimeLoop`：既有的 runtime 測試、`tests/common/runtime_sdl`、ui_editor 的測試照常通過，不修改期待值。
- 不設比結構條件更嚴的時間上界（FF-2 的規則）。
- 依賴圖只新增 `GYO::Time → GYO::Base`、`GYO::Threads → GYO::Time`、`GYO::Engine → GYO::Time`；檢查 Engine target 的依賴閉包，確認 Time、Threads 沒有 SDL 邊。

突變（Python subprocess 加逾時，跨平台；只有預期的斷言觸發才算 killed）：
- 期限前提早返回。
- 丟掉 Notify。
- latch 失效。
- 拿掉停止時的 Notify，讓 join 逾時。

L2（事前宣告，只記錄，本機 macOS）：
- 以 Waiter、cv、`sleep_until` 各做 16.7 ms 週期的等待 30 秒；分兩種情境：CLI 程序，以及 Engine 自有的 SDL 視窗測試程式。涵蓋當時出現的主機狀態。
- 方向規則：同一輪中 Waiter 的 P99 晚醒低於 cv，否則停下。預期 P99 ≤1 ms，只記錄。

跨平台：
- Windows 的精度：只在消費端主持的 LAN 場次量（消費端的範圍）。
- Linux：CI 只驗功能，實機未驗證。
- 不做 CI 只記錄的計時測試：現行 workflow 拿不到通過測試的輸出，不為此修改共通 CI。

### Architecture Delta

1. 需求：消費端的多角色要在固定步期限醒來，也要能被其他角色喚醒；之後消費端的伺服器端 Tick 與本計畫的 Trace 寫檔共用同一個原語。
2. 問題：
   - Engine 沒有時鐘宣告，也沒有等待原語，`engine/` 內同時用三種時鐘。
   - 消費端與 9 個驗收迴圈各自手寫時鐘加等待。
   - macOS 上 std 與 SDL 的等待都有 25～50% 的計時器合併 leeway，沒有任何晚醒紀錄。
   - 執行緒的生命週期寫法各不相同，平台知識（編譯器旗標）漏進消費端。
3. 邊界：新增 Engine 子系統 `engine/time`、`engine/threads`。這是 SDL 後端以外第一批含 OS 原生程式碼的 Engine 模組。
4. 影響：
   - Engine：兩個新 target 與測試；`RuntimeLoop` 與 SdlGpu 改用時間基準。
   - 消費端：第一個使用者是消費端的模擬角色，同一個 PR。
   - 不變：`FixedTickRuntime`、PlatformSDL、ui_editor 的行為。
5. 依賴方向：新增 `GYO::Time → GYO::Base`、`GYO::Threads → GYO::Time`、`GYO::Engine → GYO::Time`，以及消費端 → `GYO::Time`／`GYO::Threads`。`GYO::Net`、PlatformSDL 的邊不變；不含 SDL 的消費端角色仍不連結 SDL。
6. Ownership：
   - Engine：時鐘域的宣告、等待精度、晚醒統計的型別、執行緒的生命週期與平台對應。
   - 消費端：迴圈、Tick 政策、門檻、停止順序。
7. 為什麼較小的做法不夠：
   - 在消費端內用 cv 等待，會繼承 macOS 的 leeway。
   - `SDL_DelayNS` 在 macOS 也是 nanosleep，沒有 notify；伺服器端的角色也不連結 SDL。
   - 把 kqueue 或 Win32 寫在消費端，等於讓平台知識漏進產品（AGENTS §6、§9）。
   - 放進 `GYO::Engine` 的話，`GYO::Net` 與 Trace 這些 Base 層的使用者就用不到，或得多一條往上的依賴。

### 停止條件

- 在本機，Waiter 不比 cv 精確。
- 需要改 `FixedTickRuntime` 或 `RuntimeLoop` 的語意。
- 後端需要新的第三方依賴，或需要 SDL 邊。
- 公開 API 的形狀只有單一呼叫點能用：重新規劃。

### 平台表

| 平台 | TT-1 |
|---|---|
| macOS Intel（Metal） | L1、L2（只記錄） |
| macOS arm64 | CI 的 L1；實機未執行 |
| Windows x64 | CI 的 L1；精度由消費端的 LAN 場次量 |
| Linux x64 | CI 的 L1；實機未執行 |
| GPU（Linux lavapipe） | 不適用 |
| 消費端的整合 | 消費端的批次 |

---

## TT-2：Trace

### 目標與範圍

做：
- `engine/trace` → `GYO::Trace`：依賴 `GYO::Threads → GYO::Time → GYO::Base`。
- 多生產者的有界佇列。一筆紀錄＝{kind, 時間基準的 ns, 預先序列化的內容行}。
- 一條背景寫檔執行緒（`GYO::Threads`），由 Waiter 的 Notify 喚醒，不輪詢；只在佇列由空轉為非空、或達到批次門檻時才 Notify。
- 丟棄計數、帶計數的 `trace_end`、`Good()`、檔案 sink。
- `engine/base` 的 `Log.hpp`：等級、類別、訊息，送到可替換的 atomic handler，做法比照 `Assert.hpp:49-50` 的 AssertionHandler；預設輸出到 stderr。
- `SdlGpuRenderDevice` 的慢 acquire 日誌改走 facade。

不做：
- 不把消費端的 schema 或事件種類搬進 Engine。
- 不做錄製重播。

### 驗收點

- L1：
  - 8 個生產者同時寫入時，佇列有上限，丟棄計數精確。
  - 寫檔執行緒由紀錄喚醒，沒有輪詢計時器；`trace_end` 是最後一筆。
  - handler 的替換是執行緒安全的。
  - 依賴圖只新增 `GYO::Trace → GYO::Threads`。
  - CI 四平台。
- 突變：讓丟棄不計數時，測試觸發。
- 消費端既有 trace 的位元組相容，屬消費端的檢查。

### Architecture Delta

1. 需求：消費端要用結構化的紀錄補上診斷缺口；Engine 自己的診斷日誌也需要一個去處。
2. 問題：消費端的日誌、量測與驗收寫出器各自實作（有界 sink、condvar 佇列、每行 flush）；Engine 的慢 acquire 日誌用 `SDL_GetTicksNS` 與 SDL_Log，和其他紀錄對不上時間。
3. 邊界：新增 `engine/trace`，並在 `engine/base` 加日誌 facade。
4. 影響：Engine 新 target 與 SdlGpu 的日誌去處；消費端的紀錄改接。
5. 依賴方向：`GYO::Trace → GYO::Threads → GYO::Time → GYO::Base`。
6. Ownership：Engine 擁有傳輸（佇列、寫檔、丟棄計數、時間戳）；消費端擁有紀錄的種類、內容與 schema。
7. 為什麼較小的做法不夠：只做日誌 facade，Engine 與消費端的背景寫出仍會各自重做，消費端的寫出器也會繼續以 20 ms 輪詢。

### 停止條件

- 既有種類的 trace schema 必須改。
- 消費端的 schema 或事件種類必須搬進 Engine。

### 平台表

| 平台 | TT-2 |
|---|---|
| 全部 | CI 的 L1；不涉及精度 |
