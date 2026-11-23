# 視窗（`engine/modules/window`）與 `window_camera`

更新：2026-11-23（新頁，SDL 線子批 W `v8-arch-gyo2-sdl-w`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准）第 3.3、6.2、6.6、10.2 節的 W 列；`window_camera` 是 X1 留給 W 的膠水（[transform-camera 宣告](transform-camera-declaration.zh-Hant.md)第 4.2 節）。**Design Invariants 的最終文字在合併前核准定**。

Owner：架構。程式：`engine/modules/window/`（`GYO::Engine::Window`，target `GYO::Window`，登記單位 `WindowModule`）、`engine/modules/window_camera/`（`GYO::Engine::WindowCamera`，target `GYO::WindowCamera`，登記單位 `WindowCameraModule`）。行為測試：`tests/common/input/`（ctest `window.module`）、`plugins/sdl_window/tests/`。

## 一個例子

Object_Connect 開一個 1280×720、不能改大小的視窗：產品選項放 `WindowSettings{size={1280,720}, resizable=false}`（G6），`sdl_window` 在 `Configure` 照它建窗。第一輪 `sdl_window` 先寫目前的大小與焦點，`window` 的 `WindowState` 從此知道大小，`window_camera` 把它寫進 `CameraTargetSizes` 的主目標，相機在同一幀的 `PostUpdate` 算出投影。玩家按視窗的關閉鈕：`sdl_window` 寫 `WindowCloseRequested`，`window` 的 System 在 `closeWhenRequested` 開著時設 `WindowRequest.close`，`sdl_window` 在 `Last` 關窗，runner 跑完這一幀就結束（Success）。要先問「確定離開？」的遊戲把 `closeWhenRequested` 關掉、自己讀 `WindowCloseRequested`、決定時才設 `close`。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math、GYO::Time ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Window
GYO::Window、GYO::Camera ◄── GYO::WindowCamera
```

- `window`：`library_only=no`、`default=yes`、沒有 `depends`（不依賴 `input`、`camera`、任何 Plugin）。沒有後端時只是空的資料（同 Bevy 的 `WindowPlugin` 在 `DefaultPlugins`）。
- `window_camera`：`default=yes`、`depends=window;camera`。產品自己填 `CameraTargetSizes{0}` 時關掉它。

## 2. 用法與後果

- **`WindowSettings`**（產品選項，後端在 `Configure` 讀；同 Bevy 的 `WindowPlugin::primary_window`）：標題、邏輯大小、能不能改大小（G6）、隱藏（無頭測試）、`closeWhenRequested` 的初值。
- **`Message`**（寫的一方：後端、測試）：`WindowResized`（物理大小＋縮放比；Bevy 的 `WindowResized` 與 `WindowScaleFactorChanged` 合一）、`WindowFocused`、`WindowCloseRequested`（來源：視窗的關閉鈕／OS 要整個程式結束〔macOS Cmd+Q、登出〕／行程的關機請求〔Ctrl+C、SIGTERM〕）、`ClipboardChanged`（系統剪貼簿的文字，超過上限截在 UTF-8 的邊界、`truncated`）。寫的一方排在 `Main`／`PreUpdate`、`before` `kWindowSet`。
- **狀態**（只由 `UpdateWindowState` 寫，WN-2）：`WindowState`（物理大小、縮放比、焦點、`known`＝收過 `WindowResized`；`LogicalSize()`＝物理／縮放比）、`Clipboard`（剪貼簿的鏡像）。
- **請求**（遊戲寫，後端在 `Main`／`Last` 讀、晚一幀生效；值和上次被接受的不同才呼叫）：`WindowRequest`（標題、能不能改大小、游標形狀、游標顯不顯示、游標的抓法 `cursorGrab`〔None／Confined 關在視窗裡／Locked 藏起來固定不動、`input` 的 `MouseMotion` 是裝置的量；Bevy 的 `CursorGrabMode`〕、`close`、`closeWhenRequested`）；一次性的用 `Message`：`ResizeWindow`（改大小：玩家也會拉，常駐的請求會每幀和玩家搶）、`WarpCursor`（把系統游標移到視窗的某個邏輯像素，G3）、`CopyToClipboard`。
- **關窗與結束迴圈**（WN-4；總宣告第 10.2 節 W 列的題）：「要求關閉」→ 誰決定結束 → runner 怎麼知道。照 Bevy 的 `close_when_requested`＋`exit_on_all_closed`＋runner 看 `AppExit` 的分工，GYO 只有一個視窗，所以收成：`WindowCloseRequested` →（`closeWhenRequested` 開著時 `window` 的 System，關掉時遊戲）設 `WindowRequest.close` → 後端在 `Last` 關窗 → 後端的 runner 跑完這一幀後結束、回 Success。runner 不看事件的內容（SDL-7），只看後端自己「關了沒」。產品擋得下：關掉 `closeWhenRequested`。
- **`SDL_EVENT_QUIT` 與 os 的關機請求**：`sdl_window` 讓 SDL 不裝訊號處理（`SDL_HINT_NO_SIGNAL_HANDLERS`）、最後一個視窗關了也不送 quit（`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE` 關），所以 `SDL_EVENT_QUIT` 只來自 OS 的「結束程式」，翻成 `WindowCloseRequested{Application}`。訊號歸 os 一家（產品呼叫 `OS::Console::InstallShutdownRequests` 時）；runner 每幀看一次 os 的旗標，第一次看到時放一個 `WindowCloseRequested{Shutdown}` 進佇列——遊戲用同一條路存檔、關窗。產品沒裝 os 的處理時 Ctrl+C 照 OS 的預設結束行程。
- **新的 World**：準備函式放空的狀態（`known=false`）；後端在那個 World 的第一輪先寫目前的大小、焦點、剪貼簿（SDL-7 例外 (2)）。所以新建或替換的 World 第一輪就知道大小，`window_camera` 同一幀就寫進相機（X1 交的題）。
- **生成世界期間**：照常寫、照常更新。
- **雜湊什麼都不寫**（視窗大小、焦點每台機器不同；同 CM-5）。
- **骨架關掉的 World**：`WindowOptions{schedule, stage}`、`WindowCameraOptions{schedule, stage}`（`window_camera` 要在 `window` 的 schedule，它排在 `kWindowSet` 之後）。

### `window_camera`

`camera` 不依賴 `window`、`window` 也不依賴 `camera`（總宣告 L-4）；Bevy 的 `camera_system` 直接讀 `Window`（`bevy_render/src/camera.rs:357-363`）。X1 列的候選（作者寫／一個依賴兩者的小單位）裡選小單位：每個帶相機和視窗的產品都要這段，讓作者各寫一次是重複的膠水；預設帶、可以關。`WriteWindowTargetSize` 在 `Main`／`PreUpdate`、`after` `kWindowSet`：`WindowState.known` 時把 `{物理大小, 縮放比}` 寫進 `CameraTargetSizes` 的 `CameraTarget{0}`，**值不同才寫**（每幀寫會讓它每幀都算「變了」）。不採的：`window` 寫（`window` 會依賴 `camera`）、`sdl_window` 寫（違反 SDL-7）、渲染側寫回（違反 DI-KS 13）——同 X1 宣告第 4.2 節。

## 3. 偏離 Bevy 的地方

| 項 | Bevy | GYO | 衝突點 | 膠水量 |
|---|---|---|---|---|
| 視窗不是實體 | `Window` 元件同時是狀態與請求 | `WindowState`（從 `Message`）＋`WindowRequest`（遊戲寫） | GYO 一個視窗、在共用處（總宣告第 6.6 節） | 兩個 Resource |
| 狀態從 `Message` 推 | winit 直接改 `Window`、同時送訊息（`bevy_winit/src/state.rs:256-257`、`:368`） | `UpdateWindowState` 從 `Message` 推 | 後端或 Kernel 的訊息緩衝丟了 `WindowResized`、`WindowFocused`、`WindowCloseRequested` 時狀態停在舊值、關閉沒反應；`sdl_window` 給視窗事件自己的佇列上限（預設 256，輸入灌爆也擠不掉），兩個上限都 ≤ Kernel 的 1024 | 一個 System |
| 改大小 | 遊戲改 `Window.resolution`；winit 把實際大小寫回，快取跟得上（`state.rs:255-257`、`system.rs:388`） | 一次性的 `ResizeWindow` | GYO 的請求不被後端寫回 | 一個 `Message` |
| 結束迴圈 | `AppExit` 訊息、runner 看 | `WindowRequest.close`、後端的 runner 看自己關了沒 | 只有一個視窗；runner 不看事件內容 | 一個欄位 |
| 剪貼簿 | 另一個 crate（arboard），同步呼叫 | `ClipboardChanged`＋鏡像、`CopyToClipboard` | SDL 只在主執行緒；System 裡不直接呼叫 SDL（`-sdl-system-calls-via-adapter`） | 一個 `Message` 一個 Resource |

## Design Invariants

<a id="wn-1"></a>
**WN-1 職責。** 登記視窗的設定型別、`Message`、狀態與請求；從 `Message` 推出 `WindowState`、`Clipboard`；`closeWhenRequested` 開著時，收到 `WindowCloseRequested` 就設 `WindowRequest.close`。不做：建窗、改窗（後端的）、相機的目標大小（`window_camera`）、決定程式何時結束（runner 的）。依賴只有 Base、Math、Time、Kernel、編排層。守衛：連結集；審查。

<a id="wn-2"></a>
**WN-2 狀態只從 `Message` 推出。** `WindowState`、`Clipboard` 只由 `UpdateWindowState` 寫，取這一幀每種 `Message` 的最後一筆；新 World 的 `known` 是 false。守衛：`window.module`「the state from the Messages…」；突變 `wn-*`。

<a id="wn-3"></a>
**WN-3 新 World 第一輪就有目前的狀態。** 列了後端的 World 在第一輪先收到後端寫的目前大小、焦點、剪貼簿（後端的責任，SDL-7 例外 (2)）；`window_camera` 同一幀寫進相機。守衛：`sdl_window.events`「a new World's first round…」（含替換後）。

<a id="wn-4"></a>
**WN-4 關窗走請求。** 關閉的要求只是 `Message`；關不關由 `WindowRequest.close` 決定（預設由 `closeWhenRequested` 設），後端套用後它的 runner 結束迴圈。守衛：`window.module`「…close request sets close」「closeWhenRequested off…」；`sdl_window.events`「the runner ends with Success…」。

**Replacement Criteria**：RC-WN 1：要第二個視窗 → 視窗改成實體或以 id 區分（重比 Bevy 的 `Window` 元件、`camera` 依賴 `window`，RC-CM 1）。RC-WN 2：`Message` 被丟造成狀態錯（大小停在舊值）在實際產品出現 → 後端改成同時寫狀態（照 Bevy），交使用者（偏離表第 2 列）。
