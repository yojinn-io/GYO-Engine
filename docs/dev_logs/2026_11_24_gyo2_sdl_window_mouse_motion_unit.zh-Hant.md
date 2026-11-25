# 2026-11-24 sdl_window：相對量的單位給產品選（`v8-arch-gyo2-relative-mouse-system-scale`）

Owner：Engine（`sdl_window`、`input` 的契約）。Object_Connect 移植實作卡做的（拖線鎖游標 #328 的設計審查 S1；主會話照新 AGENTS 判選 (1)：作者選，§1-1）。分支 `claude/gyo2-sdl-window-motion-unit-d5884a`（開向 `gyo2`）。作者 Opus 5.5／high。

## 前提

Engine 要解決：`MouseMotion` 的單位只有後端知道（相對模式下 Windows 是 raw input 的滑鼠計數、沒有指標速度；macOS 是加速過的 points），遊戲換算不了。鎖住游標又自己畫游標的遊戲（Object_Connect）要游標的速度；用滑鼠轉視角的遊戲（PvP）要 raw。

## 做了什麼

1. `SDLWindowOptions::mouseMotion`（`MouseMotionUnit`）：`Device`（預設，照 SDL 給的，行為不變）／`LogicalPixels`（視窗活著時設 `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE=1`、關窗還原；`MouseMotion` 乘上每個視窗座標的邏輯像素數）。
2. adapter：`BackendSettings::logicalMotion`、`Translate` 多一個 `MotionUnit`、純函式 `MotionDelta`。
3. 契約：`Input.hpp` 的 `MouseMotion` 註解、`input` 頁（契約與 Bevy 對照表）、`sdl` 頁（對應表、hint、相對量的單位）。

## 對照

winit（Bevy）：`DeviceEvent::MouseMotion` 永遠 raw、`CursorMoved` 是位置，沒有選擇。GYO 多一個選項：SDL 3.4 在非相對模式沒有 raw，而遊戲自己畫游標時要游標的速度。

## 驗證

- 本機（Intel Mac）：`sdl_window.*` 過。新測試 [sw-motion-device]、[sw-motion-logical]（純函式，Windows 150% 的係數）、[sw-motion-default-raw]、[sw-motion-hint-held]（選項經 Product 傳到 adapter）、[sw-motion-logical-pump]、[sw-motion-hint-reset]。手動突變見 PR。
- 範圍的限制：dummy 驅動的視窗座標就是邏輯像素（係數 1），「Translate 有沒有把選項用上」只由 `MotionDelta` 的測試與審查守。Windows 上的實際速度沒有在實機量。
