# 視窗模式（窗口／無邊框全屏）與 VSync 的事前宣告（`v8-arch-gyo2-window-mode-vsync`）

日期：2026-11-25。Engine 任務卡（GYO2 主會話開的卡）。作者 Opus 5.5／high；設計審查 Opus／high（同模型，報告寫明）。
根據：[window 頁](window.zh-Hant.md)、[SDL 的 Plugin 頁](sdl.zh-Hant.md)第 5、7 節、[render 頁](render.zh-Hant.md)第 2、5 節；HANDOFF `v8-arch-gyo2-settings-resolution-fullscreen`。新 AGENTS：宣告寫完就動工。

**使用者的決定**（2026-11-25）：「OK，就這個尺寸，然後還有全屏和窗口選項。」問全屏種類與 VSync 時：「開，無邊框，VSync 要」。
- 視窗模式兩種：窗口、全屏；全屏是**無邊框全屏**（大小跟著螢幕），不做獨佔全屏。
- VSync 要：開＝`SDL_GPU_PRESENTMODE_VSYNC`，關＝`IMMEDIATE`、不支援時退回（master `SdlGpuRenderDevice.cpp:308-312` 只退回 `VSYNC`）。
- 不做：查螢幕、列可用解析度、選螢幕。解析度是遊戲自己固定的四種，送現有的 `ResizeWindow`。

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-gyo2-window-mode-vsync` | 這一批：`window` 加視窗模式（開窗時的設定、遊戲中切換的 `Message`、狀態照實回報）；`sdl_window` 用 SDL 的桌面無邊框全屏套用；`render` 加 present mode 的資料、`sdl_gpu` 套到 swapchain |

## 1. 要解決的問題（遊戲的例子）

主菜單的「設置」有三個選項：解析度（1280×720 等四種）、顯示模式（窗口／全屏）、VSync（開／關）。玩家選「全屏」：畫面變成整個螢幕、沒有標題列，遊戲的 2D 畫面照 `Letterbox` 在上下或左右留黑邊；選回「窗口」：回到原本的大小。全屏中改解析度：全屏的畫面不變，回到窗口時用新的大小。VSync 關：畫面不等螢幕的更新，幀率不被鎖在 60。

## 2. 前提：Engine 要解決

- 視窗的全屏只有後端（`sdl_window` 持有的 `SDL_Window`）做得到；遊戲只看得到 `window` 的資料。
- swapchain 的 present mode 只有 `sdl_gpu` 做得到（它擁有 GPU 裝置與 swapchain）。
- 兩者都沒有作者能繞的路：不加的話作者得自己碰 SDL（違反 §1.4、SDL-1）。

## 3. 別家

| | 視窗模式 | 切換的形 | present mode |
|---|---|---|---|
| Bevy | `Window::mode: WindowMode`（`Windowed`、`BorderlessFullscreen(MonitorSelection)`、`Fullscreen(MonitorSelection, VideoModeSelection)`，`bevy_window/src/window.rs:1348`） | 改 `Window` 元件的欄位；`bevy_winit` 的 `changed_windows` 比快取、不同才 `set_fullscreen`（`bevy_winit/src/system.rs:336-365`） | `Window::present_mode`（欄位 `window.rs:166`，enum `:1229`；`AutoVsync`、`AutoNoVsync`、`Fifo`、`FifoRelaxed`、`Immediate`、`Mailbox`）；`bevy_render` 抽出視窗、變了就重設 surface，退回的順序在 `bevy_render/src/view/window/mod.rs:882-911`（`AutoNoVsync`＝Immediate→Mailbox→Fifo） |
| SDL 3.4 | `SDL_SetWindowFullscreen`；沒設 `SDL_SetWindowFullscreenMode` 時是桌面無邊框全屏；macOS 是非同步（Spaces），結果用 `SDL_EVENT_WINDOW_ENTER_FULLSCREEN`／`LEAVE_FULLSCREEN` 回報 | — | `SDL_SetGPUSwapchainParameters`（`VSYNC` 一定支援；`IMMEDIATE`、`MAILBOX` 要先問 `SDL_WindowSupportsGPUPresentMode`） |
| master | — | — | 產品選項 `vsync`，開窗時定一次（`SdlGpuRenderDevice.cpp:308-312`），不能在遊戲中改 |

## 4. 設計

### 4.1 `window`：視窗模式

- `enum class WindowMode { Windowed, BorderlessFullscreen }`：Bevy 的 `WindowMode` 去掉獨佔全屏（使用者不做）；`BorderlessFullscreen` 不帶 `MonitorSelection`＝Bevy 的 `MonitorSelection::Current`（視窗所在的螢幕；不選螢幕是使用者的決定）。
- **開窗時**：`WindowSettings::mode`（預設 `Windowed`）。`WindowSettings::size` 是窗口時的大小。
- **遊戲中切換：`Message` `SetWindowMode{mode}`，不放進 `WindowRequest` 的狀態式欄位。** 理由同 `ResizeWindow`：玩家自己也會切（macOS 的綠色按鈕、⌃⌘F 會進全屏），常駐的請求會和玩家搶；`resizable` 玩家改不了，所以是 `WindowRequest` 的欄位。這就是現有的分法。
- **狀態照實回報**：後端寫 `Message` `WindowModeChanged{mode, time}`（SDL 的 ENTER／LEAVE_FULLSCREEN；新 World 的第一輪先寫目前的模式，SDL-7 例外 (2)）；`UpdateWindowState` 推出 `WindowState::mode`。全屏時的大小照舊由 `WindowResized` 回報（螢幕的物理像素），`window_camera`、`Letterbox` 因此照常運作，不用改。
- **`ResizeWindow` 在全屏中**：改的是回到窗口時的大小（全屏的畫面不變）。SDL 在全屏中的 `SDL_SetWindowSize` 各平台不一樣（Windows 丟掉、macOS 會去改全屏的視窗、dummy 直接回報新大小），所以由後端記住、回到窗口後再套。設置畫面因此不必管「先改模式還是先改大小」。

### 4.2 `sdl_window`：套用

- 開窗：`mode` 是全屏時建窗帶 `SDL_WINDOW_FULLSCREEN`（從不設 fullscreen mode＝桌面無邊框）。Windows 上邏輯大小的修正（`SDL_SetWindowSize` 換算）走 4.1 的「記住、回窗口再套」。
- `SetWindowMode`：adapter 呼叫 `SDL_SetWindowFullscreen`（審查 N1：不再設 `SDL_SetWindowFullscreenMode(nullptr)`——從沒設過模式，而它在 macOS 的轉場中會 `SDL_SyncWindow`）。切回窗口時 SDL 自己還原窗口的位置與大小（`SDL_video.c` 的 `windowed` 矩形）。
- 記住的大小：adapter 的請求側快取（同 `DeferredRequests`，不是事件側的狀態，SDL-7）。「SDL 的旗標是全屏」或「要了全屏、旗標還沒變」（只蓋 macOS 非同步進入的空檔；旗標變成全屏或看到 LEAVE 就不再算）時 `ResizeWindow` 記下來；每幀 `ApplyWindowRequests` 結尾在兩者都不成立時套上。**修訂（設計審查 B1）**：原稿用「遊戲最後要的模式」判斷，玩家自己離開全屏後大小會一直被扣住；改成一律問 SDL 的旗標，補測試「after the player left the game's fullscreen…」（先失敗、後修）。
- 同一幀的順序：模式先、大小後（窗口＋改大小：先離開全屏，同步的平台大小立刻套上，非同步的等 LEAVE）。
- live frame 中送的 `SetWindowMode` 和其他一次性請求一樣留到下一幀（`DeferredRequests`）。
- SDL 拒絕：照現有計數（`SDLWindowPump::RefusedRequests`）；一次性請求不重試（同 `ResizeWindow`）。

### 4.3 VSync：`render` 的資料、`sdl_gpu` 套用

- **放哪一層**：render World 的資料。`render` 加 `enum class PresentMode { AutoVsync, AutoNoVsync }`（Bevy 的兩個 Auto 值）與 Resource `Presentation{presentMode}`（主目標＝視窗的呈現方式），`render_world` 登記、新 World 放預設 `AutoVsync`。`sdl_gpu` 的 `GPUSubmit` 每幀讀、和上次套用的不同才改 swapchain。
- **遊戲中能切換**：是。設置畫面要即時生效；產品選項（master 的形）只能開窗時定一次，所以不採。
- **為什麼不放 `window`（偏離 Bevy）**：Bevy 的 `present_mode` 在 `Window` 上、`bevy_render` 抽出視窗來讀。GYO 的視窗不是實體、`window` 在列了 `sdl_window` 的那個 World，render World 沒有視窗（window 頁偏離表第 1 列）；`sdl_gpu` 只讀 render World（SDL-5）。放 `window` 的話要嘛 `sdl_gpu` 跨 World 讀、要嘛 `sdl_window` 經 Product 的 buffer 轉給 `sdl_gpu`——兩條都是繞過 render World 的資料流。present mode 是 swapchain 的性質，swapchain 歸渲染這一側。衝突點：兩個 World 的遊戲，設定在遊戲 World，要搬到 render World——作者在 render World 的 `Extract` 用 `ExtractRes<自己的設定>` 寫 `Presentation`（一個 System）；單一 World 直接寫。膠水量：一個 Resource、一個欄位。
- **只帶兩個 Auto 值**：使用者要的是開關；`Fifo`／`Immediate`／`Mailbox` 指定值沒有消費者，加值不破壞相容，有人點名再加（render 頁範囲の限界）。
- **退回**：`AutoVsync`＝`VSYNC`（SDL 沒有 FifoRelaxed）；`AutoNoVsync`＝`IMMEDIATE`→`MAILBOX`→`VSYNC`（照 Bevy；master 只有 IMMEDIATE→VSYNC，`MAILBOX` 也不等螢幕、不撕裂，比退回 VSYNC 更接近「關」）。選法是不含 SDL 的純函式（測得到），adapter 只問支援、呼叫 SDL。
- **實際用的模式**：`SDLGPUStatus::ActivePresentMode()`（`Vsync`／`Immediate`／`Mailbox`）；設定的次數 `PresentModeSets()`（審查 S2：讓「變了才改」從外面看得到）。SDL 拒絕時計數、記 `LastFailure`、不重試（重設 swapchain 會等 GPU 閒下來，不該每幀試）。
- **新 World**（審查 S1，選 (a)）：替換後的 `Presentation` 是預設、swapchain 跟著回 VSYNC；要保留就照 O-7 帶過去或由遊戲的設定重寫。不選 (b)「`sdl_gpu` 把目前的模式寫進新 World」：後端會變成渲染 World 資料的寫者，真相變成兩份；`window` 的 WN-3 寫的是 OS 那邊的事實（玩家也會改），present mode 只有遊戲會改。
- **VSync 關的後果**（審查 N4）：送出畫面不等螢幕，runner 也不等（`minFrameInterval` 預設 0），幀率吃滿 CPU／GPU；要上限的產品設 `SDLWindowOptions::minFrameInterval`。

## 5. 測試（外部可驗證）

| 項 | 怎麼測 | 哪裡跑 |
|---|---|---|
| `window`：`WindowModeChanged` → `WindowState::mode` | `window.module` | CI 三平台（cpu） |
| ENTER／LEAVE_FULLSCREEN → `WindowModeChanged` | `sdl_window.events` 的對照表測試（推 SDL 事件） | CI 三平台（dummy driver） |
| 切全屏、大小照實回報、切回原本的大小、全屏中改大小回窗口才生效、建窗就是全屏 | 顯示的視窗在 SDL 的 dummy driver 上（SDL 核心照常送 ENTER／LEAVE 與大小；dummy 的 `SetWindowSize` 在全屏中也會回報新大小，正好驗「記住、回窗口再套」） | CI 三平台 |
| live frame 中的 `SetWindowMode` 留到下一幀 | `sdl_window.events` 的 live frame 測試 | CI 三平台 |
| present mode 的退回順序 | 純函式的表 | CI 三平台（cpu） |
| `Presentation` 傳到 swapchain、切回來 | `sdl_gpu.frames`：顯示的視窗、`AutoNoVsync` → `ActivePresentMode` 在退回鏈上、幀照常送到；再切回 `Vsync` | Linux（Lavapipe＋Xvfb）、本機 |

**測不到、交使用者實機驗收（這台 Mac＋Windows）**：真的進／出無邊框全屏（macOS 的 Spaces 動畫、Windows 的無邊框）、macOS 非同步進入的空檔（送全屏的同一幀送大小：全屏不變、回窗口才生效；審查 S2 補充）、全屏時 `Letterbox` 的黑邊、回窗口的大小與位置、全屏中改解析度後回窗口、VSync 關時幀率不被鎖（Windows 的 D3D12／Vulkan 是否支援 IMMEDIATE）、macOS 綠色按鈕進全屏後 `WindowState::mode` 跟著變。

## 6. 範圍

- 做：第 4 節；`window`、`sdl`、`render` 頁的條文與說明；dev_log。
- 不做：設置畫面的 UI、設定的存檔與讀回（遊戲流程框架卡）；查螢幕、列解析度、選螢幕、獨佔全屏（使用者）；`Fifo` 等指定的 present mode（4.3）。
- 會撞到的：#341（os、net、http）、#338（Client 拆 World）都不碰 window、`sdl_window`、`sdl_gpu`、`render`；撞到時後合的一方 merge gyo2。

## 7. 判定（完成條件）

1. 本機 `ctest -L "cpu|gpu"` 全過（本機離線造成的除外，寫明）。
2. 相關突變全 killed：新邏輯（模式的回報、建窗的模式、記住的大小、live frame 的延後、退回順序、`Presentation` 傳到 swapchain）各至少一個。「變了才改 swapchain」由 `PresentModeSets()` 的次數守（`[gp-present-only-on-change]`）。
3. 設計審查（Opus／high）的 blocker 處理完。
4. draft PR 的 L1 三平台綠。
