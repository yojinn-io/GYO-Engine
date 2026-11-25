# 2026-11-25 視窗模式（窗口／無邊框全屏）與 VSync（`v8-arch-gyo2-window-mode-vsync`）

Owner：Engine（`window`、`render` 的契約）＋Plugin（`sdl_window`、`sdl_gpu`）。GYO2 主會話開的 Engine 任務卡。分支 `claude/gyo2-window-mode-vsync-fc8cba`（開向 `gyo2`）。作者 Opus 5.5／high。宣告：[window-mode-vsync-declaration](../architecture/target/v2/window-mode-vsync-declaration.zh-Hant.md)。

## 使用者的決定

2026-11-25：「OK，就這個尺寸，然後還有全屏和窗口選項。」問全屏種類與 VSync 時：「開，無邊框，VSync 要」。不做查螢幕、列解析度、選螢幕、獨佔全屏。

## 做了什麼

1. `window`：`WindowMode`（`Windowed`、`BorderlessFullscreen`）、`WindowSettings::mode`、一次性的 `SetWindowMode`（玩家也會切，理由同 `ResizeWindow`）、回報的 `WindowModeChanged` → `WindowState::mode`。全屏中的 `ResizeWindow` 是回到窗口時的大小。
2. `sdl_window`：`SDL_SetWindowFullscreenMode(nullptr)`＋`SDL_SetWindowFullscreen`（桌面無邊框）；`ENTER`／`LEAVE_FULLSCREEN` → `WindowModeChanged`；新 World 的第一輪寫目前的模式。全屏中的大小由 adapter 記住、回窗口（旗標也不是全屏）後才給 SDL——SDL 在全屏中的 `SDL_SetWindowSize` 各平台不同（Windows 丟掉、macOS 改全屏的視窗）。live frame 中的 `SetWindowMode` 留到下一幀。
3. `render`：`Presentation{presentMode}`（`AutoVsync`／`AutoNoVsync`，Bevy 的兩個 Auto 值），`render_world` 登記，渲染 World 的資料。偏離 Bevy（`present_mode` 在 `Window` 上）：GYO 的渲染 World 沒有視窗、`sdl_gpu` 只讀渲染 World（SDL-5）。
4. `sdl_gpu`：`Presentation` 變了才 `SDL_SetGPUSwapchainParameters`；`AutoNoVsync`＝IMMEDIATE→MAILBOX→VSYNC（Bevy 的退回鏈；master 只有 IMMEDIATE→VSYNC）；`SDLGPUStatus::ActivePresentMode()`、`RefusedPresentModes()`。

## 驗證

- 本機（macOS、Metal）：相關的 ctest 全過；`ctest -L "cpu|gpu"` 257 個中 254 過——`cmake.PluginSelection`（本機離線，既有）與 `gyo_os_tests`、`os.wake_fallback`（`OS wake signal loses no signal…` 一案 44 s 逾時；當時機器負載 94，本批沒動 os）。
- dummy 驅動上顯示的視窗：切全屏、大小＝螢幕、相機的主目標跟著、全屏中改大小不變、回窗口用新大小、建窗就是全屏、live frame 延後。Metal 上 VSync 關＝`Immediate`、切回＝`Vsync`。
- 突變：第一輪 window 側 10 個中 8 killed——`wn-mode-first`（測試的序列頭尾相同）、`sw-snapshot-mode-missing`（建窗時 SDL 自己送 ENTER，蓋過第一輪的寫入）存活，改測試（替換後的 World 第一輪知道是全屏）；gpu 側 4/4。修正後重跑見 PR。
- 設計審查（Opus／high，同模型）：B1（玩家自己離開全屏後大小一直被扣住：改成一律問 SDL 的旗標，先寫失敗的測試再修）、S1（替換 World 時 VSync 回到開：選 (a)，寫明、補測試）、S2（「變了才改」沒人守：加 `PresentModeSets()` 與突變）、N1～N8（N6「enum 從 1 起」不改：同檔 `FrameOutcome`、錯誤碼的慣例）。報告在主 checkout `build/evidence/gyo2-window-mode-vsync/`。

## Mac 實機驗收（2026-11-25，Intel、5K、縮放 2）

- 驗收程式 `examples/window_mode`＋`acceptance_macos.sh`（日誌在主 checkout `build/evidence/gyo2-window-mode-vsync/`）。
- 使用者跑 `--fullscreen` 抓到：以全屏建的窗按 W 回到 5120×2756（螢幕大小），不是 1280×720——macOS 離開 Space 時回到建窗時的 frame。修：建窗就是全屏時一律記住 `WindowSettings::size`。修後自動流程兩種開法都對（回窗口 2560×1440 px；全屏中的 1600×900 回窗口後 3200×1800）；live resize 中照常畫。使用者：「沒問題。」

## 交實機（Windows）

真的進出無邊框全屏（Spaces 動畫、非同步離開）、全屏的 `Letterbox` 黑邊、回窗口的大小與位置、全屏中改解析度後回窗口、VSync 關時幀率不被鎖（Windows D3D12／Vulkan 取到哪一個）、macOS 綠色按鈕後 `WindowState::mode` 跟著變、Windows 縮放下建窗就是全屏時回窗口的大小。
